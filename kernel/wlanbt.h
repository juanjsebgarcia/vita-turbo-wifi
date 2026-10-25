/*
 * wlanbt.h - locate SceWlanBt's text segment and the patch sites inside it on
 * any supported firmware. See wlanbt.c.
 */
#ifndef WIFITURBO_WLANBT_H
#define WIFITURBO_WLANBT_H

#include <psp2kern/types.h>
#include <stddef.h>
#include <stdint.h>

/* Text offsets of every site the plugin patches or hooks. SceWlanBt was rebuilt
 * for 3.71, so the same code sits at different offsets there. */
typedef struct WlanbtLayout {
	const char *name;
	uint32_t rssi_cb;      /* RSSI_INFO completion callback (chipobserve) */
	uint32_t htcap_insn;   /* mov.w r12,#0x2c, HT Cap Info (ht40patch)    */
	uint32_t txcap_insn;   /* movs r1,#0x20, 11N_CFG tx cap (ht40patch)   */
	uint32_t dtim_swap;    /* strb.w lr,[r4,#0xa]; mov.w lr,#3 (psprofile)*/
	uint32_t cfg_fn;       /* configuration setter (psprofile)            */
	uint32_t cfg_movw;     /* movw/movt pair loading the PS flag block    */
	uint32_t cfg_movt;
	uint32_t join;         /* join ioctl handler (roamassist)             */
	uint32_t scan;         /* scan wrapper (roamassist)                   */
} WlanbtLayout;

/* The layout of the running SceWlanBt, or NULL if it matches none we know.
 * Picked once by checking the original bytes at every site of a layout. */
const WlanbtLayout *wlanbt_layout(void);

/* Find SceWlanBt and the base of its text segment. `need` is the number of
 * bytes from the base the caller will touch (0 = no bounds check). Returns
 * WQ_OK and fills *modid / *text, or WQ_ERR_RESOLVE. Either out-pointer may
 * be NULL. */
int wlanbt_text(uint32_t need, SceUID *modid, uintptr_t *text);

/* 1 if [addr, addr+len) lies inside SceWlanBt segment `seg`, else 0. */
int wlanbt_seg_contains(int seg, uintptr_t addr, uint32_t len);

#endif /* WIFITURBO_WLANBT_H */
