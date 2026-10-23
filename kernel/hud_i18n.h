#ifndef WIFITURBO_HUD_I18N_H
#define WIFITURBO_HUD_I18N_H

/*
 * HUD label localisation (kernel-side, ASCII only).
 *
 * The HUD renders with an 8x8 ASCII font, so translations MUST be ASCII:
 * drop accents/diacritics (e.g. "conexion" not "conexión"). Scripts the font
 * cannot draw at all (Japanese, Korean, Chinese, Russian/Cyrillic) fall back to
 * English - do NOT put non-ASCII bytes in the table.
 *
 * Only genuine WORDS are translated; universal acronyms stay as-is in every
 * language (RSSI, SNR, CH, BSSID, DHCP, dBm). Keep translations SHORT - the HUD line has limited width.
 *
 * lang is the Vita system-language index (enum wq_i18n_lang_id / the value from
 * wq_i18n_lang(), passed to the kernel in WqHudState.lang).
 */

enum hud_tr_tok {
	HTR_UP = 0,       /* link up / connected      */
	HTR_DOWN,         /* link down / disconnected */
	HTR_CONNECTING,   /* associating              */
	HTR_PROFILE,      /* "PROFILE" label          */
	HTR_LAST,         /* "LAST" (last drop reason)*/
	HTR_COUNT
};

/* Return the ASCII label for token `tok` in language `lang`, English fallback
 * for any non-Latin or unknown language. Never returns NULL. */
const char *hud_tr(int lang, int tok);

#endif /* WIFITURBO_HUD_I18N_H */
