/*
 * roamassist.c - one-shot join-path roam assist for SceWlanBt (3.60-3.74).
 *
 * Why the Vita sticks to one AP: SceShell narrows the join's directed scan to
 * the ONE channel it first heard the SSID on (join request +0xb4..+0xb7), and
 * the kernel join handler FUN_81006ea0 associates to the FIRST scan entry whose
 * security matches. Two hooks, both acting only for one armed join:
 *
 *   FUN_81006ea0(ctx, req, len)  text +0x6ea0  push.w {r4-r11,lr} = 2d e9 f0 4f
 *     The ioctl 0x50110001 handler. Pre: zero req+0xb4..+0xb7, so the driver
 *     scans every allowed channel (its own "empty list => 1..14" path), save the
 *     join SSID (req+0xc len, req+0x10), and mark "join in flight" for this
 *     thread. Post: record the result and the BSSID it joined (it copies the
 *     chosen 0x84-byte entry back to req+8, so BSSID = req+8+0x28).
 *
 *   FUN_8100b3a0(ctx, buf, 0x800) text +0xb3a0  push {r3-r7,lr}; add.w r4,r0,#8
 *                                               = f8 b5 00 f1 08 04 05 46
 *     Scan wrapper (lock, FUN_81006a6c, unlock). Also serves the user scan
 *     ioctl 0x50100001, so we act ONLY when the calling thread is the one inside
 *     our hooked join. After it returns, the result buffer is:
 *       buf+0x08  u32 entry count            (FUN_810067ec)
 *       buf+0x0c  first entry; entries are contiguous, each starting with
 *                 u16 total length = 0x84 + align4(IE bytes)
 *     Entry (FUN_810067ec / FUN_81000f30): +0x04 u32 SSID len, +0x08 SSID[32],
 *     +0x28 BSSID[6], +0x2e channel, +0x30 capability, +0x32 u16 |RSSI dBm|
 *     (raw scan byte), +0x34 u16 quality % (100 for <=-50 dBm, (100-|rssi|)*2
 *     below; this is what the shell ranks by), +0x5c WPA / +0x68 RSN parsed
 *     flags, +0x84 IEs. No pointers inside an entry, so entries can be moved.
 *     We rebuild the chain in a scratch buffer with the same entries in a new
 *     order (target BSSID, then same-SSID by strongest |RSSI|, then the rest in
 *     stock order) and copy it back: same total size, same lengths, so the
 *     chain stays valid. The join loop then tries them in that order and still
 *     falls through to the next entry if an association fails.
 *
 * Both hooks run in the ioctl caller's thread (SceShell's netctl daemon), after
 * the scan's chip command completed and its mutex was released - not on the
 * 0x2f44 command path. No logging, no allocation, no chip command in either
 * hook: plain loads/stores and memcpy on a buffer the driver owns for this call.
 * Hooks are installed on the first arm (never at boot) and stay installed;
 * disarmed they cost one load and a branch.
 *
 * Addresses above are 3.60's. 3.71 moved both functions (join +0x712c, scan
 * +0xb658) with the same request and scan-entry layout; the offsets come from
 * the firmware's layout (wlanbt.c).
 */
#include <psp2kern/kernel/threadmgr.h>  /* ksceKernelGetThreadId, GetSystemTimeWide   */
#include <psp2kern/kernel/sysclib.h>    /* memcpy/memset/memcmp                      */
#include <psp2kern/types.h>
#include <taihen.h>

#include "roamassist.h"
#include "wlanbt.h"
#include "klog.h"

#define REQ_OFF_RESULT    0x08      /* chosen BSS entry copied back here      */
#define REQ_OFF_SSIDLEN   0x0C
#define REQ_OFF_SSID      0x10
#define REQ_OFF_CHANLIST  0xB4      /* 4 channel bytes, 0-terminated          */
#define REQ_MIN_LEN       0xC0      /* dispatcher copies 0xc0; join needs 0x8c*/

#define SCAN_OFF_COUNT    0x08
#define SCAN_OFF_FIRST    0x0C
#define SCAN_BUF_LEN      0x800     /* both join call sites pass 0x800        */

#define BSS_OFF_LEN       0x00
#define BSS_OFF_SSIDLEN   0x04
#define BSS_OFF_SSID      0x08
#define BSS_OFF_BSSID     0x28
#define BSS_OFF_CHANNEL   0x2E
#define BSS_OFF_RSSI      0x32      /* u16 |dBm|, smaller = stronger          */
#define BSS_FIXED_LEN     0x84

#define MAX_ENTRIES       32        /* FUN_810067ec caps raw results at 16    */
#define ARM_TTL_US        (60LL * 1000 * 1000)

static SceUID         g_join_hook = -1;
static tai_hook_ref_t g_join_ref;
static SceUID         g_scan_hook = -1;
static tai_hook_ref_t g_scan_ref;

/* Arm state (written by the syscall, consumed by the join hook). */
static volatile int      g_armed;
static volatile int      g_target_on;
static volatile uint8_t  g_target[6];
static volatile SceInt64 g_arm_time;

/* Join-in-flight state (join hook -> scan hook, same thread). */
static volatile int      g_in_join;
static volatile int      g_join_tid;
static uint32_t          g_ssid_len;
static uint8_t           g_ssid[32];

static WqRoamAssistStatus g_st;
static uint8_t g_scratch[SCAN_BUF_LEN];

static unsigned rssi_key(const uint8_t *e)
{
	unsigned r = *(const uint16_t *)(e + BSS_OFF_RSSI);
	return (r == 0 || r > 120) ? 0xFFFFu : r;   /* unknown sorts last          */
}

static int ssid_match(const uint8_t *e)
{
	uint32_t l = *(const uint32_t *)(e + BSS_OFF_SSIDLEN);
	return l == g_ssid_len && l <= 32 && memcmp(e + BSS_OFF_SSID, g_ssid, l) == 0;
}

static int bssid_eq(const uint8_t *a, const volatile uint8_t *b)
{
	for (int i = 0; i < 6; i++)
		if (a[i] != b[i]) return 0;
	return 1;
}

/* Reorder the scan result chain in place. Validates the whole chain first and
 * leaves the buffer untouched on anything unexpected. */
static void reorder(uint8_t *buf, int len)
{
	if (len < SCAN_OFF_FIRST + BSS_FIXED_LEN || len > SCAN_BUF_LEN)
		return;
	uint32_t n = *(const uint32_t *)(buf + SCAN_OFF_COUNT);
	g_st.n_entries = n;
	if (n == 0 || n > MAX_ENTRIES)
		return;

	/* Static, not on the (already deep) driver stack: only one assisted join
	 * runs at a time (g_in_join + thread id gate). */
	static uint16_t off[MAX_ENTRIES], sz[MAX_ENTRIES], key[MAX_ENTRIES];
	static uint8_t  cls[MAX_ENTRIES];  /* 0 target, 1 same SSID, 2 other        */
	static uint8_t  used[MAX_ENTRIES];
	unsigned pos = SCAN_OFF_FIRST, nmatch = 0;
	for (uint32_t i = 0; i < n; i++) {
		if (pos + BSS_FIXED_LEN > (unsigned)len)
			return;
		unsigned l = *(const uint16_t *)(buf + pos + BSS_OFF_LEN);
		if (l < BSS_FIXED_LEN || (l & 3) || pos + l > (unsigned)len)
			return;
		const uint8_t *e = buf + pos;
		off[i] = (uint16_t)pos;
		sz[i]  = (uint16_t)l;
		key[i] = (uint16_t)rssi_key(e);
		used[i] = 0;
		if (!ssid_match(e))
			cls[i] = 2;
		else {
			nmatch++;
			cls[i] = (g_target_on && bssid_eq(e + BSS_OFF_BSSID, g_target)) ? 0 : 1;
		}
		pos += l;
	}
	unsigned total = pos - SCAN_OFF_FIRST;
	g_st.n_match = nmatch;
	if (nmatch == 0)
		return;

	/* Selection: lowest class, then lowest |RSSI|, ties keep stock order. */
	unsigned w = 0, order0 = 0, changed = 0;
	for (uint32_t k = 0; k < n; k++) {
		int best = -1;
		for (uint32_t i = 0; i < n; i++) {
			if (used[i]) continue;
			if (best < 0 || cls[i] < cls[best] ||
			    (cls[i] == cls[best] && cls[i] != 2 && key[i] < key[best]))
				best = (int)i;
		}
		used[best] = 1;
		if (k == 0) order0 = (unsigned)best;
		if ((uint32_t)best != k) changed = 1;
		memcpy(g_scratch + w, buf + off[best], sz[best]);
		w += sz[best];
	}

	const uint8_t *f = buf + SCAN_OFF_FIRST;      /* stock first entry          */
	const uint8_t *b = buf + off[order0];         /* our first entry            */
	memcpy(g_st.first_bssid, f + BSS_OFF_BSSID, 6);
	memcpy(g_st.best_bssid,  b + BSS_OFF_BSSID, 6);
	g_st.first_rssi = (uint8_t)*(const uint16_t *)(f + BSS_OFF_RSSI);
	g_st.best_rssi  = (uint8_t)*(const uint16_t *)(b + BSS_OFF_RSSI);
	g_st.first_ch   = f[BSS_OFF_CHANNEL];
	g_st.best_ch    = b[BSS_OFF_CHANNEL];
	g_st.target_hit = cls[order0] == 0;

	if (changed && w == total) {
		memcpy(buf + SCAN_OFF_FIRST, g_scratch, total);
		g_st.reordered++;
	}
}

static int hook_scan(void *ctx, uint8_t *buf, int len)
{
	int r = TAI_CONTINUE(int, g_scan_ref, ctx, buf, len);
	if (g_in_join && r >= 0 && buf && ksceKernelGetThreadId() == g_join_tid) {
		g_st.scans++;
		reorder(buf, len);
	}
	return r;
}

static int hook_join(void *ctx, uint8_t *req, int len)
{
	if (!g_armed || g_in_join)
		return TAI_CONTINUE(int, g_join_ref, ctx, req, len);
	g_armed = 0;                                  /* one-shot                   */
	g_st.armed = 0;
	if (ksceKernelGetSystemTimeWide() - g_arm_time > ARM_TTL_US) {
		g_st.expired++;
		return TAI_CONTINUE(int, g_join_ref, ctx, req, len);
	}
	if (!req || len < REQ_MIN_LEN)
		return TAI_CONTINUE(int, g_join_ref, ctx, req, len);

	uint32_t sl = *(const uint32_t *)(req + REQ_OFF_SSIDLEN);
	if (sl > 32)
		return TAI_CONTINUE(int, g_join_ref, ctx, req, len);
	g_ssid_len = sl;
	memcpy(g_ssid, req + REQ_OFF_SSID, sl);

	memcpy(g_st.orig_ch, req + REQ_OFF_CHANLIST, 4);
	memset(req + REQ_OFF_CHANLIST, 0, 4);         /* => full allowed-channel sweep */
	g_st.scans = 0;
	g_st.reordered = 0;
	g_st.n_entries = g_st.n_match = 0;
	g_st.target_hit = 0;
	memset(g_st.first_bssid, 0, 6);
	memset(g_st.best_bssid, 0, 6);
	memset(g_st.joined_bssid, 0, 6);

	g_join_tid = ksceKernelGetThreadId();
	g_in_join = 1;
	int r = TAI_CONTINUE(int, g_join_ref, ctx, req, len);
	g_in_join = 0;

	g_st.join_ret = r;
	if (r >= 0)
		memcpy(g_st.joined_bssid, req + REQ_OFF_RESULT + BSS_OFF_BSSID, 6);
	g_st.seq++;
	return r;
}

static int install_hooks(void)
{
	if (g_join_hook >= 0 && g_scan_hook >= 0)
		return WQ_OK;
	const WlanbtLayout *l = wlanbt_layout();
	SceUID modid;
	uintptr_t text;
	if (!l || wlanbt_text(l->scan + 8, &modid, &text) != WQ_OK)
		return WQ_ERR_RESOLVE;
	const uint32_t off_join = l->join, off_scan = l->scan;
	const volatile uint8_t *t = (const volatile uint8_t *)text;
	static const uint8_t join0[4] = { 0x2D, 0xE9, 0xF0, 0x4F };
	static const uint8_t scan0[8] = { 0xF8, 0xB5, 0x00, 0xF1, 0x08, 0x04, 0x05, 0x46 };
	for (int i = 0; i < 8; i++) {
		if ((i < 4 && t[off_join + i] != join0[i]) ||
		    t[off_scan + i] != scan0[i]) {
			WQ_KLOGW("roamassist: original bytes mismatch (join %02x%02x%02x%02x "
			         "scan %02x%02x%02x%02x) - unexpected SceWlanBt, refusing",
			         t[off_join], t[off_join + 1],
			         t[off_join + 2], t[off_join + 3],
			         t[off_scan], t[off_scan + 1],
			         t[off_scan + 2], t[off_scan + 3]);
			return WQ_ERR_RESOLVE;
		}
	}
	/* Scan hook first: it is inert until the join hook sets g_in_join. */
	if (g_scan_hook < 0) {
		g_scan_hook = taiHookFunctionOffsetForKernel(KERNEL_PID, &g_scan_ref, modid, 0,
		                                             off_scan, 1, hook_scan);
		if (g_scan_hook < 0) {
			WQ_KLOGW("roamassist: scan hook failed (0x%08x)", (unsigned)g_scan_hook);
			g_scan_hook = -1;
			return WQ_ERR_RESOLVE;
		}
	}
	if (g_join_hook < 0) {
		g_join_hook = taiHookFunctionOffsetForKernel(KERNEL_PID, &g_join_ref, modid, 0,
		                                             off_join, 1, hook_join);
		if (g_join_hook < 0) {
			WQ_KLOGW("roamassist: join hook failed (0x%08x)", (unsigned)g_join_hook);
			g_join_hook = -1;
			return WQ_ERR_RESOLVE;
		}
	}
	g_st.hooked = 1;
	WQ_KLOGI("roamassist: hooked join +0x%x + scan +0x%x", (unsigned)off_join,
	         (unsigned)off_scan);
	return WQ_OK;
}

int roamassist_arm(unsigned flags, const uint8_t bssid[6])
{
	if (!(flags & WQ_RA_ARM)) {
		g_armed = 0;
		g_st.armed = 0;
		return WQ_OK;
	}
	int r = install_hooks();
	if (r != WQ_OK)
		return r;
	g_armed = 0;
	g_target_on = 0;
	if ((flags & WQ_RA_TARGET) && bssid) {
		for (int i = 0; i < 6; i++) g_target[i] = bssid[i];
		g_target_on = 1;
	}
	g_arm_time = ksceKernelGetSystemTimeWide();
	__sync_synchronize();
	g_armed = 1;
	g_st.armed = 1;
	WQ_KLOGI("roamassist: armed for next join (target %s)",
	         g_target_on ? "bssid" : "strongest");
	return WQ_OK;
}

int roamassist_get_status(WqRoamAssistStatus *out)
{
	if (!out)
		return WQ_ERR_ARG;
	memcpy(out, &g_st, sizeof(*out));
	out->armed = g_armed ? 1 : 0;
	return WQ_OK;
}

void roamassist_fini(void)
{
	g_armed = 0;
	if (g_join_hook >= 0) { taiHookReleaseForKernel(g_join_hook, g_join_ref); g_join_hook = -1; }
	if (g_scan_hook >= 0) { taiHookReleaseForKernel(g_scan_hook, g_scan_ref); g_scan_hook = -1; }
	g_st.hooked = 0;
}
