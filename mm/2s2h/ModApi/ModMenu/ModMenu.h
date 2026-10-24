#ifndef S2H_MOD_MENU_H
#define S2H_MOD_MENU_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    S2H_MOD_MENU_CHECKBOX,
    S2H_MOD_MENU_INT_SLIDER,
    S2H_MOD_MENU_FLOAT_SLIDER,
    S2H_MOD_MENU_SEPARATOR,
    S2H_MOD_MENU_TEXT,
    S2H_MOD_MENU_CUSTOM,
    S2H_MOD_MENU_COMBOBOX,
} S2HModMenuWidgetType;

typedef void (*S2HModMenuCallback)(void);

typedef struct {
    uint32_t structSize;
    const char* section;
    const char* sidebar;
    uint8_t column;
    S2HModMenuWidgetType type;
    const char* label;
    const char* cvar;
    const char* tooltip;
    int32_t defaultInt;
    int32_t minInt;
    int32_t maxInt;
    float defaultFloat;
    float minFloat;
    float maxFloat;
    S2HModMenuCallback onChange;
    // Combobox entries; the cvar holds the chosen index.
    const char* const* values;
    uint32_t valueCount;
} S2HModMenuWidget;

#define S2H_MOD_MENU_WIDGET_MIN_SIZE (offsetof(S2HModMenuWidget, onChange) + sizeof(((S2HModMenuWidget*)0)->onChange))
#define S2H_MOD_MENU_WIDGET_COMBOBOX_SIZE \
    (offsetof(S2HModMenuWidget, valueCount) + sizeof(((S2HModMenuWidget*)0)->valueCount))

bool ModMenu_RegisterWidget(const S2HModMenuWidget* widget);
bool ModMenu_RegisterSidebar(const char* section, const char* sidebar, const char* selectionCvar, uint8_t columns);
bool ModMenu_RegisterWidgetAt(const S2HModMenuWidget* widget, const char* anchor, bool after);
bool ModUI_BeginTab(const char* label);
void ModUI_EndTab(void);
bool ModUI_BeginSection(const char* label);
void ModUI_EndSection(void);
void ModUI_Text(const char* text);
bool ModUI_Checkbox(const char* label, bool* value);
bool ModUI_InputInt(const char* label, int32_t* value);

#ifdef __cplusplus
}
#endif

#endif
