/*
 * rxcycle.h - EXPERIMENTAL: power-cycle the Wi-Fi chip from the console so a
 * cached-image edit (kernel/rxring.c) loads with nobody at the Vita, guarded by
 * a reboot watchdog. See rxcycle.c.
 */
#ifndef WIFITURBO_RXCYCLE_H
#define WIFITURBO_RXCYCLE_H

/* Start a cycle in the background; returns at once. guard_min > 0 keeps the
 * watchdog running that many minutes after the link is back (reboot if it is
 * then down 60 s in a row). WQ_OK, or WQ_ERR_BUSY if one is already running,
 * WQ_ERR_NOT_READY if a thread could not start. */
int rxcycle_start(int guard_min);

/* As rxcycle_start, plus: require_link = 0 counts the radio being back on as
 * success (for a radio that was not associated before the cycle); marker, if
 * not NULL, is a file path removed once the cycle succeeds. */
int rxcycle_start_ex(int guard_min, int require_link, const char *marker);

/* End a running guard early (the watchdog thread then exits). */
void rxcycle_guard_stop(void);

/* 0 idle, 1 radio off, 2 radio on / waiting for the link, 3 rebooting,
 * 4 guarding, 5 watching the gateway round trip after the link is back (a new
 * cycle may start then). */
int rxcycle_phase(void);

/* 1 when no cycle is running, or its worker has finished turning the radio
 * (and Bluetooth) back on: the chip is no longer in a power transition. */
int rxcycle_settled(void);

/* 1 if the last cycle saw SceWlanBt report the chip powered off (so the
 * firmware was reloaded on power-up), else 0. */
int rxcycle_last_powered_down(void);

/* Cold-reset the console now (SceShell's reset, then a syscon reset if that
 * has not taken within 30 s). Does not return if it works. */
int rxcycle_reboot(void);

/* The same, from its own thread after 2 s, so a command reply can go out. */
int rxcycle_reboot_async(void);

#endif /* WIFITURBO_RXCYCLE_H */
