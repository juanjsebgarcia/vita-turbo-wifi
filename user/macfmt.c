/*
 * macfmt.c - MAC address helpers shared by wifiturbo.suprx (user/macaddr.c)
 * and the Settings page module (settings_app/wifiturbo_settings.c): parse a
 * typed address, and make a random one. Declared in macaddr.h.
 */
#include <psp2/kernel/processmgr.h>   /* sceKernelGetProcessTimeWide */
#include <psp2/kernel/rng.h>          /* sceKernelGetRandomNumber     */
#include <string.h>

#include "macaddr.h"

static int hexval(char c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

int wq_mac_parse(const char *s, uint8_t mac[6], int *why)
{
	int n = 0;
	uint8_t b[6];
	*why = 1;
	for (; *s && *s != ' '; s++) {
		if (*s == ':' || *s == '-' || *s == '.')
			continue;
		int v = hexval(*s);
		if (v < 0 || n >= 12)
			return -1;
		if (n & 1) b[n / 2] = (uint8_t)(b[n / 2] << 4 | v);
		else       b[n / 2] = (uint8_t)v;
		n++;
	}
	if (n != 12)
		return -1;
	if (b[0] & 1) { *why = 2; return -1; }
	int zero = 1, ff = 1;
	for (int i = 0; i < 6; i++) {
		if (b[i])         zero = 0;
		if (b[i] != 0xFF) ff = 0;
	}
	if (zero || ff) { *why = 3; return -1; }
	memcpy(mac, b, 6);
	*why = 0;
	return 0;
}

/* The console's first three bytes + three random ones, never its own address.
 * Also used by the Settings page, so it knows the new address at once. */
void wq_mac_random(uint8_t m[6], const uint8_t factory[6])
{
	memcpy(m, factory, 3);
	do {
		uint32_t x = 0;
		if (sceKernelGetRandomNumber(&x, sizeof(x)) < 0) {   /* clock fallback */
			x = (uint32_t)sceKernelGetProcessTimeWide() * 2654435761u;
			x ^= x >> 15; x *= 0x2C1B3C6Du; x ^= x >> 12;
		}
		m[3] = (uint8_t)x; m[4] = (uint8_t)(x >> 8); m[5] = (uint8_t)(x >> 16);
	} while (!memcmp(m, factory, 6));
}
