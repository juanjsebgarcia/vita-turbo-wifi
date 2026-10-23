/*
 * ht40patch.h - advertise 20/40 MHz by patching SceWlanBt's two hard-coded
 * 20 MHz immediates (ASSOCIATE HT Cap Info, 11N_CFG ht_tx_cap). Takes effect at
 * the next association. See ht40patch.c.
 */
#ifndef WIFITURBO_HT40PATCH_H
#define WIFITURBO_HT40PATCH_H

int ht40patch_set(int on);   /* 1 = patch in, 0 = release; WQ_OK or WQ_ERR_* */
int ht40patch_active(void);

#endif /* WIFITURBO_HT40PATCH_H */
