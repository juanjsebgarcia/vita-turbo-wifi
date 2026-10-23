#ifndef WQ_NETJOIN_H
#define WQ_NETJOIN_H

/*
 * Command SceShell's netctl daemon directly via its own event-flag signal (see
 * netjoin.c). wq_net_rejoin() = the "connect" command the shell's Wi-Fi toggle
 * sends; the intended fix for both a mid-session drop and a boot auto-join miss.
 */
int wq_net_rejoin(void);      /* signal the daemon to (re)connect to saved AP   */
int wq_net_icm_connect(void); /* IcmConnect: full saved-profile auto-connect,
                               * works after a boot give-up (no staged target) */
int wq_net_disconnect(void);  /* real Inet disconnect (libnetctl 0xDD96DA85)    */
int wq_net_reassociate(void); /* disconnect + IcmConnect: a fresh association  */

#endif /* WQ_NETJOIN_H */
