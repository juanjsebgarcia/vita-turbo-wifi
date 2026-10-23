/*
 * keepwifi.h - "Keep Wi-Fi on": hold off the Vita's auto-standby.
 *
 * Auto-standby suspends the console when idle, which drops Wi-Fi (and means the
 * link is gone until the user wakes and reconnects it). We tick the
 * auto-suspend timer once a second so the console stays awake and Wi-Fi stays
 * up; the screen still dims/turns off. While on, the power button only turns
 * the screen off rather than sleeping the console.
 *
 * Default ON. Toggle at runtime with the 'keepwifi on|off' command. This is the
 * opposite of a lockout (it keeps the control link alive), so it is always safe.
 */
#ifndef WIFITURBO_KEEPWIFI_H
#define WIFITURBO_KEEPWIFI_H

int  keepwifi_start(void);        /* spawn the tick thread (idempotent)         */
void keepwifi_stop(void);         /* stop the tick thread                       */
void keepwifi_set_enabled(int on);
int  keepwifi_is_enabled(void);

/* "Auto-reconnect on drop": run the reconnect watcher (default ON). */
void keepwifi_set_autoreconnect(int on);

/* Smart Mesh Roaming: on sustained weak signal, arm the
 * kernel join-path assist and reassociate, so the join lands on the strongest
 * AP of the same SSID. Runs on the keepwifi worker; independent of the keepwifi
 * standby-hold toggle. Default OFF. */
void keepwifi_set_roam(int on);
int  keepwifi_roam_enabled(void);
void keepwifi_roam_now(void);            /* one forced roam on the worker      */
void keepwifi_set_roam_precheck(int on); /* libnetctl scan veto/target (def 1) */
int  keepwifi_roam_precheck(void);
int  keepwifi_roam_busy(void);           /* 1 while a roam is queued/in flight */
const char *keepwifi_roam_last(void);    /* one-line result of the last roam   */

/* Self-contained test: netdown, then IcmConnect alone; logs PASS/FAIL to the
 * log file and falls back to the proven rejoin on FAIL. */
void keepwifi_icm_test(void);

#endif /* WIFITURBO_KEEPWIFI_H */
