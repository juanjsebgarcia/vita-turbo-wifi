/*
 * hud_i18n.c - ASCII HUD label translations (see hud_i18n.h).
 *
 * Machine-quality, ASCII only (accents dropped so the 8x8 font can draw them),
 * short to fit the HUD line. Non-Latin-script languages (Japanese, Russian,
 * Korean, Chinese) fall back to English - review/refine by native speakers.
 * Column order: HTR_UP, HTR_DOWN, HTR_CONNECTING, HTR_PROFILE, HTR_LAST. Row index = Vita system-language id (enum wq_i18n_lang_id).
 */
#include "hud_i18n.h"

#define WT_HUD_LANG_COUNT 20

static const char *const T[WT_HUD_LANG_COUNT][HTR_COUNT] = {
/* 0  JA (non-Latin -> EN) */ {"UP","DOWN","CONNECTING","PROFILE","LAST"},
/* 1  EN-US              */ {"UP","DOWN","CONNECTING","PROFILE","LAST"},
/* 2  FR                 */ {"ACTIF","COUPE","CONNEXION","PROFIL","DERNIER"},
/* 3  ES                 */ {"ACTIVO","CAIDO","CONECTANDO","PERFIL","ULTIMO"},
/* 4  DE                 */ {"AKTIV","AUS","VERBINDEN","PROFIL","LETZTE"},
/* 5  IT                 */ {"ATTIVO","GIU","CONNESSIONE","PROFILO","ULTIMO"},
/* 6  NL                 */ {"ACTIEF","UIT","VERBINDEN","PROFIEL","LAATSTE"},
/* 7  PT-PT              */ {"ATIVO","CAIDO","A LIGAR","PERFIL","ULTIMO"},
/* 8  RU (non-Latin->EN) */ {"UP","DOWN","CONNECTING","PROFILE","LAST"},
/* 9  KO (non-Latin->EN) */ {"UP","DOWN","CONNECTING","PROFILE","LAST"},
/* 10 ZH-T (non-Latin)   */ {"UP","DOWN","CONNECTING","PROFILE","LAST"},
/* 11 ZH-S (non-Latin)   */ {"UP","DOWN","CONNECTING","PROFILE","LAST"},
/* 12 FI                 */ {"YHDIST.","POIKKI","YHDISTAA","PROFIILI","VIIMEISIN"},
/* 13 SV                 */ {"UPPE","NERE","ANSLUTER","PROFIL","SENASTE"},
/* 14 DA                 */ {"OPPE","NEDE","FORBINDER","PROFIL","SIDSTE"},
/* 15 NO                 */ {"OPPE","NEDE","KOBLER","PROFIL","SISTE"},
/* 16 PL                 */ {"WLACZ.","BRAK","LACZENIE","PROFIL","OSTATNI"},
/* 17 PT-BR              */ {"ATIVO","CAIDO","CONECTANDO","PERFIL","ULTIMO"},
/* 18 EN-GB              */ {"UP","DOWN","CONNECTING","PROFILE","LAST"},
/* 19 TR                 */ {"ACIK","KAPALI","BAGLANIYOR","PROFIL","SON"},
};

const char *hud_tr(int lang, int tok)
{
	if (tok < 0 || tok >= HTR_COUNT)
		return "";
	if (lang < 0 || lang >= WT_HUD_LANG_COUNT)
		lang = 1;                      /* English(US) fallback                  */
	return T[lang][tok];
}
