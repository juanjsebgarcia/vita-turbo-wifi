/*
 * wifiturbo_lang.c - the Vita system language, as an index into the
 * translation table (enum wq_i18n_lang_id). Kept apart from the table
 * (wifiturbo_i18n.c) so wifiturbo.suprx can pick the HUD language in SceShell
 * without carrying the Settings text.
 *
 * Vita stubs required (weak, per the plugin's link convention):
 *   - SceRegistryMgr_stub_weak     (sceRegMgrGetKeyInt)
 *   - SceAppUtil_stub_weak         (sceAppUtilSystemParamGetInt)
 */
#include "wifiturbo_i18n.h"
#include "../user/imports.h"   /* wq_import_ready */

/* ---- platform imports (self-contained prototypes) ---------------------- */

/* SceRegMgrForGame: read an int registry key. Canonical path for the system
 * language on 3.60 is category "/CONFIG/SYSTEM", key "language". */
extern int sceRegMgrGetKeyInt(const char *category, const char *name, int *value);

/* SceAppUtil: SDK system-parameter read. paramId 1 == SCE_SYSTEM_PARAM_ID_LANG.
 * Behind the sceAppUtilInit() gate, hence only a fallback here. */
extern int sceAppUtilSystemParamGetInt(int paramId, int *value);

#define WT_SYSTEM_PARAM_ID_LANG 1

int wq_i18n_lang(void)
{
	int lang = -1;

	/* Primary (reliable for NPXS10015): the system registry. SceSettings
	 * already links RegMgr; /CONFIG/SYSTEM/language is the canonical store and
	 * has no init gate. */
	if (wq_import_ready((const void *)sceRegMgrGetKeyInt) &&
	    sceRegMgrGetKeyInt("/CONFIG/SYSTEM", "language", &lang) >= 0 &&
	    lang >= 0 && lang < WT_LANG_COUNT)
		return lang;

	/* Fallback: SDK system param. Only works once the host app has called
	 * sceAppUtilInit(); otherwise returns SCE_APPUTIL_ERROR_NOT_INITIALIZED. */
	lang = -1;
	if (wq_import_ready((const void *)sceAppUtilSystemParamGetInt) &&
	    sceAppUtilSystemParamGetInt(WT_SYSTEM_PARAM_ID_LANG, &lang) >= 0 &&
	    lang >= 0 && lang < WT_LANG_COUNT)
		return lang;

	return WT_LANG_ENGLISH_US;
}
