#ifndef UNBOUND_Z64FORMMASKS_H
#define UNBOUND_Z64FORMMASKS_H

// The form masks' page over the pause Masks screen, reached with L like the equipment pages.
// Every form mod places its mask on it; whichever loads first registers the page.

#include "z64items.h"

#include "macros.h"
#include "variables.h"
#include <libultraship/bridge/consolevariablebridge.h>

#define Z64FORMMASKS_PAGE_KEY "nei.form_masks"
#define Z64FORMMASKS_PAGE (S2H_ITEM_PAGE_EQUIPMENT_FIRST + 2)
#define Z64FORMMASKS_ROWS 4
#define Z64FORMMASKS_COLUMNS 6
#define Z64FORMMASKS_FIRST_DPAD_BUTTON 3

static inline bool Z64FormMasks_HasLayoutPages(const S2HModApi* api) {
    return S2H_MOD_API_HAS(api, RegisterLayoutPage) && S2H_MOD_API_HAS(api, PlaceLayoutItem);
}

// Without equip controls the host draws the native sword and shield in the page's empty first column.
static inline void Z64FormMasks_RegisterPage(const S2HModApi* api) {
    const S2HLayoutPage page = { sizeof(S2HLayoutPage),
                                 Z64FORMMASKS_PAGE_KEY,
                                 "Form Masks",
                                 S2H_LAYOUT_EQUIPMENT,
                                 Z64FORMMASKS_ROWS,
                                 Z64FORMMASKS_COLUMNS,
                                 0,
                                 NULL,
                                 NULL,
                                 Z64FORMMASKS_PAGE,
                                 S2H_LAYOUT_FIXED_PAGE | S2H_LAYOUT_EQUIP_CONTROLS,
                                 NULL };

    api->RegisterLayoutPage(&page);
}

// Assumes the mask is registered. The cell is the mask's slot on the vanilla six-column grid.
static inline bool Z64FormMasks_PlaceMask(const S2HModApi* api, const char* maskKey, uint8_t cell) {
    if (!Z64FormMasks_HasLayoutPages(api)) {
        return false;
    }
    Z64FormMasks_RegisterPage(api);
    return api->PlaceLayoutItem(maskKey, Z64FORMMASKS_PAGE_KEY, cell);
}

// A page with equip controls hands its button presses to the item; item button 4 is A, so the D-pad skips it.
static inline void Z64FormMasks_AssignToPressedButton(const S2HModApi* api, const char* maskKey) {
    static const uint16_t buttons[] = { BTN_CLEFT, BTN_CDOWN, BTN_CRIGHT, BTN_DRIGHT, BTN_DLEFT, BTN_DDOWN, BTN_DUP };

    if (gPlayState == NULL || !S2H_MOD_API_HAS(api, EquipCustomItem)) {
        return;
    }
    const uint16_t pressed = CONTROLLER1(&gPlayState->state)->press.button;
    const bool canUseDpad = CVarGetInteger("gEnhancements.Dpad.DpadEquips", 0) != 0;
    for (uint8_t i = 0; i < ARRAY_COUNT(buttons); i++) {
        if (i >= Z64FORMMASKS_FIRST_DPAD_BUTTON && !canUseDpad) {
            return;
        }
        if (CHECK_BTN_ALL(pressed, buttons[i])) {
            api->EquipCustomItem(i < Z64FORMMASKS_FIRST_DPAD_BUTTON ? i + 1 : i + 2, maskKey);
            return;
        }
    }
}

#endif
