/*
 * advanced.h - the libnetctl scan reader (and the read-only `adv scan` command).
 *
 * Runs in the user module (inside SceShell). The libnetctl IpcInt scan verbs
 * are resolved at runtime by NID. Smart Mesh Roaming's pre-check (keepwifi.c) and
 * `selftest` use the reader; `adv scan` prints the same scan as text.
 */
#ifndef WIFITURBO_ADVANCED_H
#define WIFITURBO_ADVANCED_H

#include "../include/wifiturbo.h"

/* Resolve the runtime libnetctl scan verbs (non-fatal; the reader and
 * `adv scan` report "n/a" until they resolve). */
int  wq_adv_init(void);
void wq_adv_fini(void);

/* Execute one `adv <args>` command (args is the text after "adv "). Writes a
 * human-readable reply into out (NUL-terminated, <= outcap). */
int  wq_adv_exec(const char *args, char *out, int outcap);

/* 1 if the libnetctl scan verbs resolved on this build. */
int  wq_adv_scan_available(void);

/* Run one broad scan and parse up to `max` 0x60-byte records into `out`.
 * Returns the number of records written (>=0), or a negative WQ_ERR_*.
 * A pure read - it never changes the link. */
int  wq_adv_scan_read(WqScanEntry *out, int max);

#endif /* WIFITURBO_ADVANCED_H */
