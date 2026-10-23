/*
 * chipobserve.h - SNR / noise floor from the SceWlanBt driver's own RSSI_INFO
 * replies, by HOOKING its RSSI_INFO completion callback (FUN_81004838) rather
 * than sending any command.
 *
 * The hook is installed lazily (chipobserve_rssi_ensure, from
 * wqChipObserveGetLink, which the user side only calls while connected), never
 * at module_start, so it can never touch the cold boot or the association burst.
 */
#ifndef WIFITURBO_CHIPOBSERVE_H
#define WIFITURBO_CHIPOBSERVE_H

#include "../include/wifiturbo.h"

int  chipobserve_get_link(WqLinkInternals *out);      /* parsed from replies    */
int  chipobserve_rssi_ensure(void);  /* hook the RSSI_INFO callback (SNR/NF)    */
void chipobserve_fini(void);         /* remove the hook on stop                */

#endif /* WIFITURBO_CHIPOBSERVE_H */
