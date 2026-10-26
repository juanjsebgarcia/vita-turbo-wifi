/*
 * diag.h - read-only Wi-Fi diagnostics (SAFE TIER).
 *
 * Pure reads: SceNetCtl userland info + the SNR / noise floor the kernel
 * parses from the driver's own RSSI_INFO replies. No link state is touched.
 */
#ifndef WIFITURBO_DIAG_H
#define WIFITURBO_DIAG_H

#include "../include/wifiturbo.h"

int  diag_init(void);   /* sceNetCtlInit + register the drop-reason callback  */
void diag_fini(void);

/* 1 once SceShell has initialised SceNetCtl (a state read succeeds), else 0.
 * Read-only: never initialises netctl. */
int  diag_netctl_ready(void);

/* Sample everything into *out (HUD-ready). Safe to call frequently. */
int  diag_sample(WqHudState *out);

/* Read-only self-test probe: WQ_OK if SceNetCtl state is readable; fills
 * *state_out with the current WQ_NETCTL_* state when non-NULL. */
int  diag_selftest(int *state_out);

#endif /* WIFITURBO_DIAG_H */
