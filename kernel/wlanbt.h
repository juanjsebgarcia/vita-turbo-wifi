/*
 * wlanbt.h - locate SceWlanBt's text segment on any supported firmware.
 * See wlanbt.c.
 */
#ifndef WIFITURBO_WLANBT_H
#define WIFITURBO_WLANBT_H

#include <psp2kern/types.h>
#include <stddef.h>
#include <stdint.h>

/* Find SceWlanBt and the base of its text segment. `need` is the number of
 * bytes from the base the caller will touch (0 = no bounds check). Returns
 * WQ_OK and fills *modid / *text, or WQ_ERR_RESOLVE. Either out-pointer may
 * be NULL. */
int wlanbt_text(uint32_t need, SceUID *modid, uintptr_t *text);

/* 1 if [addr, addr+len) lies inside SceWlanBt segment `seg`, else 0. */
int wlanbt_seg_contains(int seg, uintptr_t addr, uint32_t len);

#endif /* WIFITURBO_WLANBT_H */
