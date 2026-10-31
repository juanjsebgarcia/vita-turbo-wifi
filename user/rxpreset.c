/*
 * rxpreset.c - EXPERIMENTAL: the "Wi-Fi receive buffer" setting.
 *
 * Three presets of the chip RX ring (kernel/rxring.c): 0 Stock (83 slots),
 * 1 Enhanced (127), 2 Maximum (149). Changing it edits SceWlanBt's cached
 * firmware image and power-cycles the radio (user/rxcycle.c, Bluetooth
 * included) so the chip loads it. No reboot is needed.
 *
 * Boot: the chip already runs the stock firmware when plugins start, so the
 * saved preset (settings.bin, loaded by wq_settings_boot) is re-applied with
 * one radio power-cycle as soon as the radio is on - before the first join
 * where possible, so the console connects once instead of connecting,
 * dropping and connecting again. If Wi-Fi is off, only the cached image is
 * edited; the chip picks it up the next time the radio powers on.
 *
 * Boot-loop guard: RXP_MARKER is written before every power-cycle and removed
 * when it succeeds. If the cycle's watchdog had to reboot, the marker survives;
 * the next boot finds it, saves Stock and does not retry.
 *
 * Accepted on the p223 (3.60-3.65) and p226 (3.67-3.74) chip images. p226
 * has not yet run on a console: it is offered as untested in the alpha.
 */
#include <psp2/kernel/threadmgr.h>
#include <psp2/net/netctl.h>   /* sceNetCtlInetGetState */
#include <psp2/io/fcntl.h>
#include <string.h>

#include "rxpreset.h"
#include "rxcycle.h"
#include "settings_entry.h"    /* wq_settings_note */
#include "log.h"
#include "../include/wifiturbo.h"
#include "../include/wifiturbo_kernel_api.h"

extern int sceWlanGetConfiguration(void);   /* NID 0x70AF9BD6; &9 = radio on */

#define RXP_MARKER   "ur0:data/wifiturbo/rxring.try"
#define RXP_BOOT_WAIT_S 90                  /* boot: wait this long for the link */

static const int   k_slots[3] = { 0, 127, 149 };
static const char *const k_name[3] = { "stock", "enhanced", "maximum" };

static volatile int g_busy;
static int g_req_level, g_req_boot;

static int link_up(void)
{
	int state = -1;
	return sceNetCtlInetGetState(&state) == 0 && state == 3 && (sceWlanGetConfiguration() & 9);
}

static int marker_exists(void)
{
	SceUID fd = sceIoOpen(RXP_MARKER, SCE_O_RDONLY, 0);
	if (fd < 0)
		return 0;
	sceIoClose(fd);
	return 1;
}

static void marker_write(void)
{
	SceUID fd = sceIoOpen(RXP_MARKER, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0666);
	if (fd >= 0) {
		sceIoWrite(fd, "1", 1);
		sceIoClose(fd);
	}
}

static void run(int level, int from_boot)
{
	if (from_boot) {
		if (marker_exists()) {
			sceIoRemove(RXP_MARKER);
			WQ_LOGW("rxpreset: the last power-cycle did not complete (watchdog reboot) - "
			        "falling back to stock");
			wq_settings_note("rxpreset stock");
			return;
		}
		if (level == 0)
			return;                     /* the chip already runs stock after boot */
		/* Wait for the radio, not the link: cycling before the first join
		 * means a single connect. (A link that is already up is fine too:
		 * the cycle then requires it back, as before.) */
		int s = 0;
		while (s < RXP_BOOT_WAIT_S && !link_up() && !(sceWlanGetConfiguration() & 9)) {
			sceKernelDelayThread(1000 * 1000);
			s++;
		}
		sceKernelDelayThread(1000 * 1000);   /* let the driver settle */
	}

	WqRxRingStats st;
	memset(&st, 0, sizeof(st));
	wqRxRing(WQ_RXRING_OP_STATUS, &st);
	if (!st.present || st.loaded != 1 || (st.build != 223 && st.build != 226)) {
		WQ_LOGW("rxpreset: %s refused (present %u loaded %u build p%u; only p223/p226 are known)",
		        k_name[level], st.present, st.loaded, st.build);
		return;
	}
	int op = level ? (WQ_RXRING_OP_APPLY | (k_slots[level] << 8)) : WQ_RXRING_OP_RESTORE;
	int r = wqRxRing(op, &st);
	if (r != WQ_OK) {
		WQ_LOGW("rxpreset: %s: image edit failed rc=%d (why %u)", k_name[level], r, st.why);
		return;
	}
	if (!(sceWlanGetConfiguration() & 9)) {
		WQ_LOGI("rxpreset: %s applied to the cached image; Wi-Fi is off, so it loads "
		        "the next time the radio powers on", k_name[level]);
		return;
	}
	int was_up = link_up();
	marker_write();
	r = rxcycle_start_ex(0, was_up, RXP_MARKER);
	if (r != WQ_OK) {
		sceIoRemove(RXP_MARKER);
		WQ_LOGW("rxpreset: %s: power-cycle not started rc=%d", k_name[level], r);
		return;
	}
	WQ_LOGI("rxpreset: %s (%u slots): power-cycling the radio%s", k_name[level], st.slots,
	        from_boot ? " (boot)" : "");
}

static int preset_thread(SceSize args, void *argp)
{
	run(g_req_level, g_req_boot);
	g_busy = 0;
	return sceKernelExitDeleteThread(0);
}

static int start(int level, int from_boot)
{
	if (level < 0 || level > 2)
		return WQ_ERR_ARG;
	if (g_busy)
		return WQ_ERR_BUSY;
	g_busy = 1;
	g_req_level = level;
	g_req_boot = from_boot;
	SceUID t = sceKernelCreateThread("WqRxPreset", preset_thread, 0x40, 0x2000, 0, 0, NULL);
	if (t < 0) {
		g_busy = 0;
		return WQ_ERR_NOT_READY;
	}
	sceKernelStartThread(t, 0, NULL);
	return WQ_OK;
}

int rxpreset_set(int level)
{
	return start(level, 0);
}

void rxpreset_boot(int level)
{
	if (start(level, 1) != WQ_OK)
		WQ_LOGW("rxpreset: boot apply not started");
}

int rxpreset_level(const char *name)
{
	for (int i = 0; i < 3; i++) {
		const char *a = name, *b = k_name[i];
		while (*a && *a == *b) { a++; b++; }
		if (!*a && !*b)
			return i;
	}
	return -1;
}
