/*
 * hud.h - read-only Wi-Fi diagnostics overlay, kernel side.
 *
 * The overlay is drawn by hooking SceDisplay's set-framebuffer path, so it
 * appears over whatever is on screen in any app. It renders ONLY data the user
 * module hands it (WqHudState); it issues no commands and changes no link
 * state, which is why it is the lowest-risk validation of the whole stack.
 */
#ifndef WIFITURBO_HUD_H
#define WIFITURBO_HUD_H

#include "../include/wifiturbo.h"

int  hud_init(void);     /* install the sceDisplaySetFrameBuf hook            */
void hud_fini(void);     /* remove it                                         */
int  hud_set_enabled(int enabled);
int  hud_get_status(void);                   /* read-only; WQ_HUD_STATUS_* bits */
int  hud_set_state(const WqHudState *state); /* copy-in from user             */

/* Framebuffer PIXEL-WRITE gate (OFF until 'hud on' arms it with the overlay). */
int  hud_set_draw_armed(int on);

#endif /* WIFITURBO_HUD_H */
