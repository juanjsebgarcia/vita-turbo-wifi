/*
 * wifiturbo_qm.c - Wi-Fi Turbo rows in the PS button quick menu.
 *
 * An optional client plugin for QuickMenuReborn: QuickMenuReborn (under *main)
 * loads every .suprx in ur0:QuickMenuReborn/ (or ux0:) into SceShell, and
 * rebuilds the widgets registered here each time the quick menu opens.
 *
 * Rows: "Keep Wi-Fi on" (check box) and the power profile (three buttons, the
 * active one lit). State is read with wqUiReadState and changed with
 * wqUiSubmit - the same SceWifiTurbo mailbox the Settings page uses - so a
 * change here is applied, saved and kept exactly like one made in Settings.
 *
 * Links SceWifiTurbo strongly: without the kernel module this plugin fails to
 * load and the quick menu simply shows nothing extra.
 */
#include <psp2/kernel/modulemgr.h>
#include <psp2/types.h>

#include "quickmenureborn/qm_reborn.h"

#include "../include/wifiturbo.h"
#include "../include/wifiturbo_kernel_api.h"
#include "../settings_app/wifiturbo_i18n.h"

/* QuickMenuReborn keeps every client's widgets in one list keyed by these ids,
 * so they carry our prefix. */
#define ID_SEPARATOR   "wifiturbo_qm_sep"
#define ID_TITLE       "wifiturbo_qm_title"
#define ID_KEEP_PLANE  "wifiturbo_qm_keep_plane"
#define ID_KEEP_TEXT   "wifiturbo_qm_keep_text"
#define ID_KEEP_BOX    "wifiturbo_qm_keep_box"
#define ID_PROF_TEXT   "wifiturbo_qm_prof_text"
#define ID_PROF_PLANE  "wifiturbo_qm_prof_plane"

/* In WQ_PROFILE_* order. */
static const char *const k_prof_id[3] = {
	"wifiturbo_qm_prof_maxperf", "wifiturbo_qm_prof_balanced", "wifiturbo_qm_prof_maxbatt",
};
static const char *const k_prof_cmd[3] = {
	"profile maxperf", "profile balanced", "profile maxbatt",
};
static const int k_prof_text[3] = {
	WT_I18N_ITEM_MAXPERF, WT_I18N_ITEM_BALANCED, WT_I18N_ITEM_BATTERY,
};

#define PROF_BUTTON_W   260.0f
#define PROF_BUTTON_H    60.0f
#define PROF_BUTTON_GAP 280.0f   /* centre-to-centre */

/* The active profile is drawn solid, the others dimmed. */
static void show_profile(int prof)
{
	for (int i = 0; i < 3; i++)
		QuickMenuRebornSetWidgetColor(k_prof_id[i], 1.0f, 1.0f, 1.0f, i == prof ? 1.0f : 0.45f);
}

/* The menu was just built: show the current state (it may have changed in
 * Settings or over the command port since the menu was last open). Runs as the
 * OnLoad of the last widget we register, when all of ours exist. */
ONLOAD_HANDLER(on_menu_open)
{
	(void)id;
	WqUiState st;
	if (wqUiReadState(&st) < 0)
		return;
	QuickMenuRebornSetCheckBoxState(ID_KEEP_BOX, st.keepwifi ? CHECKBOX_ON : CHECKBOX_OFF);
	show_profile(st.profile);
}

BUTTON_HANDLER(on_keep_toggle)
{
	(void)id; (void)hash; (void)eventId; (void)userDat;
	int on = QuickMenuRebornGetCheckboxValue(ID_KEEP_BOX) == 1;
	wqUiSubmit(on ? "keepwifi on" : "keepwifi off");
}

BUTTON_HANDLER(on_profile)
{
	(void)id; (void)hash; (void)eventId;
	int prof = (int)userDat;
	wqUiSubmit(k_prof_cmd[prof]);   /* reconnects Wi-Fi for a few seconds */
	show_profile(prof);             /* the published state follows within 1 s */
}

static void text_row(const char *id, const char *parent, float w, float h, float x, const char *label)
{
	QuickMenuRebornRegisterWidget(id, parent, text);
	QuickMenuRebornSetWidgetSize(id, w, h, 0, 0);
	QuickMenuRebornSetWidgetColor(id, 1, 1, 1, 1);
	QuickMenuRebornSetWidgetPosition(id, x, 0, 0, 0);
	QuickMenuRebornSetWidgetLabel(id, label);
}

static void container(const char *id, float h)
{
	QuickMenuRebornRegisterWidget(id, NULL, plane);
	QuickMenuRebornSetWidgetSize(id, SCE_PLANE_WIDTH, h, 0, 0);
	QuickMenuRebornSetWidgetColor(id, 1, 1, 1, 0);   /* invisible container */
}

int module_start(SceSize argc, const void *args);
int _start(SceSize argc, const void *args) __attribute__ ((weak, alias("module_start")));

int module_start(SceSize argc, const void *args)
{
	(void)argc; (void)args;

	QuickMenuRebornSeparator(ID_SEPARATOR, SCE_SEPARATOR_HEIGHT);
	text_row(ID_TITLE, NULL, SCE_PLANE_WIDTH, 50, 0, wq_i18n(WT_I18N_TITLE));

	/* Keep Wi-Fi on: label left, check box right. */
	container(ID_KEEP_PLANE, 80);
	text_row(ID_KEEP_TEXT, ID_KEEP_PLANE, 500, 75, -255, wq_i18n(WT_I18N_ROW_KEEP));
	QuickMenuRebornRegisterWidget(ID_KEEP_BOX, ID_KEEP_PLANE, check_box);
	QuickMenuRebornSetWidgetSize(ID_KEEP_BOX, 48, 48, 0, 0);
	QuickMenuRebornSetWidgetColor(ID_KEEP_BOX, 1, 1, 1, 1);
	QuickMenuRebornSetWidgetPosition(ID_KEEP_BOX, 350, 0, 0, 0);
	QuickMenuRebornRegisterEventHanlder(ID_KEEP_BOX, QMR_BUTTON_RELEASE_ID, on_keep_toggle, NULL);

	/* Power profile: a caption, then one button per profile. */
	text_row(ID_PROF_TEXT, NULL, SCE_PLANE_WIDTH, 50, 0, wq_i18n(WT_I18N_ROW_POWER));
	container(ID_PROF_PLANE, PROF_BUTTON_H + 30);
	for (int i = 0; i < 3; i++) {
		QuickMenuRebornRegisterWidget(k_prof_id[i], ID_PROF_PLANE, button);
		QuickMenuRebornSetWidgetSize(k_prof_id[i], PROF_BUTTON_W, PROF_BUTTON_H, 0, 0);
		QuickMenuRebornSetWidgetPosition(k_prof_id[i], (i - 1) * PROF_BUTTON_GAP, 0, 0, 0);
		QuickMenuRebornSetWidgetLabel(k_prof_id[i], wq_i18n(k_prof_text[i]));
		QuickMenuRebornRegisterEventHanlder(k_prof_id[i], QMR_BUTTON_RELEASE_ID, on_profile, (void *)i);
	}
	show_profile(-1);
	QuickMenuRebornAssignOnLoadHandler(on_menu_open, k_prof_id[2]);

	return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args)
{
	(void)argc; (void)args;
	for (int i = 2; i >= 0; i--)
		QuickMenuRebornUnregisterWidget(k_prof_id[i]);
	QuickMenuRebornUnregisterWidget(ID_PROF_PLANE);
	QuickMenuRebornUnregisterWidget(ID_PROF_TEXT);
	QuickMenuRebornUnregisterWidget(ID_KEEP_BOX);
	QuickMenuRebornUnregisterWidget(ID_KEEP_TEXT);
	QuickMenuRebornUnregisterWidget(ID_KEEP_PLANE);
	QuickMenuRebornUnregisterWidget(ID_TITLE);
	QuickMenuRebornRemoveSeparator(ID_SEPARATOR);
	return SCE_KERNEL_STOP_SUCCESS;
}
