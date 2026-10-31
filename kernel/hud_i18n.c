/*
 * hud_i18n.c - ASCII HUD label translations (see hud_i18n.h).
 *
 * Machine-quality, ASCII only (accents dropped so the 8x8 font can draw them),
 * short to fit the HUD line. Non-Latin-script languages (Japanese, Russian,
 * Korean, Chinese) fall back to English - review/refine by native speakers.
 * Column order: HTR_UP, HTR_DOWN, HTR_CONNECTING, HTR_PROFILE, HTR_LAST, HTR_PENDING, HTR_NOTAPPLIED. Row index = Vita system-language id (enum wq_i18n_lang_id).
 */
#include "hud_i18n.h"

#define WT_HUD_LANG_COUNT 20

static const char *const T[WT_HUD_LANG_COUNT][HTR_COUNT] = {
/* 0  JA (non-Latin -> EN) */ {"UP","DOWN","CONNECTING","PROFILE","LAST","pending","not applied"},
/* 1  EN-US              */ {"UP","DOWN","CONNECTING","PROFILE","LAST","pending","not applied"},
/* 2  FR                 */ {"ACTIF","COUPE","CONNEXION","PROFIL","DERNIER","en attente","non applique"},
/* 3  ES                 */ {"ACTIVO","CAIDO","CONECTANDO","PERFIL","ULTIMO","pendiente","no aplicada"},
/* 4  DE                 */ {"AKTIV","AUS","VERBINDEN","PROFIL","LETZTE","ausstehend","nicht aktiv"},
/* 5  IT                 */ {"ATTIVO","GIU","CONNESSIONE","PROFILO","ULTIMO","in attesa","non applicato"},
/* 6  NL                 */ {"ACTIEF","UIT","VERBINDEN","PROFIEL","LAATSTE","bezig","niet toegepast"},
/* 7  PT-PT              */ {"ATIVO","CAIDO","A LIGAR","PERFIL","ULTIMO","pendente","nao aplicado"},
/* 8  RU (non-Latin->EN) */ {"UP","DOWN","CONNECTING","PROFILE","LAST","pending","not applied"},
/* 9  KO (non-Latin->EN) */ {"UP","DOWN","CONNECTING","PROFILE","LAST","pending","not applied"},
/* 10 ZH-T (non-Latin)   */ {"UP","DOWN","CONNECTING","PROFILE","LAST","pending","not applied"},
/* 11 ZH-S (non-Latin)   */ {"UP","DOWN","CONNECTING","PROFILE","LAST","pending","not applied"},
/* 12 FI                 */ {"YHDIST.","POIKKI","YHDISTAA","PROFIILI","VIIMEISIN","odottaa","ei kaytossa"},
/* 13 SV                 */ {"UPPE","NERE","ANSLUTER","PROFIL","SENASTE","vantar","ej tillampad"},
/* 14 DA                 */ {"OPPE","NEDE","FORBINDER","PROFIL","SIDSTE","afventer","ikke anvendt"},
/* 15 NO                 */ {"OPPE","NEDE","KOBLER","PROFIL","SISTE","venter","ikke brukt"},
/* 16 PL                 */ {"WLACZ.","BRAK","LACZENIE","PROFIL","OSTATNI","oczekuje","nie ustawiony"},
/* 17 PT-BR              */ {"ATIVO","CAIDO","CONECTANDO","PERFIL","ULTIMO","pendente","nao aplicado"},
/* 18 EN-GB              */ {"UP","DOWN","CONNECTING","PROFILE","LAST","pending","not applied"},
/* 19 TR                 */ {"ACIK","KAPALI","BAGLANIYOR","PROFIL","SON","bekliyor","uygulanmadi"},
};

const char *hud_tr(int lang, int tok)
{
	if (tok < 0 || tok >= HTR_COUNT)
		return "";
	if (lang < 0 || lang >= WT_HUD_LANG_COUNT)
		lang = 1;                      /* English(US) fallback                  */
	return T[lang][tok];
}
