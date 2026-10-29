#ifndef C_TYPES_H_QM_REBORN
#define C_TYPES_H_QM_REBORN

#ifdef QM_REBORN_INTERNAL
#ifdef __SNC__
#include <kernel/types.h>
#else
#include <psp2/types.h>
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*ECallback)(const char *refID, SceInt32 hash, SceInt32 eventId, void *userDat);
typedef void (*VoidCallback)(const char *refID);

typedef struct
{
    ECallback function;
    void *userDat;
    SceInt32 id;
} CallbackData;

typedef enum
{
    button = 0,
    check_box,
    text,
    plane,
    slidebar,
    progressbar_touch,
    busyindicator,
} QMRWidgetType;

typedef enum
{
    //ON
    CHECKBOX_ON,
    //OFF
    CHECKBOX_OFF,
    //Previous saved state
    CHECKBOX_PREV_STATE
} CheckBoxState;

typedef struct
{
    float r;
    float g;
    float b;
    float a;
} widgetColor;

typedef struct
{
    float x;
    float y;
    float z;
    float w;
} vector4;

typedef struct
{
    union{
        char styleID[256];
        int hash;
    } StyleInfo;

    int useHash;

    char type[32];
} advancedData;

typedef struct
{
    char label[0x400];
    char refId[100];
    char parentRefId[100];
    char textureId[100];
    char textureBaseId[100];
    //Set weather the widget has a parent or not
    int hasParent;

    QMRWidgetType type;
    SceUInt32 hash;

    vector4 pos;
    widgetColor col;
    vector4 size;

    //Main Callbacks
    CallbackData *Callbacks;
    //Size of main callback array
    SceInt32 CallbackNum;

    VoidCallback OnLoad;
    VoidCallback OnDelete;

    //Meant for internal use only!
    VoidCallback OnSave;
    VoidCallback OnRecall;

    int isAdvanced;
    advancedData advancedData;

    //Actuall Widget Class Pointer, ONLY ACCESS WHEN QUICKMENU IS OPEN
    void *widget;

} widgetData;

vector4 QuickMenuRebornMakeWidgetVector4(float x, float y, float z, float w);
widgetColor QuickMenuRebornMakeWidgetColor(float r, float g, float b, float a);

#ifdef __cplusplus
}
#endif

#endif