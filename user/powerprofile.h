/*
 * powerprofile.h - Wi-Fi power profiles (max performance / balanced /
 * battery saver) and the 40 MHz (HT40) option on 2.4 GHz.
 *
 * Every function here changes link-affecting chip state, so every one arms the
 * auto-revert harness BEFORE applying.
 */
#ifndef WIFITURBO_POWERPROFILE_H
#define WIFITURBO_POWERPROFILE_H

#include "../include/wifiturbo.h"

/* Apply a power/latency profile (WQ_PROFILE_*): snapshot the driver PS flag and
 * the DTIM-skip patch state, arm a capped revert, set the new state and
 * reassociate. Returns the safety token
 * (>=0) or negative WQ_ERR_*. `cap_ms` 0 -> 45 s. */
int wq_power_set_profile(int profile, uint32_t cap_ms);

/* The name of the last profile applied (for the HUD). */
const char *wq_power_profile_name(int profile);

/* The last profile applied (WQ_PROFILE_*), for the HUD display. */
int wq_power_current_profile(void);

/* Set the HUD's displayed profile without applying it to the chip (boot). */
void wq_power_set_display_profile(int profile);

/* Boot: apply the saved profile + 40 MHz state (no auto-revert, no hooks). */
int wq_power_boot_restore(int profile, int ht40);

/*
 * Toggle 40 MHz channel width on 2.4 GHz (kernel/ht40patch.c: HT Cap Info and
 * 11N_CFG 0xCD). A width change reassociates, so this always arms the
 * auto-revert harness first.
 * Needs a HT40-capable AP; may be disallowed by regulatory / BT coexistence.
 * Returns the safety token (>=0) or negative WQ_ERR_*.
 */
int wq_power_set_ht40(int enable, uint32_t cap_ms);

#endif /* WIFITURBO_POWERPROFILE_H */
