/*
 * rxring.h - EXPERIMENTAL: enlarge the Wi-Fi chip's receive ring. See rxring.c
 * for the safety rules.
 */
#ifndef WIFITURBO_RXRING_H
#define WIFITURBO_RXRING_H

#include "../include/wifiturbo_kernel_api.h"   /* WqRxRingStats, WQ_RXRING_* */

/* WQ_RXRING_OP_*; always fills *out. WQ_OK or WQ_ERR_*. */
int  rxring_op(int op, WqRxRingStats *out);

/* module_stop: if a size is applied, restore the stock bytes in the cached
 * image so a module reload (or another plugin) sees stock firmware. */
void rxring_fini(void);

#endif /* WIFITURBO_RXRING_H */
