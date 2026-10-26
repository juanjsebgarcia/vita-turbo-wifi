/*
 * wifiturbo_i18n.h - Internationalization (i18n) for the "Wi-Fi Turbo" Settings
 * sub-menu (NPXS10015 / SceSettings injection, firmware 3.60-3.74).
 *
 * This header declares:
 *   - a stable string ID per UI string (submenu title, row titles, list items,
 *     per-row hints),
 *   - the Vita system-language index set (SCE_SYSTEM_PARAM_LANG_*),
 *   - helpers to detect the current language, look up a UTF-8 string for it
 *     (English(US) is the fallback), and decode UTF-8 -> UTF-16.
 *
 * The translation table lives in wifiturbo_i18n.c and is MACHINE-TRANSLATED
 * (except English); it is meant to be reviewed/refined by native speakers.
 *
 * C99, no char16_t / -fshort-wchar. UTF-16 output uses plain unsigned short.
 */
#ifndef WIFITURBO_I18N_H
#define WIFITURBO_I18N_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Language indices. These deliberately match the Vita SDK
 * SCE_SYSTEM_PARAM_LANG_* enumeration AND the raw integer stored in the
 * /CONFIG/SYSTEM/language registry key, so the value returned by
 * wq_i18n_lang() indexes the translation table directly.
 */
enum wq_i18n_lang_id {
	WT_LANG_JAPANESE       = 0,
	WT_LANG_ENGLISH_US     = 1,   /* fallback language */
	WT_LANG_FRENCH         = 2,
	WT_LANG_SPANISH        = 3,
	WT_LANG_GERMAN         = 4,
	WT_LANG_ITALIAN        = 5,
	WT_LANG_DUTCH          = 6,
	WT_LANG_PORTUGUESE_PT  = 7,
	WT_LANG_RUSSIAN        = 8,
	WT_LANG_KOREAN         = 9,
	WT_LANG_CHINESE_T      = 10,  /* Traditional */
	WT_LANG_CHINESE_S      = 11,  /* Simplified  */
	WT_LANG_FINNISH        = 12,
	WT_LANG_SWEDISH        = 13,
	WT_LANG_DANISH         = 14,
	WT_LANG_NORWEGIAN      = 15,
	WT_LANG_POLISH         = 16,
	WT_LANG_PORTUGUESE_BR  = 17,
	WT_LANG_ENGLISH_GB     = 18,
	WT_LANG_TURKISH        = 19,

	WT_LANG_COUNT          = 20
};

/*
 * String IDs. One per user-visible string in the Wi-Fi Turbo sub-menu.
 * Keep WT_I18N_COUNT last.
 */
enum wq_i18n_id {
	/* Sub-menu title */
	WT_I18N_TITLE = 0,          /* "Wi-Fi Turbo"              */

	/* Row titles */
	WT_I18N_ROW_KEEP,           /* "Keep Wi-Fi on"            */
	WT_I18N_ROW_HUD,            /* "Wi-Fi HUD overlay"        */
	WT_I18N_ROW_RECONNECT,      /* "Auto-reconnect on drop"   */
	WT_I18N_ROW_ROAM,           /* "Smart Mesh Roaming"       */
	WT_I18N_ROW_HT40,           /* "40 MHz"    */
	WT_I18N_ROW_POWER,          /* "Power profile"            */

	/* List items (values for the "Power profile" row) */
	WT_I18N_ITEM_MAXPERF,       /* "Max performance"          */
	WT_I18N_ITEM_BALANCED,      /* "Balanced"                 */
	WT_I18N_ITEM_BATTERY,       /* "Battery saver"            */

	/* Per-row hints (help text), one per row above */
	WT_I18N_HINT_KEEP,
	WT_I18N_HINT_HUD,
	WT_I18N_HINT_RECONNECT,
	WT_I18N_HINT_ROAM,
	WT_I18N_HINT_HT40,
	WT_I18N_HINT_POWER,

	/* Bluetooth reminder row (shown only while Bluetooth is on) */
	WT_I18N_ROW_BTWARN,         /* "Bluetooth impacts Wi-Fi performance" */
	WT_I18N_HINT_BTWARN,

	/* Developer mode row (last entry) */
	WT_I18N_ROW_DEVMODE,        /* "Developer mode (remote control)" */
	WT_I18N_HINT_DEVMODE,

	/* EXPERIMENTAL chip RX buffer row and its three list items */
	WT_I18N_ROW_RXRING,         /* "Wi-Fi receive buffer"     */
	WT_I18N_ITEM_STOCK,         /* "Default" (stock firmware) */
	WT_I18N_ITEM_ENHANCED,      /* "Enhanced"                 */
	WT_I18N_ITEM_MAXIMUM,       /* "Maximum"                  */
	WT_I18N_HINT_RXRING,

	/* HUD position row + its four screen-corner list items */
	WT_I18N_ROW_HUDPOS,         /* "HUD position"             */
	WT_I18N_HINT_HUDPOS,
	WT_I18N_ITEM_POS_TL,        /* corner items, in WQ_HUD_POS_* order */
	WT_I18N_ITEM_POS_TR,
	WT_I18N_ITEM_POS_BL,
	WT_I18N_ITEM_POS_BR,

	WT_I18N_COUNT
};

/*
 * Return the current system-language index (0..WT_LANG_COUNT-1).
 *
 * For NPXS10015 the RELIABLE source is the /CONFIG/SYSTEM/language registry key
 * (read with sceRegMgrGetKeyInt), because SceSettings already links RegMgr and
 * that key has no init gate. sceAppUtilSystemParamGetInt() is used only as a
 * fallback - it returns SCE_APPUTIL_ERROR_NOT_INITIALIZED unless the host app
 * has already called sceAppUtilInit(). On any failure this returns
 * WT_LANG_ENGLISH_US.
 */
int wq_i18n_lang(void);

/*
 * Look up the UTF-8 string for `id` in the current system language.
 * Falls back to English(US) for any missing entry, and returns "" for an
 * out-of-range id. Never returns NULL.
 */
const char *wq_i18n(int id);

/* Same as wq_i18n() but for an explicit language index (for testing / reuse). */
const char *wq_i18n_for(int id, int lang);

/*
 * Decode a NUL-terminated UTF-8 string into a NUL-terminated UTF-16 buffer.
 * Handles 1/2/3-byte sequences (BMP). 4-byte sequences (astral planes, needing
 * surrogate pairs) are skipped - the Wi-Fi Turbo strings are all BMP. Malformed
 * bytes are skipped. `out_max` is the capacity of `out` in unsigned shorts,
 * including the NUL terminator.
 *
 * Returns the number of UTF-16 code units written, NOT counting the terminator.
 * Always NUL-terminates when out_max >= 1.
 */
int wq_i18n_to_u16(const char *utf8, unsigned short *out, int out_max);

#ifdef __cplusplus
}
#endif

#endif /* WIFITURBO_I18N_H */
