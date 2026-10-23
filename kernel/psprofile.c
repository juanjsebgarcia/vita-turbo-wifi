/*
 * psprofile.c - "maxbatt" DTIM skipping by swapping two instructions in the
 * SceWlanBt PS_MODE_ENH builder.
 *
 * At every join, FUN_81006ea0 calls FUN_8100b7d4(ctx, ctx+0x1d50 != 0). With
 * power save on it calls the synchronous PS_MODE_ENH (0xE4) builder
 * FUN_81006014(ctx, null_pkt=0, bcn_miss=5, adhoc=0, mode=1). That builder
 * hard-codes the rest of the mwifiex_ps_param TLV (0x172):
 *   body+0x0a multiple_dtims = 1      (lr = #1, also the TLV type high byte)
 *   body+0x0e local_listen   = 0
 *   body+0x14 delay_to_ps    = 0x03e8 (r12 = #0xe8, lr = #0x03)
 *
 * The multiple_dtims store shares its register with byte 5 (the 0x01 of TLV
 * type 0x0172), so its immediate cannot be patched on its own. But the next
 * instruction loads lr = 3 for delay_to_ps's high byte, and swapping the two
 * makes the store write 3 instead:
 *
 *   81006082  84 f8 0a e0   strb.w lr, [r4, #0xa]   ; multiple_dtims = 1
 *   81006086  4f f0 03 0e   mov.w  lr, #0x3
 *   ->
 *   81006082  4f f0 03 0e   mov.w  lr, #0x3
 *   81006086  84 f8 0a e0   strb.w lr, [r4, #0xa]   ; multiple_dtims = 3
 *
 * Byte 5 has already been stored with lr = 1 (0x8100607e), and the only later
 * use of lr is the delay_to_ps high-byte store (0x810060ac), which still gets
 * 3. There are no branches in this block and neither instruction sets flags.
 * So the result is exactly {multiple_dtims 3, everything else stock}: the
 * station wakes for every 3rd DTIM instead of every one. The Vita tells the AP
 * listen_interval = 10 in ASSOCIATE (FUN_81004b10, body+8), so 3 DTIM periods
 * stay inside what the AP has agreed to buffer for as long as DTIM <= 3.
 *
 * FUN_81006014 has exactly one caller (FUN_8100b7d4, the join path). The async
 * builder FUN_81005f5c (live PS toggle, standby/WoWLAN) is NOT touched.
 * Takes effect at the next join; costs nothing at runtime (no hooks). The
 * original bytes are verified first so another firmware is refused rather
 * than corrupted.
 */
#include <psp2kern/kernel/sysclib.h>    /* memset                                     */
#include <psp2kern/types.h>
#include <taihen.h>

#include "psprofile.h"
#include "wlanbt.h"
#include "klog.h"
#include "../include/wifiturbo.h"

#define PSB_SWAP_OFF   0x6082u   /* 8 bytes: strb.w lr,[r4,#0xa] ; mov.w lr,#3 */
#define PSB_SWAP_LEN   8u

static const uint8_t k_orig[PSB_SWAP_LEN]    = { 0x84, 0xF8, 0x0A, 0xE0, 0x4F, 0xF0, 0x03, 0x0E };
static const uint8_t k_patched[PSB_SWAP_LEN] = { 0x4F, 0xF0, 0x03, 0x0E, 0x84, 0xF8, 0x0A, 0xE0 };

static SceUID g_inj_dtim = -1;

int psprofile_dtim_active(void)
{
	return g_inj_dtim >= 0;
}

int psprofile_dtim_set(int on)
{
	if (!on) {
		if (g_inj_dtim >= 0) {
			taiInjectReleaseForKernel(g_inj_dtim);
			g_inj_dtim = -1;
			WQ_KLOGI("psprofile: released (multiple_dtims 1 at next join)");
		}
		return WQ_OK;
	}
	if (psprofile_dtim_active())
		return WQ_OK;

	SceUID modid;
	uintptr_t text;
	if (wlanbt_text(PSB_SWAP_OFF + PSB_SWAP_LEN, &modid, &text) != WQ_OK)
		return WQ_ERR_RESOLVE;
	const volatile uint8_t *t = (const volatile uint8_t *)text + PSB_SWAP_OFF;

	/* Firmware check: the exact original instruction pair (3.60-3.68). */
	for (unsigned i = 0; i < PSB_SWAP_LEN; i++) {
		if (t[i] != k_orig[i]) {
			WQ_KLOGW("psprofile: original bytes mismatch at +%u "
			         "(%02x %02x %02x %02x %02x %02x %02x %02x) - unexpected "
			         "SceWlanBt, refusing", i, t[0], t[1], t[2], t[3], t[4],
			         t[5], t[6], t[7]);
			return WQ_ERR_RESOLVE;
		}
	}

	g_inj_dtim = taiInjectDataForKernel(KERNEL_PID, modid, 0, PSB_SWAP_OFF,
	                                    k_patched, PSB_SWAP_LEN);
	if (g_inj_dtim < 0) {
		WQ_KLOGW("psprofile: inject failed (0x%08x)", (unsigned)g_inj_dtim);
		g_inj_dtim = -1;
		return WQ_ERR_CHIP;
	}
	WQ_KLOGI("psprofile: applied (PS_MODE_ENH multiple_dtims 1 -> 3) - next join");
	return WQ_OK;
}

/* ---- PS on/off flag (maxperf vs balanced/maxbatt) --------------------------
 * The join path reads the driver global 0x8100ff90 (ctx+0x1d50): 0 => send
 * PS_MODE_ENH disable, 1 => enable. The supported setter is
 * FUN_81009628(code, from_kernel) cases 6 (off) / 7 (on). A user call
 * (sceWlanSetConfiguration -> from_kernel = 0) is silently DROPPED while the
 * driver state has bit 0x20 set (connected): only codes 2/3/4/8/10 pass that
 * gate, yet it still returns 0 (verified on hardware).
 * The driver itself calls it with from_kernel = 1 (0x81009602), which skips the
 * gate, so we do the same.
 *
 * The flag's runtime address comes from the loader-relocated movw/movt pair
 * at 0x8100966c (lr = &0x8100ff80, the block the setter's gate path stores
 * into); the flag is +0x10. Both instructions and the prologue are verified
 * first, and the result must land inside a SceWlanBt data segment. */
#define CFG_FN_OFF        0x9628u
#define CFG_MOVW_OFF      0x966Cu
#define CFG_MOVT_OFF      0x9672u
#define CFG_FLAG_DELTA    0x10u
#define CFG_PS_OFF        6
#define CFG_PS_ON         7

static int (*g_cfg_fn)(unsigned code, int from_kernel);
static volatile int *g_ps_flag;

static uint32_t t2_imm16(uint16_t h1, uint16_t h2)
{
	return ((uint32_t)(h1 & 0xF) << 12) | ((uint32_t)((h1 >> 10) & 1) << 11) |
	       ((uint32_t)((h2 >> 12) & 7) << 8) | (h2 & 0xFF);
}

static int resolve_ps_flag(void)
{
	if (g_cfg_fn && g_ps_flag)
		return WQ_OK;
	uintptr_t text;
	if (wlanbt_text(CFG_MOVT_OFF + 4, NULL, &text) != WQ_OK)
		return WQ_ERR_RESOLVE;
	const volatile uint8_t  *t8 = (const volatile uint8_t *)text;
	if (t8[CFG_FN_OFF] != 0x2D || t8[CFG_FN_OFF + 1] != 0xE9 ||
	    t8[CFG_FN_OFF + 2] != 0xF0 || t8[CFG_FN_OFF + 3] != 0x43) {
		WQ_KLOGW("psprofile: FUN_81009628 prologue mismatch - refusing");
		return WQ_ERR_RESOLVE;
	}
	const volatile uint16_t *w = (const volatile uint16_t *)(text + CFG_MOVW_OFF);
	const volatile uint16_t *m = (const volatile uint16_t *)(text + CFG_MOVT_OFF);
	if ((w[0] & 0xFBF0) != 0xF240 || ((w[1] >> 8) & 0xF) != 14 ||
	    (m[0] & 0xFBF0) != 0xF2C0 || ((m[1] >> 8) & 0xF) != 14) {
		WQ_KLOGW("psprofile: flag movw/movt mismatch - refusing");
		return WQ_ERR_RESOLVE;
	}
	uintptr_t flag = ((t2_imm16(m[0], m[1]) << 16) | t2_imm16(w[0], w[1])) + CFG_FLAG_DELTA;
	if (!wlanbt_seg_contains(1, flag, 4)) {
		WQ_KLOGW("psprofile: flag 0x%08x outside SceWlanBt data - refusing", (unsigned)flag);
		return WQ_ERR_RESOLVE;
	}
	g_ps_flag = (volatile int *)flag;
	g_cfg_fn = (int (*)(unsigned, int))(text + CFG_FN_OFF + 1);   /* Thumb */
	WQ_KLOGI("psprofile: PS flag at 0x%08x (now %d)", (unsigned)flag, *g_ps_flag);
	return WQ_OK;
}

int psprofile_ps_get(void)
{
	int r = resolve_ps_flag();
	return r < 0 ? r : (*g_ps_flag ? 1 : 0);
}

int psprofile_ps_set(int on)
{
	int r = resolve_ps_flag();
	if (r < 0)
		return r;
	r = g_cfg_fn(on ? CFG_PS_ON : CFG_PS_OFF, 1);
	int now = *g_ps_flag ? 1 : 0;
	WQ_KLOGI("psprofile: FUN_81009628(%d, 1) rc=0x%08x -> flag %d",
	         on ? CFG_PS_ON : CFG_PS_OFF, (unsigned)r, now);
	return r < 0 ? r : now;
}
