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
 * SceWlanBt has the same code layout from 3.60 to 3.70. 3.71 rebuilt it, and
 * 3.71 to 3.74 share the new layout: the code at every site the plugin uses is
 * unchanged, but it moved (by 0x10c to 0x2b8 bytes), and its data layout is
 * the same. The layout is chosen by checking the original bytes at every site
 * of each table, so a driver that matches neither is left alone; each caller
 * still checks its own bytes before touching anything.
 */
#include <psp2kern/kernel/modulemgr.h>  /* ksceKernelSearchModuleByName */
#include <taihen.h>

#include "wlanbt.h"
#include "klog.h"
#include "../include/wifiturbo.h"

/* taihenModuleUtils (no SDK header). */
int module_get_offset(SceUID pid, SceUID modid, int segidx, size_t offset, uintptr_t *addr);

static const WlanbtLayout k_layouts[] = {
	{ "3.60-3.70", 0x4838u, 0x501Au, 0xB65Au, 0x6082u, 0x9628u, 0x966Cu, 0x9672u, 0x6EA0u, 0xB3A0u, 0x2DB4u, 0x73F0u, 0x2C08u },
	{ "3.71-3.74", 0x4944u, 0x5146u, 0xB912u, 0x623Au, 0x98ACu, 0x98F0u, 0x98F6u, 0x712Cu, 0xB658u, 0x2DB4u, 0x7684u, 0x2C08u },
};

/* Original bytes at each site, the same on every layout. */
static const uint8_t k_rssi_cb[6] = { 0x03, 0x69, 0x1E, 0x2B, 0x1D, 0xD1 };
static const uint8_t k_htcap[4]   = { 0x4F, 0xF0, 0x2C, 0x0C };
static const uint8_t k_txcap[2]   = { 0x20, 0x21 };
static const uint8_t k_dtim[8]    = { 0x84, 0xF8, 0x0A, 0xE0, 0x4F, 0xF0, 0x03, 0x0E };
static const uint8_t k_cfg_fn[4]  = { 0x2D, 0xE9, 0xF0, 0x43 };
static const uint8_t k_join[4]    = { 0x2D, 0xE9, 0xF0, 0x4F };
static const uint8_t k_scan[8]    = { 0xF8, 0xB5, 0x00, 0xF1, 0x08, 0x04, 0x05, 0x46 };

static int bytes_eq(const volatile uint8_t *t, uint32_t off, const uint8_t *b, unsigned n)
{
	for (unsigned i = 0; i < n; i++)
		if (t[off + i] != b[i])
			return 0;
	return 1;
}

/* Text bytes the layout touches, from the base. */
static uint32_t layout_need(const WlanbtLayout *l)
{
	uint32_t n = l->cfg_movt + 4;
	if (l->txcap_insn + sizeof(k_txcap) > n) n = l->txcap_insn + sizeof(k_txcap);
	if (l->scan + sizeof(k_scan) > n)        n = l->scan + sizeof(k_scan);
	return n;
}

static int layout_matches(const WlanbtLayout *l, const volatile uint8_t *t)
{
	return bytes_eq(t, l->rssi_cb, k_rssi_cb, sizeof(k_rssi_cb)) &&
	       bytes_eq(t, l->htcap_insn, k_htcap, sizeof(k_htcap)) &&
	       bytes_eq(t, l->txcap_insn, k_txcap, sizeof(k_txcap)) &&
	       bytes_eq(t, l->dtim_swap, k_dtim, sizeof(k_dtim)) &&
	       bytes_eq(t, l->cfg_fn, k_cfg_fn, sizeof(k_cfg_fn)) &&
	       bytes_eq(t, l->join, k_join, sizeof(k_join)) &&
	       bytes_eq(t, l->scan, k_scan, sizeof(k_scan));
}

const WlanbtLayout *wlanbt_layout(void)
{
	static const WlanbtLayout *s_layout;
	static int s_done;
	if (s_done)
		return s_layout;
	SceUID modid;
	uintptr_t text, last;
	if (wlanbt_text(0, &modid, &text) != WQ_OK)
		return NULL;                          /* not loaded yet: try again later */
	for (unsigned i = 0; i < sizeof(k_layouts) / sizeof(k_layouts[0]); i++) {
		const WlanbtLayout *l = &k_layouts[i];
		/* Skip a table whose sites run past this driver's text. */
		if (module_get_offset(KERNEL_PID, modid, 0, layout_need(l) - 1, &last) < 0)
			continue;
		if (layout_matches(l, (const volatile uint8_t *)text)) {
			s_layout = l;
			break;
		}
	}
	s_done = 1;
	if (s_layout)
		WQ_KLOGI("wlanbt: SceWlanBt layout %s", s_layout->name);
	else
		WQ_KLOGW("wlanbt: SceWlanBt matches no known layout - driver features off");
	return s_layout;
}

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
