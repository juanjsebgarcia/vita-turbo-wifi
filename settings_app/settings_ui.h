/*
 * settings_ui.h - Settings-app (NPXS10015) hooks, in their own module,
 * wifiturbo_settings.suprx (settings_main.c installs them at module_start).
 */
#ifndef WIFITURBO_SETTINGS_UI_H
#define WIFITURBO_SETTINGS_UI_H

void wq_settings_hooks_install(void);
void wq_settings_hooks_release(void);

#endif /* WIFITURBO_SETTINGS_UI_H */
