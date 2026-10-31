/*
 * settings_entry.h - the resident (SceShell) side of the Settings page: the
 * mailbox pump, boot restore and the persisted-state fold (see
 * settings_entry.c).
 */
#ifndef WIFITURBO_SETTINGS_ENTRY_H
#define WIFITURBO_SETTINGS_ENTRY_H

/* Resident-side pump (call periodically from SceShell): drains the Settings
 * app's command mailbox, applies + persists changes, and publishes state. */
void wq_settings_pump(void);

/* Boot: load persisted settings and re-apply them (call once from the resident
 * worker after subsystems are up). */
void wq_settings_boot(void);

/* Fold a setting-changing command into the persisted state (save + publish).
 * Called from wq_dispatch_exec so EVERY command surface (Settings UI, :1339,
 * internal) updates the one source of truth. No-op for non-setting commands. */
void wq_settings_note(const char *line);

/* user/macaddr.c only: fold its "mac state ..." / "mac factory ..." decision
 * lines. wq_settings_note ignores those, so no command surface (:1339) can
 * write the saved address directly. */
void wq_settings_note_mac(const char *line);

#endif /* WIFITURBO_SETTINGS_ENTRY_H */
