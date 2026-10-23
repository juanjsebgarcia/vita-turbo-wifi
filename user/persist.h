/*
 * persist.h - save/load the wifiturbo settings across reboots.
 *
 * A tiny file (ur0:data/wifiturbo/settings.bin) holding the WqUiState the user
 * configured. The resident core loads+applies it at boot and re-saves on every
 * change, so toggles/profile survive a reboot. Defaults (keepwifi ON) are used
 * when no file exists yet.
 */
#ifndef WIFITURBO_PERSIST_H
#define WIFITURBO_PERSIST_H

#include "../include/wifiturbo_kernel_api.h"   /* WqUiState */

void wq_persist_load(WqUiState *out);       /* fills defaults if no/invalid file */
void wq_persist_save(const WqUiState *s);

#endif /* WIFITURBO_PERSIST_H */
