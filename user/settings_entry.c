/*
 * settings_entry.c - the resident (SceShell) side of the Settings page.
 *
 * The Settings app (NPXS10015, settings_app/wifiturbo_settings.c) is a separate
 * process; it queues dispatcher commands through the kernel mailbox
 * (uibridge.c). This file drains them (wq_settings_pump), folds every
 * setting-changing command into the persisted state (wq_settings_note) and
 * restores the saved settings at boot (wq_settings_boot).
 */
#include <string.h>

#include "settings_entry.h"
#include "dispatch.h"
#include "keepwifi.h"
#include "rxpreset.h"       /* rxpreset_boot - chip RX buffer setting */
#include "powerprofile.h"   /* wq_power_set_display_profile - HUD profile line */
#include "persist.h"
#include "log.h"
#include "safety.h"         /* confirm Settings-originated changes */
#include "../include/wifiturbo.h"
#include "../include/wifiturbo_kernel_api.h"   /* wqUi* mailbox, wqHudGetStatus */

/* Authoritative resident settings, persisted across reboots. Loaded at boot
 * (wq_settings_boot), updated whenever a setting command flows through, and
 * published so the Settings UI reflects it. */
static WqUiState g_settings;
static int       g_settings_ready = 0;

static int str_pre(const char *s, const char *p)
{
	while (*p) { if (*s++ != *p++) return 0; }
	return 1;
}

/* Fold a dispatch command that changes a setting into g_settings. Returns 1 if
 * a value actually changed. */
static int settings_track(const char *line)
{
	WqUiState b = g_settings;
	if      (str_pre(line, "keepwifi "))      g_settings.keepwifi      = str_pre(line, "keepwifi on");
	/* 'hud pos ...' MUST be caught before the 'hud ' overlay toggle, or it would
	 * read as 'hud off'. Accepts 0..3 or tl/tr/bl/br (the :1339 surface). */
	else if (str_pre(line, "hud pos ")) {
		const char *a = line + 8;             /* past "hud pos " */
		int pos = -1;
		if      (a[0] == '0' || (a[0] == 't' && a[1] == 'l')) pos = 0;
		else if (a[0] == '1' || (a[0] == 't' && a[1] == 'r')) pos = 1;
		else if (a[0] == '2' || (a[0] == 'b' && a[1] == 'l')) pos = 2;
		else if (a[0] == '3' || (a[0] == 'b' && a[1] == 'r')) pos = 3;
		if (pos >= 0) g_settings.hudpos = pos;
	}
	else if (str_pre(line, "hud "))           g_settings.hud           = str_pre(line, "hud on");
	else if (str_pre(line, "autoreconnect ")) g_settings.autoreconnect = str_pre(line, "autoreconnect on");
	else if (str_pre(line, "ht40 "))          g_settings.ht40          = str_pre(line, "ht40 on");
	/* only 'roam on|off' - 'roam now/status/precheck' must not clear it */
	else if (str_pre(line, "devmode "))       g_settings.devmode       = str_pre(line, "devmode on");
	else if (str_pre(line, "roam on") || str_pre(line, "roam off"))
	                                          g_settings.roam          = str_pre(line, "roam on");
	else if (str_pre(line, "profile "))       g_settings.profile       =
	             str_pre(line, "profile maxperf") ? 0 :
	             str_pre(line, "profile balanced") ? 1 : 2;
	else if (str_pre(line, "rxpreset stock") || str_pre(line, "rxpreset enhanced") ||
	         str_pre(line, "rxpreset maximum"))
	                                          g_settings.rxring        =
	             str_pre(line, "rxpreset maximum") ? 2 :
	             str_pre(line, "rxpreset enhanced") ? 1 : 0;
	else return 0;
	return memcmp(&b, &g_settings, sizeof(b)) != 0;
}

/* The ONE place any command path folds a setting change into the persisted
 * state. Called from wq_dispatch_exec, so the Settings UI, the :1339 surface and
 * any internal caller all update + persist + publish identically - single source
 * of truth. */
void wq_settings_note(const char *line)
{
	if (!g_settings_ready) { wq_persist_load(&g_settings); g_settings_ready = 1; }
	if (settings_track(line)) {
		wq_persist_save(&g_settings);
		wqUiPublish(&g_settings);
		WQ_LOGI("settings: state updated+saved via '%s'", line);
	}
}

/* Boot: load persisted settings and re-apply them, then publish. Called from
 * the resident worker once the subsystems are up. */
void wq_settings_boot(void)
{
	char rep[128];
	wq_persist_load(&g_settings);
	g_settings_ready = 1;
	wq_dispatch_exec(g_settings.keepwifi ? "keepwifi on" : "keepwifi off", rep, sizeof(rep));
	wqHudSetPos(g_settings.hudpos);   /* set the corner before the overlay arms  */
	wq_dispatch_exec(g_settings.hud ? "hud on" : "hud off", rep, sizeof(rep));
	wq_dispatch_exec(g_settings.autoreconnect ? "autoreconnect on" : "autoreconnect off", rep, sizeof(rep));
	wq_dispatch_exec(g_settings.roam ? "roam on" : "roam off", rep, sizeof(rep));
	wq_dispatch_exec(g_settings.devmode ? "devmode on" : "devmode off", rep, sizeof(rep));
	/* Profile + 40 MHz: driver patches and the PS flag, applied before the
	 * first join (or with one reassociation if already connected). */
	wq_power_boot_restore(g_settings.profile, g_settings.ht40);
	/* Chip RX buffer: the chip booted on the stock image; re-apply the saved
	 * preset with one radio power-cycle once Wi-Fi is up (user/rxpreset.c). */
	rxpreset_boot(g_settings.rxring);
	wqUiPublish(&g_settings);
	WQ_LOGI("persist: applied saved settings at boot");
}

/* Resident-side pump: called periodically from the SceShell process (off the
 * keepwifi tick). Drains any command the Settings app queued via the kernel
 * mailbox and runs it through the shared core, then publishes the current state
 * so the Settings toggles reflect reality. */
void wq_settings_pump(void)
{
	if (!g_settings_ready) { wq_persist_load(&g_settings); g_settings_ready = 1; }

	char line[WQ_UI_CMD_MAX], rep[256];
	while (wqUiDrain(line, sizeof(line))) {         /* drain the WHOLE ring       */
		line[WQ_UI_CMD_MAX - 1] = 0;
		WQ_LOGI("settings: mailbox cmd '%s'", line);
		/* exec folds the change into g_settings + persists via wq_settings_note,
		 * the same path the :1339 surface uses - one source of truth. */
		wq_dispatch_exec(line, rep, sizeof(rep));
		/* A profile or 40 MHz change made in Settings is kept as-is. The user
		 * undoes it right there (untick / pick another profile), which works
		 * with the link down, and boot re-applies the saved choice. The 45 s
		 * auto-revert is only for remote :1339 changes, where a change that
		 * broke the link would leave no way back - and nobody in the Settings
		 * UI could ever send 'confirm'. */
		if (str_pre(line, "profile ") || str_pre(line, "ht40 ")) {
			int tok = wq_safety_pending();
			if (tok >= 0) {
				wq_safety_confirm(tok);
				WQ_LOGI("settings: '%s' kept (no auto-revert from Settings)", line);
			}
		}
	}
	wqUiPublish(&g_settings);                      /* periodic reflect           */
}
