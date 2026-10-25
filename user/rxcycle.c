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
 */
#include <psp2/kernel/threadmgr.h>
#include <psp2/net/netctl.h>   /* sceNetCtlInetGetState */
#include <taihen.h>            /* taiGetModuleExportFunc (SceShell cold reset) */
#include <psp2/io/fcntl.h>     /* sceIoRemove (marker)                        */
#include <string.h>            /* memset                                      */

#include "rxcycle.h"
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

static volatile int g_phase;        /* rxcycle_phase()                         */
static volatile int g_worker_done;
static volatile int g_guard_s;      /* guard time left after the link is back  */
static volatile int g_require_link; /* 0: radio on counts as success           */
static char         g_marker[64];   /* removed on success ("" = none)          */
static volatile int g_last_pdown;   /* 1 if the last cycle saw the chip power off */

static int worker_thread(SceSize args, void *argp)
{
	sceKernelDelayThread(RXC_REPLY_US);
	g_phase = 1;
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

static int watchdog_thread(SceSize args, void *argp)
{
	for (int s = 0; s < RXC_DEADLINE_S; s++) {
		sceKernelDelayThread(1000 * 1000);
		if (!g_worker_done)
			continue;
		if (g_require_link ? !link_up() : !(sceWlanGetConfiguration() & 9))
			continue;
		WQ_LOGI("rxcycle: %s %d s after start", g_require_link ? "link back" : "radio back on", s + 1);
		if (g_marker[0]) {
			sceIoRemove(g_marker);
			g_marker[0] = 0;
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
		g_phase = 0;
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

static int reboot_thread(SceSize args, void *argp)
{
	sceKernelDelayThread(RXC_REPLY_US);
	rxcycle_reboot();
	return sceKernelExitDeleteThread(0);
}

int rxcycle_reboot_async(void)
{
	SceUID t = sceKernelCreateThread("WqReboot", reboot_thread, 0x40, 0x2000, 0, 0, NULL);
	if (t < 0)
		return WQ_ERR_NOT_READY;
	sceKernelStartThread(t, 0, NULL);
	return WQ_OK;
}

int rxcycle_last_powered_down(void)
{
	return g_last_pdown;
}

int rxcycle_phase(void)
{
	return g_phase;
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
	if (g_phase != 0)
		return WQ_ERR_BUSY;
	g_worker_done = 0;
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
