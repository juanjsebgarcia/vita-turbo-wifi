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
#include <string.h>

#include "qmr.h"

#include "../include/wifiturbo.h"
#include "../include/wifiturbo_kernel_api.h"
#include "../settings_app/wifiturbo_i18n.h"

/* QuickMenuReborn keeps every client's widgets in one list keyed by these ids,
 * so they carry our prefix. */
#define ID_SEPARATOR   "wifiturbo_qm_sep"
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

/* "Wi-Fi Turbo - <row>": the product name is the same in every language. */
static const char *prefixed(char *buf, int cap, int row)
{
	const char *parts[3] = { wq_i18n(WT_I18N_TITLE), " - ", wq_i18n(row) };
	int n = 0;
	for (int k = 0; k < 3; k++)
		for (const char *c = parts[k]; *c && n < cap - 1; c++)
			buf[n++] = *c;
	buf[n] = 0;
	return buf;
}

/* Labels in the current system language, set as each row is built so a
 * language change shows without a reboot. Only from a row's own OnLoad: while
 * the menu is open QuickMenuReborn applies a label to the live widget at once,
 * so a row that isn't built yet must not be touched. */
static const char *label_for(const char *id, char *buf, int cap)
{
	if (!strcmp(id, ID_KEEP_TEXT))
		return prefixed(buf, cap, WT_I18N_ROW_KEEP);
	if (!strcmp(id, ID_PROF_TEXT))
		return wq_i18n(WT_I18N_ROW_POWER);
	for (int i = 0; i < 3; i++)
		if (!strcmp(id, k_prof_id[i]))
			return wq_i18n(k_prof_text[i]);
	return "";
}

static void on_label_load(const char *id)
{
	char buf[128];   /* QuickMenuReborn copies each label */
	QuickMenuRebornSetWidgetLabel(id, label_for(id, buf, sizeof(buf)));
}

/* The menu is being built: show the current state (it may have changed in
 * Settings or over the command port since the menu was last open).
 *
 * The check box can't be set directly: QuickMenuRebornSetCheckBoxState passes
 * the state in the wrong argument and always unticks it. QuickMenuReborn's
 * default recall sets it correctly from its saved-state file, so store the
 * current value there before the box is built - this runs as the OnLoad of
 * its parent plane, which is created first. */
static void on_keep_load(const char *id)
{
	(void)id;
	WqUiState st;
	if (wqUiReadState(&st) >= 0)
		QuickMenuRebornSaveCheckBoxState(ID_KEEP_BOX, st.keepwifi ? 1 : 0);
}

/* Runs as the OnLoad of the last widget we register, when all of ours exist. */
static void on_menu_open(const char *id)
{
	on_label_load(id);
	WqUiState st;
	if (wqUiReadState(&st) >= 0)
		show_profile(st.profile);
}

static void on_keep_toggle(const char *id, SceInt32 hash, SceInt32 event, void *user)
{
	(void)id; (void)hash; (void)event; (void)user;
	int on = QuickMenuRebornGetCheckboxValue(ID_KEEP_BOX) > 0;   /* live widget: ticked */
	wqUiSubmit(on ? "keepwifi on" : "keepwifi off");
}

static void on_profile(const char *id, SceInt32 hash, SceInt32 event, void *user)
{
	(void)id; (void)hash; (void)event;
	int prof = (int)user;
	wqUiSubmit(k_prof_cmd[prof]);   /* reconnects Wi-Fi for a few seconds */
	show_profile(prof);             /* the published state follows within 1 s */
}

static void text_row(const char *id, const char *parent, float w, float h, float x)
{
	QuickMenuRebornRegisterWidget(id, parent, QMR_TEXT);
	QuickMenuRebornSetWidgetSize(id, w, h, 0, 0);
	QuickMenuRebornSetWidgetColor(id, 1, 1, 1, 1);
	QuickMenuRebornSetWidgetPosition(id, x, 0, 0, 0);
	QuickMenuRebornAssignOnLoadHandler(on_label_load, id);
}

static void container(const char *id, float h)
{
	QuickMenuRebornRegisterWidget(id, NULL, QMR_PLANE);
	QuickMenuRebornSetWidgetSize(id, QMR_ROW_WIDTH, h, 0, 0);
	QuickMenuRebornSetWidgetColor(id, 1, 1, 1, 0);   /* invisible container */
}

int module_start(SceSize argc, const void *args);
int _start(SceSize argc, const void *args) __attribute__ ((weak, alias("module_start")));

int module_start(SceSize argc, const void *args)
{
	(void)argc; (void)args;

	QuickMenuRebornSeparator(ID_SEPARATOR, QMR_SEPARATOR_HEIGHT);

	/* Keep Wi-Fi on: label left, check box right. QuickMenuReborn's text style
	 * centres text in its box, so the box is shifted left of the check box. */
	container(ID_KEEP_PLANE, 80);
	QuickMenuRebornAssignOnLoadHandler(on_keep_load, ID_KEEP_PLANE);
	text_row(ID_KEEP_TEXT, ID_KEEP_PLANE, 640, 75, -185);
	QuickMenuRebornRegisterWidget(ID_KEEP_BOX, ID_KEEP_PLANE, QMR_CHECK_BOX);
	QuickMenuRebornSetWidgetSize(ID_KEEP_BOX, 48, 48, 0, 0);
	QuickMenuRebornSetWidgetColor(ID_KEEP_BOX, 1, 1, 1, 1);
	QuickMenuRebornSetWidgetPosition(ID_KEEP_BOX, 350, 0, 0, 0);
	QuickMenuRebornAssignDefaultCheckBoxRecall(ID_KEEP_BOX);
	QuickMenuRebornRegisterEventHanlder(ID_KEEP_BOX, QMR_EVENT_RELEASE, on_keep_toggle, NULL);

	/* Power profile: a centred caption, then one button per profile. */
	text_row(ID_PROF_TEXT, NULL, QMR_ROW_WIDTH, 50, 0);
	container(ID_PROF_PLANE, PROF_BUTTON_H + 30);
	for (int i = 0; i < 3; i++) {
		QuickMenuRebornRegisterWidget(k_prof_id[i], ID_PROF_PLANE, QMR_BUTTON);
		QuickMenuRebornSetWidgetSize(k_prof_id[i], PROF_BUTTON_W, PROF_BUTTON_H, 0, 0);
		QuickMenuRebornSetWidgetPosition(k_prof_id[i], (i - 1) * PROF_BUTTON_GAP, 0, 0, 0);
		QuickMenuRebornRegisterEventHanlder(k_prof_id[i], QMR_EVENT_RELEASE, on_profile, (void *)i);
		QuickMenuRebornAssignOnLoadHandler(on_label_load, k_prof_id[i]);
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
	QuickMenuRebornRemoveSeparator(ID_SEPARATOR);
	return SCE_KERNEL_STOP_SUCCESS;
}
