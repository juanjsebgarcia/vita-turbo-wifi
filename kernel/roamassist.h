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

#endif /* WIFITURBO_ROAMASSIST_H */
