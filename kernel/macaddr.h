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
/* An address per network (see macaddr.c): on with the 16-byte secret. */
int  macaddr_pernet(int on, const uint32_t key[4]);
void macaddr_pernet_stats(WqMacPernetStats *out);
void macaddr_init(void);   /* module_start: the op mutex */
/* module_start, after macaddr_init: saved mode per network -> on before
 * SceShell starts (a thread waits for the driver); joins held until then. */
void macaddr_pernet_boot(void);
/* module_stop, before roamassist_fini: ends the boot thread. */
void macaddr_pernet_stop(void);
void macaddr_fini(void);

#endif /* WIFITURBO_MACADDR_H */
