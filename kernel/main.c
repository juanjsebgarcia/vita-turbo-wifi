/*
 * main.c - SceWifiTurbo kernel module entry point.
 *
 * Responsibilities:
 *   - bring up the logger (klog.c) and the HUD hook (hud.c);
 *   - publish the user-callable "SceWifiTurbo" export library (see
 *     include/wifiturbo_kernel_api.h and wifiturbo_kernel.yml).
 *
 * The exported wrappers are the user->kernel boundary. Pointer arguments point
 * into user memory, so each wrapper copies the payload in/out with
 * ksceKernelMemcpyUserToKernel / KernelToUser (= ksceKernelCopy{From,To}User),
 * which safely cross the boundary (the standard Vita kernel pattern, a DACR
 * switch).
 *
 * NOTE ON SAFETY: nothing in the kernel module decides policy. Every
 * link-affecting setter here performs a single atomic set and returns. The
 * mandatory capped auto-revert is owned by the user module's safety harness
 * (user/safety.c), which snapshots the previous value (via the Get calls),
 * applies the new one, and schedules the revert from a thread that survives a
 * network teardown. This separation is deliberate: the kernel cannot be left
 * holding a one-way change.
 */
#include <psp2kern/kernel/modulemgr.h>
#include <psp2kern/kernel/threadmgr.h>
#include <psp2kern/kernel/sysmem.h>    /* ksceKernelMemcpy{User,Kernel} macros */
#include <psp2kern/kernel/sysclib.h>

#include "../include/wifiturbo.h"
#include "../include/wifiturbo_kernel_api.h"
#include "ht40patch.h"
#include "chipobserve.h"
#include "roamassist.h"
#include "hud.h"
#include "psprofile.h"
#include "klog.h"

/* ---- copy-in/out helpers ------------------------------------------------ */

/* ksceKernelCopyToUser safely crosses the user boundary on its own. */
static int copy_to_user(void *user_dst, const void *src, SceSize n)
{
	return ksceKernelMemcpyKernelToUser(user_dst, src, n);
}

/* ---- exported SceWifiTurbo API (user-callable) ---------------------------- */

/* 40 MHz: patch the driver's two hard-coded 20 MHz immediates (ht40patch.c).
 * Takes effect at the next (re)association. There is no chip read, so there
 * is no genuine "previous HT cap" to return; report the 20 MHz baseline when
 * the patch was off. The user-side revert sets the OPPOSITE state
 * (user/powerprofile.c ht40_revert). */
int wqChipSet11nHt40(int enable, uint16_t *prev_htcap_out)
{
	uint16_t prev = ht40patch_active() ? 0x116e : 0x112c;
	WQ_KLOGD("wqChipSet11nHt40(enable=%d) -> ht40patch", enable);
	int r = ht40patch_set(enable);
	if (prev_htcap_out)
		copy_to_user(prev_htcap_out, &prev, sizeof(prev));
	return r;
}

/* ---- power profiles: maxbatt DTIM-skip patch ---------------------------- */

/* on != 0: swap two instructions in SceWlanBt's join-time PS_MODE_ENH builder
 * so the next join sends multiple_dtims = 3; on == 0: release (stock 1). The
 * PS on/off half of the profiles is the driver's own sceWlanSetConfiguration
 * 6/7 path, called from the user module. */
int wqPsSetDtimSkip(int on)
{
	WQ_KLOGI("psprofile: wqPsSetDtimSkip(%d)", on);
	return psprofile_dtim_set(on);
}

/* 1 if the DTIM-skip patch is currently injected, else 0. */
int wqPsGetDtimSkip(void)
{
	return psprofile_dtim_active();
}

/* Driver PS flag (read by the join path). Set through the driver's own
 * config setter with from_kernel = 1 - the user sceWlanSetConfiguration path
 * drops 6/7 while connected. Returns the flag after the call (0/1) or <0. */
int wqPsSetFlag(int on)
{
	return psprofile_ps_set(on);
}

int wqPsGetFlag(void)
{
	return psprofile_ps_get();
}

/* Join-path roam assist (kernel/roamassist.c): arm the NEXT SceWlanBt join
 * to sweep every channel and try the strongest same-SSID BSS first (or the
 * given BSSID, mac0 = bytes 0..3, mac1 = bytes 4..5, with WQ_RA_TARGET).
 * One-shot; flags without WQ_RA_ARM disarm. Hooks install on first arm. */
int wqRoamAssistArm(unsigned flags, unsigned mac0, unsigned mac1)
{
	uint8_t b[6] = { (uint8_t)mac0, (uint8_t)(mac0 >> 8), (uint8_t)(mac0 >> 16),
	                 (uint8_t)(mac0 >> 24), (uint8_t)mac1, (uint8_t)(mac1 >> 8) };
	return roamassist_arm(flags, b);
}

int wqRoamAssistGetStatus(WqRoamAssistStatus *out)
{
	WqRoamAssistStatus k;
	int r = roamassist_get_status(&k);
	if (r == WQ_OK && out)
		copy_to_user(out, &k, sizeof(k));
	return r;
}

int wqHudSetEnabled(int enabled)
{
	WQ_KLOGI("hud: %s overlay", enabled ? "enable" : "disable");
	return hud_set_enabled(enabled);
}

int wqHudSetState(const WqHudState *state)
{
	/* hud_set_state copies in from user itself (it needs the raw user ptr). */
	return hud_set_state(state);
}

/* ---- read-only self-test probes ----------------------------------------- */

int wqSelfTestPing(void)
{
	/* Pure proof-of-reachability: no chip, no link, no state change. A caller
	 * that gets WQ_SELFTEST_MAGIC back knows the syscall path is intact even if
	 * the chip transport never resolved. */
	WQ_KLOGD("selftest: ping");
	return (int)WQ_SELFTEST_MAGIC;
}

int wqHudGetStatus(void)
{
	return hud_get_status();
}

int wqHudSetDrawArmed(int on)
{
	return hud_set_draw_armed(on);
}

/* ---- SNR / noise floor from the driver's RSSI_INFO replies ------------- */

int wqChipObserveGetLink(WqLinkInternals *out)
{
	WqLinkInternals k;
	/* The user side only calls this while connected, so the RSSI_INFO callback
	 * hook (SNR/NF) is first installed after the association burst. */
	chipobserve_rssi_ensure();
	int r = chipobserve_get_link(&k);
	if (r == WQ_OK)
		copy_to_user(out, &k, sizeof(k));
	return r;
}

/* ---- module lifecycle --------------------------------------------------- */

int module_start(SceSize argc, const void *args)
{
	(void)argc; (void)args;

	wq_klog_init();   /* FIRST, so the whole bring-up is captured                */
	WQ_KLOGI("=== SceWifiTurbo kernel v%s start ===", WQ_VERSION_STR);

	int rh = hud_init();
	if (rh == WQ_OK)
		WQ_KLOGI("hud_init: sceDisplay hook installed");
	else
		WQ_KLOGW("hud_init: HUD hook NOT installed (rc=%d)", rh);

	WQ_KLOGI("kernel start complete");
	return 0; /* SCE_KERNEL_START_SUCCESS */
}

int module_stop(SceSize argc, const void *args)
{
	(void)argc; (void)args;
	WQ_KLOGI("=== SceWifiTurbo kernel stop ===");
	chipobserve_fini();   /* remove the RSSI_INFO callback hook if installed    */
	roamassist_fini();    /* remove the join-path roam hooks if installed       */
	psprofile_dtim_set(0); /* restore the stock PS_MODE_ENH builder          */
	hud_fini();
	wq_klog_fini();
	return 0; /* SCE_KERNEL_STOP_SUCCESS */
}
