/*
 * keepwifi.c - hold off auto-standby so Wi-Fi stays up (see keepwifi.h).
 *
 * A 1 Hz sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND) keeps
 * the auto-standby timer from ever expiring, and leaves the screen's dim/off
 * timers alone. No power lock: the tick alone is enough. Known limitation:
 * while ticking, a power-button press only turns the screen off - the Vita
 * does not sleep (tested on device; with nothing ticking, it sleeps).
 */
#include <psp2/kernel/processmgr.h>   /* sceKernelPowerTick                      */
#include <psp2/kernel/threadmgr.h>
#include <psp2/power.h>               /* scePowerIsPowerOnline / IsBatteryCharging / GetBatteryLifePercent */

#include <psp2/registrymgr.h>   /* sceRegMgrGetKeyInt - the user's Wi-Fi intent  */

#include "keepwifi.h"
#include "keepalive.h"        /* keepalive_tick - gateway heartbeat, anti-idle */
#include "netjoin.h"          /* wq_net_rejoin - command the netctl daemon      */
#include "settings_entry.h"   /* wq_settings_pump - driven off this tick       */
#include "diag.h"             /* diag_sample - netctl connection state         */
#include "advanced.h"         /* wq_adv_scan_read - roam pre-check scan        */
#include "log.h"
#include "../include/wifiturbo.h"
#include "../include/wifiturbo_kernel_api.h"  /* wqRoamAssist*          */

#include <string.h>

/* ---- reconnect watcher: keepwifi ON => Wi-Fi is never left off --------------
 * If we ever end up disconnected while the user DOES want Wi-Fi on (their
 * /CONFIG/NET wifi_flag), re-assert the radio so the shell rejoins the saved
 * network. Confirmed NIDs: SceWlanBt / library SceWlan 0x22821EA3,
 * sceWlanGetConfiguration 0x70AF9BD6 (returns a bitmask, &9 = radio on),
 * sceWlanSetConfiguration 0xDBE8AEF2 (enable). Only ever turns the radio ON
 * (never off), from this suspend-surviving thread - no self-lockout. A user who
 * turned Wi-Fi off (wifi_flag=0) is respected and left alone. */
/* The user "SceWlan" library (module SceWlanBt, lib NID 0x22821EA3) is a
 * kernel:false export, so VitaSDK ships a user link stub (SceWlanBt_stub_weak):
 * call the functions directly rather than resolving at runtime. An earlier
 * taiGetModuleExportFunc("SceWlanBt", ...) returned n/a on-device because that
 * kernel module is not in this user process's address space; the link stub is
 * SceShell's own import thunk, which binds correctly. */
extern int sceWlanGetConfiguration(void);   /* NID 0x70AF9BD6; &9 = radio on    */
extern int sceWlanSetConfiguration(int en); /* NID 0xDBE8AEF2; 1 = enable radio */

static int user_wants_wifi(void)
{
	int v = 0;
	/* default to "wants wifi" if the key can't be read */
	return sceRegMgrGetKeyInt("/CONFIG/NET", "wifi_flag", &v) == 0 ? (v != 0) : 1;
}

/* Seconds spent continuously disconnected (reset on connect / unknown state). */
static int g_disc_ticks;

/* Log every netctl state transition, so a reconnect lever's effect is visible
 * (0 disconnected, 1 connecting, 2 finalizing/IP, 3 connected; -1 unknown). */
static int g_last_state = -2;

static void log_state(const WqHudState *s, int ok)
{
	int st = ok ? s->netctl_state : -1;
	if (st == g_last_state)
		return;
	WQ_LOGI("keepwifi: netctl state %d -> %d (ssid=%s rssi=%d)", g_last_state, st,
	        ok && s->ssid[0] ? s->ssid : "-", ok ? s->rssi_dbm : 0);
	g_last_state = st;
}

/* ---- icmtest: prove IcmConnect alone reconnects from the given-up state -----
 * A netctl disconnect sets the daemon's [+0x28]&1 latch, the same one a boot
 * auto-join give-up leaves, so this
 * reproduces the boot case without a reboot. Runs on this worker (the :1339
 * session dies with the link): disconnect, wait, IcmConnect ONLY (the watcher
 * is paused), watch the state for 60s, then fall back to the proven rejoin if
 * nothing came back - it can never strand the Vita. */
#define WQ_ICMTEST_KICK_S   4
#define WQ_ICMTEST_WAIT_S   60

static volatile int g_icmtest;      /* seconds into the test; 0 = not running    */

void keepwifi_icm_test(void) { g_icmtest = 1; }

static void icm_test_tick(void)
{
	WqHudState s;
	int ok = diag_sample(&s) == WQ_OK;
	log_state(&s, ok);
	int t = g_icmtest++;
	if (t == 1) {
		WQ_LOGW("icmtest: netdown");
		wq_net_disconnect();
	} else if (t == 1 + WQ_ICMTEST_KICK_S) {
		WQ_LOGW("icmtest: IcmConnect only (watcher paused)");
		wq_net_icm_connect();
	} else if (t > 1 + WQ_ICMTEST_KICK_S && ok && s.netctl_state == WQ_NETCTL_CONNECTED) {
		WQ_LOGW("icmtest: PASS - reconnected %ds after IcmConnect",
		        t - 1 - WQ_ICMTEST_KICK_S);
		g_icmtest = 0;
	} else if (t >= 1 + WQ_ICMTEST_KICK_S + WQ_ICMTEST_WAIT_S) {
		WQ_LOGW("icmtest: FAIL - no reconnect %ds after IcmConnect; fallback rejoin",
		        WQ_ICMTEST_WAIT_S);
		wq_net_rejoin();
		g_icmtest = 0;
	}
}

/* ---- Smart Mesh Roaming -----------------------------------------------------
 * On sustained weak signal, move to a better AP of
 * the SAME SSID:
 *   1. optional pre-check: a libnetctl scan (advanced.c reader). If it works and
 *      shows no other BSS of this SSID at least WQ_ROAM_MARGIN_DB stronger than
 *      the current link, skip (a reconnect would only land on the same AP). If
 *      it finds one, that BSSID becomes the kernel's preferred target. If the
 *      scan is unavailable or fails, roam blind (strongest wins);
 *   2. arm the kernel join-path assist (kernel/roamassist.c, one-shot): the
 *      next SceWlanBt join sweeps every channel and tries the strongest
 *      same-SSID BSS (or the target) first;
 *   3. wq_net_reassociate(): real libnetctl disconnect + IcmConnect (proven);
 *   4. watch for up to WQ_ROAM_GIVEUP_S, owning reconnects meanwhile; log the
 *      BSSID before/after. Fallback if the link does not come back: disarm,
 *      plain IcmConnect, the event-flag rejoin, then hand back to the keepwifi
 *      watcher - never stranded.
 * Anti-flap: a cooldown between attempts, and a roam only when the pre-check
 * sees an AP at least WQ_ROAM_MARGIN_DB stronger. A blind roam (scan unusable)
 * is allowed once per weak episode (recover above WQ_ROAM_OK_DBM, or move to
 * another AP, to re-arm); below WQ_ROAM_DIRE_DBM an empty pre-check counts as
 * unusable, since the join scan sees every channel and the sweep may not. Default OFF ('roam on' / the Settings row). */
#define WQ_ROAM_WEAK_DBM    (-75)   /* sustained below this => consider roaming  */
#define WQ_ROAM_OK_DBM      (-70)   /* recover above this => re-arm for next time*/
#define WQ_ROAM_WEAK_SECS   6       /* consecutive weak samples before firing    */
#define WQ_ROAM_COOLDOWN_S  60      /* minimum seconds between roams             */
#define WQ_ROAM_RECHECK_S   15      /* nothing better found: look again after    */
#define WQ_ROAM_DIRE_DBM    (-82)   /* this weak: roam blind if pre-check is empty*/
#define WQ_ROAM_SWEEPS      3       /* pre-check sweeps merged (off-channel APs  */
                                    /* are hit-and-miss in any single one)       */
#define WQ_ROAM_MARGIN_DB   6       /* pre-check: candidate must beat cur by this */
#define WQ_ROAM_SCAN_MAX    16
#define WQ_ROAM_FB_ICM_S    20      /* not back by then: disarm + plain IcmConnect*/
#define WQ_ROAM_FB_REJOIN_S 35      /* then the event-flag rejoin                */
#define WQ_ROAM_FB_ICM2_S   50      /* then IcmConnect once more                 */
#define WQ_ROAM_GIVEUP_S    60      /* then hand back to reconnect_watch         */

static volatile int g_roam_on;
static volatile int g_roam_req;     /* ':1339 roam now' -> forced roam          */
static volatile int g_roam_precheck = 1;
static int g_roam_weak;             /* consecutive weak samples                  */
static int g_roam_armed = 1;        /* one BLIND roam per weak episode           */
static int g_roam_cool;             /* cooldown seconds remaining                */

/* In-flight roam (owned by roam_tick while g_roam_t > 0). */
static volatile int g_roam_t;       /* seconds since the reassociate; 0 = idle   */
static int g_roam_saw_down;
static int g_roam_before_rssi;
static uint32_t g_roam_seq0;
static uint8_t g_roam_before[6];
static char g_roam_status[96] = "no roam yet";

void keepwifi_set_roam(int on)  { g_roam_on = on ? 1 : 0; g_roam_weak = 0;
                                  WQ_LOGI("roam: Smart Mesh Roaming %s", on ? "ON" : "off"); }
int  keepwifi_roam_enabled(void) { return g_roam_on; }
void keepwifi_roam_now(void)     { g_roam_req = 1; }
void keepwifi_set_roam_precheck(int on) { g_roam_precheck = on ? 1 : 0; }
int  keepwifi_roam_precheck(void) { return g_roam_precheck; }
int  keepwifi_roam_busy(void)    { return g_roam_t > 0 || g_roam_req; }
const char *keepwifi_roam_last(void) { return g_roam_status; }

static void mac_str(char *o, const uint8_t *m)
{
	static const char hx[] = "0123456789abcdef";
	for (int i = 0; i < 6; i++) {
		*o++ = hx[m[i] >> 4];
		*o++ = hx[m[i] & 15];
		if (i < 5) *o++ = ':';
	}
	*o = 0;
}

static int mac_eq(const uint8_t *a, const uint8_t *b)
{
	return memcmp(a, b, 6) == 0;
}

static void set_status(const char *a, const char *b, const char *c)
{
	char *p = g_roam_status, *e = g_roam_status + sizeof(g_roam_status) - 1;
	const char *parts[3] = { a, b, c };
	for (int k = 0; k < 3; k++)
		for (const char *s = parts[k]; s && *s && p < e; s++) *p++ = *s;
	*p = 0;
}

/* Pre-check scan. Returns 1 = roam (target filled if *has_target), 0 = skip
 * (scan worked, nothing better), -1 = scan unusable (roam blind). */
static int roam_precheck(const WqHudState *cur, uint8_t target[6], int *has_target)
{
	static WqScanEntry sc[WQ_ROAM_SCAN_MAX], all[WQ_ROAM_SCAN_MAX];
	*has_target = 0;
	if (!g_roam_precheck || !wq_adv_scan_available())
		return -1;
	/* Merge a few sweeps, keeping each BSSID's best reading. */
	int na = 0, best = -1, seen_self = 0, sweeps = 0;
	for (; sweeps < WQ_ROAM_SWEEPS; sweeps++) {
		int n = wq_adv_scan_read(sc, WQ_ROAM_SCAN_MAX);
		for (int i = 0; i < n; i++) {
			int k = 0;
			while (k < na && !mac_eq(all[k].bssid, sc[i].bssid)) k++;
			if (k == na) {
				if (na == WQ_ROAM_SCAN_MAX) continue;
				all[na++] = sc[i];
			} else if (sc[i].rssi_dbm > all[k].rssi_dbm) {
				all[k].rssi_dbm = sc[i].rssi_dbm;
			}
		}
		best = -1;
		for (int i = 0; i < na; i++) {
			if (strncmp(all[i].ssid, cur->ssid, WQ_SSID_MAX) != 0)
				continue;
			if (mac_eq(all[i].bssid, cur->bssid)) { seen_self = 1; continue; }
			if (best < 0 || all[i].rssi_dbm > all[best].rssi_dbm)
				best = i;
		}
		if (best >= 0 && all[best].rssi_dbm >= cur->rssi_dbm + WQ_ROAM_MARGIN_DB) {
			sweeps++;
			break;                        /* good enough: stop scanning   */
		}
	}
	if (na == 0) {
		WQ_LOGW("roam: pre-check scan unusable - roaming blind");
		return -1;
	}
	if (best < 0) {
		/* Our own AP missing too means the scan did not cover our SSID's
		 * channels - don't trust its "nothing better". */
		if (!seen_self || cur->rssi_dbm <= WQ_ROAM_DIRE_DBM) {
			WQ_LOGW("roam: pre-check (%d sweeps) found no other AP at %ddBm - roaming blind",
			        sweeps, cur->rssi_dbm);
			return -1;
		}
		WQ_LOGI("roam: pre-check (%d sweeps): no other AP of '%s' in range - skip",
		        sweeps, cur->ssid);
		return 0;
	}
	char m[18];
	mac_str(m, all[best].bssid);
	if (all[best].rssi_dbm < cur->rssi_dbm + WQ_ROAM_MARGIN_DB) {
		if (cur->rssi_dbm <= WQ_ROAM_DIRE_DBM) {
			WQ_LOGW("roam: pre-check: best other %s %ddBm, cur %ddBm - roaming blind",
			        m, (int)all[best].rssi_dbm, cur->rssi_dbm);
			return -1;
		}
		WQ_LOGI("roam: pre-check: best other %s ch%d %ddBm vs cur %ddBm - skip",
		        m, (int)all[best].channel, (int)all[best].rssi_dbm, cur->rssi_dbm);
		return 0;
	}
	WQ_LOGI("roam: pre-check (%d sweeps): target %s ch%d %ddBm (cur %ddBm)",
	        sweeps, m, (int)all[best].channel, (int)all[best].rssi_dbm, cur->rssi_dbm);
	memcpy(target, all[best].bssid, 6);
	*has_target = 1;
	return 1;
}

/* Fire one roam. forced = manual 'roam now' (ignores the pre-check veto);
 * allow_blind = roam even when the pre-check scan is unusable.
 * Returns 0 = skipped, 1 = roamed towards a target, 2 = roamed blind. */
static int roam_start(const WqHudState *cur, int forced, int allow_blind)
{
	char m[18];
	mac_str(m, cur->bssid);
	WQ_LOGW("roam: %s - leaving %s ch%u %ddBm ('%s')", forced ? "manual" : "weak signal",
	        m, (unsigned)cur->channel, cur->rssi_dbm, cur->ssid);

	uint8_t target[6] = { 0 };
	int has_target = 0;
	int pc = roam_precheck(cur, target, &has_target);
	if (pc == 0 && !forced) {
		set_status("skipped: no better AP than ", m, "");
		return 0;
	}
	if (pc < 0 && !forced && !allow_blind) {
		set_status("skipped: scan unusable", "", "");
		return 0;
	}

	WqRoamAssistStatus ks;
	memset(&ks, 0, sizeof(ks));
	wqRoamAssistGetStatus(&ks);
	g_roam_seq0 = ks.seq;
	unsigned mac0 = target[0] | (target[1] << 8) | (target[2] << 16) | ((unsigned)target[3] << 24);
	unsigned mac1 = target[4] | (target[5] << 8);
	int r = wqRoamAssistArm(WQ_RA_ARM | (has_target ? WQ_RA_TARGET : 0), mac0, mac1);
	if (r != WQ_OK) {
		/* Without the assist a reconnect lands on the same AP: don't drop
		 * the link for nothing. */
		WQ_LOGW("roam: kernel assist unavailable (%d) - not roaming", r);
		set_status("failed: kernel assist unavailable", "", "");
		return 0;
	}
	memcpy(g_roam_before, cur->bssid, 6);
	g_roam_before_rssi = cur->rssi_dbm;
	g_roam_saw_down = 0;
	r = wq_net_reassociate();         /* disconnect, 1.5 s, IcmConnect          */
	if (r != WQ_OK) {
		/* Disconnect unresolved/failed: the link is most likely untouched.
		 * Keep watching anyway - if it did drop, the fallback recovers it. */
		wqRoamAssistArm(0, 0, 0);
		WQ_LOGW("roam: reassociate failed (%d) - assist disarmed", r);
		set_status("failed: reassociate error", "", "");
	}
	g_roam_t = 1;
	return has_target ? 1 : 2;
}

/* One second of an in-flight roam. Owns reconnects until it sets g_roam_t = 0. */
static void roam_tick(void)
{
	int t = g_roam_t++;
	WqHudState s;
	int ok = diag_sample(&s) == WQ_OK;
	if (ok && s.netctl_state != WQ_NETCTL_CONNECTED)
		g_roam_saw_down = 1;

	if (ok && s.netctl_state == WQ_NETCTL_CONNECTED && (g_roam_saw_down || t >= 10)) {
		WqRoamAssistStatus ks;
		memset(&ks, 0, sizeof(ks));
		wqRoamAssistGetStatus(&ks);
		wqRoamAssistArm(0, 0, 0);             /* never leave it armed        */
		char a[18], b[18];
		mac_str(a, g_roam_before);
		mac_str(b, s.bssid);
		int moved = !mac_eq(g_roam_before, s.bssid);
		WQ_LOGW("roam: back in %ds: %s %ddBm -> %s ch%u %ddBm (%s)", t, a,
		        g_roam_before_rssi, b, (unsigned)s.channel, s.rssi_dbm,
		        moved ? "MOVED" : g_roam_saw_down ? "same AP" : "link never dropped");
		if (ks.seq != g_roam_seq0) {
			char f[18], k[18], j[18];
			mac_str(f, ks.first_bssid);
			mac_str(k, ks.best_bssid);
			mac_str(j, ks.joined_bssid);
			WQ_LOGI("roam: kernel: %u BSS (%u ours) scans=%u reordered=%u "
			        "stock-first %s -%u ch%u", ks.n_entries, ks.n_match, ks.scans,
			        ks.reordered, f, ks.first_rssi, ks.first_ch);
			WQ_LOGI("roam: kernel: put-first %s -%u ch%u hit=%u joined %s "
			        "ret=0x%08x shell-ch %u", k, ks.best_rssi, ks.best_ch,
			        ks.target_hit, j, (unsigned)ks.join_ret, ks.orig_ch[0]);
		} else {
			WQ_LOGW("roam: kernel assist was NOT used by this join (armed=%u expired=%u)",
			        ks.armed, ks.expired);
		}
		if (moved)
			set_status("moved ", a, " -> ");
		else
			set_status(g_roam_saw_down ? "same AP " : "link never dropped ", a, "");
		if (moved) {
			g_roam_armed = 1;                 /* new AP: blind roam allowed again */
			char *p = g_roam_status;
			while (*p) p++;
			if (p + 18 < g_roam_status + sizeof(g_roam_status)) mac_str(p, s.bssid);
		}
		g_roam_t = 0;
		return;
	}

	if (t == WQ_ROAM_FB_ICM_S) {
		WQ_LOGW("roam: not back after %ds - fallback: disarm assist + plain IcmConnect", t);
		wqRoamAssistArm(0, 0, 0);
		wq_net_icm_connect();
	} else if (t == WQ_ROAM_FB_REJOIN_S) {
		WQ_LOGW("roam: not back after %ds - fallback: netctl rejoin", t);
		wq_net_rejoin();
	} else if (t == WQ_ROAM_FB_ICM2_S) {
		WQ_LOGW("roam: not back after %ds - fallback: IcmConnect again", t);
		wq_net_icm_connect();
	} else if (t >= WQ_ROAM_GIVEUP_S) {
		WQ_LOGE("roam: not back after %ds - handing over to the keepwifi watcher", t);
		set_status("failed: link not back, keepwifi watcher took over", "", "");
		wqRoamAssistArm(0, 0, 0);
		g_roam_t = 0;
	}
}

static void smart_roam(void)
{
	if (g_roam_cool > 0) g_roam_cool--;
	if (g_roam_t > 0)
		return;                           /* roam_tick owns the link          */
	if (g_roam_req) {
		g_roam_req = 0;
		WqHudState s;
		if (diag_sample(&s) != WQ_OK || s.netctl_state != WQ_NETCTL_CONNECTED) {
			WQ_LOGW("roam now: not connected - ignored");
			set_status("roam now ignored: not connected", "", "");
			return;
		}
		g_roam_cool = WQ_ROAM_COOLDOWN_S;
		roam_start(&s, 1, 1);
		return;
	}
	if (!g_roam_on)
		return;
	WqHudState s;
	if (diag_sample(&s) != WQ_OK || s.netctl_state != WQ_NETCTL_CONNECTED) {
		g_roam_weak = 0;                  /* only roam while connected            */
		return;
	}
	int rssi = s.rssi_dbm;
	if (rssi >= WQ_ROAM_OK_DBM) {         /* healthy: re-arm                      */
		g_roam_armed = 1;
		g_roam_weak = 0;
		return;
	}
	if (rssi > WQ_ROAM_WEAK_DBM) {        /* middling: decay, so one good sample  */
		if (g_roam_weak > 0) g_roam_weak--;   /* doesn't restart the count        */
		return;
	}
	if (++g_roam_weak < WQ_ROAM_WEAK_SECS)
		return;                           /* not sustained yet                    */
	g_roam_weak = 0;
	if (g_roam_cool > 0)
		return;                           /* cooling down after the last attempt  */
	/* Still weak after a roam is fine: try again once cooled, as long as the
	 * pre-check sees a clearly stronger AP. Blind roams stay one per episode. */
	int r = roam_start(&s, 0, g_roam_armed);
	g_roam_cool = r ? WQ_ROAM_COOLDOWN_S : WQ_ROAM_RECHECK_S;
	if (r == 2)
		g_roam_armed = 0;
}

static void reconnect_watch(void)
{
	if (!user_wants_wifi())
		return;                         /* user disabled Wi-Fi - respect it     */
	WqHudState s;
	int ok = diag_sample(&s) == WQ_OK;
	log_state(&s, ok);
	if (!ok) {
		g_disc_ticks = 0;               /* state unknown - do nothing           */
		return;
	}
	if (s.netctl_state != WQ_NETCTL_DISCONNECTED) {
		g_disc_ticks = 0;
		return;                         /* connected, or a connect in progress   */
	}                                   /* - never prod a running connect        */

	/* Confirmed disconnected while Wi-Fi should be on. */
	g_disc_ticks++;
	int cfg = sceWlanGetConfiguration();
	if ((cfg & 9) == 0) {
		/* Radio off: turn it back on (monotonic OFF->ON, no live link to lose). */
		WQ_LOGW("keepwifi: disconnected + radio off -> re-enabling Wi-Fi");
		sceWlanSetConfiguration(1);
		return;
	}
	/* Radio on but not associated. Two levers, never interleaved closely (the
	 * rejoin's disconnect half would abort an ICM connect in flight):
	 *  - 5s: the event-flag edge (wq_net_rejoin), PROVEN for a mid-session
	 *    drop. Reconnects the staged (same) AP; ignored after a boot give-up.
	 *  - 12s, then every 30s: IcmConnect (wq_net_icm_connect), the shell's own
	 *    "auto-connect now" - a full saved-profile scan/rank/join. PROVEN after
	 *    a boot give-up (state 0->1 1s after the kick, connected ~3s later).
	 *    30s spacing never overlaps a run. */
	if (g_disc_ticks == 5) {
		WQ_LOGW("keepwifi: disconnected %ds -> netctl rejoin (same AP)", g_disc_ticks);
		wq_net_rejoin();
	} else if (g_disc_ticks >= 12 && (g_disc_ticks - 12) % 30 == 0) {
		WQ_LOGW("keepwifi: disconnected %ds -> IcmConnect (saved-network auto-connect)",
		        g_disc_ticks);
		wq_net_icm_connect();
	}
}

static SceUID       g_thid = -1;
static volatile int g_enabled = 1;   /* default ON - the user wants Wi-Fi kept up */
static volatile int g_autorecon = 1;  /* "Auto-reconnect on drop" - default ON        */
static volatile int g_run     = 0;

/* Should we hold auto-suspend off this tick?
 *
 * Only while the Vita is running on its BATTERY. On external power with the
 * battery not yet full we must NOT hold suspend off, because the console only
 * tops up the final ~10% in standby; a permanent hold therefore caps charging
 * at ~89-90% (very visible on the PCH-1000 OLED). Releasing the hold lets an
 * IDLE, charging Vita reach standby and charge to 100% - active use (plugged in
 * and playing) keeps itself awake through the system's own activity timer, so
 * that case is unaffected and Wi-Fi stays up. Any read failure is treated as
 * "on battery" so Wi-Fi is kept up (fail toward the feature, never toward a
 * silent charge cap). See GitHub issue #6. */
static int keepwifi_hold_suspend(void)
{
	if (!g_enabled)
		return 0;
	if (scePowerIsPowerOnline() != 1)   /* on battery (or unknown) -> keep Wi-Fi up */
		return 1;
	/* On external power: hold only once the battery is full, so charging can
	 * complete. IsBatteryCharging can read 0 at the stalled cap, so also gate on
	 * the percentage. */
	if (scePowerGetBatteryLifePercent() >= 100 && scePowerIsBatteryCharging() != 1)
		return 1;
	return 0;                           /* charging/topping up -> allow standby   */
}

static int keepwifi_thread(SceSize args, void *argp)
{
	(void)args; (void)argp;
	while (g_run) {
		if (keepwifi_hold_suspend())      /* standby only: screen may still dim/turn off */
			sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND);
		if (g_icmtest) {
			icm_test_tick();     /* owns reconnects while the test runs         */
		} else if (g_roam_t > 0) {
			roam_tick();         /* owns reconnects while a roam is in flight   */
		} else {
			if (g_enabled)
				keepalive_tick();    /* heartbeat: stop the AP ageing an idle Vita  */
			if (g_autorecon)
				reconnect_watch();   /* auto-reconnect on => reconnect if dropped   */
		}
		smart_roam();         /* independent of keepwifi: roam on weak signal   */
		wq_settings_pump();   /* drain the Settings mailbox + publish state     */
		sceKernelDelayThread(1000 * 1000);
	}
	return 0;
}

int keepwifi_start(void)
{
	if (g_thid >= 0)
		return WQ_OK;
	g_run = 1;
	g_thid = sceKernelCreateThread("WqKeepWifi", keepwifi_thread,
	                               0x40, 0x4000, 0, 0, NULL);
	if (g_thid < 0) {
		g_run = 0;
		WQ_LOGE("keepwifi: thread create failed (%d)", g_thid);
		return WQ_ERR_NOT_READY;
	}
	sceKernelStartThread(g_thid, 0, NULL);
	WQ_LOGI("keepwifi: started (default %s)", g_enabled ? "ON" : "off");
	return WQ_OK;
}

void keepwifi_stop(void)
{
	if (g_thid < 0)
		return;
	g_run = 0;
	sceKernelWaitThreadEnd(g_thid, NULL, NULL);
	sceKernelDeleteThread(g_thid);
	g_thid = -1;
}

void keepwifi_set_enabled(int on)
{
	g_enabled = on ? 1 : 0;
	if (keepwifi_hold_suspend())   /* only on battery - don't cap charging on AC */
		sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND);
	WQ_LOGI("keepwifi: %s by request", g_enabled ? "ON" : "off");
}

int keepwifi_is_enabled(void) { return g_enabled; }

/* "Auto-reconnect on drop": the reconnect watcher (rejoin at 5 s, then
 * IcmConnect at 12 s and every 30 s). Independent of "Keep Wi-Fi on", which
 * only holds off standby and sends the idle keep-alive. */
void keepwifi_set_autoreconnect(int on)
{
	g_autorecon = on ? 1 : 0;
	g_disc_ticks = 0;
	WQ_LOGI("keepwifi: auto-reconnect %s", g_autorecon ? "ON" : "off");
}
