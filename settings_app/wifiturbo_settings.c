/*
 * wifiturbo_settings.c - Settings-app (NPXS10015) front-end for wifiturbo.
 *
 * Follows HENkaku's proven 3.60 pattern (plugin/user.c): hook sceKernelLoadStartModule in
 * SceSettings to wait for system_settings_core.suprx, then hook
 * scePafMiscLoadXmlLayout (inject our page), and the SceRegMgr get/set/info in
 * SceSystemSettingsCore (back our toggles with a private /CONFIG/WIFITURBO
 * namespace). State + actions cross to the resident SceShell core via the
 * SceWifiTurbo kernel mailbox (wqUiReadState / wqUiSubmit, see uibridge.c).
 *
 * This module is loaded with  *NPXS10015  in ur0:tai/config.txt.
 *
 * All hooks are import hooks (passthrough), so if anything doesn't match the
 * page simply doesn't appear; nothing crashes.
 */
#include <psp2/kernel/modulemgr.h>
#include <psp2/kernel/processmgr.h>   /* sceKernelGetProcessTimeWide (state re-read) */
#include <taihen.h>

#include "../include/wifiturbo_kernel_api.h"   /* WqUiState, wqUi* syscalls     */
#include "../user/log.h"                       /* WQ_LOG* (shared log file)     */
#include "wifiturbo_i18n.h"                    /* localised UI strings          */
#include "../user/macaddr.h"                   /* wq_mac_parse (address field)  */

/* ---- tiny local string helpers (no paf libc dependency) ---------------- */
static int s_len(const char *s){ int n=0; while(s&&s[n]) n++; return n; }
static int s_eqn(const char *a, const char *b, int n){
	for(int i=0;i<n;i++){ if(a[i]!=b[i]) return 0; if(!a[i]) return 1; } return 1; }
static const char *mem_find(const char *h, int hn, const char *n, int nn){
	for(int i=0;i+nn<=hn;i++){ int j=0; while(j<nn&&h[i+j]==n[j]) j++; if(j==nn) return h+i; }
	return 0; }
static void s_cpy(char *d, const char *s, int n){ for(int i=0;i<n;i++) d[i]=s[i]; }

/* ---- the Wi-Fi Turbo page (embedded) ----------------------------------- */
/* Hosted in the network_settings_plugin context. Registry-backed rows reflect
 * state via the RegMgr hooks below; <link> is spliced into the Network page. */
/* A navigable sub-group spliced into the Network page's own setting_list.
 * Literal titles (HENkaku's menu uses literal titles too, so no GetText hook is
 * needed). Registry-backed on /CONFIG/WIFITURBO, intercepted by the RegMgr hooks
 * below. No made-up child file - the Settings app can't navigate to a file it
 * doesn't know (that was the "can't enter" bug); a nested setting_list is a real
 * sub-page it can enter. */
static const char WT_HEAD[] =
"<setting_list id=\"wifiturbo_settings\" title=\"wt_t_title\" icon=\"tex_spanner\">";

/* Informational row, spliced in only while Bluetooth is on: a button with no
 * handler, so it shows a title and a '?' hint but has no control to change.
 * (A toggle_switch can't be made read-only from the layout.) */
static const char WT_BTWARN[] =
 "<button id=\"wt_btwarn\" title=\"wt_t_btwarn\" icon=\"tex_spanner\" hint=\"wt_h_btwarn\"/>";

static const char WT_ROWS[] =
 "<toggle_switch id=\"wt_keepwifi\" title=\"wt_t_keep\" icon=\"tex_spanner\" hint=\"wt_h_keepwifi\" key=\"/CONFIG/WIFITURBO/keepwifi\"/>"
 "<list id=\"wt_hud\" title=\"wt_t_hud\" icon=\"tex_spanner\" hint=\"wt_h_hud\" key=\"/CONFIG/WIFITURBO/hud\">"
  "<list_item id=\"wt_hd0\" title=\"wt_i_hudoff\" value=\"0\"/>"
  "<list_item id=\"wt_hd2\" title=\"wt_i_hudcompact\" value=\"2\"/>"
  "<list_item id=\"wt_hd1\" title=\"wt_i_hudextended\" value=\"1\"/>"
 "</list>"
 "<list id=\"wt_hudpos\" title=\"wt_t_hudpos\" icon=\"tex_spanner\" hint=\"wt_h_hudpos\" key=\"/CONFIG/WIFITURBO/hudpos\">"
  "<list_item id=\"wt_pos0\" title=\"wt_i_pos0\" value=\"0\"/>"
  "<list_item id=\"wt_pos1\" title=\"wt_i_pos1\" value=\"1\"/>"
  "<list_item id=\"wt_pos2\" title=\"wt_i_pos2\" value=\"2\"/>"
  "<list_item id=\"wt_pos3\" title=\"wt_i_pos3\" value=\"3\"/>"
 "</list>"
 "<toggle_switch id=\"wt_hudgame\" title=\"wt_t_hudgame\" icon=\"tex_spanner\" hint=\"wt_h_hudgame\" key=\"/CONFIG/WIFITURBO/hudgame\"/>"
 "<toggle_switch id=\"wt_autorecon\" title=\"wt_t_autorecon\" icon=\"tex_spanner\" hint=\"wt_h_autorecon\" key=\"/CONFIG/WIFITURBO/autoreconnect\"/>"
 "<toggle_switch id=\"wt_roam\" title=\"wt_t_roam\" icon=\"tex_spanner\" hint=\"wt_h_roam\" key=\"/CONFIG/WIFITURBO/roam\"/>"
 "<toggle_switch id=\"wt_ht40\" title=\"wt_t_ht40\" icon=\"tex_spanner\" hint=\"wt_h_ht40\" key=\"/CONFIG/WIFITURBO/ht40\"/>"
 "<list id=\"wt_profile\" title=\"wt_t_profile\" icon=\"tex_spanner\" hint=\"wt_h_profile\" key=\"/CONFIG/WIFITURBO/profile\">"
  "<list_item id=\"wt_p0\" title=\"wt_i_maxperf\" value=\"0\"/>"
  "<list_item id=\"wt_p1\" title=\"wt_i_balanced\" value=\"1\"/>"
  "<list_item id=\"wt_p2\" title=\"wt_i_battery\" value=\"2\"/>"
 "</list>"
 "<list id=\"wt_rxring\" title=\"wt_t_rxring\" icon=\"tex_spanner\" hint=\"wt_h_rxring\" key=\"/CONFIG/WIFITURBO/rxring\">"
  "<list_item id=\"wt_r0\" title=\"wt_i_stock\" value=\"0\"/>"
  "<list_item id=\"wt_r1\" title=\"wt_i_enhanced\" value=\"1\"/>"
  "<list_item id=\"wt_r2\" title=\"wt_i_maximum\" value=\"2\"/>"
 "</list>"
 "<list id=\"wt_macmode\" title=\"wt_t_macmode\" icon=\"tex_spanner\" hint=\"wt_h_macmode\" key=\"/CONFIG/WIFITURBO/macmode\">"
  "<list_item id=\"wt_mm0\" title=\"wt_i_stock\" value=\"0\"/>"
  "<list_item id=\"wt_mm1\" title=\"wt_i_custom\" value=\"1\"/>"
 "</list>"
 "<text_field id=\"wt_macaddr\" title=\"wt_t_macaddr\" icon=\"tex_spanner\" hint=\"wt_h_macaddr\" key=\"/CONFIG/WIFITURBO/macaddr\" keyboard_type=\"basic_latin\" max_length=\"17\" no_space=\"on\"/>"
 "<button id=\"wt_macrand\" title=\"wt_t_macrand\" icon=\"tex_spanner\" hint=\"wt_h_macrand\"/>"
 "<list id=\"wt_maccur\" title=\"wt_t_maccur\" icon=\"tex_spanner\" hint=\"wt_h_maccur\" key=\"/CONFIG/WIFITURBO/maccur\">"
  "<list_item id=\"wt_mc0\" title=\"wt_i_maccur\" value=\"0\"/>"
 "</list>"
 "<toggle_switch id=\"wt_devmode\" title=\"wt_t_devmode\" icon=\"tex_spanner\" hint=\"wt_h_devmode\" key=\"/CONFIG/WIFITURBO/devmode\"/>"
"</setting_list>";

/* Help text for the '?' buttons. The Settings renderer resolves a row's hint=
 * attribute through scePafToplevelGetText and wants a UTF-16 string; literal
 * text in the XML shows blank. We hook GetText and, for our hint ids, convert
 * the localised UTF-8 help string into UTF-16 in a shared buffer (C99-safe - no
 * char16_t/u"" needed; only one '?' modal is shown at a time). */
typedef unsigned short wt_u16;
static wt_u16 g_hintbuf[320];


/* Resolve a title/item/hint id to the localised UTF-16 string for the current
 * system language (wifiturbo_i18n.{h,c}; English(US) fallback). Every visible
 * string - titles, list items and hints - carries an id and comes through here
 * via the GetText hook, so the whole sub-menu follows the system language. */
static void wt_mac_current(char *out, int cap);

static const wt_u16 *WT_TEXT(const char *id)
{
	int wtid = -1;
	#define M(name, i) if (s_eqn(id, name, sizeof(name))) wtid = (i);
	M("wt_t_title",     WT_I18N_TITLE)
	M("wt_t_keep",      WT_I18N_ROW_KEEP)
	M("wt_t_hud",       WT_I18N_ROW_HUD)
	M("wt_t_hudpos",    WT_I18N_ROW_HUDPOS)
	M("wt_t_hudgame",   WT_I18N_ROW_HUDGAME)
	M("wt_t_autorecon", WT_I18N_ROW_RECONNECT)
	M("wt_t_roam",      WT_I18N_ROW_ROAM)
	M("wt_t_ht40",      WT_I18N_ROW_HT40)
	M("wt_t_profile",   WT_I18N_ROW_POWER)
	M("wt_i_maxperf",   WT_I18N_ITEM_MAXPERF)
	M("wt_i_balanced",  WT_I18N_ITEM_BALANCED)
	M("wt_i_battery",   WT_I18N_ITEM_BATTERY)
	M("wt_i_pos0",      WT_I18N_ITEM_POS_TL)
	M("wt_i_pos1",      WT_I18N_ITEM_POS_TR)
	M("wt_i_pos2",      WT_I18N_ITEM_POS_BL)
	M("wt_i_pos3",      WT_I18N_ITEM_POS_BR)
	M("wt_h_keepwifi",  WT_I18N_HINT_KEEP)
	M("wt_h_hud",       WT_I18N_HINT_HUD)
	M("wt_h_hudpos",    WT_I18N_HINT_HUDPOS)
	M("wt_h_hudgame",   WT_I18N_HINT_HUDGAME)
	M("wt_h_autorecon", WT_I18N_HINT_RECONNECT)
	M("wt_h_roam",      WT_I18N_HINT_ROAM)
	M("wt_h_ht40",      WT_I18N_HINT_HT40)
	M("wt_h_profile",   WT_I18N_HINT_POWER)
	M("wt_t_btwarn",    WT_I18N_ROW_BTWARN)
	M("wt_h_btwarn",    WT_I18N_HINT_BTWARN)
	M("wt_t_devmode",   WT_I18N_ROW_DEVMODE)
	M("wt_h_devmode",   WT_I18N_HINT_DEVMODE)
	M("wt_t_rxring",    WT_I18N_ROW_RXRING)
	M("wt_i_stock",     WT_I18N_ITEM_STOCK)
	M("wt_i_enhanced",  WT_I18N_ITEM_ENHANCED)
	M("wt_i_maximum",   WT_I18N_ITEM_MAXIMUM)
	M("wt_h_rxring",    WT_I18N_HINT_RXRING)
	M("wt_t_macmode",   WT_I18N_ROW_MACMODE)
	M("wt_i_custom",    WT_I18N_ITEM_CUSTOM)
	M("wt_h_macmode",   WT_I18N_HINT_MACMODE)
	M("wt_t_macaddr",   WT_I18N_ROW_MACADDR)
	M("wt_h_macaddr",   WT_I18N_HINT_MACADDR)
	M("wt_t_macrand",   WT_I18N_ROW_MACRAND)
	M("wt_h_macrand",   WT_I18N_HINT_MACRAND)
	M("wt_t_maccur",    WT_I18N_ROW_MACCUR)
	M("wt_i_hudoff",    WT_I18N_ITEM_HUD_OFF)
	M("wt_i_hudcompact", WT_I18N_ITEM_HUD_COMPACT)
	M("wt_i_hudextended", WT_I18N_ITEM_HUD_EXTENDED)
	M("wt_h_maccur",    WT_I18N_HINT_MACCUR)
	#undef M
	/* The read-only row's one item is the address in use: shown inline. */
	if (s_eqn(id, "wt_i_maccur", 12)) {
		char t[18];
		wt_mac_current(t, sizeof(t));
		int n = 0;
		while (t[n] && n < (int)(sizeof(g_hintbuf) / sizeof(g_hintbuf[0])) - 1) {
			g_hintbuf[n] = (wt_u16)(unsigned char)t[n];
			n++;
		}
		g_hintbuf[n] = 0;
		return g_hintbuf;
	}
	if (wtid < 0)
		return 0;
	wq_i18n_to_u16(wq_i18n(wtid), g_hintbuf,
	               (int)(sizeof(g_hintbuf) / sizeof(g_hintbuf[0])));
	return g_hintbuf;
}

/* ---- hooks ------------------------------------------------------------- */
static SceUID g_h_load=-1, g_h_xml=-1, g_h_get=-1, g_h_set=-1, g_h_info=-1, g_h_text=-1;
static SceUID g_h_gets=-1, g_h_sets=-1, g_h_funcs=-1;
static tai_hook_ref_t r_load, r_xml, r_get, r_set, r_info, r_text, r_gets, r_sets, r_funcs;
static int g_installed = 0;   /* install the UI hooks once */

static char g_pagebuf[16384];  /* plugin-owned spliced layout buffer */
static int  g_mac_refresh_pending;   /* see wt_mac_refresh_current */

extern int sceRegMgrGetKeyInt(const char *category, const char *name, int *value);

/* The page fragment for this visit: head + (Bluetooth reminder if BT is on) +
 * rows. Rebuilt each time the Network page loads, so it follows the current
 * Bluetooth setting. Returns the length (no NUL). */
static char g_group[sizeof(WT_HEAD) + sizeof(WT_BTWARN) + sizeof(WT_ROWS)];
static int build_group(void)
{
	int bt = 0, n = 0;
	if (sceRegMgrGetKeyInt("/CONFIG/BT", "bt_enable", &bt) < 0)
		bt = 0;
	s_cpy(g_group + n, WT_HEAD, sizeof(WT_HEAD) - 1);  n += sizeof(WT_HEAD) - 1;
	if (bt) { s_cpy(g_group + n, WT_BTWARN, sizeof(WT_BTWARN) - 1); n += sizeof(WT_BTWARN) - 1; }
	s_cpy(g_group + n, WT_ROWS, sizeof(WT_ROWS) - 1);  n += sizeof(WT_ROWS) - 1;
	return n;
}

static int is_wt(const char *cat){ return cat && s_eqn(cat,"/CONFIG/WIFITURBO",17); }

/* scePafMiscLoadXmlLayout(out, buf, size, a4): serve our page, or splice our
 * link into the Network page before its </setting_list>. */
static int xml_hook(void *out, void *buf, int size, void *a4)
{
	const char *s = (const char *)buf;
	/* PRECISE match: the Network page's OWN layout (its plugin="" attr), never
	 * the top-level page that merely links to it. */
	if (size > 0 && size < (int)sizeof(g_pagebuf) - (int)sizeof(g_group) &&
	    mem_find(s, size, "plugin=\"network_settings_plugin\"", 32)) {
		WQ_LOGI("[SET] network page (size=%d)", size);
		g_mac_refresh_pending = 0;         /* never refresh rows mid-build */

		/* Splice before the LAST </setting_list> = the page's own closing tag,
		 * so our group is one entry at the end of the Network list. (The exact
		 * "2nd position" insert was fragile and duplicated the entry into
		 * sub-menus - reverted to this proven single-entry splice.) */
		const char *ins = 0, *p2 = s;
		for (;;) {
			const char *f = mem_find(p2, size-(int)(p2-s), "</setting_list>", 15);
			if (!f) break;
			ins = f; p2 = f + 15;
		}
		WQ_LOGI("[SET] network page: close-tag %s", ins ? "found - splicing" : "NOT found");
		if (ins) {
			int head = (int)(ins - s), ll = build_group();
			s_cpy(g_pagebuf, s, head);
			s_cpy(g_pagebuf + head, g_group, ll);
			s_cpy(g_pagebuf + head + ll, ins, size - head);
			buf = g_pagebuf; size = size + ll;
		}
	}
	return TAI_CONTINUE(int, r_xml, out, buf, size, a4);
}

/* RegMgr GET/SET/INFO for our private namespace -> the resident core. */
/* Local state cache, so a toggle made here is remembered immediately on
 * re-read (the resident core publishes ~1 s later). Re-read from the core when
 * the page asks again after a while - e.g. on returning to the page after a
 * change in the quick menu - except just after a change made here, which the
 * published state may not show yet. */
#define WT_RESEED_US      1500000ull   /* re-read if the copy is older than this */
#define WT_SET_SETTLE_US  3000000ull   /* ...but not this soon after our own set  */
static WqUiState g_ui;
static int       g_ui_init = 0;
static SceUInt64 g_seed_t, g_set_t;

static int *wt_field(const char *name)
{
	if (s_eqn(name,"keepwifi",9))       return &g_ui.keepwifi;
	if (s_eqn(name,"hudpos",7))         return &g_ui.hudpos;
	if (s_eqn(name,"hudgame",8))        return &g_ui.hudgame;
	if (s_eqn(name,"hud",4))            return &g_ui.hud;
	if (s_eqn(name,"autoreconnect",14)) return &g_ui.autoreconnect;
	if (s_eqn(name,"roam",5))           return &g_ui.roam;
	if (s_eqn(name,"ht40",5))           return &g_ui.ht40;
	if (s_eqn(name,"profile",8))        return &g_ui.profile;
	if (s_eqn(name,"devmode",8))        return &g_ui.devmode;
	if (s_eqn(name,"rxring",7))         return &g_ui.rxring;
	if (s_eqn(name,"macmode",8))        return &g_ui.macmode;
	return 0;
}
static void wt_seed(void)
{
	SceUInt64 now = sceKernelGetProcessTimeWide();
	if (g_ui_init && (now - g_seed_t < WT_RESEED_US || now - g_set_t < WT_SET_SETTLE_US))
		return;
	if (wqUiReadState(&g_ui) >= 0)
		g_ui_init = 1;
	g_seed_t = now;
}

/* ---- MAC address rows ------------------------------------------------- *
 * Address and Randomize are greyed out while the mode is Default, through the
 * Settings core's own row enable/disable exports (what SceSettings uses for
 * its dependent rows, e.g. media-transfer passcode). Both look the row up by
 * id and do nothing if it does not exist yet. */
#define WT_NID_ROW_ENABLE   0xF832D8FB
#define WT_NID_ROW_DISABLE  0x9D058B17
#define WT_NID_ROW_SHOW     0x84029629   /* (id, 1) add / (id, 0) remove a row */
static void (*g_row_enable)(const char *id), (*g_row_disable)(const char *id);
static int  (*g_row_show)(const char *id, int show);

/* Resolve the core's row exports once; 1 if all three are there. */
static int wt_mac_rows_resolve(void)
{
	if (!g_row_enable || !g_row_disable || !g_row_show) {
		uintptr_t e = 0, d = 0;
		if (taiGetModuleExportFunc("SceSystemSettingsCore", TAI_ANY_LIBRARY,
		                           WT_NID_ROW_ENABLE, &e) < 0 ||
		    taiGetModuleExportFunc("SceSystemSettingsCore", TAI_ANY_LIBRARY,
		                           WT_NID_ROW_DISABLE, &d) < 0 || !e || !d)
			return 0;
		g_row_enable  = (void (*)(const char *))e;
		g_row_disable = (void (*)(const char *))d;
		uintptr_t v = 0;
		if (taiGetModuleExportFunc("SceSystemSettingsCore", TAI_ANY_LIBRARY,
		                           WT_NID_ROW_SHOW, &v) >= 0 && v)
			g_row_show = (int (*)(const char *, int))v;
	}
	return g_row_enable && g_row_disable && g_row_show;
}

static void wt_mac_rows_update(void)
{
	wt_mac_rows_resolve();
	if (!g_row_enable || !g_row_disable)
		return;
	void (*f)(const char *) = g_ui.macmode == 1 ? g_row_enable : g_row_disable;
	f("wt_macaddr");
	f("wt_macrand");
}

static void wt_mac_words(const uint8_t *m, int *w0, int *w1)
{
	*w0 = (int)(m[0] | m[1] << 8 | m[2] << 16 | (unsigned)m[3] << 24);
	*w1 = (int)(m[4] | m[5] << 8);
}

/* "AA:BB:CC:DD:EE:FF" from the two WqUiState words, or dashes if not known. */
static void wt_mac_text(int w0, int w1, char *out, int cap)
{
	static const char h[] = "0123456789ABCDEF";
	if (cap < 18)
		return;
	if (!w0 && !w1) {
		s_cpy(out, "--:--:--:--:--:--", 18);
		return;
	}
	unsigned b[6] = { (unsigned)w0 & 255, ((unsigned)w0 >> 8) & 255, ((unsigned)w0 >> 16) & 255,
	                  ((unsigned)w0 >> 24) & 255, (unsigned)w1 & 255, ((unsigned)w1 >> 8) & 255 };
	int n = 0;
	for (int i = 0; i < 6; i++) {
		if (i) out[n++] = ':';
		out[n++] = h[b[i] >> 4];
		out[n++] = h[b[i] & 15];
	}
	out[n] = 0;
}

/* The address in use (or about to be): the custom one in Custom, else the
 * console's own. */
static void wt_mac_current(char *out, int cap)
{
	wt_seed();
	if (g_ui.macmode == 1) wt_mac_text(g_ui.mac0, g_ui.mac1, out, cap);
	else                   wt_mac_text(g_ui.fmac0, g_ui.fmac1, out, cap);
}

/* Rebuild the address rows so they show the address in use now: a row reads
 * its value once, when it is built, and a text field then keeps whatever was
 * last typed in it, even an address we refused. Removing and re-adding a row
 * makes the page read it again (SceSettings shows and hides its
 * parental-control rows the same way). Re-adding enables the row, so the
 * grey-out is applied after. */
static void wt_mac_refresh(void)
{
	if (g_row_show || wt_mac_rows_resolve()) {
		int a = g_row_show("wt_macaddr", 0), b = g_row_show("wt_macaddr", 1);
		int c = g_row_show("wt_maccur", 0),  d = g_row_show("wt_maccur", 1);
		WQ_LOGI("[SET] mac rows re-added: addr %08x/%08x cur %08x/%08x",
		        (unsigned)a, (unsigned)b, (unsigned)c, (unsigned)d);
	}
	wt_mac_rows_update();
}

/* After a typed address the keyboard is still up when the value arrives, and
 * rows re-added then do not show; the field is NOT read again when it closes
 * (re-adding on the next read ran during the next page build and crashed the
 * Settings app). The refresh waits for the page to regain focus (wt_cb1/2)
 * and is dropped whenever the page is built again (g_mac_refresh_pending). */

/* 1 once the row was re-added. 0x80100846 = the row is not on the current
 * page: while the keyboard is still closing, it is the current page. */
static int wt_mac_refresh_current(void)
{
	int ok = 0;
	if (g_row_show || wt_mac_rows_resolve()) {
		int c = g_row_show("wt_maccur", 0), d = g_row_show("wt_maccur", 1);
		ok = c >= 0 && d >= 0;
		WQ_LOGI("[SET] mac current row re-added after the keyboard: %08x/%08x",
		        (unsigned)c, (unsigned)d);
	}
	if (ok)
		wt_mac_rows_update();
	return ok;
}

/* wqUiSubmit copies WQ_UI_CMD_MAX bytes from its argument, so every command
 * goes through a buffer that size, never a shorter string literal. */
static void wt_submit(const char *a, const char *b)
{
	char line[WQ_UI_CMD_MAX];
	int n = 0;
	while (a && *a && n < WQ_UI_CMD_MAX - 1) line[n++] = *a++;
	while (b && *b && n < WQ_UI_CMD_MAX - 1) line[n++] = *b++;
	line[n] = 0;
	wqUiSubmit(line);
}

/* A new random custom address from the console's own: saved locally, then
 * sent to the resident core as an explicit address. */
static void wt_mac_new_random(void)
{
	uint8_t f[6], m[6];
	char hex[13];
	static const char h[] = "0123456789ABCDEF";
	f[0] = (uint8_t)g_ui.fmac0; f[1] = (uint8_t)(g_ui.fmac0 >> 8);
	f[2] = (uint8_t)(g_ui.fmac0 >> 16); f[3] = (uint8_t)(g_ui.fmac0 >> 24);
	f[4] = (uint8_t)g_ui.fmac1; f[5] = (uint8_t)(g_ui.fmac1 >> 8);
	if (!g_ui.fmac0 && !g_ui.fmac1) {      /* own address not known yet */
		g_ui.macmode = 1;
		wt_submit("mac custom", NULL);
		return;
	}
	wq_mac_random(m, f);
	for (int i = 0; i < 6; i++) { hex[2*i] = h[m[i] >> 4]; hex[2*i+1] = h[m[i] & 15]; }
	hex[12] = 0;
	g_ui.macmode = 1;
	wt_mac_words(m, &g_ui.mac0, &g_ui.mac1);
	g_set_t = sceKernelGetProcessTimeWide();
	wt_submit("mac set ", hex);
}

static int get_hook(const char *cat, const char *name, int *val)
{
	if (is_wt(cat) && val) {
		wt_seed();
		int *f = wt_field(name);
		*val = f ? *f : 0;
		/* Developer mode is the row after the MAC rows: by the time the page
		 * asks for it, those rows exist and can be greyed out. */
		if (s_eqn(name,"devmode",8))
			wt_mac_rows_update();
		return 0;
	}
	return TAI_CONTINUE(int, r_get, cat, name, val);
}
static int set_hook(const char *cat, const char *name, int v)
{
	if (is_wt(cat)) {
		wt_seed();
		int *f = wt_field(name);
		if (f) *f = v;   /* remember immediately for this session's re-reads */
		g_set_t = sceKernelGetProcessTimeWide();
		char line[WQ_UI_CMD_MAX]; int n = 0;
		if (s_eqn(name,"profile",8)) {
			const char *p = (v==0)?"profile maxperf":(v==1)?"profile balanced":"profile maxbatt";
			while (p[n]) { line[n]=p[n]; n++; }
		} else if (s_eqn(name,"rxring",7)) {
			const char *p = (v==2)?"rxpreset maximum":(v==1)?"rxpreset enhanced":"rxpreset stock";
			while (p[n]) { line[n]=p[n]; n++; }
		} else if (s_eqn(name,"hud",4)) {
			/* 0 Off, 2 Compact, 1 Extended (1 = the old 'on'). */
			const char *p = v == 2 ? "hud compact" : v ? "hud extended" : "hud off";
			while (p[n]) { line[n]=p[n]; n++; }
		} else if (s_eqn(name,"maccur",7)) {
			return 0;                         /* read-only row */
		} else if (s_eqn(name,"macmode",8)) {
			/* Custom always generates a new address (the console's own first
			 * half kept), made here so the page can show it at once; Default
			 * goes back to the console's own address. */
			if (v) wt_mac_new_random();
			else   wt_submit("mac stock", NULL);
			wt_mac_refresh();
			return 0;
		} else if (s_eqn(name,"hudpos",7)) {
			const char *pre = "hud pos ";              /* -> "hud pos 0..3" */
			while (pre[n]) { line[n]=pre[n]; n++; }
			line[n++] = (char)('0' + (v & 3));
		} else {
			int ln = s_len(name); s_cpy(line, name, ln); n = ln;
			line[n++]=' '; const char *o = v?"on":"off";
			line[n++]=o[0]; line[n++]=o[1]; if(o[2]) line[n++]=o[2];
		}
		line[n] = 0;
		wqUiSubmit(line);
		return 0;
	}
	return TAI_CONTINUE(int, r_set, cat, name, v);
}
/* SceRegMgrKeysInfo as HENkaku's Settings plugin uses it. */
typedef struct { int size; const char *name; int type; int unk; } wt_keys_info;

static int info_hook(const char *cat, void *info, int unk)
{
	if (is_wt(cat)) {
		wt_keys_info *ki = (wt_keys_info *)info;
		if (ki)
			ki->type = (ki->name && s_eqn(ki->name, "macaddr", 8)) ? 0x00030001 /* string */
			                                                       : 0x00040000; /* int */
		return 0;
	}
	return TAI_CONTINUE(int, r_info, cat, info, unk);
}

/* String keys: only the MAC address. It shows the custom address in Custom
 * and the console's own address in Default. A typed address is checked
 * (12 hex digits, not a group, all-zero or broadcast address) and refused
 * otherwise; a valid one switches the mode to Custom. */
static int gets_hook(const char *cat, const char *name, char *str, int len)
{
	if (is_wt(cat)) {
		if (str && len > 0 && s_eqn(name, "macaddr", 8)) {
			char t[18];
			wt_seed();
			if (g_ui.macmode == 1) wt_mac_text(g_ui.mac0, g_ui.mac1, t, sizeof(t));
			else                   wt_mac_text(g_ui.fmac0, g_ui.fmac1, t, sizeof(t));
			int n = s_len(t);
			if (n > len - 1) n = len - 1;
			s_cpy(str, t, n);
			str[n] = 0;
		} else if (str && len > 0) {
			str[0] = 0;
		}
		return 0;
	}
	return TAI_CONTINUE(int, r_gets, cat, name, str, len);
}

static int sets_hook(const char *cat, const char *name, const char *str, int len)
{
	if (is_wt(cat)) {
		uint8_t m[6];
		int why = 0;
		if (str && s_eqn(name, "macaddr", 8) && wq_mac_parse(str, m, &why) == 0) {
			char hex[13];
			static const char h[] = "0123456789ABCDEF";
			for (int i = 0; i < 6; i++) { hex[2*i] = h[m[i] >> 4]; hex[2*i+1] = h[m[i] & 15]; }
			hex[12] = 0;
			wt_seed();
			g_ui.macmode = 1;
			wt_mac_words(m, &g_ui.mac0, &g_ui.mac1);
			g_set_t = sceKernelGetProcessTimeWide();
			wt_submit("mac set ", hex);
			WQ_LOGI("[SET] MAC address typed: %s", hex);
			g_mac_refresh_pending = 1;
		} else if (str && s_eqn(name, "macaddr", 8)) {
			WQ_LOGW("[SET] MAC address refused (why %d)", why);
			g_mac_refresh_pending = 1;
		}
		return 0;
	}
	return TAI_CONTINUE(int, r_sets, cat, name, str, len);
}

/* Randomize: a button row reports through its page plugin's OnButtonEvent
 * (funcs[6]), so the Network page's handler is wrapped (HENkaku's method). */
static int (*g_net_button)(const char *id, int a2, void *a3);
static int net_button_hook(const char *id, int a2, void *a3)
{
	if (id && s_eqn(id, "wt_macrand", 11)) {
		if (g_ui.macmode == 1) {
			wt_seed();
			wt_mac_new_random();
			wt_mac_refresh();
		}
		return 0;
	}
	return g_net_button ? g_net_button(id, a2, a3) : 0;
}

/* The Network page's plugin callbacks 1 and 2 run when the page gains and
 * loses focus (rarely, with 1/0), which is what a
 * keyboard closing does. A refresh left pending by a typed address runs from
 * there, after the page's own callback. The busy per-row callbacks (0, 3, 4,
 * 5) are left alone: wrapping and logging them lagged and crashed the page. */
typedef int (*wt_cbfn)(int, int, int, int);
static wt_cbfn g_net_cb1, g_net_cb2;

/* Retried on each focus callback until our rows are on the current page
 * again (at most 8 tries; any page build drops it). */
static void wt_after_focus(int which, int a, int b)
{
	if (!g_mac_refresh_pending)
		return;
	WQ_LOGI("[SET] focus cb%d (%08x %08x) - refreshing Current MAC address (try %d)",
	        which, (unsigned)a, (unsigned)b, g_mac_refresh_pending);
	if (wt_mac_refresh_current() || ++g_mac_refresh_pending > 8)
		g_mac_refresh_pending = 0;
}
static int wt_cb1(int a, int b, int c, int d)
{
	int r = g_net_cb1 ? g_net_cb1(a, b, c, d) : 0;
	wt_after_focus(1, a, b);
	return r;
}
static int wt_cb2(int a, int b, int c, int d)
{
	int r = g_net_cb2 ? g_net_cb2(a, b, c, d) : 0;
	wt_after_focus(2, a, b);
	return r;
}

static int funcs_hook(void *a1, int a2, uint32_t *funcs)
{
	int res = TAI_CONTINUE(int, r_funcs, a1, a2, funcs);
	const char *plugin = a1 ? (const char *)((uint32_t *)a1)[1] : 0;
	if (plugin && funcs && s_eqn(plugin, "network_settings_plugin", 23) &&
	    funcs[6] != (uint32_t)net_button_hook) {
		g_net_button = (int (*)(const char *, int, void *))funcs[6];
		funcs[6] = (uint32_t)net_button_hook;
		if (funcs[1] && funcs[1] != (uint32_t)wt_cb1) {
			g_net_cb1 = (wt_cbfn)funcs[1];
			funcs[1] = (uint32_t)wt_cb1;
		}
		if (funcs[2] && funcs[2] != (uint32_t)wt_cb2) {
			g_net_cb2 = (wt_cbfn)funcs[2];
			funcs[2] = (uint32_t)wt_cb2;
		}
	}
	return res;
}

/* Install the UI hooks once system_settings_core.suprx is up (HENkaku trick). */
/* sceKernelLoadStartModule(path, args, argp, flags, option) - 5 args. */
/* scePafToplevelGetText(arg, char **msg) -> text for a label/hint id. Return our
 * UTF-16 help text for our hint ids; else fall through. */
static void *text_hook(void *arg, char **msg)
{
	if (msg && *msg) {
		const wt_u16 *h = WT_TEXT(*msg);
		if (h) return (void *)h;
	}
	return TAI_CONTINUE(void *, r_text, arg, msg);
}

/* NID 0x2DCC4AFA as SceSettings imports it takes SIX args incl. int *status
 * (confirmed in HENkaku user.c). Wrong arity corrupts the shared hook chain and
 * broke HENkaku's own Settings setup - this 6-arg form is the fix. */
static SceUID load_hook(const char *path, SceSize args, void *argp,
                        int flags, void *option, int *status)
{
	SceUID ret = TAI_CONTINUE(SceUID, r_load, path, args, argp, flags, option, status);
	int match = (path && mem_find(path, s_len(path), "system_settings_core", 20)) ? 1 : 0;
	if (path)
		WQ_LOGI("[SET] load rc=0x%08x match=%d: %s", (unsigned)ret, match, path);
	if (match && !g_installed) {
		g_installed = 1;
		WQ_LOGI("[SET] core loaded - installing UI hooks");
		g_h_xml  = taiHookFunctionImport(&r_xml, "SceSettings",
		              0x3D643CE8, 0x19FE55A8, xml_hook);              /* ScePafMisc */
		g_h_get  = taiHookFunctionImport(&r_get, "SceSystemSettingsCore",
		              0xC436F916, 0x16DDF3DC, get_hook);
		g_h_set  = taiHookFunctionImport(&r_set, "SceSystemSettingsCore",
		              0xC436F916, 0xD72EA399, set_hook);
		g_h_info = taiHookFunctionImport(&r_info,"SceSystemSettingsCore",
		              0xC436F916, 0x58421DD1, info_hook);
		g_h_text = taiHookFunctionImport(&r_text,"SceSystemSettingsCore",
		              0x4D9A9DD0, 0x19CEFDA7, text_hook);   /* scePafToplevelGetText */
		g_h_gets = taiHookFunctionImport(&r_gets,"SceSystemSettingsCore",
		              0xC436F916, 0xE188382F, gets_hook);   /* sceRegMgrGetKeyStr */
		g_h_sets = taiHookFunctionImport(&r_sets,"SceSystemSettingsCore",
		              0xC436F916, 0x41D320C5, sets_hook);   /* sceRegMgrSetKeyStr */
		g_h_funcs = taiHookFunctionImport(&r_funcs, "SceSettings",
		              0x4D9A9DD0, 0xF5354FEF, funcs_hook);  /* scePafToplevelInitPluginFunctions */
	}
	return ret;
}

/* Called from the main module's module_start ONLY when it detects it is running
 * inside the Settings app process (NPXS10015). The same wifiturbo.suprx is
 * listed under both *main and *NPXS10015; in SceShell it does the normal
 * bring-up, here it installs just these Settings hooks. */
void wq_settings_hooks_install(void)
{
	g_h_load = taiHookFunctionImport(&r_load, "SceSettings",
	              0xCAE9ACE6, 0x2DCC4AFA, load_hook);   /* sceKernelLoadStartModule */
	WQ_LOGI("[SET] LoadStartModule import hook: uid=0x%08x", (unsigned)g_h_load);
	wqHudRegisterProc(WQ_HUD_PROC_SETTINGS);   /* the HUD may draw over Settings */
}

void wq_settings_hooks_release(void)
{
	if (g_h_funcs>=0){ taiHookRelease(g_h_funcs,r_funcs);g_h_funcs=-1;}
	if (g_h_sets>=0) { taiHookRelease(g_h_sets, r_sets); g_h_sets=-1; }
	if (g_h_gets>=0) { taiHookRelease(g_h_gets, r_gets); g_h_gets=-1; }
	if (g_h_text>=0) { taiHookRelease(g_h_text, r_text); g_h_text=-1; }
	if (g_h_info>=0) { taiHookRelease(g_h_info, r_info); g_h_info=-1; }
	if (g_h_set>=0)  { taiHookRelease(g_h_set,  r_set);  g_h_set=-1;  }
	if (g_h_get>=0)  { taiHookRelease(g_h_get,  r_get);  g_h_get=-1;  }
	if (g_h_xml>=0)  { taiHookRelease(g_h_xml,  r_xml);  g_h_xml=-1;  }
	if (g_h_load>=0) { taiHookRelease(g_h_load, r_load); g_h_load=-1; }
}
