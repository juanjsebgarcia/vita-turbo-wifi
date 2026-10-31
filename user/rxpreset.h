/*
 * rxpreset.h - EXPERIMENTAL: the chip RX ring setting (Stock / Enhanced /
 * Maximum). See rxpreset.c.
 */
#ifndef WIFITURBO_RXPRESET_H
#define WIFITURBO_RXPRESET_H

/* Apply preset 0 stock / 1 enhanced (127) / 2 maximum (149) in the background:
 * edit the cached chip image, then power-cycle the radio if Wi-Fi is on.
 * WQ_OK, WQ_ERR_ARG, WQ_ERR_BUSY (one already running) or WQ_ERR_NOT_READY. */
int rxpreset_set(int level);

/* Boot: re-apply the saved preset once Wi-Fi is up (falls back to stock if the
 * previous power-cycle never completed). */
void rxpreset_boot(int level);

/* "stock" / "enhanced" / "maximum" -> 0 / 1 / 2, else -1. */
int rxpreset_level(const char *name);

/* The boot run: 0 not started, 1 deciding (waiting for the radio), 2 a
 * radio power-cycle was started, 3 it ended without one (stock, a marker
 * fallback, an unknown image, Wi-Fi off, or a refusal). */
int rxpreset_boot_state(void);

#endif /* WIFITURBO_RXPRESET_H */
