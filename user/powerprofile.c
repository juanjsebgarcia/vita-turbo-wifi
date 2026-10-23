/*
 * powerprofile.c
 *
 * Power profiles:
 *   WQ_PROFILE_MAX_PERF    - STA power save OFF. The driver's own setter
 *                            FUN_81009628(6, from_kernel=1) clears ctx+0x1d50,
 *                            the flag the join path uses to choose DIS_AUTO_PS
 *                            (0xFE) over EN_AUTO_PS (kernel/psprofile.c).
 *   WQ_PROFILE_BALANCED    - STA power save ON with Sony's stock parameters
 *                            (FUN_81009628(7, 1); multiple_dtims 1,
 *                            delay_to_ps 1000 ms, bcn_miss 5, mode auto).
 *   WQ_PROFILE_MAX_BATTERY - balanced + the kernel DTIM-skip patch
 *                            (wqPsSetDtimSkip: the join-time PS_MODE_ENH sends
 *                            multiple_dtims 3, i.e. wake every 3rd DTIM).
 *
 * Every profile change reassociates (wq_net_reassociate) inside the safety
 * harness, so the join path re-sends PS_MODE_ENH with the new values, and is
 * reverted (old PS flag + old patch state + reassociate) unless confirmed.
 * Nothing here hooks the 0x2db4/0x2f44 send helpers. At boot the saved profile
 * and 40 MHz state are applied once, without the harness (wq_power_boot_restore).
 *
 * HT40: two immediate
 * patches in kernel/ht40patch.c, same harness pattern.
 */
#include <string.h>
#include <psp2/kernel/threadmgr.h>   /* sceKernelDelayThread                     */

#include "powerprofile.h"
#include "safety.h"
#include "log.h"
#include <psp2/net/netctl.h>  /* sceNetCtlInetGetState (boot restore)  */
#include "netjoin.h"          /* wq_net_reassociate: apply at an association      */
#include "../include/wifiturbo_kernel_api.h"

/* The PS flag is set kernel-side (wqPsSetFlag -> FUN_81009628(6|7, 1)). The
 * user sceWlanSetConfiguration(6/7) reaches the same setter with
 * from_kernel = 0, which the driver silently drops while connected (state bit
 * 0x20) yet still returns 0 (verified on hardware). */

/* ---- power-save profile ------------------------------------------------- */

typedef struct {
	uint8_t  target_profile;   /* WQ_PROFILE_*                                */
	uint8_t  prev_profile;     /* HUD profile before the change               */
	uint8_t  prev_ps_on;       /* driver PS flag before the change            */
	uint8_t  prev_dtim_skip;   /* DTIM-skip patch state before the change     */
} PowerRevertCtx;

static int g_cur_profile = WQ_PROFILE_BALANCED;   /* last applied, for the HUD   */

const char *wq_power_profile_name(int profile)
{
	switch (profile) {
	case WQ_PROFILE_MAX_PERF:    return "MAXPERF";
	case WQ_PROFILE_BALANCED:    return "BALANCED";
	case WQ_PROFILE_MAX_BATTERY: return "MAXBATT";
	default:                     return "?";
	}
}

static int ps_flag_on(void)
{
	return wqPsGetFlag() == 1;
}

/* Set the driver's PS flag and verify the read-back. FUN_81009628 returns 0
 * without doing anything when the driver is not in its "up" state
 * (ctx+0x704 & 0x400004 != 4, or bit 5 set), so the return code alone proves
 * nothing. */
static int ps_flag_set(int on)
{
	/* Kernel side: the driver's setter with from_kernel = 1, because the user
	 * sceWlanSetConfiguration(6/7) is silently dropped while connected
	 * (verified on hardware). Returns the real flag read back. */
	int now = wqPsSetFlag(on);
	WQ_LOGI("power: PS flag set %d -> now %d", on ? 1 : 0, now);
	if (now != (on ? 1 : 0))
		return WQ_ERR_CHIP;
	return WQ_OK;
}

/* Put the driver in the state for (ps_on, dtim_skip), then reassociate so the
 * join path re-sends PS_MODE_ENH. Order: patch first, then the flag (a flag
 * change also posts a live PS command with stock params), then a short settle
 * so the worker finishes that command before the disconnect. */
static int power_set_state(int ps_on, int dtim_skip)
{
	int r = wqPsSetDtimSkip(dtim_skip);
	if (r != WQ_OK) {
		WQ_LOGW("power: wqPsSetDtimSkip(%d) rc=%d", dtim_skip, r);
		return r;
	}
	r = ps_flag_set(ps_on);
	if (r != WQ_OK)
		return r;
	sceKernelDelayThread(300 * 1000);
	r = wq_net_reassociate();
	if (r != WQ_OK)
		WQ_LOGW("power: reassociate rc=%d (PS state set; applies at next join)", r);
	return WQ_OK;
}

static int power_apply(void *ctxp)
{
	PowerRevertCtx *c = (PowerRevertCtx *)ctxp;
	int ps_on = c->target_profile != WQ_PROFILE_MAX_PERF;
	int skip  = c->target_profile == WQ_PROFILE_MAX_BATTERY;
	WQ_LOGI("power: apply %s (ps=%d dtim_skip=%d; prev ps=%d skip=%d) - reassociating",
	        wq_power_profile_name(c->target_profile), ps_on, skip,
	        c->prev_ps_on, c->prev_dtim_skip);
	int r = power_set_state(ps_on, skip);
	if (r == WQ_OK)
		g_cur_profile = c->target_profile;
	return r;
}

/* Watchdog / explicit revert: restore the snapshot. Needs no link. */
static int power_revert(void *ctxp)
{
	PowerRevertCtx *c = (PowerRevertCtx *)ctxp;
	WQ_LOGW("power: revert to ps=%d dtim_skip=%d (%s) - reassociating",
	        c->prev_ps_on, c->prev_dtim_skip, wq_power_profile_name(c->prev_profile));
	g_cur_profile = c->prev_profile;
	return power_set_state(c->prev_ps_on, c->prev_dtim_skip);
}

int wq_power_current_profile(void) { return g_cur_profile; }

/* Boot: put the driver into the saved profile + 40 MHz state, without the
 * safety harness. The patches and the PS flag only matter at an association:
 * at boot the link is normally still down (the first join is the watcher's
 * IcmConnect), so they simply apply to that join. If the shell already
 * connected on its own, one reassociation applies them now. No driver hooks
 * are involved. */
int wq_power_boot_restore(int profile, int ht40)
{
	if (profile < WQ_PROFILE_MAX_PERF || profile > WQ_PROFILE_MAX_BATTERY)
		profile = WQ_PROFILE_BALANCED;
	int ps_on = profile != WQ_PROFILE_MAX_PERF;
	int skip  = profile == WQ_PROFILE_MAX_BATTERY;
	uint16_t prev = 0;
	int r1 = wqPsSetDtimSkip(skip);
	int r2 = ps_flag_set(ps_on);
	int r3 = wqChipSet11nHt40(ht40 ? 1 : 0, &prev);
	g_cur_profile = profile;
	int state = 0;
	int up = sceNetCtlInetGetState(&state) == 0 && state == 3;
	WQ_LOGI("power: boot restore %s, 40 MHz %s (dtim rc=%d ps rc=%d ht40 rc=%d)%s",
	        wq_power_profile_name(profile), ht40 ? "on" : "off", r1, r2, r3,
	        up ? " - already connected, reassociating" : " - applies at the first join");
	if (up)
		wq_net_reassociate();
	return (r1 || r2 || r3) ? WQ_ERR_CHIP : WQ_OK;
}

/* Set the HUD's displayed profile WITHOUT applying it. */
void wq_power_set_display_profile(int profile)
{
	if (profile >= WQ_PROFILE_MAX_PERF && profile <= WQ_PROFILE_MAX_BATTERY)
		g_cur_profile = profile;
}

int wq_power_set_profile(int profile, uint32_t cap_ms)
{
	if (profile < WQ_PROFILE_MAX_PERF || profile > WQ_PROFILE_MAX_BATTERY)
		return WQ_ERR_ARG;

	PowerRevertCtx ctx;
	memset(&ctx, 0, sizeof(ctx));
	ctx.target_profile = (uint8_t)profile;
	ctx.prev_profile   = (uint8_t)g_cur_profile;
	ctx.prev_ps_on     = (uint8_t)ps_flag_on();
	ctx.prev_dtim_skip = (uint8_t)(wqPsGetDtimSkip() > 0);

	/* The reassociation must come back before 'confirm' can reach us: 45 s,
	 * like HT40. Unconfirmed => old PS flag + old patch state + reassociate. */
	if (!cap_ms)
		cap_ms = 45000;
	return wq_safety_arm(wq_power_profile_name(profile), cap_ms,
	                     power_apply, power_revert, &ctx, sizeof(ctx));
}

/* ---- 40 MHz on 2.4 GHz (HT40) ------------------------------------------ */

typedef struct {
	uint16_t prev_htcap;   /* HT cap word before the change                   */
	uint8_t  enable;       /* requested state                                 */
} Ht40RevertCtx;

static int ht40_apply(void *ctxp)
{
	Ht40RevertCtx *c = (Ht40RevertCtx *)ctxp;
	uint16_t prev = 0;
	/* wqChipSet11nHt40 returns the pre-change HT cap so the revert is exact. */
	int r = wqChipSet11nHt40(c->enable, &prev);
	if (r == WQ_OK) {
		c->prev_htcap = prev;
		WQ_LOGI("power: ht40 %s applied (prev htcap=0x%04x) - reassociating",
		        c->enable ? "on" : "off", prev);
		/* The patch only bites at an association: reassociate now, inside the
		 * revert window, so a failed 40 MHz join is undone automatically. */
		wq_net_reassociate();
	} else {
		WQ_LOGW("power: ht40 %s apply rc=%d", c->enable ? "on" : "off", r);
	}
	return r;
}

static int ht40_revert(void *ctxp)
{
	Ht40RevertCtx *c = (Ht40RevertCtx *)ctxp;
	/* Restore by applying the opposite of what was requested. This assumes the
	 * request changed the state: an unconfirmed no-op (e.g. 'ht40 off' while
	 * already off) reverts to the opposite. Remote (:1339) only. */
	uint16_t dummy = 0;
	int r = wqChipSet11nHt40(c->enable ? 0 : 1, &dummy);
	WQ_LOGW("power: ht40 reverted to %s - reassociating", c->enable ? "off" : "on");
	wq_net_reassociate();
	return r;
}

int wq_power_set_ht40(int enable, uint32_t cap_ms)
{
	/* Routed through the harness because a width change can drop the link. */
	Ht40RevertCtx ctx;
	memset(&ctx, 0, sizeof(ctx));
	ctx.enable = enable ? 1 : 0;
	return wq_safety_arm(enable ? "HT40on" : "HT40off", cap_ms,
	                     ht40_apply, ht40_revert, &ctx, sizeof(ctx));
}
