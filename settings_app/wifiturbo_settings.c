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
#include <taihen.h>

#include "../include/wifiturbo_kernel_api.h"   /* WqUiState, wqUi* syscalls     */
#include "../user/log.h"                       /* WQ_LOG* (shared log file)     */
#include "wifiturbo_i18n.h"                    /* localised UI strings          */

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
 "<toggle_switch id=\"wt_hud\" title=\"wt_t_hud\" icon=\"tex_spanner\" hint=\"wt_h_hud\" key=\"/CONFIG/WIFITURBO/hud\"/>"
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
static const wt_u16 *WT_TEXT(const char *id)
{
	int wtid = -1;
	#define M(name, i) if (s_eqn(id, name, sizeof(name))) wtid = (i);
	M("wt_t_title",     WT_I18N_TITLE)
	M("wt_t_keep",      WT_I18N_ROW_KEEP)
	M("wt_t_hud",       WT_I18N_ROW_HUD)
	M("wt_t_autorecon", WT_I18N_ROW_RECONNECT)
	M("wt_t_roam",      WT_I18N_ROW_ROAM)
	M("wt_t_ht40",      WT_I18N_ROW_HT40)
	M("wt_t_profile",   WT_I18N_ROW_POWER)
	M("wt_i_maxperf",   WT_I18N_ITEM_MAXPERF)
	M("wt_i_balanced",  WT_I18N_ITEM_BALANCED)
	M("wt_i_battery",   WT_I18N_ITEM_BATTERY)
	M("wt_h_keepwifi",  WT_I18N_HINT_KEEP)
	M("wt_h_hud",       WT_I18N_HINT_HUD)
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
	#undef M
	if (wtid < 0)
		return 0;
	wq_i18n_to_u16(wq_i18n(wtid), g_hintbuf,
	               (int)(sizeof(g_hintbuf) / sizeof(g_hintbuf[0])));
	return g_hintbuf;
}

/* ---- hooks ------------------------------------------------------------- */
static SceUID g_h_load=-1, g_h_xml=-1, g_h_get=-1, g_h_set=-1, g_h_info=-1, g_h_text=-1;
static tai_hook_ref_t r_load, r_xml, r_get, r_set, r_info, r_text;
static int g_installed = 0;   /* install the UI hooks once */

static char g_pagebuf[16384];  /* plugin-owned spliced layout buffer */

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
/* Local state cache for this Settings session, so a toggle is remembered
 * immediately on re-read (the resident mailbox has ~1s latency and doesn't yet
 * publish every field). Seeded from the resident state on first use. */
static WqUiState g_ui;
static int       g_ui_init = 0;

static int *wt_field(const char *name)
{
	if (s_eqn(name,"keepwifi",9))       return &g_ui.keepwifi;
	if (s_eqn(name,"hud",4))            return &g_ui.hud;
	if (s_eqn(name,"autoreconnect",14)) return &g_ui.autoreconnect;
	if (s_eqn(name,"roam",5))           return &g_ui.roam;
	if (s_eqn(name,"ht40",5))           return &g_ui.ht40;
	if (s_eqn(name,"profile",8))        return &g_ui.profile;
	if (s_eqn(name,"devmode",8))        return &g_ui.devmode;
	if (s_eqn(name,"rxring",7))         return &g_ui.rxring;
	return 0;
}
static void wt_seed(void) { if (!g_ui_init) { wqUiReadState(&g_ui); g_ui_init = 1; } }

static int get_hook(const char *cat, const char *name, int *val)
{
	if (is_wt(cat) && val) {
		wt_seed();
		int *f = wt_field(name);
		*val = f ? *f : 0;
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
		char line[WQ_UI_CMD_MAX]; int n = 0;
		if (s_eqn(name,"profile",8)) {
			const char *p = (v==0)?"profile maxperf":(v==1)?"profile balanced":"profile maxbatt";
			while (p[n]) { line[n]=p[n]; n++; }
		} else if (s_eqn(name,"rxring",7)) {
			const char *p = (v==2)?"rxpreset maximum":(v==1)?"rxpreset enhanced":"rxpreset stock";
			while (p[n]) { line[n]=p[n]; n++; }
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
static int info_hook(const char *cat, void *info, int unk)
{
	if (is_wt(cat)) { if (info) *(int *)((char *)info + 8) = 0x00040000; /* int */ return 0; }
	return TAI_CONTINUE(int, r_info, cat, info, unk);
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
}

void wq_settings_hooks_release(void)
{
	if (g_h_text>=0) { taiHookRelease(g_h_text, r_text); g_h_text=-1; }
	if (g_h_info>=0) { taiHookRelease(g_h_info, r_info); g_h_info=-1; }
	if (g_h_set>=0)  { taiHookRelease(g_h_set,  r_set);  g_h_set=-1;  }
	if (g_h_get>=0)  { taiHookRelease(g_h_get,  r_get);  g_h_get=-1;  }
	if (g_h_xml>=0)  { taiHookRelease(g_h_xml,  r_xml);  g_h_xml=-1;  }
	if (g_h_load>=0) { taiHookRelease(g_h_load, r_load); g_h_load=-1; }
}
