/*
 * ht40patch.c - advertise 20/40 MHz by patching two immediates in SceWlanBt.
 *
 * The driver hard-codes
 * 20 MHz in exactly two places, both plain instruction immediates:
 *
 *   FUN_81004b10 (ASSOCIATE 0x12 builder), HT Capabilities TLV 0x2d:
 *     8100501a  4f f0 2c 0c   mov.w r12, #0x2c     ; HT Cap Info low byte
 *     -> 0x6e: + bit1 (Supported Channel Width 20/40) + bit6 (SGI-40),
 *        so HT Cap Info 0x112c -> 0x116e.
 *   FUN_8100b570 (associate), the only 11N_CFG (0xCD) call site:
 *     8100b65a  20 21         movs r1, #0x20       ; ht_tx_cap = SGI-20
 *     -> 0x62: 20/40 + SGI-20 + SGI-40 (Linux mwifiex's default
 *        MWIFIEX_FW_DEF_HTTXCFG for this chip family).
 *
 * Both only run during an association, so the patch takes effect at the next
 * (re)association and costs nothing at runtime: no hooks, nothing on the
 * 0x2f44 command path, which is unsafe to hook. Each original byte is
 * verified first, so another firmware is refused rather than corrupted.
 */
#include <psp2kern/kernel/modulemgr.h>  /* ksceKernelSearchModuleByName/GetModuleInfo */
#include <psp2kern/kernel/sysclib.h>    /* memset                                     */
#include <psp2kern/types.h>             /* SCE_KERNEL_PROCESS_ID                      */
#include <taihen.h>

#include "ht40patch.h"
#include "klog.h"
#include "../include/wifiturbo.h"

#define HTCAP_INSN_OFF   0x501Au   /* mov.w r12, #imm8 (4 bytes)        */
#define HTCAP_IMM_OFF    0x501Cu   /* its imm8 byte                     */
#define HTCAP_20         0x2Cu
#define HTCAP_40         0x6Eu

#define TXCAP_INSN_OFF   0xB65Au   /* movs r1, #imm8 (2 bytes)          */
#define TXCAP_20         0x20u
#define TXCAP_40         0x62u

static SceUID g_inj_htcap = -1;
static SceUID g_inj_txcap = -1;

static void release_all(void)
{
	if (g_inj_htcap >= 0) { taiInjectReleaseForKernel(g_inj_htcap); g_inj_htcap = -1; }
	if (g_inj_txcap >= 0) { taiInjectReleaseForKernel(g_inj_txcap); g_inj_txcap = -1; }
}

int ht40patch_active(void)
{
	return g_inj_htcap >= 0 && g_inj_txcap >= 0;
}

int ht40patch_set(int on)
{
	if (!on) {
		release_all();
		WQ_KLOGI("ht40patch: released (20 MHz at next association)");
		return WQ_OK;
	}
	if (ht40patch_active())
		return WQ_OK;

	SceUID modid = ksceKernelSearchModuleByName("SceWlanBt");
	if (modid < 0) {
		WQ_KLOGW("ht40patch: SceWlanBt not found (%d)", modid);
		return WQ_ERR_RESOLVE;
	}
	SceKernelModuleInfo info;
	memset(&info, 0, sizeof(info));
	info.size = sizeof(info);
	if (ksceKernelGetModuleInfo(SCE_KERNEL_PROCESS_ID, modid, &info) < 0 ||
	    !info.segments[0].vaddr || info.segments[0].memsz <= TXCAP_INSN_OFF + 1) {
		WQ_KLOGW("ht40patch: SceWlanBt text segment unavailable");
		return WQ_ERR_RESOLVE;
	}
	const volatile uint8_t *t = (const volatile uint8_t *)info.segments[0].vaddr;

	/* Firmware check: the exact original instructions (3.60). */
	if (t[HTCAP_INSN_OFF] != 0x4F || t[HTCAP_INSN_OFF + 1] != 0xF0 ||
	    t[HTCAP_IMM_OFF] != HTCAP_20 || t[HTCAP_IMM_OFF + 1] != 0x0C ||
	    t[TXCAP_INSN_OFF] != TXCAP_20 || t[TXCAP_INSN_OFF + 1] != 0x21) {
		WQ_KLOGW("ht40patch: original bytes mismatch (%02x %02x %02x %02x / %02x %02x) "
		         "- not 3.60 SceWlanBt, refusing",
		         t[HTCAP_INSN_OFF], t[HTCAP_INSN_OFF + 1], t[HTCAP_IMM_OFF],
		         t[HTCAP_IMM_OFF + 1], t[TXCAP_INSN_OFF], t[TXCAP_INSN_OFF + 1]);
		return WQ_ERR_RESOLVE;
	}

	static const uint8_t htcap40 = HTCAP_40;
	static const uint8_t txcap40 = TXCAP_40;
	g_inj_htcap = taiInjectDataForKernel(KERNEL_PID, modid, 0, HTCAP_IMM_OFF, &htcap40, 1);
	g_inj_txcap = taiInjectDataForKernel(KERNEL_PID, modid, 0, TXCAP_INSN_OFF, &txcap40, 1);
	if (g_inj_htcap < 0 || g_inj_txcap < 0) {
		WQ_KLOGW("ht40patch: inject failed (htcap=0x%08x txcap=0x%08x)",
		         (unsigned)g_inj_htcap, (unsigned)g_inj_txcap);
		if (g_inj_htcap < 0) g_inj_htcap = -1;
		if (g_inj_txcap < 0) g_inj_txcap = -1;
		release_all();
		return WQ_ERR_CHIP;
	}
	WQ_KLOGI("ht40patch: applied (HT cap 0x116e, 11N_CFG tx 0x62) - next association");
	return WQ_OK;
}
