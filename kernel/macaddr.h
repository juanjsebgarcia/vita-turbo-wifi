/*
 * macaddr.h - EXPERIMENTAL: give the Wi-Fi interface a chosen MAC address
 * through the driver's own get-MAC call, and keep SceNetPs's copy in step.
 * See macaddr.c.
 */
#ifndef WIFITURBO_MACADDR_H
#define WIFITURBO_MACADDR_H

#include <stdint.h>
#include "../include/wifiturbo_kernel_api.h"

/* One WQ_MAC_OP_*; always fills *out. */
int  macaddr_op(int op, const uint8_t mac[6], WqMacStatus *out);
void macaddr_init(void);   /* module_start: the op mutex */
void macaddr_fini(void);

#endif /* WIFITURBO_MACADDR_H */
