/**
 * Ocarina of Time strength for every form: each form keeps its own strength and the upgrades add to it.
 * Without the Power Bracelet under "Shuffle Grab" every form starts one level lower; a form that still
 * reaches the Human level grabs anyway. The same offsets live in Ship's form mods (z64strength.h).
 */

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "z64items.h"
#include "z64logic.h"
#include "2s2h/Rando/Types.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "overlays/actors/ovl_En_Bom/z_en_bom.h"
#include "2s2h/ModApi/RandoInfCustom/RandoInfCustom.h"
#include "z64rumble.h"
#include "overlays/actors/ovl_Door_Ana/z_door_ana.h"

#define BRACELET_KEY "oot.power_bracelet"
#define GORON_BRACELET_KEY "oot.goron_bracelet"
#define SILVER_GAUNTLETS_KEY "oot.silver_gauntlets"
#define GOLDEN_GAUNTLETS_KEY "oot.golden_gauntlets"
#define STRENGTH_CHAIN "oot.strength"
#define STRENGTH_PICKUP "oot.progressive.strength"
#define SHUFFLE_GRAB_OPTION "oot.shuffle_grab"
#define OOT_ICON(name) "__OTR__textures/oot_icon_item_static/" name
#define OOT_NAME(name) "__OTR__textures/oot_item_name_static/" name
#define OOT_GLOVES(name) "__OTR__objects/oot_object_gi_gloves/" name
// MM's NES textbox glyph for the A button.
#define GLYPH_A "\xB0"

static const ALIGN_ASSET(2) char sGrabIcon[] = "__OTR__textures/icon_item_static/gGrabTex";
static const ALIGN_ASSET(2) char sGrabDL[] = "__OTR__objects/object_gi_grab/gGiGrabDL";
static const ALIGN_ASSET(2) char sGoronBraceletDL[] = "__OTR__objects/oot_object_gi_bracelet/gGiGoronBraceletDL";
static const ALIGN_ASSET(2) char sGauntletsDL[] = OOT_GLOVES("gGiGauntletsDL");
static const ALIGN_ASSET(2) char sGauntletsPlateDL[] = OOT_GLOVES("gGiGauntletsPlateDL");
static const ALIGN_ASSET(2) char sSilverColorDL[] = OOT_GLOVES("gGiSilverGauntletsColorDL");
static const ALIGN_ASSET(2) char sGoldenColorDL[] = OOT_GLOVES("gGiGoldenGauntletsColorDL");
static const ALIGN_ASSET(2) char sSilverPlateColorDL[] = OOT_GLOVES("gGiSilverGauntletsPlateColorDL");
static const ALIGN_ASSET(2) char sGoldenPlateColorDL[] = OOT_GLOVES("gGiGoldenGauntletsPlateColorDL");

static const char* const sRequiredHooks[] = { "OnLoadFile", "OnSceneInit", "OnResolveCustomGetItem",
    "OnActorDestroy", "OnCustomItemReceive", "OnActorUpdate" };
static const S2HModRequirements sRequirements = { sizeof(S2HModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const S2HModApi* sApi;
static s32 sUnclampedStrength;
static bool sAreStrengthRulesRegistered;
static Actor* sStrengthPickupActor;
static const char* sStrengthPickupStage;

static void ResetStrengthPresentation(void) {
    sStrengthPickupActor=NULL;
    sStrengthPickupStage=NULL;
}

static const char* const sGauntletKeys[] = { GORON_BRACELET_KEY, SILVER_GAUNTLETS_KEY, GOLDEN_GAUNTLETS_KEY };

static bool IsGrabShuffled(void) {
    return gSaveContext.save.shipSaveInfo.saveType == SAVETYPE_RANDO &&
           sApi->GetRandoOption(SHUFFLE_GRAB_OPTION) != 0;
}

static bool HasGrab(void) {
    return !IsGrabShuffled() || sApi->IsCustomItemOwned(BRACELET_KEY);
}

static u16 GetGauntletLevel(void) {
    for (u16 level = ARRAY_COUNT(sGauntletKeys); level > 0; level--) {
        if (sApi->IsCustomItemOwned(sGauntletKeys[level - 1])) {
            return level;
        }
    }
    return 0;
}

// The custom forms ride the Human transformation, so only a bare Fierce Deity is the vanilla one.
static bool IsFierceDeity(void) {
    const char* customForm = sApi->GetActiveForm();
    return GET_PLAYER_FORM == PLAYER_FORM_FIERCE_DEITY && (customForm == NULL || customForm[0] == '\0');
}

// Registered after every ModInit, so the form mods have already written the form's own strength.
static void AddStrengthUpgrades(bool* should, va_list args) {
    s32* strength = va_arg(args, s32*);
    const s32 formStrength = IsFierceDeity() ? PLAYER_STRENGTH_GORON : *strength;

    sUnclampedStrength = formStrength + GetGauntletLevel() - (HasGrab() ? 0 : 1);
    *strength = CLAMP(sUnclampedStrength, PLAYER_STRENGTH_DEKU, PLAYER_STRENGTH_GORON);
}

static s32 GetUnclampedStrength(void) {
    Player_GetStrength();
    return sUnclampedStrength;
}

static bool IsGoronOnlyCarry(Actor* actor) {
    if (actor->id == ACTOR_EN_BOM) {
        return ((EnBom*)actor)->isPowderKeg;
    }
    return ((actor->id == ACTOR_EN_ISHI) && (actor->params & 1)) || (actor->id == ACTOR_EN_MM);
}

// Human strength grabs; what vanilla keeps for Gorons needs Goron strength instead.
static void DecideLift(bool* should, va_list args) {
    (void)va_arg(args, Player*);
    Actor* actor = va_arg(args, Actor*);
    const s32 required = IsGoronOnlyCarry(actor) ? PLAYER_STRENGTH_GORON : PLAYER_STRENGTH_HUMAN;

    *should = GetUnclampedStrength() >= required;
}

static void RestrictPowderKeg(bool* should, va_list args) {
    ItemId* item = va_arg(args, ItemId*);

    if (*item == ITEM_POWDER_KEG) {
        *should = GetUnclampedStrength() < PLAYER_STRENGTH_GORON;
    }
}

static void RegisterStrengthRules(void) {
    if (sAreStrengthRulesRegistered) {
        return;
    }
    sAreStrengthRulesRegistered = true;
    sApi->RegisterVB(VB_USE_STRENGTH_UPGRADE, AddStrengthUpgrades);
    sApi->RegisterVB(VB_ITEM_BE_RESTRICTED, RestrictPowderKeg);
    if (!sApi->RegisterVBByName("VB_PLAYER_LIFT_ACTOR", DecideLift)) {
        fprintf(stderr, "[oot_collectables] The host has no VB_PLAYER_LIFT_ACTOR; lifting stays vanilla\n");
    }
}

static bool CanCarryKegInLogic(void* userData) {
    return sApi->GetRandoOption(SHUFFLE_GRAB_OPTION) == 0 || Z64Logic_HasKey(BRACELET_KEY);
}

// Without the Power Bracelet a Goron drops to Human strength and cannot carry a Powder Keg.
static const int32_t sKegChecks[] = {
    RC_ENEMY_DROP_ALIEN,
    RC_GORON_RACETRACK_GOLD_DUST,
    RC_GORON_VILLAGE_MEDIGORON,
    RC_ROMANI_RANCH_ALIENS,
    RC_ROMANI_RANCH_BARN_COW_LEFT,
    RC_ROMANI_RANCH_BARN_COW_MIDDLE,
    RC_ROMANI_RANCH_BARN_COW_RIGHT,
    RC_ROMANI_RANCH_EPONAS_SONG,
    RC_ROMANI_RANCH_FIELD_COW_ENTRANCE,
    RC_ROMANI_RANCH_FIELD_COW_NEAR_HOUSE_BACK,
    RC_ROMANI_RANCH_FIELD_COW_NEAR_HOUSE_FRONT,
};

static const int32_t sKegEvents[] = {
    RE_COWS_FROM_ALIENS,
    RE_DESTROY_MILK_ROAD_BOULDER,
    RE_IKANA_CASTLE_MAIN_SUNLIGHT,
};

static void RegisterKegLogic(void) {
    S2HLogicSelector selectors[1 + ARRAY_COUNT(sKegEvents)];
    selectors[0] = Z64Logic_SelectAny();
    selectors[0].targets = S2H_LOGIC_TARGET_LOCATIONS;
    selectors[0].checks = sKegChecks;
    selectors[0].checkCount = ARRAY_COUNT(sKegChecks);
    for (u32 i = 0; i < ARRAY_COUNT(sKegEvents); i++) {
        selectors[1 + i] = Z64Logic_SelectAny();
        selectors[1 + i].targets = S2H_LOGIC_TARGET_EVENTS;
        selectors[1 + i].logicEvent = sKegEvents[i];
    }
    S2HLogicRule rule = { sizeof(S2HLogicRule) };
    rule.name = "oot_collectables.keg_needs_grab";
    rule.effect = S2H_LOGIC_REQUIRE;
    rule.condition = CanCarryKegInLogic;
    rule.include = selectors;
    rule.includeCount = ARRAY_COUNT(selectors);
    sApi->RegisterLogicRule(&rule);
}

static void RegisterRulesOnLoad(s32 fileNum) {
    ResetStrengthPresentation();
    RegisterStrengthRules();
}

static void RegisterRulesOnScene(s16 sceneId, s8 spawnNum) {
    ResetStrengthPresentation();
    RegisterStrengthRules();
}

// Progressive stage 1 is strength 0 (the ability to grab), not a gauntlet bonus.
static u16 ReadGauntletLevel(const SaveContext* save) {
    if(RandoInfCustom_IsLogicSimulation())return 0;
    u16 gauntlet=GetGauntletLevel();
    return gauntlet>0?gauntlet+1:(HasGrab()?1:0);
}

static u16 StrengthFirstLevel(void) {
    return IsGrabShuffled()?1:2;
}

static void ApplyGauntletLevel(SaveContext* save, u16 level) {
    if (RandoInfCustom_IsLogicSimulation()) {
        return;
    }
    sApi->SetCustomItemOwned(BRACELET_KEY,level>0);
    for (u16 i = 0; i < ARRAY_COUNT(sGauntletKeys); i++) {
        sApi->SetCustomItemOwned(sGauntletKeys[i], level > i+1);
    }
}

static void DrawOpa(PlayState* play, const char* displayList) {
    if (!sApi->HasResource(displayList)) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)displayList);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawPowerBracelet(PlayState* play, S2HGetItemEntry* entry) {
    DrawOpa(play, sGrabDL);
}

static void DrawGoronBracelet(PlayState* play, S2HGetItemEntry* entry) {
    DrawOpa(play, sGoronBraceletDL);
}

// OoT GetItem_DrawOpa10Xlu32: color then gloves in OPA, plate color then plate in XLU; ID 1 is golden.
static void DrawGauntlets(PlayState* play, S2HGetItemEntry* entry) {
    const bool isGolden = entry->drawItemId != 0;
    GraphicsContext* gfx = play->state.gfxCtx;

    if (!sApi->HasResource(sGauntletsDL)) {
        return;
    }
    OPEN_DISPS(gfx);
    Gfx_SetupDL25_Opa(gfx);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, gfx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)(isGolden ? sGoldenColorDL : sSilverColorDL));
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sGauntletsDL);
    Gfx_SetupDL25_Xlu(gfx);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, gfx);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)(isGolden ? sGoldenPlateColorDL : sSilverPlateColorDL));
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sGauntletsPlateDL);
    CLOSE_DISPS(gfx);
}

typedef struct {
    const char* key;
    const char* icon;
    const char* name;
    const char* label;
    const char* text;
    S2HGetItemDrawFunc draw;
    u16 drawItemId;
} StrengthItem;

static const StrengthItem sItems[] = {
    { BRACELET_KEY, sGrabIcon, NULL, "Power Bracelet",
      "You found the %rPower Bracelet%w!&Now you can grab and lift things&with " GLYPH_A ".", DrawPowerBracelet, 0 },
    { GORON_BRACELET_KEY, OOT_ICON("gItemIconGoronsBraceletTex"), OOT_NAME("gGoronsBraceletItemNameENGTex"),
      "Goron's Bracelet",
      "You got the %rGoron's Bracelet%w!&Its strength adds to every form&you take.", DrawGoronBracelet, 0 },
    { SILVER_GAUNTLETS_KEY, OOT_ICON("gItemIconSilverGauntletsTex"), OOT_NAME("gSilverGauntletsItemNameENGTex"),
      "Silver Gauntlets",
      "You found the %rSilver Gauntlets%w!&You feel the power to lift big&things with " GLYPH_A "!",
      DrawGauntlets, 0 },
    { GOLDEN_GAUNTLETS_KEY, OOT_ICON("gItemIconGoldenGauntletsTex"), OOT_NAME("gGoldenGauntletsItemNameENGTex"),
      "Golden Gauntlets",
      "You found the %yGolden Gauntlets%w!&You can feel even more power&coursing through your arms!",
      DrawGauntlets, 1 },
};

static bool IsStrengthPickup(const char* key) {
    if(key==NULL)return false;
    if(!strcmp(key,STRENGTH_PICKUP) || !strcmp(key,BRACELET_KEY))return true;
    for(u32 i=0;i<ARRAY_COUNT(sGauntletKeys);i++)if(!strcmp(key,sGauntletKeys[i]))return true;
    return false;
}

static void DrawProgressiveStrength(PlayState* play,S2HGetItemEntry* entry) {
    const char* next=sApi->GetNextProgressiveItem(STRENGTH_PICKUP,S2H_RANDO_INF_SAVE);
    const S2HCustomItemDefinition* stage=next?sApi->FindCustomItem(next):NULL;
    if(stage && stage->getItemEntry.drawFunc && stage->getItemEntry.drawFunc!=DrawProgressiveStrength){
        S2HGetItemEntry drawing=stage->getItemEntry;
        drawing.drawFunc(play,&drawing);
    }
}

static void ResolveStrengthPickup(Actor* actor,PlayState* play,S2HGetItemEntry* entry,const char** key) {
    if(!key || !IsStrengthPickup(*key))return;
    bool held=actor && (actor->parent || (actor->id==ACTOR_EN_ITEM00 && actor->params==ITEM00_NOTHING &&
                                          ((EnItem00*)actor)->unk152>=0));
    const char* next=held && actor==sStrengthPickupActor?sStrengthPickupStage:
                     sApi->GetNextProgressiveItem(*key,S2H_RANDO_INF_SAVE);
    const S2HCustomItemDefinition* stage=next?sApi->FindCustomItem(next):NULL;
    if(!stage)return;
    if(held){sStrengthPickupActor=actor;sStrengthPickupStage=next;}
    *key=next;
    if(entry)*entry=stage->getItemEntry;
}

static void ForgetStrengthPickup(Actor* actor) {
    if(actor==sStrengthPickupActor)ResetStrengthPresentation();
}

static void PrepareStrengthText(const char* key) {
    if(RandoInfCustom_IsLogicSimulation() || !IsStrengthPickup(key))return;
    const char* next=sApi->GetNextProgressiveItem(key,S2H_RANDO_INF_SAVE);
    if(next)CustomItemRegistry_PrepareAcquisitionMessage(next);
}

static bool RegisterItem(const StrengthItem* item, const S2HCustomItemRandomizer* randomizer) {
    S2HCustomItemDefinition definition = Z64Items_Define(item->key, item->icon, item->name);

    definition.presentationFlags = S2H_ITEM_HIDE_FROM_KALEIDO | S2H_ITEM_HIDE_FROM_SAVE_EDITOR;
    definition.displayName = item->label;
    definition.getItemScale = 1.0f;
    definition.getItemEntry.drawFunc = item->draw;
    definition.getItemEntry.drawItemId = item->drawItemId;
    Z64Items_SetTextbox(&definition, item->text);
    if (randomizer != NULL) {
        Z64Items_SetLogic(&definition, randomizer);
    }
    return Z64Items_Register(sApi, &definition);
}

static const char* const sShuffleGrabValues[] = { "Off", "On" };
static const S2HModRandoOption sShuffleGrabOption = {
    sizeof(S2HModRandoOption), SHUFFLE_GRAB_OPTION, "Shuffle Grab",
    "Every form starts one strength level lower until the Power Bracelet is found.",
    sShuffleGrabValues, ARRAY_COUNT(sShuffleGrabValues), 0, S2H_MOD_RANDO_WIDGET_COMBOBOX,
};

static const S2HCustomItemRandomizer sBraceletPool = {
    .structSize = sizeof(S2HCustomItemRandomizer),
    .flags = S2H_CUSTOM_ITEM_RANDO_ADVANCEMENT | S2H_CUSTOM_ITEM_RANDO_PROGRESSIVE,
    .type = S2H_CUSTOM_ITEM_TYPE_ITEM,
    .poolCount = 1,
    .logicKey = BRACELET_KEY,
    .hintKey = BRACELET_KEY,
    .progressionGroup = STRENGTH_CHAIN,
    .poolModeOption = SHUFFLE_GRAB_OPTION,
    .poolModeValue = 1,
};

static const S2HCustomItemRandomizer sStrengthPool = {
    .structSize = sizeof(S2HCustomItemRandomizer),
    .flags = S2H_CUSTOM_ITEM_RANDO_ADVANCEMENT | S2H_CUSTOM_ITEM_RANDO_PROGRESSIVE,
    .type = S2H_CUSTOM_ITEM_TYPE_ITEM,
    .poolCount = ARRAY_COUNT(sGauntletKeys),
    .logicKey = STRENGTH_PICKUP,
    .hintKey = STRENGTH_PICKUP,
    .progressionGroup = STRENGTH_CHAIN,
};

static bool RegisterStrengthChain(void) {
    static const S2HProgressiveLevel levels[] = { { BRACELET_KEY }, { GORON_BRACELET_KEY },
                                                  { SILVER_GAUNTLETS_KEY }, { GOLDEN_GAUNTLETS_KEY } };
    // The pool keeps three upgrade copies plus the existing conditional Shuffle
    // Grab copy. All four identities now advance the same presentation chain.
    StrengthItem pickup = sItems[0];
    pickup.key = STRENGTH_PICKUP;
    pickup.label = "Progressive Strength";
    pickup.draw = DrawProgressiveStrength;
    if (!RegisterItem(&pickup, &sStrengthPool)) {
        return false;
    }
    const S2HProgressiveChain chain = {
        .structSize = sizeof(S2HProgressiveChain),
        .key = STRENGTH_CHAIN,
        .pickupKey = STRENGTH_PICKUP,
        .levels = levels,
        .levelCount = ARRAY_COUNT(levels),
        .firstLevel = 1,
        .readLevel = ReadGauntletLevel,
        .applyLevel = ApplyGauntletLevel,
        .readFirstLevel = StrengthFirstLevel,
    };
    return sApi->RegisterProgressiveChain(&chain);
}

S2H_MOD_EXPORT void ModSetApi(const S2HModApi* api) {
    sApi = api;
}

S2H_MOD_EXPORT const S2HModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

static void RumbleNearHiddenGrotto(Actor* actor) {
    if (actor->id != ACTOR_DOOR_ANA || !sApi->IsCustomItemOwned("oot.stone_of_agony") ||
        gPlayState == NULL || (gPlayState->gameplayFrames % 32) != 0) {
        return;
    }
    const s32 type = DOORANA_GET_TYPE(actor);
    if ((type != DOORANA_TYPE_HIDDEN_BOMB && type != DOORANA_TYPE_HIDDEN_STORMS) ||
        actor->scale.x != 0.0f || actor->xzDistToPlayer > 200.0f) {
        return;
    }
    Rumble_Request(actor->xyzDistToPlayerSq, 100, 8, 12);
}

static void RegisterStoneOfAgony(void) {
    static const S2HCustomItemRandomizer randomizer = {
        .structSize = sizeof(S2HCustomItemRandomizer),
        .type = S2H_CUSTOM_ITEM_TYPE_ITEM,
        .poolCount = 1,
        .logicKey = "oot.stone_of_agony",
        .hintKey = "oot.stone_of_agony",
    };
    S2HCustomItemDefinition item = Z64Items_Define("oot.stone_of_agony",
        "__OTR__textures/oot_icon_item_24_static/gQuestIconStoneOfAgonyTex", NULL);
    item.displayName = "Stone of Agony";
    item.pauseText = "%rStone of Agony&%wThe controller rumbles near a hidden grotto. No button is needed.";
    item.getItemText = "You got %rStone of Agony%w!&It makes the controller rumble near hidden grottos.";
    item.preferredPage = S2H_ITEM_PAGE_QUEST_VANILLA + 7;
    item.preferredSlot = 1;
    item.randomizer = &randomizer;
    Z64Items_SetGetItemModel(&item, "__OTR__objects/oot_object_gi_map/gGiStoneOfAgonyDL");
    Z64Items_Register(sApi, &item);
    S2H_REGISTER_HOOK(sApi, OnActorUpdate, RumbleNearHiddenGrotto);
}

static void RestrictPirateDetection(bool* should, va_list args) {
    if (gPlayState == NULL || !sApi->IsCustomItemOwned("oot.gerudo_card")) {
        return;
    }
    switch (gPlayState->sceneId) {
        case SCENE_PIRATE:
        case SCENE_KAIZOKU:
        case SCENE_TORIDE:
            *should = false;
            break;
        default:
            break;
    }
}

static void RegisterGerudoCard(void) {
    static const S2HCustomItemRandomizer randomizer = {
        .structSize = sizeof(S2HCustomItemRandomizer),
        .type = S2H_CUSTOM_ITEM_TYPE_ITEM,
        .poolCount = 1,
        .logicKey = "oot.gerudo_card",
        .hintKey = "oot.gerudo_card",
    };
    if (!S2H_MOD_API_HAS(sApi, RegisterVBByName) ||
        !sApi->RegisterVBByName("VB_PIRATE_DETECT_PLAYER", RestrictPirateDetection)) {
        return;
    }
    S2HCustomItemDefinition item = Z64Items_Define("oot.gerudo_card",
        "__OTR__textures/oot_icon_item_24_static/gQuestIconGerudosCardTex", NULL);
    item.displayName = "Gerudo Membership Card";
    item.pauseText = "%rGerudo Membership Card&%wCarry it past patrols in Pirates' Fortress.";
    item.getItemText = "You got %rGerudo Membership Card%w!&Patrols in Pirates' Fortress will let you pass.";
    item.preferredPage = S2H_ITEM_PAGE_QUEST_VANILLA + 7;
    item.preferredSlot = 2;
    item.randomizer = &randomizer;
    Z64Items_SetGetItemModel(&item, "__OTR__objects/oot_object_gi_gerudo/gGiGerudoCardDL");
    Z64Items_Register(sApi, &item);
}

S2H_MOD_EXPORT void ModInit(void) {
    if (!S2H_MOD_API_HAS(sApi, RegisterVBByName) || !S2H_MOD_API_HAS(sApi, RegisterProgressiveChain) ||
        !S2H_MOD_API_HAS(sApi, RegisterRandoOption) || !S2H_MOD_API_HAS(sApi, GetActiveForm)) {
        return;
    }
    sApi->RegisterRandoOption(&sShuffleGrabOption);
    if (S2H_MOD_API_HAS(sApi, RegisterLogicRule) && Z64Logic_Init(sApi)) {
        RegisterKegLogic();
    }
    RegisterStoneOfAgony();
    RegisterGerudoCard();
    if (!RegisterItem(&sItems[0], &sBraceletPool)) {
        return;
    }
    for (u32 i = 1; i < ARRAY_COUNT(sItems); i++) {
        if (!RegisterItem(&sItems[i], NULL)) {
            return;
        }
    }
    if (!RegisterStrengthChain()) {
        fprintf(stderr, "[oot_collectables] Cannot register %s\n", STRENGTH_CHAIN);
        return;
    }
    S2H_REGISTER_HOOK(sApi, OnResolveCustomGetItem, ResolveStrengthPickup);
    S2H_REGISTER_HOOK(sApi, OnActorDestroy, ForgetStrengthPickup);
    S2H_REGISTER_HOOK(sApi, OnCustomItemReceive, PrepareStrengthText);
    S2H_REGISTER_HOOK(sApi, OnLoadFile, RegisterRulesOnLoad);
    S2H_REGISTER_HOOK(sApi, OnSceneInit, RegisterRulesOnScene);
}
