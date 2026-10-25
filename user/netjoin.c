/*
 * netjoin.c - command SceShell's netctl daemon to (re)connect, by calling the
 * shell's OWN connect-signal routine in place.
 *
 * FUN_8115a028(enable) (3.60 address) is what the shell runs to tell
 * the netctl daemon to connect/disconnect - it just sets an event-flag bit on a
 * SceShell global (0x400=connect, 0x800=disconnect). Rather than resolve that
 * data global ourselves (SceShell's RW data segment is relocated independently
 * of text at runtime, so its static address is wrong), we CALL FUN_8115a028 at
 * its text address: the function reaches its own global through the loader's
 * relocation, so it always hits the right one. We are injected into SceShell, so
 * the call runs in-process.
 *
 * Address resolution: the function sits at text +0x15a028 on 3.60 and 3.61 and
 * +0x15a080 from 3.63 to 3.74 (SceShell was rebuilt). Rather than trust either
 * offset, we match its first 64 bytes against a signature with the
 * firmware-dependent parts masked out (the movw/movt pair loading its data
 * global, which moves with the data segment, and the PC-relative bl/blx to
 * import stubs). The known offsets are tried first; otherwise the whole text
 * segment is scanned and exactly one match is required. No match = the call
 * is skipped (callers fall back to IcmConnect). The result is cached. No
 * firmware writes, no radio bounce.
 */
#include <psp2/types.h>
#include <psp2/kernel/threadmgr.h>   /* sceKernelDelayThread                     */
#include <psp2/kernel/threadmgr/eventflag.h>
#include <psp2/kernel/sysmem.h>     /* sceKernelGetMemBlockInfoByAddr (peek)    */
#include <psp2/kernel/modulemgr.h>  /* sceKernelGetModuleInfo + segments[]      */
#include <psp2/net/net.h>           /* sceNetSocket/Close (IcmConnect handle)   */
#include <taihen.h>

#include "netjoin.h"
#include "log.h"
#include "../include/wifiturbo.h"

/* Signature of the shell's connect-signal routine (first 64 bytes, Thumb).
 * Mask 0x00 = firmware-dependent byte (movw/movt immediates, bl/blx targets). */
static const uint8_t k_sig[64] = {
	0x10, 0xB5, 0x01, 0x1C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x68, 0xC9, 0xB2, 0xB0, 0xF1, 0xFF, 0x3F, 0x1B, 0xD0, 0x69, 0xB1,
	0x7F, 0xF4, 0x00, 0x61, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x68, 0x5F, 0xF4, 0x80, 0x61, 0x00, 0x00,
	0x00, 0x00, 0x0C, 0xE0, 0x7F, 0xF4, 0x80, 0x61, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
};
static const uint8_t k_sig_mask[64] = {
	0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00,
	0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
};
/* Where it was found in the shipped firmware: 3.60-3.61; 3.63-3.74. */
static const uint32_t k_sig_known[] = { 0x15A028u, 0x15A080u };

static int sig_match(const uint8_t *p)
{
	for (int i = 0; i < (int)sizeof(k_sig); i++)
		if ((p[i] & k_sig_mask[i]) != k_sig[i])
			return 0;
	return 1;
}

/* Runtime address (Thumb bit set) of the shell's connect-signal routine, or 0.
 * SceShell is ASLR'd per boot, so everything is relative to segments[0].vaddr. */
static uintptr_t resolve_connect_sig(void)
{
	static uintptr_t s_fn;
	static int s_done;
	if (s_done)
		return s_fn;
	s_done = 1;

	tai_module_info_t ti;
	ti.size = sizeof(ti);
	if (taiGetModuleInfo("SceShell", &ti) < 0)
		return 0;
	SceKernelModuleInfo mi;
	mi.size = sizeof(mi);
	if (sceKernelGetModuleInfo(ti.modid, &mi) < 0)
		return 0;
	const uint8_t *text = (const uint8_t *)mi.segments[0].vaddr;
	uint32_t size = mi.segments[0].memsz;
	if (!text || size < sizeof(k_sig)) {
		WQ_LOGW("netjoin: bad SceShell text seg vaddr=0x%08x memsz=0x%x",
		        (unsigned)(uintptr_t)text, (unsigned)size);
		return 0;
	}

	uint32_t off = 0;
	int found = 0;
	for (unsigned k = 0; k < sizeof(k_sig_known) / sizeof(k_sig_known[0]); k++) {
		uint32_t o = k_sig_known[k];
		if (o <= size - sizeof(k_sig) && sig_match(text + o)) {
			off = o;
			found = 1;
			break;
		}
	}
	if (!found) {
		for (uint32_t o = 0; o <= size - sizeof(k_sig) && found < 2; o += 2) {
			if (sig_match(text + o)) {
				off = o;
				found++;
			}
		}
	}
	if (found != 1) {
		WQ_LOGW("netjoin: connect-sig %s in SceShell - rejoin disabled",
		        found ? "ambiguous" : "not found");
		return 0;
	}
	s_fn = ((uintptr_t)text + off) | 1u;       /* Thumb bit                      */
	WQ_LOGI("netjoin: connect-sig at SceShell text+0x%x", (unsigned)off);
	return s_fn;
}

static int call_toggle(char enable, const char *what)
{
	uintptr_t fn = resolve_connect_sig();
	if (!fn) {
		WQ_LOGW("netjoin: %s - connect-sig unresolved", what);
		return WQ_ERR_NOT_READY;
	}
	((void (*)(char))fn)(enable);
	WQ_LOGI("netjoin: %s -> connect-sig(%d)", what, enable);
	return WQ_OK;
}

int wq_net_rejoin(void)
{
	/* The daemon wakes on the connect bit TRANSITIONING 0->1. After a boot
	 * auto-join give-up the connect bit is already set, so just re-signalling
	 * connect makes no edge. Force one: signal disconnect (clears it + parks the
	 * daemon in its wait), then signal connect (the 0->1 edge that wakes it).
	 * Proven on-device: netdown(0) then the watcher's rejoin(1) reconnected. */
	int r = call_toggle(0, "rejoin-off");
	if (r != WQ_OK)
		return r;
	sceKernelDelayThread(400 * 1000);   /* let the daemon process the disconnect */
	return call_toggle(1, "rejoin-on"); /* 0->1 edge => daemon connects          */
}

/* ---- IcmConnect: the shell's own "auto-connect to saved networks now" -------
 * SceShell/Settings bring the network up
 * with: s = sceNetSocket(name,AF_INET,SOCK_DGRAM,0); IcmConnect(s, 0x80); close.
 * net_ps raises an interface event, and the netctl daemon's callback turns it
 * into daemon command bit 2 = full saved-profile scan/rank/join, even after the
 * boot auto-join gave up (the case wq_net_rejoin's edge cannot fix). 0x80 =
 * MSG_DONTWAIT: return at once, never block our thread.
 * libnet (module "SceNet") exports the same entry (0x81002808) twice:
 * SceNetInternal 0x9E8A69FD / 0x93F2FF08 (what the shell imports) and public
 * SceNet 0x6BF8B2A2 / 0x84257F65. No SDK stub, so resolve at runtime. */
#define ICM_MODNAME          "SceNet"
#define ICM_LIB_INTERNAL     0x9E8A69FDu
#define ICM_NID_INTERNAL     0x93F2FF08u
#define ICM_LIB_PUBLIC       0x6BF8B2A2u
#define ICM_NID_PUBLIC       0x84257F65u
#define ICM_FLAG_DONTWAIT    0x80

typedef int (*fn_icm_connect)(int sock, int flags);
static fn_icm_connect g_icm;

static fn_icm_connect resolve_icm(void)
{
	if (g_icm)
		return g_icm;
	uintptr_t fp = 0;
	if (taiGetModuleExportFunc(ICM_MODNAME, ICM_LIB_INTERNAL, ICM_NID_INTERNAL, &fp) < 0 || !fp) {
		fp = 0;
		if (taiGetModuleExportFunc(ICM_MODNAME, ICM_LIB_PUBLIC, ICM_NID_PUBLIC, &fp) < 0)
			fp = 0;
	}
	g_icm = (fn_icm_connect)fp;
	return g_icm;
}

int wq_net_icm_connect(void)
{
	fn_icm_connect icm = resolve_icm();
	if (!icm) {
		WQ_LOGW("netjoin: icm - IcmConnect unresolved");
		return WQ_ERR_RESOLVE;
	}
	int s = sceNetSocket("WqIcmConnect", SCE_NET_AF_INET, SCE_NET_SOCK_DGRAM, 0);
	if (s < 0) {
		WQ_LOGW("netjoin: icm - socket failed 0x%08x", (unsigned)s);
		return WQ_ERR_NOT_READY;
	}
	int r = icm(s, ICM_FLAG_DONTWAIT);
	sceNetSocketClose(s);
	WQ_LOGI("netjoin: icm -> IcmConnect(0x80) rc=0x%08x", (unsigned)r);
	return WQ_OK;
}

/* Real Inet disconnect: libnetctl 0xDD96DA85 (IPMI 0x2000a, no args), what
 * Remote Play calls before joining another AP. The shell's event-flag
 * "disconnect" (FUN_8115a028(0)) does NOT drop an established link (netctl
 * state stayed 3 on-device), so it cannot force a reassociation. Lib NID
 * 0xC6083AE1 read from libnetctl.suprx's export table. */
#define NETCTL_MODNAME        "SceLibNetCtl"
#define NETCTL_LIB_NID        0xC6083AE1u
#define NETCTL_NID_DISCONNECT 0xDD96DA85u

int wq_net_disconnect(void)
{
	static int (*disc)(void);
	if (!disc) {
		uintptr_t fp = 0;
		if (taiGetModuleExportFunc(NETCTL_MODNAME, NETCTL_LIB_NID,
		                           NETCTL_NID_DISCONNECT, &fp) < 0 || !fp) {
			WQ_LOGW("netjoin: disconnect - 0xDD96DA85 unresolved");
			return WQ_ERR_RESOLVE;
		}
		disc = (int (*)(void))fp;
	}
	int r = disc();
	WQ_LOGI("netjoin: disconnect -> netctl 0xDD96DA85 rc=0x%08x", (unsigned)r);
	return r < 0 ? WQ_ERR_CHIP : WQ_OK;
}

/* Force a fresh association: real disconnect, then the shell's own
 * auto-connect (IcmConnect). Used to apply association-time settings (HT40). */
int wq_net_reassociate(void)
{
	int r = wq_net_disconnect();
	if (r != WQ_OK)
		return r;
	sceKernelDelayThread(1500 * 1000);   /* let the daemon settle in idle      */
	return wq_net_icm_connect();
}
