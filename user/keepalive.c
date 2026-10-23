/*
 * keepalive.c - send a tiny UDP frame to the default gateway periodically so
 * the access point never ages the (idle) Vita out of the BSS.
 *
 * ROOT CAUSE: many access points age out idle stations. An idle
 * Vita drops off Wi-Fi every few minutes even at excellent signal with no TX
 * retries, always the same AP - i.e. not range,
 * not interference, not roaming. The trigger is going IDLE: with little/no
 * traffic the AP's inactivity timeout deauthenticates the station. A single
 * over-the-air frame every KEEPALIVE_PERIOD_S resets that timer, so an
 * otherwise-idle session is never considered idle. This is the real preventive
 * fix - user-mode networking only, no chip/driver hooks, so it carries none of
 * that risk.
 *
 * The gateway is resolved DYNAMICALLY from SceNetCtl
 * (SCE_NETCTL_INFO_GET_DEFAULT_ROUTE) every send - never hardcoded - so it
 * follows DHCP and any network change. When disconnected there is no default
 * route, so gateway_be() returns 0 and we simply skip (self-gating).
 */
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <string.h>

#include "keepalive.h"
#include "log.h"
#include "../include/wifiturbo.h"

#define KEEPALIVE_PERIOD_S  45   /* comfortably under a typical ~5 min idle cap */
#define KEEPALIVE_PORT      9    /* discard port - we only need to TX one frame */

static int          g_sec_count;
static unsigned     g_last_gw;   /* for a one-line log when the gateway changes */

/* Current default gateway as a network-order IPv4, or 0 if none (disconnected).
 * Dotted-quad parse mirrors dispatch.c's (no sceNetInetPton dependency). */
static unsigned gateway_be(void)
{
	SceNetCtlInfo info;
	memset(&info, 0, sizeof(info));
	if (sceNetCtlInetGetInfo(SCE_NETCTL_INFO_GET_DEFAULT_ROUTE, &info) < 0)
		return 0;
	if (!info.default_route[0])
		return 0;
	unsigned oc[4] = {0, 0, 0, 0};
	const char *q = info.default_route;
	for (int k = 0; k < 4; k++) {
		while (*q >= '0' && *q <= '9') { oc[k] = oc[k] * 10 + (unsigned)(*q - '0'); q++; }
		if (*q == '.') q++;
	}
	return sceNetHtonl((oc[0] << 24) | (oc[1] << 16) | (oc[2] << 8) | oc[3]);
}

static void send_one(void)
{
	unsigned gw = gateway_be();
	if (!gw)
		return;                           /* disconnected - nothing to do       */
	if (gw != g_last_gw) {
		g_last_gw = gw;
		WQ_LOGI("keepalive: heartbeat -> gateway 0x%08x :%d every %ds",
		        sceNetNtohl(gw), KEEPALIVE_PORT, KEEPALIVE_PERIOD_S);
	}
	int fd = sceNetSocket("wqkeepalive", SCE_NET_AF_INET, SCE_NET_SOCK_DGRAM, 0);
	if (fd < 0)
		return;
	SceNetSockaddrIn sa;
	memset(&sa, 0, sizeof(sa));
	sa.sin_family      = SCE_NET_AF_INET;
	sa.sin_port        = sceNetHtons(KEEPALIVE_PORT);
	sa.sin_addr.s_addr = gw;
	unsigned char b = 0;
	sceNetSendto(fd, &b, 1, 0, (SceNetSockaddr *)&sa, sizeof(sa));
	sceNetSocketClose(fd);
}

void keepalive_tick(void)
{
	if (++g_sec_count < KEEPALIVE_PERIOD_S)
		return;
	g_sec_count = 0;
	send_one();
}
