/*
 * settings_ui.h - Settings-app (NPXS10015) hooks, compiled into the one
 * wifiturbo.suprx. The main module_start calls install() only when it detects it
 * is running inside the Settings process (the suprx is listed under both *main
 * and *NPXS10015 in config.txt).
 */
#ifndef WIFITURBO_SETTINGS_UI_H
#define WIFITURBO_SETTINGS_UI_H

void wq_settings_hooks_install(void);
void wq_settings_hooks_release(void);

#endif /* WIFITURBO_SETTINGS_UI_H */
