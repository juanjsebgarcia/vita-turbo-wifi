/*
 * macaddr.c - EXPERIMENTAL: a chosen MAC address for the Wi-Fi interface.
 *
 * Where the address lives (3.60-3.74, same on both driver builds):
 *
 *   - the chip, which comes back on its factory address at every power-up;
 *   - the driver's copy at ctx+0x728, read from the chip once, at the first
 *     bring-up after boot (worker event bit 0), before plugins load. Later
 *     power-ups (Wi-Fi off/on, resume) reload the firmware but do not ask
 *     again;
 *   - the attach descriptor (ctx+0x630, word 0x17 = byte 0x5c), and SceNetPs's
 *     wlan0 link-level address copied from it once at attach. ether_output takes
 *     the source address and ARP the sender address from that copy.
 *
 * SET: the driver's own get-MAC control call (what SceShell's ioctl 0x40180004
 * reaches: control entry = desc word 0x14 -> get-MAC wrapper -> synchronous
 * MAC_ADDRESS 0x4D, which allocates the node, sends it through the async send
 * helper, waits for the reply and frees the node). A one-shot in the send hook
 * turns that one GET (action 0) into a SET (action 1) of the chosen address;
 * the reply carries the address now in use. Only if it matches are the driver
 * copy, the descriptor and wlan0 rewritten. The user side re-sends after every
 * chip power-up, counted by a hook on the post-firmware init.
 *
 * Hook bodies are plain loads/stores; nothing in them logs. The send helper
 * also carries every RSSI_INFO poll, so its non-0x4D path is one compare.
 *
 * Nothing is hooked until something asks for more than STATUS, so a console
 * left on its own address never carries the hooks. LIVE and SET never call
 * into the driver while the chip is off, inside the post-firmware init, or
 * within WQ_MAC_SETTLE_MS after it (WQ_ERR_BUSY: the caller retries). The
 * ops that touch the driver are serialised by one mutex.
 */
#include <psp2kern/kernel/sysclib.h>   /* memset */
#include <psp2kern/kernel/sysmem.h>    /* ksceKernelGetPaddr */
#include <psp2kern/kernel/threadmgr.h> /* mutex, ksceKernelGetSystemTimeWide */
#include <psp2kern/types.h>
#include <taihen.h>

#include "macaddr.h"
#include "wlanbt.h"
#include "klog.h"
#include "../include/wifiturbo.h"

#define CMD_MAC_ADDRESS   0x4D
#define MAC_BODY_LEN      8
#define CTX_MAC           0x728     /* driver's copy of the address          */
#define CTX_DESC          0x630     /* SceNetPs driver descriptor            */
#define DESC_W_CONTAINER  0x0d      /* SceNetPs per-interface container      */
#define DESC_W_CTL        0x14      /* driver control entry                  */
#define DESC_W_TYPE       0x16      /* 2 = wlan                              */
#define DESC_MAC_OFF      0x5c      /* word 0x17: address copied at attach   */
#define DESC_CTX_BACK     0x6c      /* the control entry's ctx pointer       */
#define CONT_W_IFNET      0x0c
#define CONT_W_LLADDR     0x0d      /* = LLADDR(ifnet sockaddr_dl)           */
#define IFNET_NAME        0x14
#define IFNET_SDL         0x118

#define CTL_GET_MAC       0x40180004   /* SceShell's get-MAC ioctl                */
#define CTL_BUF_LEN       0x18
#define CTL_BUF_MAC       0x0c         /* wrapper: buf+0xc = address, +0x14 = 6   */

#define MTX_TIMEOUT_US    (2 * 1000 * 1000)

static const uint8_t k_send_sig[8] = { 0x2D, 0xE9, 0xF0, 0x47, 0x00, 0xF1, 0x2C, 0x08 };
/* Both prologues below load the stack-guard address with a movw/movt pair
 * whose immediates the module loader fills in, so those bytes are compared
 * through a mask (instruction and register only).
 * The control entry's includes ldr.w r9,[r0,#0x6c] (desc+0x6c = ctx). */
static const uint8_t k_ctl_sig[16]  = { 0x2D, 0xE9, 0xF0, 0x4F, 0x40, 0xF2, 0x00, 0x08,
                                        0xD0, 0xF8, 0x6C, 0x90, 0xC0, 0xF2, 0x00, 0x08 };
static const uint8_t k_ctl_mask[16] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xF0, 0xFB, 0x00, 0x0F,
                                        0xFF, 0xFF, 0xFF, 0xFF, 0xF0, 0xFB, 0x00, 0x0F };
static const uint8_t k_init_sig[12]  = { 0x2D, 0xE9, 0xF0, 0x43, 0x40, 0xF2, 0x00, 0x04,
                                         0xC0, 0xF2, 0x00, 0x04 };
static const uint8_t k_init_mask[12] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xF0, 0xFB, 0x00, 0x0F,
                                         0xF0, 0xFB, 0x00, 0x0F };

static SceUID         g_send_hook = -1, g_init_hook = -1;
static tai_hook_ref_t g_send_ref, g_init_ref;

/* Shared with the hooks: plain words and bytes only. */
static volatile uintptr_t g_ctx;            /* driver ctx seen at the send helper */
static volatile int       g_oneshot, g_oneshot_used;
static uint8_t            g_oneshot_mac[6];
static volatile uint32_t  g_powerups, g_sets;
static volatile int       g_in_init;        /* inside the post-firmware init      */
static volatile SceUInt64 g_powerup_t;      /* when the last init returned        */
static SceUID             g_mtx = -1;

static uint8_t  g_factory[6], g_live[6];
static int      g_have_factory;
static int32_t  g_last_rc;
static uint32_t g_ctl_check = 1;
static uint32_t g_chain = 1;                /* last walk's result (WALK/SET)      */
static uint8_t  g_lladdr[6], g_descmac[6];  /* what that walk read                */

/* Kernel heap blocks can sit low (the driver ctx is around 0x01f0xxxx), so
 * this only rules out NULL-page and unaligned values; the walk's back-pointer,
 * type, name and length checks are what make it safe. */
static int kptr_ok(uintptr_t p)
{
	return p >= 0x10000u && (p & 3) == 0;
}

static int mac_valid(const uint8_t *m)
{
	int zero = 1, ff = 1;
	for (int i = 0; i < 6; i++) {
		if (m[i])         zero = 0;
		if (m[i] != 0xFF) ff = 0;
	}
	return !zero && !ff && !(m[0] & 1);
}

static int mac_eq(const volatile uint8_t *a, const volatile uint8_t *b)
{
	for (int i = 0; i < 6; i++)
		if (a[i] != b[i])
			return 0;
	return 1;
}

static void mac_cpy(volatile uint8_t *d, const volatile uint8_t *s)
{
	for (int i = 0; i < 6; i++)
		d[i] = s[i];
}

/* [p, p+len) is mapped kernel memory: checked before every SceNetPs pointer
 * is read, so a stale or foreign pointer fails the walk instead of faulting. */
static int kmapped(uintptr_t p, uint32_t len)
{
	uintptr_t pa;
	return kptr_ok(p) && ksceKernelGetPaddr((void *)p, &pa) >= 0 &&
	       ksceKernelGetPaddr((void *)(p + len - 1), &pa) >= 0;
}

/* Walk ctx -> wlan0's link-level address, checking each step. Returns 0 and
 * the two address pointers, or the number of the step that failed. */
static int netps_chain(uintptr_t ctx, uint8_t **lladdr_out, uint8_t **descmac_out)
{
	if (!kmapped(ctx + CTX_DESC, DESC_CTX_BACK + 4))
		return 1;
	const volatile uint32_t *desc = (const volatile uint32_t *)(ctx + CTX_DESC);
	if (desc[DESC_W_TYPE] != 2)
		return 2;
	static const int need[4] = { 0x10, 0x11, 0x12, 0x14 };   /* driver callbacks */
	for (int i = 0; i < 4; i++)
		if (!wlanbt_seg_contains(0, desc[need[i]] & ~1u, 2))
			return 3;
	uintptr_t cont = desc[DESC_W_CONTAINER];
	if (!kmapped(cont, (CONT_W_LLADDR + 1) * 4) ||
	    *(const volatile uint32_t *)cont != (uint32_t)(uintptr_t)desc)
		return 4;
	const volatile uint32_t *c = (const volatile uint32_t *)cont;
	uintptr_t ifnet = c[CONT_W_IFNET];
	if (!kmapped(ifnet, IFNET_SDL + 4) || *(const volatile uint32_t *)ifnet != (uint32_t)cont)
		return 5;
	const volatile char *name = (const volatile char *)(ifnet + IFNET_NAME);
	if (name[0] != 'w' || name[1] != 'l' || name[2] != 'a' || name[3] != 'n')
		return 6;
	uintptr_t sdl = *(const volatile uint32_t *)(ifnet + IFNET_SDL);
	if (!kmapped(sdl, 8 + 16 + 6))                  /* header, longest name, address */
		return 7;
	const volatile uint8_t *s = (const volatile uint8_t *)sdl;
	if (s[6] != 6 || s[5] > 16)                     /* sdl_alen, sdl_nlen */
		return 8;
	uintptr_t ll = sdl + 8 + s[5];
	if (c[CONT_W_LLADDR] != (uint32_t)ll)
		return 9;
	*lladdr_out  = (uint8_t *)ll;
	*descmac_out = (uint8_t *)(ctx + CTX_DESC + DESC_MAC_OFF);
	return 0;
}

/* async send(ctx, node, cmd, len). */
static int hook_send(int ctx, int node, unsigned cmd, int len)
{
	if (!g_ctx)
		g_ctx = (uintptr_t)ctx;
	if (g_oneshot && (cmd & 0xFFFF) == CMD_MAC_ADDRESS && len == MAC_BODY_LEN && node) {
		volatile uint8_t *b = *(volatile uint8_t **)(uintptr_t)(node + 8);
		if (b && b[0] == 0 && b[1] == 0) {
			b[0] = 1;                             /* HostCmd_ACT_GEN_SET */
			mac_cpy(b + 2, g_oneshot_mac);
			g_oneshot = 0;
			g_oneshot_used = 1;
			g_sets++;
		}
	}
	return TAI_CONTINUE(int, g_send_ref, ctx, node, cmd, len);
}

/* Post-firmware init, run by the driver worker at every power-up with the
 * driver ctx in r0 (3.60 0x810087be: mov r0, r4 - the worker's ctx). Learn
 * the ctx here, before any command, so the SET can go out before the first
 * join, and count the power-up once the init has returned. It takes one
 * argument (3.60 and 3.71: only r0 is read, nothing above its own frame), so
 * passing r1-r3 through is harmless. */
static int hook_init(int a0, int a1, int a2, int a3)
{
	if (!g_ctx && kptr_ok((uintptr_t)a0))
		g_ctx = (uintptr_t)a0;
	g_in_init = 1;
	int r = TAI_CONTINUE(int, g_init_ref, a0, a1, a2, a3);
	g_powerup_t = ksceKernelGetSystemTimeWide();
	g_powerups++;
	g_in_init = 0;
	return r;
}

/* 1 if the chip is on, initialised and settled, so a control call is safe. */
static int chip_ready(void)
{
	unsigned pw;
	if (g_in_init || wlanbt_power_word(&pw) != WQ_OK || (pw & 3) != 3)
		return 0;
	return ksceKernelGetSystemTimeWide() - g_powerup_t >= WQ_MAC_SETTLE_MS * 1000ull;
}

static int sig_ok(const volatile uint8_t *t, uint32_t off, const uint8_t *sig,
                  const uint8_t *mask, unsigned n)
{
	for (unsigned i = 0; i < n; i++) {
		uint8_t m = mask ? mask[i] : 0xFF;
		if ((t[off + i] & m) != (sig[i] & m))
			return 0;
	}
	return 1;
}

static int hooks_ensure(void)
{
	if (g_send_hook >= 0 && g_init_hook >= 0)
		return WQ_OK;
	const WlanbtLayout *l = wlanbt_layout();
	SceUID modid;
	uintptr_t text;
	uint32_t need = 0;
	if (l) {
		need = l->cmd_send + 8;
		if (l->ctl + 16 > need)      need = l->ctl + 16;
		if (l->pwr_init + 12 > need) need = l->pwr_init + 12;
	}
	if (!l || wlanbt_text(need, &modid, &text) != WQ_OK)
		return WQ_ERR_RESOLVE;
	const volatile uint8_t *t = (const volatile uint8_t *)text;
	int bad = !sig_ok(t, l->cmd_send, k_send_sig, NULL, 8) ? 1 :
	          !sig_ok(t, l->ctl, k_ctl_sig, k_ctl_mask, 16) ? 2 :
	          !sig_ok(t, l->pwr_init, k_init_sig, k_init_mask, 12) ? 3 : 0;
	if (bad) {
		WQ_KLOGW("macaddr: original bytes mismatch at site %d - refusing", bad);
		return WQ_ERR_RESOLVE;
	}
	if (g_init_hook < 0) {
		g_init_hook = taiHookFunctionOffsetForKernel(KERNEL_PID, &g_init_ref, modid, 0,
		                                             l->pwr_init, 1, hook_init);
		if (g_init_hook < 0) {
			WQ_KLOGW("macaddr: init hook failed (0x%08x)", (unsigned)g_init_hook);
			g_init_hook = -1;
			return WQ_ERR_RESOLVE;
		}
	}
	if (g_send_hook < 0) {
		g_send_hook = taiHookFunctionOffsetForKernel(KERNEL_PID, &g_send_ref, modid, 0,
		                                             l->cmd_send, 1, hook_send);
		if (g_send_hook < 0) {
			WQ_KLOGW("macaddr: send hook failed (0x%08x)", (unsigned)g_send_hook);
			g_send_hook = -1;
			return WQ_ERR_RESOLVE;
		}
	}
	WQ_KLOGI("macaddr: hooked send +0x%x, init +0x%x; control +0x%x (%s)",
	         (unsigned)l->cmd_send, (unsigned)l->pwr_init, (unsigned)l->ctl, l->name);
	return WQ_OK;
}

typedef int (*ctl_fn)(void *desc, unsigned cmd, void *buf, unsigned len);

/* desc word 0x14 must be the driver's control entry, and the entry must find
 * this ctx at desc+0x6c, as SceNetPs's calls rely on. */
static ctl_fn ctl_resolve(uintptr_t ctx)
{
	const WlanbtLayout *l = wlanbt_layout();
	uintptr_t text;
	g_ctl_check = 1;
	if (!l || !kptr_ok(ctx) || wlanbt_text(l->ctl + 16, NULL, &text) != WQ_OK)
		return NULL;
	const volatile uint32_t *desc = (const volatile uint32_t *)(ctx + CTX_DESC);
	uint32_t entry = desc[DESC_W_CTL];
	if ((entry & ~1u) != (uint32_t)text + l->ctl || !(entry & 1)) {
		g_ctl_check = 2;
		return NULL;
	}
	if (*(const volatile uint32_t *)(ctx + CTX_DESC + DESC_CTX_BACK) != (uint32_t)ctx) {
		g_ctl_check = 3;
		return NULL;
	}
	g_ctl_check = 0;
	return (ctl_fn)(uintptr_t)entry;
}

/* The driver's own get-MAC. With set != NULL the one-shot turns its GET into
 * a SET of *set. Fills g_live/g_last_rc. */
static int ctl_get_mac(uintptr_t ctx, const uint8_t *set)
{
	ctl_fn fn = ctl_resolve(ctx);
	if (!fn)
		return WQ_ERR_RESOLVE;
	if (!chip_ready())
		return WQ_ERR_BUSY;
	uint32_t buf[CTL_BUF_LEN / 4];
	memset(buf, 0, sizeof(buf));
	buf[1] = CTL_BUF_LEN;
	g_oneshot_used = 0;
	if (set) {
		mac_cpy(g_oneshot_mac, set);
		g_oneshot = 1;
	}
	int rc = fn((void *)(ctx + CTX_DESC), CTL_GET_MAC, buf, CTL_BUF_LEN);
	g_oneshot = 0;                       /* never leave it armed for a later GET */
	g_last_rc = rc;
	if (rc < 0)
		return WQ_ERR_CHIP;
	mac_cpy(g_live, (const uint8_t *)buf + CTL_BUF_MAC);
	return WQ_OK;
}

static int op_set(uintptr_t ctx, const uint8_t *mac)
{
	if (!mac_valid(mac))
		return WQ_ERR_ARG;
	if (!g_have_factory) {               /* unmodified until our first SET */
		mac_cpy(g_factory, (const volatile uint8_t *)(ctx + CTX_MAC));
		g_have_factory = 1;
	}
	/* Find wlan0 before the chip is touched, so a walk that fails leaves
	 * every copy as it was rather than the chip and driver half changed. */
	uint8_t *ll, *dm;
	g_chain = (uint32_t)netps_chain(ctx, &ll, &dm);
	if (g_chain != 0)
		return WQ_ERR_RESOLVE;
	uint32_t pu = g_powerups;
	int r = ctl_get_mac(ctx, mac);
	WQ_KLOGI("macaddr: SET %02x:%02x:%02x:%02x:%02x:%02x rc=0x%08x used=%d chip "
	         "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4],
	         mac[5], (unsigned)g_last_rc, g_oneshot_used, g_live[0], g_live[1],
	         g_live[2], g_live[3], g_live[4], g_live[5]);
	if (r != WQ_OK)
		return r;
	if (g_powerups != pu || g_in_init)
		return WQ_ERR_BUSY;               /* the chip restarted meanwhile: on its own address again */
	/* rc 0 with no MAC_ADDRESS sent (seen ~1.5 s after a radio power-up,
	 * while SceShell starts its scan): the chip was not touched, so it is
	 * worth asking again shortly. */
	if (!g_oneshot_used)
		return WQ_ERR_BUSY;
	if (!mac_eq(g_live, mac))
		return WQ_ERR_CHIP;               /* the chip did not take it: change nothing else */
	mac_cpy((volatile uint8_t *)(ctx + CTX_MAC), mac);
	mac_cpy(ll, mac);
	mac_cpy(dm, mac);
	mac_cpy(g_lladdr, mac);
	mac_cpy(g_descmac, mac);
	return WQ_OK;
}

static void walk(uintptr_t ctx)
{
	uint8_t *ll, *dm;
	ctl_resolve(ctx);
	g_chain = (uint32_t)netps_chain(ctx, &ll, &dm);
	if (g_chain == 0) {
		mac_cpy(g_lladdr, ll);
		mac_cpy(g_descmac, dm);
	}
}

static void fill(WqMacStatus *o)
{
	memset(o, 0, sizeof(*o));
	o->hooked   = g_send_hook >= 0 && g_init_hook >= 0;
	o->powerups = g_powerups;
	o->sets     = g_sets;
	o->last_rc  = g_last_rc;
	o->have_factory = g_have_factory;
	mac_cpy(o->factory, g_factory);
	mac_cpy(o->live, g_live);
	/* wlan0 and the descriptor as the last WALK/SET found them: STATUS is
	 * polled, and reads nothing outside the driver ctx. */
	o->chain     = g_chain;
	o->ctl_check = g_ctl_check;
	mac_cpy(o->lladdr, g_lladdr);
	mac_cpy(o->descmac, g_descmac);
	uintptr_t ctx = g_ctx;
	o->ctx_seen = ctx != 0;
	if (!ctx)
		return;
	mac_cpy(o->drv, (const volatile uint8_t *)(ctx + CTX_MAC));
	if (!g_have_factory)
		mac_cpy(o->factory, o->drv);     /* not modified yet, so this is it */
	o->have_factory = 1;
}

int macaddr_op(int op, const uint8_t mac[6], WqMacStatus *out)
{
	int r = WQ_OK;
	if (op != WQ_MAC_OP_STATUS) {
		unsigned to = MTX_TIMEOUT_US;
		if (g_mtx < 0 || ksceKernelLockMutex(g_mtx, 1, &to) < 0) {
			r = WQ_ERR_BUSY;
		} else {
			r = hooks_ensure();
			if (r == WQ_OK) {
				uintptr_t ctx = g_ctx;
				switch (op) {
				case WQ_MAC_OP_HOOK:
					break;
				case WQ_MAC_OP_WALK:
					if (ctx)
						walk(ctx);
					break;
				case WQ_MAC_OP_LIVE:
					r = ctx ? ctl_get_mac(ctx, NULL) : WQ_ERR_NOT_READY;
					break;
				case WQ_MAC_OP_SET:
					r = ctx ? op_set(ctx, mac) : WQ_ERR_NOT_READY;
					break;
				default:
					r = WQ_ERR_ARG;
				}
			}
			ksceKernelUnlockMutex(g_mtx, 1);
		}
	}
	if (out)
		fill(out);
	return r;
}

void macaddr_init(void)
{
	g_mtx = ksceKernelCreateMutex("WqMacKMtx", 0, 0, NULL);
}

void macaddr_fini(void)
{
	if (g_mtx >= 0)
		ksceKernelLockMutex(g_mtx, 1, NULL);   /* no op left inside the driver */
	if (g_send_hook >= 0) {
		taiHookReleaseForKernel(g_send_hook, g_send_ref);
		g_send_hook = -1;
	}
	if (g_init_hook >= 0) {
		taiHookReleaseForKernel(g_init_hook, g_init_ref);
		g_init_hook = -1;
	}
	if (g_mtx >= 0) {
		ksceKernelUnlockMutex(g_mtx, 1);
		ksceKernelDeleteMutex(g_mtx);
		g_mtx = -1;
	}
}
