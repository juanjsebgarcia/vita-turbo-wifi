/*
 * qmr.h - the QuickMenuReborn calls wifiturbo_qm.c makes.
 *
 * QuickMenuReborn (https://github.com/Ibrahim778/QuickMenuReborn, V2.5) is a
 * separately installed SceShell plugin that exports a "QuickMenuReborn"
 * library; only the functions used here are declared. qmr_imports.yml lists
 * their NIDs for the link stub.
 */
#ifndef WIFITURBO_QMR_H
#define WIFITURBO_QMR_H

#include <psp2/types.h>

/* Widget kinds for QuickMenuRebornRegisterWidget. */
#define QMR_BUTTON      0
#define QMR_CHECK_BOX   1
#define QMR_TEXT        2
#define QMR_PLANE       3

#define QMR_EVENT_RELEASE     0x10000008   /* button / check box released     */
#define QMR_ROW_WIDTH         835.0f       /* width of Sony's quick-menu rows */
#define QMR_SEPARATOR_HEIGHT  20.0f

typedef void (*QmrEventHandler)(const char *id, SceInt32 hash, SceInt32 event, void *user);
typedef void (*QmrIdHandler)(const char *id);

/* Widgets are keyed by string id; they are (re)built each time the menu opens,
 * in registration order, under `parent` (NULL = the menu itself). */
void *QuickMenuRebornRegisterWidget(const char *id, const char *parent, int kind);
int   QuickMenuRebornUnregisterWidget(const char *id);
int   QuickMenuRebornSeparator(const char *id, float height);
int   QuickMenuRebornRemoveSeparator(const char *id);

int   QuickMenuRebornSetWidgetSize(const char *id, float x, float y, float z, float w);
int   QuickMenuRebornSetWidgetPosition(const char *id, float x, float y, float z, float w);
int   QuickMenuRebornSetWidgetColor(const char *id, float r, float g, float b, float a);
int   QuickMenuRebornSetWidgetLabel(const char *id, const char *label);

int   QuickMenuRebornRegisterEventHanlder(const char *id, SceInt32 event, QmrEventHandler fn, void *user);
int   QuickMenuRebornAssignOnLoadHandler(QmrIdHandler fn, const char *id);

/* Check box: the live state while the menu is open, else the saved one. The
 * default recall ticks the box from the saved state as it is built. */
int   QuickMenuRebornGetCheckboxValue(const char *id);
int   QuickMenuRebornSaveCheckBoxState(const char *id, int state);
int   QuickMenuRebornAssignDefaultCheckBoxRecall(const char *id);

#endif /* WIFITURBO_QMR_H */
