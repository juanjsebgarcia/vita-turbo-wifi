/*
 * advanced.c - the libnetctl scan reader and the read-only `adv scan` command.
 *
 * See advanced.h. Field offsets come from include/wifiturbo.h.
 */
#include <psp2/net/netctl.h>
#include <psp2/kernel/threadmgr.h>
#include <string.h>
#include <taihen.h>

#include "advanced.h"
#include "log.h"

/* ---- tiny string helpers (no printf/sscanf dependency) ------------------ */

static int a_eq(const char *a, const char *b)
{
	while (*a && *b) { if (*a++ != *b++) return 0; }
	return *a == *b;
}
static char *a_put(char *p, char *end, const char *s)
{
	while (*s && p < end - 1) *p++ = *s++;
	*p = 0;
	return p;
}
static char *a_puti(char *p, char *end, int v)
{
	char t[12]; int n = 0, neg = v < 0; unsigned u = neg ? -(unsigned)v : (unsigned)v;
	if (!u) t[n++] = '0';
	while (u) { t[n++] = '0' + u % 10; u /= 10; }
	if (neg && p < end - 1) *p++ = '-';
	while (n && p < end - 1) *p++ = t[--n];
	*p = 0;
	return p;
}
static char *a_puthex(char *p, char *end, unsigned v, int digits)
{
	const char *h = "0123456789ABCDEF";
	for (int i = (digits - 1) * 4; i >= 0; i -= 4)
		if (p < end - 1) *p++ = h[(v >> i) & 0xF];
	*p = 0;
	return p;
}

/* ========================================================================= */
/* libnetctl scan verbs (resolved by NID)                                    */
/* ========================================================================= */

/*
 * The libnetctl IpcInt scan functions are exported by NID from the live
 * SceLibNetCtl module. We resolve them with taiHEN; if any fails to resolve the
 * reader reports "n/a" (never guessing an address).
 */
#define WQ_NETCTL_MODNAME   "SceLibNetCtl"  /* module name (libnetctl.suprx module info) */
#define WQ_NETCTL_LIB_NID   0xC6083AE1u   /* SceNetCtl lib NID, read from libnetctl.suprx exports */

typedef int (*fn_scan_req)(void *ssids, unsigned count, void *cfg);
typedef int (*fn_scan_count)(void);
typedef int (*fn_scan_rec)(int idx, void *rec);
typedef int (*fn_getresult)(int event, int *err);
typedef int (*fn_arm_event)(int type);

static struct {
	int resolved;
	fn_scan_req     scan_req;
	fn_scan_count   scan_count;
	fn_scan_rec     scan_rec;
	fn_getresult    getresult;
	fn_arm_event    arm_event;
} g_nc;

static int resolve_one(uint32_t nid, void *out)
{
	uintptr_t fp = 0;
	int r = taiGetModuleExportFunc(WQ_NETCTL_MODNAME, WQ_NETCTL_LIB_NID, nid, &fp);
	if (r < 0 || !fp)
		return WQ_ERR_RESOLVE;
	*(uintptr_t *)out = fp;
	return WQ_OK;
}

int wq_adv_init(void)
{
	memset(&g_nc, 0, sizeof(g_nc));
	/* All-or-nothing: if any verb fails to resolve, leave g_nc.resolved = 0 so
	 * the scan fails safe rather than half-wire. */
	if (resolve_one(WQ_NETCTL_NID_SCAN_REQ,     &g_nc.scan_req)     != WQ_OK ||
	    resolve_one(WQ_NETCTL_NID_SCAN_COUNT,   &g_nc.scan_count)   != WQ_OK ||
	    resolve_one(WQ_NETCTL_NID_SCAN_REC,     &g_nc.scan_rec)     != WQ_OK ||
	    resolve_one(WQ_NETCTL_NID_GETRESULT,    &g_nc.getresult)    != WQ_OK ||
	    resolve_one(WQ_NETCTL_NID_ARM_EVENT,    &g_nc.arm_event)    != WQ_OK) {
		memset(&g_nc, 0, sizeof(g_nc));
		WQ_LOGW("adv: libnetctl scan verbs NOT resolved "
		        "(confirm SceNetCtl lib NID) - scan reports n/a");
		return WQ_ERR_RESOLVE;
	}
	g_nc.resolved = 1;
	WQ_LOGI("adv: libnetctl scan verbs resolved");
	return WQ_OK;
}

void wq_adv_fini(void)
{
	memset(&g_nc, 0, sizeof(g_nc));
}

/* Trigger a scan (IPMI 0x20010) and wait for SCAN_REQ_FINISHED (event 4),
 * retrying on the shared busy latch 0x80412187. */
static int do_scan_request(void)
{
	for (int attempt = 0; attempt < 10; attempt++) {
		g_nc.arm_event(WQ_NETCTL_EVENT_SCAN_DONE);     /* arm event 4 first */
		int r = g_nc.scan_req(NULL, 0, NULL);          /* broad scan, 0 SSIDs */
		if (r == 0)
			break;
		if ((unsigned)r == WQ_SHELL_ERR_BUSY) {        /* retry on busy */
			sceKernelDelayThread(200 * 1000);
			continue;
		}
		return WQ_ERR_CHIP;
	}
	/* Pump callbacks and poll for results (up to ~4 s). The count goes
	 * non-zero at once with the PREVIOUS scan's results; the new ones land
	 * ~1.2 s in. Wait out the sweep, then until the count holds steady. */
	int last = -1, steady = 0;
	for (int i = 0; i < 40; i++) {
		sceNetCtlCheckCallback();
		int err = 0;
		g_nc.getresult(WQ_NETCTL_EVENT_SCAN_DONE, &err);
		int count = g_nc.scan_count();
		steady = (count == last) ? steady + 1 : 0;
		last = count;
		if (count > 0 && i >= 18 && steady >= 3) {
			WQ_LOGD("adv: scan complete, %d APs", count);
			return count;
		}
		sceKernelDelayThread(100 * 1000);
	}
	if (last > 0)
		return last;
	WQ_LOGD("adv: scan ran but reported 0 APs");
	return 0;   /* scan ran but reported no APs */
}

static const char *sec_name(int sec)
{
	/* The Settings app's own security-type mapping. */
	switch (sec) {
	case 0: return "open";
	case 1: return "WEP";
	case 3: case 4: case 5: case 6: return "WPA";
	case 7: return "open";
	default: return "?";
	}
}

/* The record holds RSSI as a magnitude (84 = -84 dBm), like the driver's own
 * scan entries; normalise to signed dBm so comparisons pick the strongest. */
static int scanrec_dbm(const uint8_t *rec)
{
	int v = (int8_t)rec[WQ_SCANREC_OFF_RSSIDBM];
	return v > 0 ? -v : v;
}

static int adv_scan(char *p, char *end)
{
	if (!g_nc.resolved) {
		a_put(p, end, "err: scan verbs not resolved on this build "
		              "(confirm SceNetCtl lib NID)");
		return WQ_ERR_RESOLVE;
	}
	int count = do_scan_request();
	if (count <= 0) { a_put(p, end, count == 0 ? "scan: 0 APs" : "err: scan failed");
		return count == 0 ? WQ_OK : WQ_ERR_CHIP; }

	p = a_put(p, end, "scan ");
	p = a_puti(p, end, count);
	p = a_put(p, end, " APs:");
	int shown = count > 8 ? 8 : count;   /* keep the reply inside its buffer */
	for (int i = 0; i < shown; i++) {
		uint8_t rec[WQ_SCANREC_SIZE];
		memset(rec, 0, sizeof(rec));
		if (g_nc.scan_rec(i, rec) != 0)
			continue;
		/* SSID is NUL-padded 0x21 bytes at +0x18. */
		char ssid[0x22];
		memcpy(ssid, rec + WQ_SCANREC_OFF_SSID, 0x21);
		ssid[0x21] = 0;
		int sec = (int)rec[WQ_SCANREC_OFF_SECURITY];
		int rssi = scanrec_dbm(rec);
		int ch = (int)rec[WQ_SCANREC_OFF_CHANNEL];
		const uint8_t *b = rec + WQ_SCANREC_OFF_BSSID;

		p = a_put(p, end, "\n  ");
		p = a_put(p, end, ssid[0] ? ssid : "<hidden>");
		p = a_put(p, end, " ch");
		p = a_puti(p, end, ch);
		p = a_put(p, end, " ");
		p = a_puti(p, end, rssi);
		p = a_put(p, end, "dBm ");
		p = a_put(p, end, sec_name(sec));
		p = a_put(p, end, " ");
		for (int k = 0; k < 6; k++) {
			p = a_puthex(p, end, b[k], 2);
			if (k < 5 && p < end - 1) *p++ = ':';
		}
		*p = 0;
	}
	return WQ_OK;
}

/* ---- reusable scan reader (smart-roam pre-check, selftest) --------------- */
/* The SAME resolved verbs + do_scan_request() `adv scan` uses, parsed into
 * WqScanEntry records instead of text. */
int wq_adv_scan_available(void)
{
	return g_nc.resolved ? 1 : 0;
}

int wq_adv_scan_read(WqScanEntry *out, int max)
{
	if (!out || max <= 0)
		return WQ_ERR_ARG;
	if (!g_nc.resolved)
		return WQ_ERR_RESOLVE;   /* verbs not confirmed -> caller fails safe   */

	int count = do_scan_request();
	if (count < 0)
		return WQ_ERR_CHIP;
	if (count == 0)
		return 0;

	int want = count < max ? count : max;
	int got = 0;
	for (int i = 0; i < want; i++) {
		uint8_t rec[WQ_SCANREC_SIZE];
		memset(rec, 0, sizeof(rec));
		if (g_nc.scan_rec(i, rec) != 0)
			continue;
		WqScanEntry *e = &out[got];
		memset(e, 0, sizeof(*e));
		memcpy(e->bssid, rec + WQ_SCANREC_OFF_BSSID, 6);
		/* SSID is NUL-padded 0x21 bytes at +0x18. */
		memcpy(e->ssid, rec + WQ_SCANREC_OFF_SSID, WQ_SSID_MAX);
		e->ssid[WQ_SSID_MAX] = 0;
		e->security = (int32_t)rec[WQ_SCANREC_OFF_SECURITY];
		e->rssi_dbm = scanrec_dbm(rec);
		e->channel  = (int32_t)rec[WQ_SCANREC_OFF_CHANNEL];
		got++;
	}
	return got;
}

/* ========================================================================= */
/* Command dispatch                                                          */
/* ========================================================================= */

int wq_adv_exec(const char *args, char *out, int outcap)
{
	char *p = out, *end = out + outcap;
	out[0] = 0;

	if (!args || !args[0] || a_eq(args, "help")) {
		a_put(p, end, "adv: scan");
		return WQ_OK;
	}

	if (a_eq(args, "scan"))
		return adv_scan(p, end);

	a_put(p, end, "err: unknown adv cmd (try 'adv help')");
	return WQ_ERR_ARG;
}
