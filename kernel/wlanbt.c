/*
 * wlanbt.c - locate SceWlanBt's text segment on any supported firmware.
 *
 * ksceKernelGetModuleInfo lives in SceModulemgrForKernel, whose library and
 * function NIDs changed in 3.63 (3.60: 0xC445FA63/0xD269F915, 3.63+:
 * 0x92C9FFC2/0xDAA90093), so importing it directly stops the whole module
 * loading on 3.65 and 3.68. taiHEN's module_get_offset (taihenModuleUtils) does the
 * same lookup on every firmware taiHEN supports, and refuses an offset past
 * the end of the segment.
 *
 * SceWlanBt itself is byte-identical on 3.60, 3.65 and 3.68 apart from its
 * version stamp, so the patch offsets used elsewhere apply to all three; each
 * caller still checks the original bytes before touching anything.
 */
#include <psp2kern/kernel/modulemgr.h>  /* ksceKernelSearchModuleByName */
#include <taihen.h>

#include "wlanbt.h"
#include "klog.h"
#include "../include/wifiturbo.h"

/* taihenModuleUtils (no SDK header). */
int module_get_offset(SceUID pid, SceUID modid, int segidx, size_t offset, uintptr_t *addr);

int wlanbt_text(uint32_t need, SceUID *modid_out, uintptr_t *text_out)
{
	SceUID modid = ksceKernelSearchModuleByName("SceWlanBt");
	if (modid < 0) {
		WQ_KLOGW("wlanbt: SceWlanBt not found (0x%08x)", (unsigned)modid);
		return WQ_ERR_RESOLVE;
	}
	uintptr_t base = 0, last = 0;
	int r = module_get_offset(KERNEL_PID, modid, 0, 0, &base);
	if (r >= 0 && need)
		r = module_get_offset(KERNEL_PID, modid, 0, need - 1, &last);
	if (r < 0 || !base) {
		WQ_KLOGW("wlanbt: text segment unavailable (0x%08x, need 0x%x)",
		         (unsigned)r, (unsigned)need);
		return WQ_ERR_RESOLVE;
	}
	if (modid_out) *modid_out = modid;
	if (text_out)  *text_out  = base;
	return WQ_OK;
}

int wlanbt_seg_contains(int seg, uintptr_t addr, uint32_t len)
{
	SceUID modid = ksceKernelSearchModuleByName("SceWlanBt");
	uintptr_t base = 0, last = 0;
	if (modid < 0 || len == 0 ||
	    module_get_offset(KERNEL_PID, modid, seg, 0, &base) < 0 || !base || addr < base)
		return 0;
	return module_get_offset(KERNEL_PID, modid, seg, addr - base + len - 1, &last) >= 0;
}
