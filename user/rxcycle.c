/*
 * rxcycle.c - EXPERIMENTAL: power-cycle the Wi-Fi chip from the console.
 *
 * kernel/rxring.c edits SceWlanBt's cached firmware image; the chip only loads
 * it at its next power-up, and it powers down only when WLAN and BT are both
 * off. This does the same Wi-Fi off/on the Settings master switch does -
 * sceWlanSetConfiguration(0), 5 s, (1) - from a thread on the console, so it
 * works even though the switch drops our own link. It does
 * not touch /CONFIG/NET wifi_flag, so the user's Wi-Fi setting is unchanged.
 * The chip only powers down with Bluetooth off too, so if Bluetooth is on it is
 * switched off for the cycle and back on afterwards (sceBtSetConfiguration, the
 * same call SceShell's Bluetooth toggle makes; /CONFIG/BT is not touched).
 * Paired devices drop and reconnect.
 *
 * Watchdog: a second thread gives the link RXC_DEADLINE_S to come back
 * (netctl connected and the radio on). If it does not, nobody can reach the
 * console, so it cold-resets it: a reboot always returns to the stock image,
 * and the network comes back. It is a separate thread so it still fires if
 * the worker blocks inside the driver.
 *
 * require_link = 0 (the radio was on but not associated before the cycle):
 * success is the radio being back on; no reboot for a missing network.
 * marker: a file removed once the cycle succeeds. If the watchdog reboots, the
 * file survives, and the next boot sees it (user/rxpreset.c) and falls back
 * to stock instead of retrying the same firmware.
 *
 * Guard: with 'guard <min>', the watchdog keeps running for that long after
 * the link is back and reboots if the link then stays down RXC_GUARD_DOWN_S in
 * a row - for a firmware change that might fail some time after it loads (a
 * ring the hardware cannot index shows up as lost beacons, i.e. a disconnect).
 *
 * Round trip: netctl can keep reporting "connected" over a link that carries
 * nothing (seen after a MAC address change under a live association), so
 * "connected" alone does not prove the link is back. Before the radio goes
 * off, the worker asks the gateway once (a TCP connect to port 53: accepted
 * or refused, either is an answer from the other end). If it answered then,
 * the link only counts as back once it answers again, and the watchdog keeps
 * asking for RXC_RTT_WATCH_S more and reboots if it stays silent
 * RXC_RTT_DOWN_S in a row (a MAC address re-send lands in that window). If
 * it did not answer before the cycle (no link, or a network that ignores
 * such connects), the cycle is judged as before, on netctl alone.
 */
#include <psp2/kernel/threadmgr.h>
#include <psp2/net/netctl.h>   /* sceNetCtlInetGetState */
#include <psp2/net/net.h>      /* the gateway round trip                      */
#include <psp2/kernel/processmgr.h>   /* sceKernelGetProcessTimeWide          */
#include <taihen.h>            /* taiGetModuleExportFunc (SceShell cold reset) */
#include <psp2/io/fcntl.h>     /* sceIoRemove (marker)                        */
#include <string.h>            /* memset                                      */

#include "rxcycle.h"
#include "keepalive.h"         /* wq_gateway_be                               */
#include "keepwifi.h"          /* keepwifi_power_busy                         */
#include "netjoin.h"           /* wq_net_icm_connect                          */
#include "log.h"
#include "../include/wifiturbo.h"
#include "../include/wifiturbo_kernel_api.h"   /* wqForceReboot */

extern int sceWlanGetConfiguration(void);   /* NID 0x70AF9BD6; &9 = radio on    */
extern int sceWlanSetConfiguration(int en); /* NID 0xDBE8AEF2; 0 off, 1 on      */
extern int sceBtGetConfiguration(void);     /* NID 0x9D619F35; &9 = Bluetooth on */
extern int sceBtSetConfiguration(int en);   /* NID 0x9B1F2964; 0 off, 1 on       */

/* SceShell's cold reset (the export vitacompanion's 'reboot' uses). */
#define RXC_SHELL_LIB_NID      0xD2B1C8AE
#define RXC_SHELL_COLDRESET    0x636544FB

#define RXC_REPLY_US       (2 * 1000 * 1000)   /* let the :1339 reply go out  */
#define RXC_OFF_US         (5 * 1000 * 1000)   /* radio off this long         */
#define RXC_DEADLINE_S     120                 /* link back by then or reboot */
#define RXC_FORCE_AFTER_US (30 * 1000 * 1000)  /* shell reset did not take    */
#define RXC_GUARD_DOWN_S   60                  /* guard: down this long = reboot */
#define RXC_PDOWN_POLLS    30                  /* wait up to 3 s for chip power-off */
#define RXC_RTT_PORT       53                  /* gateway port asked (DNS)    */
#define RXC_RTT_TIMEOUT_US (2 * 1000 * 1000)   /* one round trip, at most     */
#define RXC_RTT_WATCH_S    90                  /* keep asking after the link is back */
#define RXC_RTT_DOWN_S     45                  /* silent this long = reboot   */

static volatile int g_phase;        /* rxcycle_phase()                         */
static volatile int g_worker_done;
static volatile unsigned g_gen;     /* bumped by every start: an older watchdog stands down */
static volatile int g_guard_s;      /* guard time left after the link is back  */
static volatile int g_require_link; /* 0: radio on counts as success           */
static char         g_marker[64];   /* removed on success ("" = none)          */
static volatile int g_last_pdown;   /* 1 if the last cycle saw the chip power off */
static volatile unsigned g_rtt_gw;  /* gateway that answered before the cycle; 0 = none */

/* 1 if `gw` answers a TCP connect within RXC_RTT_TIMEOUT_US: accepted or
 * refused, either way a frame went out and one came back. Non-blocking, so
 * it always returns. */
static int gw_answers(unsigned gw)
{
	if (!gw)
		return 0;
	int fd = sceNetSocket("wqrxcrtt", SCE_NET_AF_INET, SCE_NET_SOCK_STREAM, 0);
	if (fd < 0)
		return 0;
	int one = 1, ans = 0;
	sceNetSetsockopt(fd, SCE_NET_SOL_SOCKET, SCE_NET_SO_NBIO, &one, sizeof(one));
	SceNetSockaddrIn sa;
	memset(&sa, 0, sizeof(sa));
	sa.sin_family      = SCE_NET_AF_INET;
	sa.sin_port        = sceNetHtons(RXC_RTT_PORT);
	sa.sin_addr.s_addr = gw;
	SceInt64 until = (SceInt64)sceKernelGetProcessTimeWide() + RXC_RTT_TIMEOUT_US;
	for (;;) {
		int r = sceNetConnect(fd, (SceNetSockaddr *)&sa, sizeof(sa));
		if (r >= 0 || r == (int)SCE_NET_ERROR_EISCONN || r == (int)SCE_NET_ERROR_ECONNREFUSED) {
			ans = 1;
			break;
		}
		int err = 0;
		unsigned len = sizeof(err);
		if (sceNetGetsockopt(fd, SCE_NET_SOL_SOCKET, SCE_NET_SO_ERROR, &err, &len) >= 0 &&
		    (err == SCE_NET_ECONNREFUSED || err == (int)SCE_NET_ERROR_ECONNREFUSED)) {
			ans = 1;
			break;
		}
		if ((r != (int)SCE_NET_ERROR_EINPROGRESS && r != (int)SCE_NET_ERROR_EALREADY) ||
		    (SceInt64)sceKernelGetProcessTimeWide() >= until)
			break;                     /* unreachable, timed out, or another error */
		sceKernelDelayThread(100 * 1000);
	}
	sceNetSocketClose(fd);
	return ans;
}

static int worker_thread(SceSize args, void *argp)
{
	sceKernelDelayThread(RXC_REPLY_US);
	g_phase = 1;
	/* Round-trip baseline (see the top of the file); only for a cycle that
	 * needs the link back. */
	unsigned gw = g_require_link ? wq_gateway_be() : 0;
	g_rtt_gw = gw_answers(gw) ? gw : 0;
	WQ_LOGI("rxcycle: gateway round trip before the cycle: %s", !gw ? "no gateway" :
	        g_rtt_gw ? "answered - required after it" : "no answer - netctl alone decides");
	int bt = (sceBtGetConfiguration() & 9) != 0;
	if (bt) {
		int rb = sceBtSetConfiguration(0);
		WQ_LOGI("rxcycle: Bluetooth off for the cycle rc=0x%08x", (unsigned)rb);
	}
	int r0 = sceWlanSetConfiguration(0);
	WQ_LOGI("rxcycle: radio off rc=0x%08x (cfg now 0x%x)", (unsigned)r0, sceWlanGetConfiguration());
	/* The firmware only reloads if the chip really lost power: SceWlanBt's
	 * power word (seg1+0x4) bit 0 must clear. Bluetooth's switch-off can lag,
	 * so poll for up to RXC_PDOWN_POLLS x 100 ms and log the outcome. */
	WqRxRingStats ps;
	int down = 0, polls = 0;
	for (; polls < RXC_PDOWN_POLLS; polls++) {
		memset(&ps, 0, sizeof(ps));
		wqRxRing(WQ_RXRING_OP_STATUS, &ps);
		if (ps.present && !(ps.power & 1)) { down = 1; break; }
		sceKernelDelayThread(100 * 1000);
	}
	if (down)
		WQ_LOGI("rxcycle: chip powered down after %d ms (power word 0x%x)", polls * 100, ps.power);
	else
		WQ_LOGW("rxcycle: chip still powered after %d ms (power word 0x%x) - the "
		        "firmware will not reload on this cycle", polls * 100, ps.power);
	g_last_pdown = down;
	sceKernelDelayThread(RXC_OFF_US);
	g_phase = 2;
	int r1 = sceWlanSetConfiguration(1);
	WQ_LOGI("rxcycle: radio on rc=0x%08x (cfg now 0x%x)", (unsigned)r1, sceWlanGetConfiguration());
	if (bt) {
		int rb = sceBtSetConfiguration(1);
		WQ_LOGI("rxcycle: Bluetooth back on rc=0x%08x", (unsigned)rb);
	}
	g_worker_done = 1;
	return sceKernelExitDeleteThread(0);
}

static int link_up(void)
{
	int state = -1;
	return sceNetCtlInetGetState(&state) == 0 && state == 3 && (sceWlanGetConfiguration() & 9);
}

static SceInt64 now_us(void)
{
	return (SceInt64)sceKernelGetProcessTimeWide();
}

/* After the link is back: keep asking the gateway that answered before the
 * cycle. Returns 1 if it rebooted. Stands down (0) when a newer cycle starts,
 * the Vita is going to sleep, the radio is off, or the network changed:
 * silence is only a dead link when nothing else explains it. */
static int rtt_watch(unsigned gen)
{
	unsigned gw = g_rtt_gw;
	g_phase = 5;
	WQ_LOGI("rxcycle: watching the gateway round trip for %d s (reboot if silent %d s)",
	        RXC_RTT_WATCH_S, RXC_RTT_DOWN_S);
	SceInt64 end = now_us() + (SceInt64)RXC_RTT_WATCH_S * 1000000, last_ok = now_us();
	int fails = 0;
	while (now_us() < end) {
		sceKernelDelayThread(3 * 1000 * 1000);
		if (gen != g_gen)
			return 0;
		const char *why = keepwifi_power_busy() ? "sleep" :
		                  !(sceWlanGetConfiguration() & 9) ? "radio off" : NULL;
		unsigned cur = why ? 0 : wq_gateway_be();
		if (!why && cur && cur != gw)
			why = "network changed";
		if (why) {
			WQ_LOGI("rxcycle: round-trip watch ended (%s)", why);
			return 0;
		}
		if (gw_answers(gw)) {
			if (fails)
				WQ_LOGI("rxcycle: gateway answers again after %d s",
				        (int)((now_us() - last_ok) / 1000000));
			last_ok = now_us();
			fails = 0;
			continue;
		}
		/* Silent for RXC_RTT_DOWN_S, over at least 8 asks (so a clock jump
		 * alone never counts). */
		if (++fails == 3) {
			int st = -1;
			sceNetCtlInetGetState(&st);
			WQ_LOGW("rxcycle: gateway silent (netctl state %d)", st);
		}
		if (fails >= 8 && now_us() - last_ok >= (SceInt64)RXC_RTT_DOWN_S * 1000000) {
			WQ_LOGE("rxcycle: gateway silent %d s after the cycle - rebooting to stock",
			        (int)((now_us() - last_ok) / 1000000));
			rxcycle_reboot();
			return 1;
		}
	}
	WQ_LOGI("rxcycle: round-trip watch ended (link healthy)");
	return 0;
}

static int watchdog_thread(SceSize args, void *argp)
{
	unsigned gen = g_gen;
	SceInt64 deadline = now_us() + (SceInt64)RXC_DEADLINE_S * 1000000, t0 = now_us();
	while (now_us() < deadline) {
		sceKernelDelayThread(1000 * 1000);
		if (!g_worker_done)
			continue;
		if (g_require_link ? !link_up() : !(sceWlanGetConfiguration() & 9))
			continue;
		if (g_require_link && g_rtt_gw && !gw_answers(g_rtt_gw))
			continue;                      /* "connected", but nothing comes back yet */
		WQ_LOGI("rxcycle: %s %d s after start%s", g_require_link ? "link back" : "radio back on",
		        (int)((now_us() - t0) / 1000000),
		        g_require_link && g_rtt_gw ? " (gateway answers)" : "");
		if (g_marker[0]) {
			sceIoRemove(g_marker);
			g_marker[0] = 0;
		}
		/* No link before the cycle (boot): SceShell gave up auto-connecting
		 * while the radio was off and only retries after ~12 s. If nothing
		 * has started a join 3 s after the radio is back (a MAC address
		 * re-send would have), ask for one. */
		if (!g_require_link) {
			sceKernelDelayThread(3 * 1000 * 1000);
			int st = -1;
			if (sceNetCtlInetGetState(&st) == 0 && st == 0 && (sceWlanGetConfiguration() & 9)) {
				WQ_LOGI("rxcycle: no join started after the cycle - IcmConnect");
				wq_net_icm_connect();
			}
		}
		if (g_guard_s > 0) {
			WQ_LOGI("rxcycle: guarding %d s (reboot if down %d s in a row)", g_guard_s, RXC_GUARD_DOWN_S);
			g_phase = 4;
			int down = 0;
			for (; g_guard_s > 0; g_guard_s--) {
				sceKernelDelayThread(1000 * 1000);
				down = link_up() ? 0 : down + 1;
				if (down == 10)
					WQ_LOGW("rxcycle: guard: link down 10 s");
				if (down >= RXC_GUARD_DOWN_S) {
					WQ_LOGE("rxcycle: guard: link down %d s - rebooting to stock", down);
					rxcycle_reboot();
					return sceKernelExitDeleteThread(0);
				}
			}
			WQ_LOGI("rxcycle: guard ended");
		}
		if (g_require_link && g_rtt_gw && rtt_watch(gen))
			return sceKernelExitDeleteThread(0);
		/* Back to idle, unless a newer cycle already took over from the watch. */
		int ph = g_phase;
		if (gen == g_gen)
			__atomic_compare_exchange_n(&g_phase, &ph, 0, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
		return sceKernelExitDeleteThread(0);
	}
	WQ_LOGE("rxcycle: link not back after %d s (worker %s) - rebooting to stock",
	        RXC_DEADLINE_S, g_worker_done ? "done" : "STUCK");
	rxcycle_reboot();
	return sceKernelExitDeleteThread(0);
}

int rxcycle_reboot(void)
{
	g_phase = 3;
	uintptr_t fn = 0;
	int r = taiGetModuleExportFunc("SceShellSvc", RXC_SHELL_LIB_NID, RXC_SHELL_COLDRESET, &fn);
	WQ_LOGW("rxcycle: SceShell cold reset (resolve rc=0x%08x)", (unsigned)r);
	if (r >= 0 && fn)
		((int (*)(int))fn)(0);
	sceKernelDelayThread(RXC_FORCE_AFTER_US);
	WQ_LOGE("rxcycle: still running 30 s after the shell reset - syscon reset");
	return wqForceReboot();
}

int rxcycle_last_powered_down(void)
{
	return g_last_pdown;
}

int rxcycle_phase(void)
{
	return g_phase;
}

int rxcycle_settled(void)
{
	int ph = g_phase;
	return ph == 0 || ((ph == 2 || ph == 4 || ph == 5) && g_worker_done);
}

void rxcycle_guard_stop(void)
{
	g_guard_s = 0;
}

int rxcycle_start(int guard_min)
{
	return rxcycle_start_ex(guard_min, 1, 0);
}

int rxcycle_start_ex(int guard_min, int require_link, const char *marker)
{
	/* Test and set: one cycle at a time. A round-trip watch (phase 5) gives
	 * way to a new cycle: its watchdog sees g_gen move and stands down. */
	int idle = 0, watching = 5;
	if (!__atomic_compare_exchange_n(&g_phase, &idle, 1, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST) &&
	    !__atomic_compare_exchange_n(&g_phase, &watching, 1, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
		return WQ_ERR_BUSY;
	g_gen++;
	g_worker_done = 0;
	g_rtt_gw = 0;
	g_require_link = require_link;
	g_marker[0] = 0;
	if (marker) {
		int i = 0;
		for (; marker[i] && i < (int)sizeof(g_marker) - 1; i++) g_marker[i] = marker[i];
		g_marker[i] = 0;
	}
	g_guard_s = guard_min > 0 ? guard_min * 60 : 0;
	g_phase = 1;
	SceUID wd = sceKernelCreateThread("WqRxCycleWatchdog", watchdog_thread, 0x40, 0x2000, 0, 0, NULL);
	if (wd < 0) {
		g_phase = 0;
		WQ_LOGE("rxcycle: watchdog thread create failed (%d) - not cycling", wd);
		return WQ_ERR_NOT_READY;
	}
	SceUID wk = sceKernelCreateThread("WqRxCycle", worker_thread, 0x40, 0x2000, 0, 0, NULL);
	if (wk < 0) {
		sceKernelDeleteThread(wd);
		g_phase = 0;
		WQ_LOGE("rxcycle: worker thread create failed (%d) - not cycling", wk);
		return WQ_ERR_NOT_READY;
	}
	/* Watchdog first: once the radio can go off, a reboot must be armed. */
	sceKernelStartThread(wd, 0, NULL);
	sceKernelStartThread(wk, 0, NULL);
	WQ_LOGI("rxcycle: started (off %d s, reboot if the link is not back in %d s)",
	        RXC_OFF_US / 1000000, RXC_DEADLINE_S);
	return WQ_OK;
}
