/*
 * hud.c - draw the Wi-Fi HUD over the live framebuffer.
 *
 * Technique (the standard taiHEN overlay idiom): hook SceDisplay's
 * sceDisplaySetFrameBuf export. The caller hands us the framebuffer base,
 * pitch, pixel format and size every frame; we let the original run, then blit
 * our text into the same buffer before the display controller scans it out.
 *
 * This is strictly read + overdraw: no Wi-Fi state is touched here. The data
 * comes from the user module via hud_set_state().
 *
 * Some titles render to framebuffers that should not be written from the
 * kernel, so drawing is guarded by a plausibility check on pitch and size and
 * is simply skipped if the buffer looks wrong, rather than risk a bad write.
 */
#include <psp2kern/kernel/threadmgr.h>
#include <psp2kern/kernel/sysmem.h>    /* ksceKernelMemcpyUserToKernel         */
#include <psp2kern/kernel/sysclib.h>   /* kernel memcpy/memset                 */
#include <psp2kern/kernel/debug.h>     /* ksceDebugPrintf (one-shot hot-path)  */
#include <psp2kern/display.h>
#include <taihen.h>

#include "hud.h"
#include "hud_i18n.h"
#include "font8x8.h"
#include "klog.h"
#include "ht40patch.h"                         /* ht40patch_active            */
#include "../include/wifiturbo_kernel_api.h"   /* WQ_HUD_STATUS_* bits           */

#define HUD_SCALE      2          /* 8x8 glyph drawn at 2x = 16px tall        */
#define HUD_MARGIN_X   24
#define HUD_MARGIN_Y   24
#define HUD_FG         0xFFFFFFFFu /* opaque white (ABGR8888)                 */
#define HUD_SHADOW     0xFF000000u /* opaque black drop shadow                */

static SceUID   g_hook = -1;
static tai_hook_ref_t g_ref;
static int      g_enabled = 0;
static int      g_draw_armed = 0;   /* framebuffer PIXEL-WRITE gate - OFF default */
static int      g_pos = WQ_HUD_POS_DEFAULT;   /* screen corner; WQ_HUD_POS_*   */

/* In-game HUD off (default): draw only into frames set by the registered
 * system processes, so a game's or app's own frames are left alone. Each pid is
 * one aligned word, written once by its process, so the hook reads it unlocked. */
static int      g_ingame = 0;
static volatile SceUID g_sys_pid[WQ_HUD_PROC_COUNT] = { -1, -1 };

/* Double-buffered HUD state so the hook never reads a torn struct. */
static WqHudState g_state[2];
static volatile int g_front = 0;
static SceUID   g_state_mtx = -1;

/* ---- framebuffer text blitter ------------------------------------------ */

static inline void put_px(uint32_t *fb, int pitch, int w, int h,
                          int x, int y, uint32_t color)
{
	if (x < 0 || y < 0 || x >= w || y >= h)
		return;
	fb[y * pitch + x] = color;
}

static void draw_char(uint32_t *fb, int pitch, int w, int h,
                      int ox, int oy, char ch, uint32_t color)
{
	if ((unsigned char)ch < WQ_FONT_FIRST || (unsigned char)ch > WQ_FONT_LAST)
		return;
	const unsigned char *g = wq_font8x8[(unsigned char)ch - WQ_FONT_FIRST];
	for (int row = 0; row < 8; row++) {
		unsigned char bits = g[row];
		for (int col = 0; col < 8; col++) {
			if (!(bits & (1u << col)))
				continue;
			for (int sy = 0; sy < HUD_SCALE; sy++)
				for (int sx = 0; sx < HUD_SCALE; sx++)
					put_px(fb, pitch, w, h,
					       ox + col * HUD_SCALE + sx,
					       oy + row * HUD_SCALE + sy, color);
		}
	}
}

static void draw_text(uint32_t *fb, int pitch, int w, int h,
                      int ox, int oy, const char *s)
{
	int x = ox;
	for (; *s; s++) {
		if (*s == '\n') { oy += (8 * HUD_SCALE) + 4; x = ox; continue; }
		/* 1px drop shadow for legibility over any background. */
		draw_char(fb, pitch, w, h, x + 1, oy + 1, *s, HUD_SHADOW);
		draw_char(fb, pitch, w, h, x, oy, *s, HUD_FG);
		x += (8 * HUD_SCALE);
	}
}

/* Tiny, allocation-free formatting helpers (no libc printf in kernel). */
static char *put_str(char *p, char *end, const char *s)
{
	while (*s && p < end - 1) *p++ = *s++;
	return p;
}
static char *put_int(char *p, char *end, int v)
{
	char tmp[12];
	int n = 0, neg = v < 0;
	unsigned uv = neg ? (unsigned)(-v) : (unsigned)v;
	if (!uv) tmp[n++] = '0';
	while (uv) { tmp[n++] = '0' + (uv % 10); uv /= 10; }
	if (neg && p < end - 1) *p++ = '-';
	while (n && p < end - 1) *p++ = tmp[--n];
	return p;
}
static char *put_hex2(char *p, char *end, unsigned v)
{
	const char *h = "0123456789ABCDEF";
	if (p < end - 1) *p++ = h[(v >> 4) & 0xF];
	if (p < end - 1) *p++ = h[v & 0xF];
	return p;
}

static const char *state_name(int lang, int st)
{
	switch (st) {
	case WQ_NETCTL_DISCONNECTED: return hud_tr(lang, HTR_DOWN);
	case WQ_NETCTL_CONNECTING:   return hud_tr(lang, HTR_CONNECTING);
	case WQ_NETCTL_FINALIZING:   return "DHCP";               /* acronym: universal */
	case WQ_NETCTL_CONNECTED:    return hud_tr(lang, HTR_UP);
	default:                     return "?";
	}
}

#define HUD_MAX_LINES  5
#define HUD_LINE_CHARS  96

static void render(uint32_t *fb, int pitch, int w, int h)
{
	int idx = g_front;
	const WqHudState *s = &g_state[idx];
	if (!s->valid)
		return;

	/* Build every visible line first, so the whole block can be anchored to the
	 * chosen corner (we need its height and widest line for right/bottom
	 * alignment). Each entry is one physical line; the text itself is still
	 * left-aligned within the block, exactly as the fixed top-left HUD was. */
	char  lines[HUD_MAX_LINES][HUD_LINE_CHARS];
	int   n = 0, maxlen = 0;
	char *p, *end;

	/* line 1: channel width offered + state + SSID */
	p = lines[n]; end = p + HUD_LINE_CHARS;
	p = put_str(p, end, ht40patch_active() ? "40MHz WiFi " : "20MHz WiFi ");
	p = put_str(p, end, state_name(s->lang, s->netctl_state));
	p = put_str(p, end, " ");
	p = put_str(p, end, s->ssid[0] ? s->ssid : "-");
	*p = 0;
	if ((int)(p - lines[n]) > maxlen) maxlen = (int)(p - lines[n]);
	n++;

	/* line 2: RSSI dBm/%, SNR, channel */
	p = lines[n]; end = p + HUD_LINE_CHARS;
	p = put_str(p, end, "RSSI ");
	p = put_int(p, end, s->rssi_dbm);
	p = put_str(p, end, "dBm ");
	p = put_int(p, end, (int)s->rssi_pct);
	p = put_str(p, end, "%");
	if (s->snr_valid) {                    /* hidden until a real reading     */
		p = put_str(p, end, "  SNR ");
		p = put_int(p, end, s->snr_db);
	}
	p = put_str(p, end, "  CH ");
	p = put_int(p, end, (int)s->channel);
	*p = 0;
	if ((int)(p - lines[n]) > maxlen) maxlen = (int)(p - lines[n]);
	n++;

	/* line 3: BSSID */
	p = lines[n]; end = p + HUD_LINE_CHARS;
	p = put_str(p, end, "BSSID ");
	for (int i = 0; i < 6; i++) {
		p = put_hex2(p, end, s->bssid[i]);
		if (i < 5 && p < end - 1) *p++ = ':';
	}
	*p = 0;
	if ((int)(p - lines[n]) > maxlen) maxlen = (int)(p - lines[n]);
	n++;

	/* line 4: profile + last failure reason (human-ish) */
	p = lines[n]; end = p + HUD_LINE_CHARS;
	p = put_str(p, end, hud_tr(s->lang, HTR_PROFILE)); p = put_str(p, end, " ");
	p = put_str(p, end, s->profile[0] ? s->profile : "-");
	if (s->last_result) {
		p = put_str(p, end, "  "); p = put_str(p, end, hud_tr(s->lang, HTR_LAST)); p = put_str(p, end, " ");
		switch ((unsigned)s->last_result) {
		case WQ_REASON_DEAUTH:       p = put_str(p, end, "DEAUTH"); break;
		case WQ_REASON_BEACON_LOST:  p = put_str(p, end, "BCNLOST"); break;
		case WQ_REASON_DHCP_TIMEOUT: p = put_str(p, end, "DHCP-TO"); break;
		default:
			p = put_str(p, end, "0x");
			p = put_hex2(p, end, ((unsigned)s->last_result >> 8) & 0xFF);
			p = put_hex2(p, end, (unsigned)s->last_result & 0xFF);
			break;
		}
	}
	*p = 0;
	if ((int)(p - lines[n]) > maxlen) maxlen = (int)(p - lines[n]);
	n++;

	/* line 5: transient notice (safety harness etc.) - only when present */
	if (s->notice[0]) {
		p = lines[n]; end = p + HUD_LINE_CHARS;
		p = put_str(p, end, s->notice);
		*p = 0;
		if ((int)(p - lines[n]) > maxlen) maxlen = (int)(p - lines[n]);
		n++;
	}

	/* Anchor the block. Glyphs are 8 px wide at 1x, so a column is 8*HUD_SCALE;
	 * a line is one glyph tall plus the 4 px inter-line gap used below. */
	int line_h = (8 * HUD_SCALE) + 4;
	int box_w  = maxlen * 8 * HUD_SCALE;
	int box_h  = n * line_h;
	int right  = (g_pos == WQ_HUD_POS_TOP_RIGHT || g_pos == WQ_HUD_POS_BOTTOM_RIGHT);
	int bottom = (g_pos == WQ_HUD_POS_BOTTOM_LEFT || g_pos == WQ_HUD_POS_BOTTOM_RIGHT);
	int x = right  ? w - HUD_MARGIN_X - box_w : HUD_MARGIN_X;
	int y = bottom ? h - HUD_MARGIN_Y - box_h : HUD_MARGIN_Y;
	if (x < 0) x = 0;
	if (y < 0) y = 0;

	for (int i = 0; i < n; i++, y += line_h)
		draw_text(fb, pitch, w, h, x, y, lines[i]);
}

/* ---- the sceDisplaySetFrameBuf hook ------------------------------------ */

/* pParam->base is a user-space CDRAM VA (A8B8G8R8 960x544, confirmed on
 * device). Writing it from this kernel hook faults under the default ARM
 * domain permissions. Open the Domain Access Control Register to all-manager
 * for the duration of the blit, then restore it - the standard Vita kernel-
 * overlay technique. Returns the previous DACR to restore. */
static inline uint32_t hud_dacr_open(void)
{
	uint32_t prev;
	__asm__ volatile("mrc p15, 0, %0, c3, c0, 0" : "=r"(prev));
	__asm__ volatile("mcr p15, 0, %0, c3, c0, 0" :: "r"(0xFFFFFFFFu));
	__asm__ volatile("isb");
	return prev;
}
static inline void hud_dacr_restore(uint32_t prev)
{
	__asm__ volatile("mcr p15, 0, %0, c3, c0, 0" :: "r"(prev));
	__asm__ volatile("isb");
}

/* Hook ksceDisplaySetFrameBufInternal(head, index, pParam, sync) - the
 * kernel funnel every set-framebuffer call passes through, so the overlay is
 * system-wide. We draw only for the main panel (head 0). */
static int hook_set_framebuf(int head, int index,
                             const SceDisplayFrameBuf *pParam, int sync)
{
	int ret = TAI_CONTINUE(int, g_ref, head, index, pParam, sync);

	if (head != 0 || !pParam || !pParam->base)
		return ret;

	/* Plausibility guard: only draw into a sane 32bpp linear buffer. */
	int w = (int)pParam->width, h = (int)pParam->height,
	    pitch = (int)pParam->pitch;
	if (w <= 0 || h <= 0 || pitch < w || w > 1920 || h > 1088)
		return ret;

	/* The read-only HUD draws only when enabled AND the draw is armed. */
	if (!g_enabled || !g_draw_armed)
		return ret;

	/* Called in the context of the process setting the frame. */
	if (!g_ingame) {
		SceUID pid = ksceKernelGetProcessId();
		if (pid != g_sys_pid[WQ_HUD_PROC_SHELL] && pid != g_sys_pid[WQ_HUD_PROC_SETTINGS])
			return ret;
	}

	/* Open the DACR so this kernel context may write the user-mapped CDRAM,
	 * blit, then restore it. */
	uint32_t prev_dacr = hud_dacr_open();
	render((uint32_t *)pParam->base, pitch, w, h);
	hud_dacr_restore(prev_dacr);
	return ret;
}

int hud_init(void)
{
	memset(g_state, 0, sizeof(g_state));
	g_state_mtx = ksceKernelCreateMutex("WqHudState", 0, 0, NULL);
	if (g_state_mtx < 0)
		return WQ_ERR_NOT_READY;

	/* Hook ksceDisplaySetFrameBufInternal in SceDisplay. NIDs from the VitaSDK
	 * 3.60 db (vita-headers db/360/SceDisplay.yml):
	 *   SceDisplayForDriver library    = 0x9FED47AC
	 *   ksceDisplaySetFrameBufInternal = 0x16466675 */
	g_hook = taiHookFunctionExportForKernel(KERNEL_PID, &g_ref,
	                                        "SceDisplay",
	                                        0x9FED47AC, /* SceDisplayForDriver  */
	                                        0x16466675, /* …SetFrameBufInternal */
	                                        hook_set_framebuf);
	if (g_hook < 0) {
		WQ_KLOGW("hud_init: taiHookFunctionExportForKernel failed (%d)", g_hook);
		return WQ_ERR_RESOLVE;
	}
	g_draw_armed = 0;
	WQ_KLOGI("hud_init: sceDisplaySetFrameBuf hook installed (uid=%d)", g_hook);
	return WQ_OK;
}

void hud_fini(void)
{
	if (g_hook >= 0) {
		taiHookReleaseForKernel(g_hook, g_ref);
		g_hook = -1;
	}
	if (g_state_mtx >= 0) {
		ksceKernelDeleteMutex(g_state_mtx);
		g_state_mtx = -1;
	}
}

int hud_set_enabled(int enabled)
{
	g_enabled = enabled ? 1 : 0;
	return WQ_OK;
}

/* Read-only status for the user-side `selftest` probe. */
int hud_get_status(void)
{
	int s = 0;
	if (g_hook >= 0)   s |= WQ_HUD_STATUS_HOOK;
	if (g_enabled)     s |= WQ_HUD_STATUS_ENABLED;
	if (g_draw_armed)  s |= WQ_HUD_STATUS_DRAW_ARMED;
	return s;
}

/* Arm/disarm the framebuffer pixel-write (see the DRAW GATE note in the hook). */
int hud_set_draw_armed(int on)
{
	g_draw_armed = on ? 1 : 0;
	WQ_KLOGI("hud: framebuffer draw %s", g_draw_armed ? "ARMED" : "disarmed");
	return WQ_OK;
}

/* Pick the screen corner the overlay draws in. Read by render() next frame; no
 * lock needed - a single aligned int, and a torn read at worst draws one frame
 * in the previous corner. Ignores out-of-range values (fail-safe). */
int hud_set_pos(int pos)
{
	if (pos < WQ_HUD_POS_TOP_LEFT || pos > WQ_HUD_POS_BOTTOM_RIGHT)
		return WQ_ERR_ARG;
	g_pos = pos;
	WQ_KLOGI("hud: position -> %d", pos);
	return WQ_OK;
}

/* Syscall context: the current process is the caller registering itself. */
int hud_register_proc(int role)
{
	if (role < 0 || role >= WQ_HUD_PROC_COUNT)
		return WQ_ERR_ARG;
	SceUID pid = ksceKernelGetProcessId();
	g_sys_pid[role] = pid;
	WQ_KLOGI("hud: system process %d = pid 0x%08x", role, (unsigned)pid);
	return WQ_OK;
}

int hud_set_ingame(int on)
{
	g_ingame = on ? 1 : 0;
	WQ_KLOGI("hud: in-game HUD %s", g_ingame ? "on" : "off");
	return WQ_OK;
}

/* Copy the state in from user memory into the back buffer, then flip. */
int hud_set_state(const WqHudState *state)
{
	if (!state)
		return WQ_ERR_ARG;
	if (g_state_mtx < 0)
		return WQ_ERR_NOT_READY;

	WqHudState tmp;
	if (ksceKernelMemcpyUserToKernel(&tmp, state, sizeof(tmp)) < 0)
		return WQ_ERR_ARG;

	ksceKernelLockMutex(g_state_mtx, 1, NULL);
	int back = g_front ^ 1;
	memcpy(&g_state[back], &tmp, sizeof(tmp));
	g_front = back;
	ksceKernelUnlockMutex(g_state_mtx, 1);
	return WQ_OK;
}
