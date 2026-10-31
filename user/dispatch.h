/*
 * dispatch.h - text command surface (in the style of vitacompanion) + the
 * reusable command core the Settings entry also calls.
 *
 * Surface 1 (this file): a small TCP listener that accepts newline-terminated
 * text commands and writes back a text reply, exactly like vitacompanion's
 * command channel. Default port 1339 (vitacompanion uses 1337 for FTP and 1338).
 *
 * Surface 2 (settings_entry.c): the same wq_dispatch_exec() core, so the two
 * surfaces never diverge.
 */
#ifndef WIFITURBO_DISPATCH_H
#define WIFITURBO_DISPATCH_H

#define WQ_DISPATCH_PORT   1339

int  wq_dispatch_start(void);   /* bring up SceNet + the listener thread      */
void wq_dispatch_stop(void);

/* Execute one command line, writing a human-readable reply into out (NUL-
 * terminated, at most outcap). Returns WQ_OK or a negative WQ_ERR_*. This is
 * transport-independent: the TCP server and the Settings entry both call it. */
int  wq_dispatch_exec(const char *line, char *out, int outcap);

/* HUD runtime control (defined in main.c). Arms/disarms BOTH the HUD overlay
 * and the sampler thread that feeds it. Neither runs at module_start - this is
 * the only path that starts them, so the plugin boots completely inert and a
 * bad renderer can never wedge the cold boot. Called by the 'hud on|off'
 * command. Returns WQ_OK. */
int  wq_hud_runtime_set(int on);
void wq_hud_set_compact(int compact);   /* 1 = Compact (one line), 0 = Extended */

#endif /* WIFITURBO_DISPATCH_H */
