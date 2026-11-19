#include <stdbool.h>
#include <stdint.h>

#include "global.h"
#include "2s2h/ModApi/ModApi.h"
#include "2s2h/ModApi/ModMenu/ModUI.h"
#include <libultraship/bridge/consolevariablebridge.h>

#define TRAP_SECTION "Rando"
#define TRAP_SIDEBAR "Item Pool"
#define TRAP_WIDGET "More Trap Types"
#define LAST_HOST_TRAP_WIDGET "Trap Types"
#define SHUFFLE_TRAPS_CVAR "gRando.Options.RO_SHUFFLE_TRAPS"
#define TELEPORT_CVAR "gRando.Traps.Teleport"

#define SLOW_FRAMES 200
#define SLOW_SCALE 0.5f

typedef enum {
    TELEPORT_OFF,
    TELEPORT_SIMPLE,
    TELEPORT_ADVANCED,
    TELEPORT_MODE_COUNT,
} TeleportMode;

typedef struct {
    const char* label;
    const char* cvar;
    const char* tooltip;
    void (*apply)(void);
    bool leavesScene;
    const char* const* modes;
} OotTrap;

static const char* const sTeleportModes[TELEPORT_MODE_COUNT] = { "Off", "Simple", "Advanced" };

static const u16 sOwlStatueEntrances[] = {
    ENTRANCE(GREAT_BAY_COAST, 11), ENTRANCE(ZORA_CAPE, 6),
    ENTRANCE(SNOWHEAD, 3),         ENTRANCE(MOUNTAIN_VILLAGE_WINTER, 8),
    ENTRANCE(SOUTH_CLOCK_TOWN, 9), ENTRANCE(MILK_ROAD, 4),
    ENTRANCE(WOODFALL, 4),         ENTRANCE(SOUTHERN_SWAMP_POISONED, 10),
    ENTRANCE(IKANA_CANYON, 4),     ENTRANCE(STONE_TOWER, 3),
};

static const u16 sAreaEntrances[] = {
    ENTRANCE(TERMINA_FIELD, 0),        ENTRANCE(EAST_CLOCK_TOWN, 0),    ENTRANCE(WEST_CLOCK_TOWN, 0),
    ENTRANCE(NORTH_CLOCK_TOWN, 0),     ENTRANCE(LAUNDRY_POOL, 0),       ENTRANCE(ROAD_TO_SOUTHERN_SWAMP, 0),
    ENTRANCE(DEKU_PALACE, 0),          ENTRANCE(WOODFALL_TEMPLE, 0),    ENTRANCE(PATH_TO_MOUNTAIN_VILLAGE, 0),
    ENTRANCE(GORON_VILLAGE_WINTER, 0), ENTRANCE(PATH_TO_SNOWHEAD, 0),   ENTRANCE(SNOWHEAD_TEMPLE, 0),
    ENTRANCE(ROMANI_RANCH, 0),         ENTRANCE(ZORA_HALL, 0),          ENTRANCE(PIRATES_FORTRESS_EXTERIOR, 0),
    ENTRANCE(GREAT_BAY_TEMPLE, 0),     ENTRANCE(ROAD_TO_IKANA, 0),      ENTRANCE(IKANA_GRAVEYARD, 0),
    ENTRANCE(BENEATH_THE_WELL, 0),     ENTRANCE(STONE_TOWER_TEMPLE, 0),
};

static const u8 sAmmoItems[] = { ITEM_DEKU_STICK, ITEM_DEKU_NUT, ITEM_BOW, ITEM_BOMB, ITEM_BOMBCHU };

static const char* const sRequiredHooks[] = { "OnTrapRoll", "OnPlayerUpdate", "OnPlayerResolveMotionScale",
                                              "OnLoadGame" };
static const S2HModRequirements sRequirements = { sizeof(S2HModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const S2HModApi* sApi;
static int32_t sSlowFrames;

static void SlowDown(void) {
    Audio_PlaySfx(NA_SE_EN_DAIGOLON_SLEEP1);
    sSlowFrames = SLOW_FRAMES;
}

static void VoidOut(void) {
    Audio_PlaySfx(NA_SE_EN_STALKIDS_LAUGH);
    // func_80169FDC is Play_TriggerRespawn: Link returns to the entrance he came in by.
    func_80169FDC(gPlayState);
}

static void HalveAmmo(void) {
    for (uint32_t i = 0; i < ARRAY_COUNT(sAmmoItems); i++) {
        AMMO(sAmmoItems[i]) = AMMO(sAmmoItems[i]) / 2;
    }
    Audio_PlaySfx(NA_SE_EV_FROG_CRY_0);
}

static void KillLink(void) {
    gSaveContext.save.saveInfo.playerData.health = 0;
}

static u16 PickTeleportEntrance(void) {
    uint32_t owlCount = ARRAY_COUNT(sOwlStatueEntrances);
    bool isAdvanced = CVarGetInteger(TELEPORT_CVAR, TELEPORT_OFF) == TELEPORT_ADVANCED;
    uint32_t pick = Rand_Next() % (owlCount + (isAdvanced ? ARRAY_COUNT(sAreaEntrances) : 0));
    return pick < owlCount ? sOwlStatueEntrances[pick] : sAreaEntrances[pick - owlCount];
}

static void Teleport(void) {
    Audio_PlaySfx(NA_SE_EN_STALKIDS_LAUGH);
    gPlayState->nextEntrance = PickTeleportEntrance();
    gPlayState->transitionTrigger = TRANS_TRIGGER_START;
    gPlayState->transitionType = TRANS_TYPE_FADE_BLACK;
    gSaveContext.nextTransitionType = TRANS_TYPE_FADE_BLACK;
}

static const OotTrap sTraps[] = {
    { "Speed", "gRando.Traps.Speed", "Halves Link's speed for 10 seconds.", SlowDown, false, NULL },
    { "Void", "gRando.Traps.Void", "Sends Link back to the entrance he came in by.", VoidOut, true, NULL },
    { "Ammo", "gRando.Traps.Ammo", "Halves Link's sticks, nuts, arrows, bombs and bombchus.", HalveAmmo, false, NULL },
    { "Death", "gRando.Traps.Kill", "Link's hearts drop to zero.", KillLink, false, NULL },
    { "Teleport", TELEPORT_CVAR,
      "Where a Teleport Trap can send Link. Off keeps it out of the trap pool.\n\n"
      "Simple: the owl statues.\n"
      "Advanced: the owl statues, the main areas and the temple entrances.",
      Teleport, true, sTeleportModes },
};

static bool CanLeaveScene(void) {
    return gPlayState != NULL && GET_PLAYER(gPlayState)->heldItemAction != PLAYER_IA_FISHING_ROD;
}

static void OfferTrapTypes(uint32_t* candidateCount, void (**effect)(void)) {
    const OotTrap* enabledTraps[ARRAY_COUNT(sTraps)];
    uint32_t enabledCount = 0;
    bool canLeaveScene = CanLeaveScene();
    for (uint32_t i = 0; i < ARRAY_COUNT(sTraps); i++) {
        if (CVarGetInteger(sTraps[i].cvar, 0) && (canLeaveScene || !sTraps[i].leavesScene)) {
            enabledTraps[enabledCount] = &sTraps[i];
            enabledCount++;
        }
    }
    if (enabledCount == 0) {
        return;
    }
    *candidateCount += enabledCount;
    uint32_t pick = Rand_Next() % *candidateCount;
    if (pick >= enabledCount) {
        return;
    }
    *effect = enabledTraps[pick]->apply;
}

static void SlowStickSpeed(Player* player, s32 kind, f32* scale) {
    if (sSlowFrames > 0 && player == GET_PLAYER(gPlayState)) {
        *scale *= SLOW_SCALE;
    }
}

static void CountDownSlow(Player* player) {
    if (sSlowFrames > 0 && player == GET_PLAYER(gPlayState)) {
        sSlowFrames--;
    }
}

static void ResetTraps(s32 fileNum) {
    sSlowFrames = 0;
}

static void DrawTrapOption(const OotTrap* trap) {
    int32_t value = CVarGetInteger(trap->cvar, 0);
    bool isChanged;
    if (trap->modes != NULL) {
        isChanged = ModUI_Combo(trap->label, &value, trap->modes, TELEPORT_MODE_COUNT);
    } else {
        bool isOn = value != 0;
        isChanged = ModUI_Checkbox(trap->label, &isOn);
        value = isOn;
    }
    if (ModUI_IsItemHovered()) {
        ModUI_SetTooltip(trap->tooltip);
    }
    if (isChanged) {
        CVarSetInteger(trap->cvar, value);
        CVarSave();
    }
}

static void DrawTrapOptions(void) {
    if (!CVarGetInteger(SHUFFLE_TRAPS_CVAR, 0) || !ModUI_BeginTable("ootTrapTypes", 2, 0, 0.0f, 0.0f)) {
        return;
    }
    for (uint32_t i = 0; i < ARRAY_COUNT(sTraps); i++) {
        ModUI_TableNextColumn();
        DrawTrapOption(&sTraps[i]);
    }
    ModUI_EndTable();
}

static void RegisterMenu(void) {
    if (!S2H_MOD_API_HAS(sApi, RegisterMenuWidgetAt)) {
        return;
    }
    S2HModMenuWidget widget = { sizeof(S2HModMenuWidget) };
    widget.section = TRAP_SECTION;
    widget.sidebar = TRAP_SIDEBAR;
    widget.type = S2H_MOD_MENU_CUSTOM;
    widget.label = TRAP_WIDGET;
    widget.onChange = DrawTrapOptions;
    sApi->RegisterMenuWidgetAt(&widget, LAST_HOST_TRAP_WIDGET, true);
}

S2H_MOD_EXPORT void ModSetApi(const S2HModApi* api) {
    sApi = api;
}

S2H_MOD_EXPORT const S2HModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

S2H_MOD_EXPORT void ModInit(void) {
    RegisterMenu();
    // By name: the SDK's host headers may predate OnTrapRoll, which ModGetRequirements still demands at load.
    sApi->RegisterHookByName("OnTrapRoll", (void*)OfferTrapTypes);
    S2H_REGISTER_HOOK(sApi, OnPlayerUpdate, CountDownSlow);
    S2H_REGISTER_HOOK_FOR_ID(sApi, OnPlayerResolveMotionScale, SOH_PLAYER_MOTION_STICK_SPEED, SlowStickSpeed);
    S2H_REGISTER_HOOK(sApi, OnLoadGame, ResetTraps);
}
