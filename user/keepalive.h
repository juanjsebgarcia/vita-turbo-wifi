#ifndef WQ_KEEPALIVE_H
#define WQ_KEEPALIVE_H

/*
 * Periodic gateway keepalive so an idle Vita is never aged out of the BSS.
 * Driven off the keepwifi 1 Hz worker (only while keepwifi is ON). See
 * keepalive.c for the why (confirmed AP idle-timeout drop).
 */
void keepalive_tick(void);   /* call once per second from the keepwifi worker */

/* The default gateway, network order, or 0 when there is none. */
unsigned wq_gateway_be(void);

#endif /* WQ_KEEPALIVE_H */
