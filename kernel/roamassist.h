/*
 * roamassist.h - one-shot join-path roam assist (SceWlanBt join ioctl hooks).
 * See roamassist.c.
 */
#ifndef WIFITURBO_ROAMASSIST_H
#define WIFITURBO_ROAMASSIST_H

#include "../include/wifiturbo.h"

/* Arm the assist for the NEXT kernel join (ioctl 0x50110001) only. flags:
 * WQ_RA_ARM (else disarm), WQ_RA_TARGET (prefer bssid[] if it is in the scan).
 * Installs the two hooks on first use (never at boot). WQ_OK / WQ_ERR_*. */
int roamassist_arm(unsigned flags, const uint8_t bssid[6]);
int roamassist_get_status(WqRoamAssistStatus *out);
void roamassist_fini(void);

/* Called at the start of every kernel join, in the joining thread, before the
 * join handler runs (no driver lock held), with the join handler's own context
 * argument (the driver ctx + 0x580) and the network name (NULL if the request
 * could not be read). A negative return
 * refuses the join with that code (the handler is not called). NULL to clear.
 * Installs the hooks if needed. WQ_OK / WQ_ERR_*. */
typedef int (*roamassist_join_cb)(void *jctx, const uint8_t *ssid, uint32_t len);
int roamassist_set_join_cb(roamassist_join_cb cb);

/* Called before every scan that is not part of a join (the user scan ioctl
 * 0x50100001: SceShell's own looking for networks; and the background-scan
 * start 0x50100002, used by the PSP emulator), in the scanning thread,
 * no driver lock held, with the scan's context argument (driver ctx + 0x580).
 * A negative return skips the scan with that code. NULL to clear. Installs the
 * hooks if needed. */
typedef int (*roamassist_scan_cb)(void *sctx);
int roamassist_set_scan_cb(roamassist_scan_cb cb);

/* 1 while a kernel join is in flight on any thread. */
int roamassist_join_in_flight(void);

#endif /* WIFITURBO_ROAMASSIST_H */
