/*
 * dispatch.c - the command core + a vitacompanion-style TCP listener (:1339).
 *
 * Commands (newline-terminated on the socket):
 *   help
 *   status                      - network, state, signal, SNR/NF, channel
 *   hud on|off                  - toggle the overlay
 *   profile maxperf|balanced|maxbatt   - power/latency profile
 *   ht40 on|off                 - 40 MHz on 2.4 GHz
 *   confirm                     - confirm the pending change (cancel revert)
 *   revert                      - revert the pending change now
 *   autoreconnect on|off        - auto-reconnect on drop (keepwifi.c watcher)
 *   keepwifi [on|off]           - hold off auto-standby + idle keep-alive
 *   roam on|off                 - Smart Mesh Roaming (keepwifi.c): on sustained
 *                                 weak signal, kernel join assist + reassociate
 *                                 to the strongest AP of the same SSID
 *   roam now                    - one forced roam (testing; drops the link ~5s)
 *   roam precheck on|off        - libnetctl scan veto/target before a roam
 *   roam status                 - smart-roam state, last result, kernel assist
 *   rejoin | icm | icmtest | netdown   - reconnect dev tools (netjoin.c)
 *   selftest                    - read-only subsystem probe
 *   speedtest <ip> <port> [secs] [rcvbuf_kb] - TCP download into RAM
 *   udpsink <port> [secs]       - count a UDP stream (link capacity + loss)
 *   log [dump] | log level <lvl> | log clear
 *   adv scan                    - read-only libnetctl scan (advanced.c)
 *
 * Profile and ht40 changes route through the safety harness; the reply tells
 * the user a revert is armed and how to confirm it.
 */
#include <psp2/net/net.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>   /* sceKernelGetProcessTimeWide (speedtest) */
#include <psp2/kernel/clib.h>
#include <string.h>

#include "dispatch.h"
#include "diag.h"
#include "safety.h"
#include "powerprofile.h"
#include "advanced.h"
#include "keepwifi.h"
#include "netjoin.h"
#include "settings_entry.h"
#include "log.h"
#include "../include/wifiturbo.h"
#include "../include/wifiturbo_kernel_api.h"

/* ---- small string helpers (no sscanf/printf dependency) ----------------- */

static int str_eq(const char *a, const char *b)
{
	while (*a && *b) { if (*a++ != *b++) return 0; }
	return *a == *b;
}
/* match the first token of `s` against `tok`; return pointer past it + spaces */
static const char *tok_after(const char *s, const char *tok)
{
	while (*s == ' ') s++;
	const char *p = s;
	while (*tok) { if (*p++ != *tok++) return NULL; }
	if (*p && *p != ' ') return NULL;
	while (*p == ' ') p++;
	return p;
}
static char *put(char *p, char *end, const char *s)
{
	while (*s && p < end - 1) *p++ = *s++;
	*p = 0;
	return p;
}
static char *put_i(char *p, char *end, int v)
{
	char t[12]; int n = 0, neg = v < 0; unsigned u = neg ? -(unsigned)v : (unsigned)v;
	if (!u) t[n++] = '0';
	while (u) { t[n++] = '0' + u % 10; u /= 10; }
	if (neg && p < end - 1) *p++ = '-';
	while (n && p < end - 1) *p++ = t[--n];
	*p = 0;
	return p;
}
static char *put_hex(char *p, char *end, unsigned v)
{
	const char *h = "0123456789abcdef";
	for (int sh = 28; sh >= 0; sh -= 4)
		if (p < end - 1) *p++ = h[(v >> sh) & 0xf];
	*p = 0;
	return p;
}
static int parse_int(const char **sp)
{
	const char *s = *sp;
	while (*s == ' ') s++;
	int neg = 0;
	if (*s == '-') { neg = 1; s++; }
	int v = 0, any = 0;
	while (*s >= '0' && *s <= '9') { v = v * 10 + (*s++ - '0'); any = 1; }
	while (*s == ' ') s++;
	*sp = s;
	return any ? (neg ? -v : v) : 0;
}

/* ---- read-only self-test ------------------------------------------------ */

/* Probe each subsystem and report OK/FAIL, logging the detail. Changes nothing
 * (no writes, no association), so it is always safe and is the first thing to
 * run on-device to see what works. */
static int do_selftest(char *out, int outcap)
{
	char *p = out, *end = out + outcap;
	out[0] = 0;
	int fails = 0;
	WQ_LOGI("selftest: begin (read-only probe)");

	p = put(p, end, "selftest (read-only):");

	/* 1. kernel module reachable via a trivial, side-effect-free syscall. */
	int ping = wqSelfTestPing();
	int kok = (ping == (int)WQ_SELFTEST_MAGIC);
	p = put(p, end, "\n  kernel    ");
	p = put(p, end, kok ? "OK" : "FAIL");
	WQ_LOGI("selftest: kernel %s (ping rc=%d expect=0x%08x)",
	        kok ? "OK" : "FAIL", ping, (unsigned)WQ_SELFTEST_MAGIC);
	if (!kok) fails++;

	/* 2. netctl readable - state read. */
	int st = -1;
	int rnet = diag_selftest(&st);
	p = put(p, end, "\n  netctl    ");
	if (rnet == WQ_OK) {
		p = put(p, end, "OK (state=");
		p = put_i(p, end, st);
		p = put(p, end, ")");
	} else {
		p = put(p, end, "FAIL");
		fails++;
	}
	WQ_LOGI("selftest: netctl rc=%d state=%d", rnet, st);

	/* 3. scan verbs resolved (smart-roam pre-check / adv scan). */
	int scan = wq_adv_scan_available();
	p = put(p, end, "\n  scan      ");
	p = put(p, end, scan ? "OK" : "FAIL (verbs n/a)");
	WQ_LOGI("selftest: scan verbs %s", scan ? "resolved" : "n/a");
	if (!scan) fails++;

	/* 4. HUD hook installed. */
	int hud = wqHudGetStatus();
	int hook = (hud & WQ_HUD_STATUS_HOOK) != 0;
	p = put(p, end, "\n  hud       ");
	if (hook)
		p = put(p, end, (hud & WQ_HUD_STATUS_ENABLED)
		                ? "OK (hook, enabled)" : "OK (hook, off)");
	else {
		p = put(p, end, "FAIL (no hook)");
		fails++;
	}
	WQ_LOGI("selftest: hud status=0x%x", (unsigned)hud);

	p = put(p, end, "\n  => ");
	if (fails == 0) {
		p = put(p, end, "all subsystems OK");
	} else {
		p = put_i(p, end, fails);
		p = put(p, end, " subsystem(s) unavailable");
	}
	WQ_LOGI("selftest: done, %d subsystem(s) unavailable", fails);
	return WQ_OK;   /* the probe itself always succeeds */
}

/* ---- the command core --------------------------------------------------- */

/* BSD socket-option values used by the Vita stack (no SDK names for these). */
#define WQ_SOL_SOCKET   0xffff
#define WQ_SO_RCVBUF    0x1002
#define WQ_SO_RCVTIMEO  0x1006     /* int, microseconds */

static void dispatch_set_devmode(int on);   /* Developer mode, see below */

static int dispatch_exec_inner(const char *line, char *out, int outcap)
{
	char *p = out, *end = out + outcap;
	out[0] = 0;
	const char *a;

	if (!line || !line[0] || str_eq(line, "help")) {
		put(p, end,
		    "cmds: status|hud on/off|profile maxperf/balanced/maxbatt|"
		    "ht40 on/off|confirm|revert|autoreconnect on/off|keepwifi on/off|"
		    "roam on/off|roam now|roam precheck on/off|roam status|"
		    "rejoin|icm|icmtest|netdown|"
		    "selftest|speedtest <ip> <port> [secs] [rcvbuf_kb]|udpsink <port> [secs]|"
		    "log [dump]|log level <err|warn|info|debug|trace>|log clear|adv scan");
		return WQ_OK;
	}

	if (str_eq(line, "status")) {
		WqHudState s;
		diag_sample(&s);
		p = put(p, end, "ssid=");
		p = put(p, end, s.ssid[0] ? s.ssid : "-");
		p = put(p, end, " state=");
		p = put_i(p, end, s.netctl_state);
		p = put(p, end, " rssi=");
		p = put_i(p, end, s.rssi_dbm);
		p = put(p, end, "dBm(");
		p = put_i(p, end, (int)s.rssi_pct);
		p = put(p, end, "%)");
		if (s.snr_valid) {
			p = put(p, end, " snr=");
			p = put_i(p, end, s.snr_db);
			p = put(p, end, " nf=");
			p = put_i(p, end, s.nf_dbm);
		}
		p = put(p, end, " ch=");
		p = put_i(p, end, (int)s.channel);
		char st[48];
		if (wq_safety_status(st, sizeof(st))) {
			p = put(p, end, " [");
			p = put(p, end, st);
			p = put(p, end, "]");
		}
		return WQ_OK;
	}

	if ((a = tok_after(line, "hud"))) {
		/* 'hud on|off' - the overlay: sampler thread + framebuffer draw. */
		int on = str_eq(a, "on");
		wq_hud_runtime_set(on);
		put(p, end, on ? "hud on - overlay drawing, sampler running" : "hud off");
		return WQ_OK;
	}

	if ((a = tok_after(line, "profile"))) {
		int prof = -1;
		if (str_eq(a, "maxperf"))  prof = WQ_PROFILE_MAX_PERF;
		else if (str_eq(a, "balanced")) prof = WQ_PROFILE_BALANCED;
		else if (str_eq(a, "maxbatt"))  prof = WQ_PROFILE_MAX_BATTERY;
		if (prof < 0) { put(p, end, "err: profile ?"); return WQ_ERR_ARG; }
		/* Sets the driver PS flag (+ the maxbatt DTIM patch) and reassociates
		 * (~5s). Unconfirmed within 45s => previous state + reassociate. */
		int tok = wq_power_set_profile(prof, 45000);
		if (tok < 0) { put(p, end, "err: profile apply failed (reverted)"); return tok; }
		p = put(p, end, "profile ");
		p = put(p, end, wq_power_profile_name(prof));
		p = put(p, end, " - reassociating now; 'confirm' within 45s once back, else it reverts (token ");
		p = put_i(p, end, tok);
		p = put(p, end, ")");
		return WQ_OK;
	}

	if ((a = tok_after(line, "ht40"))) {
		int on = str_eq(a, "on");
		/* 45s: the apply reassociates (~8s) and the link must come back before
		 * 'confirm' can even reach us. Unconfirmed => revert + reassociate. */
		int tok = wq_power_set_ht40(on, 45000);
		if (tok < 0) { put(p, end, "err: ht40 apply failed"); return tok; }
		p = put(p, end, "ht40 ");
		p = put(p, end, on ? "on" : "off");
		p = put(p, end, " - reassociating now; 'confirm' within 45s once back, else it reverts (token ");
		p = put_i(p, end, tok);
		p = put(p, end, ")");
		return WQ_OK;
	}

	if ((a = tok_after(line, "adv")))
		return wq_adv_exec(a, out, outcap);

	if (str_eq(line, "confirm")) {
		int tok = wq_safety_pending();
		if (tok < 0) { put(p, end, "nothing pending"); return WQ_OK; }
		wq_safety_confirm(tok);
		put(p, end, "confirmed - change kept");
		return WQ_OK;
	}

	if (str_eq(line, "revert")) {
		int tok = wq_safety_pending();
		if (tok < 0) { put(p, end, "nothing pending"); return WQ_OK; }
		wq_safety_revert_now(tok);
		put(p, end, "reverted");
		return WQ_OK;
	}

	/* Developer mode: open/close the :1339 remote-control listener. */
	if ((a = tok_after(line, "devmode"))) {
		int on = str_eq(a, "on");
		dispatch_set_devmode(on);
		put(p, end, on ? "devmode on - :1339 listening" : "devmode off - :1339 closed");
		return WQ_OK;
	}

	if ((a = tok_after(line, "autoreconnect"))) {
		int on = str_eq(a, "on");
		keepwifi_set_autoreconnect(on);
		put(p, end, on ? "autoreconnect on" : "autoreconnect off");
		return WQ_OK;
	}

	if ((a = tok_after(line, "roam"))) {
		const char *b;
		/* roam now - one forced roam, run on the keepwifi worker (this :1339
		 * session dies with the link; read the log for the result). */
		if (str_eq(a, "now")) {
			if (keepwifi_roam_busy()) { put(p, end, "roam now: a roam is already in flight"); return WQ_ERR_BUSY; }
			keepwifi_roam_now();
			put(p, end, "roam now: queued - link drops ~5s; 'roam status' / log for the BSSID change");
			return WQ_OK;
		}
		if ((b = tok_after(a, "precheck"))) {
			if (str_eq(b, "on") || str_eq(b, "off"))
				keepwifi_set_roam_precheck(str_eq(b, "on"));
			put(p, end, keepwifi_roam_precheck() ? "roam precheck on" : "roam precheck off");
			return WQ_OK;
		}
		if (str_eq(a, "status")) {
			WqRoamAssistStatus ks;
			memset(&ks, 0, sizeof(ks));
			int kr = wqRoamAssistGetStatus(&ks);
			p = put(p, end, "mesh roam ");
			p = put(p, end, keepwifi_roam_enabled() ? "ON" : "off");
			p = put(p, end, keepwifi_roam_busy() ? " (in flight)" : "");
			p = put(p, end, " precheck ");
			p = put(p, end, keepwifi_roam_precheck() ? "on" : "off");
			p = put(p, end, "; last: ");
			p = put(p, end, keepwifi_roam_last());
			if (kr == WQ_OK) {
				p = put(p, end, "; kernel hooked=");  p = put_i(p, end, (int)ks.hooked);
				p = put(p, end, " armed=");           p = put_i(p, end, (int)ks.armed);
				p = put(p, end, " joins=");           p = put_i(p, end, (int)ks.seq);
				p = put(p, end, " bss=");             p = put_i(p, end, (int)ks.n_entries);
				p = put(p, end, "/");                 p = put_i(p, end, (int)ks.n_match);
				p = put(p, end, " reordered=");       p = put_i(p, end, (int)ks.reordered);
				p = put(p, end, " ret=0x");           p = put_hex(p, end, (unsigned)ks.join_ret);
			}
			return WQ_OK;
		}
		/* 'roam on|off' is the Settings "Smart Mesh Roaming" row (keepwifi.c smart
		 * roam). This is the single setting the menu persists as
		 * g_settings.roam. */
		if (str_eq(a, "on") || str_eq(a, "off")) {
			int on = str_eq(a, "on");
			keepwifi_set_roam(on);
			put(p, end, on ? "roam on - Smart Mesh Roaming (reconnect to best AP on weak signal)"
			               : "roam off");
			return WQ_OK;
		}
		put(p, end, "err: roam on|off|now|precheck on/off|status");
		return WQ_ERR_ARG;
	}

	/* ---- logging controls ---------------------------------------------- */
	if ((a = tok_after(line, "log"))) {
		const char *b;
		if ((b = tok_after(a, "level"))) {
			int lvl = -1;
			if      (str_eq(b, "err"))   lvl = WQ_LOG_ERR;
			else if (str_eq(b, "warn"))  lvl = WQ_LOG_WARN;
			else if (str_eq(b, "info"))  lvl = WQ_LOG_INFO;
			else if (str_eq(b, "debug")) lvl = WQ_LOG_DEBUG;
			else if (str_eq(b, "trace")) lvl = WQ_LOG_TRACE;
			if (lvl < 0) {
				put(p, end, "err: log level <err|warn|info|debug|trace>");
				return WQ_ERR_ARG;
			}
			wq_log_set_level(lvl);
			p = put(p, end, "log level=");
			put(p, end, wq_log_level_name(lvl));
			return WQ_OK;
		}
		if (str_eq(a, "clear")) {
			wq_log_clear();
			put(p, end, "log cleared");
			return WQ_OK;
		}
		if (!a[0] || str_eq(a, "dump")) {
			/* Dump the in-memory ring buffer straight into the reply (no disk). */
			wq_log_dump(out, outcap);
			return WQ_OK;
		}
		put(p, end, "err: log [dump] | log level <lvl> | log clear");
		return WQ_ERR_ARG;
	}

	/* ---- read-only self-test ------------------------------------------- */
	if (str_eq(line, "selftest"))
		return do_selftest(out, outcap);

	/* speedtest <ip> <port> [secs]: pull a raw byte stream from a server on the
	 * host computer and discard it, measuring pure Wi-Fi downlink throughput - no memory
	 * card, no browser. Pair with a one-liner on the host, e.g.:
	 *   while :; do cat /dev/zero; done | nc -l 9999      (or a tiny socket server)
	 * then: speedtest <host-ip> 9999 5 */
	if ((a = tok_after(line, "speedtest"))) {
		char ip[40]; int i = 0;
		while (a[i] && a[i] != ' ' && i < 39) { ip[i] = a[i]; i++; }
		ip[i] = 0;
		const char *rest = a + i;
		int port = parse_int(&rest);
		int secs = parse_int(&rest);
		int rcvkb = parse_int(&rest);   /* optional SO_RCVBUF in KiB (0 = default) */
		if (!ip[0] || port <= 0) {
			put(p, end, "usage: speedtest <ip> <port> [secs] [rcvbuf_kb]");
			return WQ_ERR_ARG;
		}
		if (secs <= 0) secs = 5;
		/* parse dotted IPv4 -> network order (no sceNetInetPton dependency) */
		unsigned oc[4] = {0,0,0,0}; const char *q = ip;
		for (int k = 0; k < 4; k++) { while (*q>='0'&&*q<='9'){oc[k]=oc[k]*10+(unsigned)(*q-'0');q++;} if(*q=='.')q++; }

		int fd = sceNetSocket("wqspeed", SCE_NET_AF_INET, SCE_NET_SOCK_STREAM, 0);
		if (fd < 0) { put(p, end, "speedtest: socket failed"); return WQ_ERR_CHIP; }
		/* The TCP window = SO_RCVBUF; set before connect so it is advertised. */
		int defbuf = 0; unsigned ol = sizeof(defbuf);
		sceNetGetsockopt(fd, WQ_SOL_SOCKET, WQ_SO_RCVBUF, &defbuf, &ol);
		if (rcvkb > 0) {
			int b = rcvkb * 1024;
			sceNetSetsockopt(fd, WQ_SOL_SOCKET, WQ_SO_RCVBUF, &b, sizeof(b));
		}
		int usebuf = 0; ol = sizeof(usebuf);
		sceNetGetsockopt(fd, WQ_SOL_SOCKET, WQ_SO_RCVBUF, &usebuf, &ol);
		SceNetSockaddrIn sa; memset(&sa, 0, sizeof(sa));
		sa.sin_family = SCE_NET_AF_INET;
		sa.sin_port = sceNetHtons((unsigned short)port);
		sa.sin_addr.s_addr = sceNetHtonl((oc[0]<<24)|(oc[1]<<16)|(oc[2]<<8)|oc[3]);
		if (sceNetConnect(fd, (SceNetSockaddr *)&sa, sizeof(sa)) < 0) {
			sceNetSocketClose(fd);
			put(p, end, "speedtest: connect failed (is the host server listening?)");
			return WQ_ERR_CHIP;
		}
		static char sink[65536];
		unsigned long long total = 0;
		SceInt64 t0 = sceKernelGetProcessTimeWide();
		SceInt64 tend = t0 + (SceInt64)secs * 1000000;
		for (;;) {
			int n = sceNetRecv(fd, sink, sizeof(sink), 0);
			if (n <= 0) break;
			total += (unsigned)n;
			if ((SceInt64)sceKernelGetProcessTimeWide() >= tend) break;
		}
		sceNetSocketClose(fd);
		SceInt64 el = sceKernelGetProcessTimeWide() - t0; if (el < 1) el = 1;
		unsigned mbps10 = (unsigned)((total * 8ULL * 10ULL) / (unsigned long long)el);
		p = put(p, end, "speedtest: ");
		p = put_i(p, end, (int)(total / (1024*1024)));
		p = put(p, end, " MiB in ");
		p = put_i(p, end, secs);
		p = put(p, end, "s = ");
		p = put_i(p, end, (int)(mbps10 / 10));
		p = put(p, end, ".");
		p = put_i(p, end, (int)(mbps10 % 10));
		p = put(p, end, " Mbps down");
		WQ_LOGI("speedtest: %u bytes in %ds (rcvbuf default=%d used=%d)",
		        (unsigned)total, secs, defbuf, usebuf);
		return WQ_OK;
	}

	/* UDP sink: count what arrives on <port> for [secs] - the raw link capacity
	 * without TCP (pair with a fixed-rate UDP sender on the host). Logs bytes,
	 * datagrams and the largest sequence number seen (sender puts a u32 seq at
	 * the start of each datagram), so loss = 1 - pkts/(maxseq+1). */
	if ((a = tok_after(line, "udpsink"))) {
		const char *rest = a;
		int port = parse_int(&rest);
		int secs = parse_int(&rest);
		if (port <= 0) { put(p, end, "usage: udpsink <port> [secs]"); return WQ_ERR_ARG; }
		if (secs <= 0) secs = 10;
		int fd = sceNetSocket("wqudpsink", SCE_NET_AF_INET, SCE_NET_SOCK_DGRAM, 0);
		if (fd < 0) { put(p, end, "udpsink: socket failed"); return WQ_ERR_CHIP; }
		int b = 1024 * 1024;
		sceNetSetsockopt(fd, WQ_SOL_SOCKET, WQ_SO_RCVBUF, &b, sizeof(b));
		int to = 200 * 1000;                       /* 200 ms recv timeout      */
		sceNetSetsockopt(fd, WQ_SOL_SOCKET, WQ_SO_RCVTIMEO, &to, sizeof(to));
		SceNetSockaddrIn sa; memset(&sa, 0, sizeof(sa));
		sa.sin_family = SCE_NET_AF_INET;
		sa.sin_port = sceNetHtons((unsigned short)port);
		if (sceNetBind(fd, (SceNetSockaddr *)&sa, sizeof(sa)) < 0) {
			sceNetSocketClose(fd);
			put(p, end, "udpsink: bind failed");
			return WQ_ERR_CHIP;
		}
		static char ubuf[2048];
		unsigned long long total = 0; unsigned pkts = 0, maxseq = 0;
		SceInt64 t0 = 0, tl = 0, tend = sceKernelGetProcessTimeWide() + (SceInt64)(secs + 5) * 1000000;
		for (;;) {
			int n = sceNetRecv(fd, ubuf, sizeof(ubuf), 0);
			SceInt64 now = sceKernelGetProcessTimeWide();
			if (n > 0) {
				if (!t0) { t0 = now; tend = now + (SceInt64)secs * 1000000; }
				tl = now; total += (unsigned)n; pkts++;
				if (n >= 4) {
					unsigned sq = (unsigned)(unsigned char)ubuf[0] << 24 |
					              (unsigned)(unsigned char)ubuf[1] << 16 |
					              (unsigned)(unsigned char)ubuf[2] << 8 |
					              (unsigned)(unsigned char)ubuf[3];
					if (sq > maxseq) maxseq = sq;
				}
			}
			if (now >= tend) break;
		}
		sceNetSocketClose(fd);
		SceInt64 el = (t0 && tl > t0) ? tl - t0 : 1;
		unsigned mbps10 = (unsigned)((total * 8ULL * 10ULL) / (unsigned long long)el);
		p = put(p, end, "udpsink: ");
		p = put_i(p, end, (int)(mbps10 / 10)); p = put(p, end, ".");
		p = put_i(p, end, (int)(mbps10 % 10)); p = put(p, end, " Mbps, pkts=");
		p = put_i(p, end, (int)pkts); p = put(p, end, " maxseq=");
		p = put_i(p, end, (int)maxseq);
		WQ_LOGI("udpsink: %u bytes %u pkts maxseq=%u in %u us",
		        (unsigned)total, pkts, maxseq, (unsigned)el);
		return WQ_OK;
	}

	/* Keep-Wi-Fi-on: hold off auto-standby so the console stays awake and Wi-Fi
	 * (and this :1339 link) stays up. Default ON. 'keepwifi' reports; 'keepwifi
	 * on|off' toggles. */
	if ((a = tok_after(line, "keepwifi"))) {
		if (str_eq(a, "on") || str_eq(a, "off")) {
			int on = str_eq(a, "on");
			keepwifi_set_enabled(on);
			put(p, end, on ? "keepwifi ON - auto-standby held off (Wi-Fi stays up)"
			               : "keepwifi off - console may auto-standby again");
			return WQ_OK;
		}
		p = put(p, end, "keepwifi: ");
		p = put(p, end, keepwifi_is_enabled() ? "ON (auto-standby held off)"
		                                       : "off (console may standby)");
		return WQ_OK;
	}

	/* Command SceShell's netctl daemon to (re)connect, via its own event-flag
	 * signal (netjoin.c). Safe to call while already connected (re-asserts). */
	if (str_eq(line, "rejoin")) {
		int r = wq_net_rejoin();
		put(p, end, r == WQ_OK ? "rejoin: netctl connect commanded"
		                       : "rejoin: event flag unresolved");
		return r;
	}
	/* IcmConnect: the shell's own saved-network auto-connect (netjoin.c). Safe
	 * while connected (the daemon ignores it unless idle/given-up). */
	/* DANGER-ish: drops the link (and this session) for up to ~65s; result is
	 * logged as 'icmtest: PASS/FAIL'. FAIL falls back to the proven rejoin. */
	if (str_eq(line, "icmtest")) {
		keepwifi_icm_test();
		put(p, end, "icmtest: started - link drops now; read the log for PASS/FAIL");
		return WQ_OK;
	}
	if (str_eq(line, "icm")) {
		int r = wq_net_icm_connect();
		put(p, end, r == WQ_OK ? "icm: IcmConnect issued (saved-network auto-connect)"
		                       : "icm: IcmConnect unresolved / socket failed");
		return r;
	}
	/* Command a disconnect - for testing the rejoin recovery. DANGER: drops the
	 * link (and this :1339 session) until netctl reconnects / keepwifi rejoins. */
	if (str_eq(line, "netdown")) {
		int r = wq_net_disconnect();
		put(p, end, r == WQ_OK ? "netdown: netctl disconnect commanded"
		                       : "netdown: event flag unresolved");
		return r;
	}
	put(p, end, "err: unknown cmd (try 'help')");
	return WQ_ERR_ARG;
}

/* Public entry: log every command received and its result/return code, then
 * delegate to the core. One wrapper captures all commands/returns without
 * threading a log call through each of the core's many return paths. The `log`
 * command's own ring dump happens inside the core (before this logs the rc),
 * so a dump shows the command that triggered it. */
int wq_dispatch_exec(const char *line, char *out, int outcap)
{
	WQ_LOGI("cmd: %s", (line && line[0]) ? line : "(empty)");
	int r = dispatch_exec_inner(line, out, outcap);
	if (r == WQ_OK)
		WQ_LOGD("cmd rc=0 (ok)");
	else
		WQ_LOGW("cmd '%s' rc=%d", (line && line[0]) ? line : "(empty)", r);
	/* Single source of truth: fold any setting change into the persisted state,
	 * whatever surface the command came from (Settings UI, :1339, internal). */
	if (line)
		wq_settings_note(line);
	return r;
}

/* ---- the TCP listener (vitacompanion style) ----------------------------- */

static int    g_run = 0;
static SceUID g_thread = -1;

/* How long the worker sleeps before touching SceNet/SceNetCtl. module_start
 * defers ALL bring-up to this thread specifically so nothing network-related
 * runs during early SceShell boot; ~3s mirrors the proven vitacompanion net
 * delay and is comfortably past the point where SceShell has initialised
 * SceNet. Touching the network before that faults hard enough to wedge boot. */
#define WQ_DEFERRED_BRINGUP_DELAY_US  (3 * 1000 * 1000)

/* Developer mode: the :1339 listener only runs while this is on (default off,
 * restored from Settings at boot). Commands from the Settings page reach
 * wq_dispatch_exec directly and never need the port. Turning it off closes the
 * listening socket, which wakes the blocked accept so the server loop parks. */
static volatile int g_devmode = 0;
static volatile int g_srv = -1;

static void dispatch_set_devmode(int on)
{
	g_devmode = on ? 1 : 0;
	if (!g_devmode && g_srv >= 0)
		sceNetSocketClose(g_srv);   /* unblocks accept(); loop closes it again */
	WQ_LOGI("dispatch: developer mode %s (:%d remote control %s)",
	        g_devmode ? "ON" : "off", WQ_DISPATCH_PORT, g_devmode ? "listening" : "closed");
}

static int server_thread(SceSize args, void *argp)
{
	(void)args; (void)argp;

	/* Off the boot-critical path: wait for SceNet to be ready, then bring up
	 * every subsystem in the order the old module_start used. All non-fatal. */
	sceKernelDelayThread(WQ_DEFERRED_BRINGUP_DELAY_US);
	if (!g_run)
		return 0;

	int rs = wq_safety_init();   /* the revert spine                           */
	if (rs != WQ_OK)
		WQ_LOGE("wq_safety_init FAILED (rc=%d) - revert harness unavailable", rs);
	int rd = diag_init();        /* SceNetCtl + drop-reason callback           */
	if (rd != WQ_OK)
		WQ_LOGW("diag_init rc=%d (SceNetCtl not ready)", rd);
	int ra = wq_adv_init();      /* resolve the libnetctl scan verbs           */
	WQ_LOGI("wq_adv_init: scan verbs %s",
	        wq_adv_scan_available() ? "resolved" : "NOT resolved (report n/a)");
	(void)ra;

	keepwifi_start();            /* default-ON: hold off auto-standby (keeps     */
	                             /* Wi-Fi up, and the :1339 link alive)          */

	/* Load persisted settings and re-apply them (subsystems are up now). */
	wq_settings_boot();

	/* Self-healing listener. If accept() starts failing persistently - e.g. the
	 * socket was torn down when Wi-Fi dropped - we close and re-create it rather
	 * than spinning forever on a dead fd (which previously left :1339 refused
	 * until a reload). A healthy idle socket blocks in accept() and never trips
	 * the rebuild; only solid, repeated errors do. The outer loop also covers
	 * the case where the network is not up yet when we first try to bind. */
	while (g_run) {
		if (!g_devmode) {            /* Developer mode off: no listener at all    */
			sceKernelDelayThread(500 * 1000);
			continue;
		}
		int srv = sceNetSocket("wqdisp", SCE_NET_AF_INET, SCE_NET_SOCK_STREAM, 0);
		if (srv >= 0) {
			SceNetSockaddrIn addr;
			memset(&addr, 0, sizeof(addr));
			addr.sin_family = SCE_NET_AF_INET;
			addr.sin_port = sceNetHtons(WQ_DISPATCH_PORT);
			addr.sin_addr.s_addr = sceNetHtonl(SCE_NET_INADDR_ANY);
			if (sceNetBind(srv, (SceNetSockaddr *)&addr, sizeof(addr)) < 0) {
				WQ_LOGE("dispatch: bind :%d failed - retrying", WQ_DISPATCH_PORT);
				sceNetSocketClose(srv);
				srv = -1;
			} else {
				sceNetListen(srv, 1);
				g_srv = srv;
				WQ_LOGI("dispatch: listening on TCP :%d", WQ_DISPATCH_PORT);
			}
		} else {
			WQ_LOGE("dispatch: sceNetSocket failed rc=0x%08x - retrying",
			        (unsigned)srv);
		}
		if (srv < 0) {               /* network not ready - wait then retry      */
			sceKernelDelayThread(2 * 1000 * 1000);
			continue;
		}

		int accept_fails = 0;
		while (g_run) {
			int cli = sceNetAccept(srv, NULL, NULL);
			if (cli < 0 && !g_devmode)
				break;               /* Developer mode switched off            */
			if (cli < 0) {
				/* A dead socket errors immediately and repeatedly; a healthy
				 * idle one blocks here. ~5s of solid failures => rebuild it. */
				if (++accept_fails >= 25) {
					WQ_LOGW("dispatch: accept failing persistently (rc=0x%08x) "
					        "- rebuilding listener", (unsigned)cli);
					break;
				}
				sceKernelDelayThread(200 * 1000);
				continue;
			}
			accept_fails = 0;
			WQ_LOGD("dispatch: client connected (fd=%d)", cli);
			char line[160];
			char reply[2048];   /* roomy for a `log`/`selftest`/`adv scan` dump  */
			for (;;) {
				int n = sceNetRecv(cli, line, sizeof(line) - 1, 0);
				if (n <= 0)
					break;
				line[n] = 0;
				/* strip trailing CR/LF */
				while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r'))
					line[--n] = 0;
				wq_dispatch_exec(line, reply, sizeof(reply));
				int rl = strlen(reply);
				reply[rl++] = '\n';
				sceNetSend(cli, reply, rl, 0);
				if (!g_devmode)
					break;           /* 'devmode off' sent over this session   */
			}
			sceNetSocketClose(cli);
			if (!g_devmode)
				break;
		}
		g_srv = -1;
		sceNetSocketClose(srv);
	}
	return 0;
}

int wq_dispatch_start(void)
{
	if (g_run)
		return WQ_OK;
	g_run = 1;
	g_thread = sceKernelCreateThread("WqDispatch", server_thread,
	                                 0x40, 0x4000, 0, 0, NULL);
	if (g_thread < 0) {
		g_run = 0;
		WQ_LOGE("dispatch: listener thread create failed rc=%d", g_thread);
		return WQ_ERR_NOT_READY;
	}
	sceKernelStartThread(g_thread, 0, NULL);
	return WQ_OK;
}

void wq_dispatch_stop(void)
{
	keepwifi_stop();   /* stop the keep-awake tick thread                       */
	if (!g_run)
		return;
	g_run = 0;
	if (g_thread >= 0) {
		sceKernelWaitThreadEnd(g_thread, NULL, NULL);
		sceKernelDeleteThread(g_thread);
		g_thread = -1;
	}
}
