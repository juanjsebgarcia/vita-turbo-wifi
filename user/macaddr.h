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

/* Settings page: queue one 'mac ...' command line for the watcher thread, so
 * the caller (the keepwifi tick) never waits on a change. 0 or WQ_ERR_BUSY. */
int wq_mac_queue(const char *line);

/* Keepwifi network thread, once a second: while no MAC watcher is running
 * (Default since boot), learn the console's own address from SceNet once
 * connected. Cheap: nothing once it is known. */
void wq_mac_net_tick(void);

/* HUD line, Custom only (0 in Default): 1 + the custom address (in use),
 * 2 + the custom address (a change to it is under way), 3 + the address in
 * use (the custom one did not take). */
int wq_mac_hud(uint8_t mac[6]);

/* Published for the Settings page: the address in use (two words, WqUiState
 * form) and 1 while a change is under way. */
void wq_mac_publish(int *use0, int *use1, int *busy);

/* Boot: the saved mode, custom address and the console's own address (each
 * as two words, WqUiState form). Starts the power-up watcher and, for
 * Custom, sends the address at a chip power-up before the first join: the RX
 * buffer preset's boot cycle (rx_cycle != 0) or one of its own. */
void wq_mac_boot(int mode, unsigned m0, unsigned m1, unsigned f0, unsigned f1, int rx_cycle);

#endif /* WIFITURBO_USER_MACADDR_H */
