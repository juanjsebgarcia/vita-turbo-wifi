/*
 * macaddr.h - EXPERIMENTAL: the "MAC address" setting (kernel/macaddr.c).
 * See macaddr.c.
 */
#ifndef WIFITURBO_USER_MACADDR_H
#define WIFITURBO_USER_MACADDR_H

#include <stdint.h>

/* Parse "AA:BB:CC:DD:EE:FF" (':' '-' '.' or no separators, any case) into
 * mac[6]. 0 if it is a usable station address, else <0 with *why set:
 * 1 not 12 hex digits, 2 group (multicast) bit set, 3 all-zero/broadcast. */
int wq_mac_parse(const char *s, uint8_t mac[6], int *why);

/* A random address: factory's first three bytes (the maker) + three random
 * bytes, never factory itself. System RNG, clock fallback. */
void wq_mac_random(uint8_t mac[6], const uint8_t factory[6]);

/* Run one 'mac ...' command (the text after "mac "); writes a reply line. */
int wq_mac_cmd(const char *args, char *out, int outcap);

/* HUD line: 1 and the custom address while one is in use, else 0. */
int wq_mac_hud(uint8_t mac[6]);

/* Boot: the saved mode, custom address and the console's own address (each
 * as two words, WqUiState form). Starts the power-up watcher and, for
 * Custom, sends the address at a chip power-up before the first join: the RX
 * buffer preset's boot cycle (rx_cycle != 0) or one of its own. */
void wq_mac_boot(int mode, unsigned m0, unsigned m1, unsigned f0, unsigned f1, int rx_cycle);

#endif /* WIFITURBO_USER_MACADDR_H */
