#include <string.h>

#include "z64items.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "2s2h/ModApi/Player/PlayerHookTypes.h"

#define MOD_NAME "spiritual_stones"
#define STATUE_ACTOR_KEY "spiritual_stones.statue"
#define STORAGE_FIELD "warps"
#define STORAGE_PASSIVES_FIELD "speed-effects-disabled"

#define STATUE_SCALE 0.005f
#define STATUE_JEWEL_HEIGHT 62.0f
#define STATUE_JEWEL_SCALE 0.10f
#define STATUE_JEWEL_SPIN 0x100
#define STONE_WALK_BOOST 1.5f
#define STONE_SWIM_BOOST 2.0f
#define STONE_CLIMBING_STATE PLAYER_STATE1_200000
#define STONE_SWIMMING_STATE PLAYER_STATE1_8000000
#define SILVER_TEXT "\x07"
#define GREEN_TEXT "\x02"

typedef enum {
    STONE_KOKIRI,
    STONE_GORON,
    STONE_ZORA,
    STONE_MAX,
} Stone;

typedef enum {
    WARP_CHOICE_PLANT,
    WARP_CHOICE_TRAVEL,
    WARP_CHOICE_NOTHING,
} WarpChoice;

typedef struct {
    const char* key;
    const char* icon;
    const char* name;
    const char* getItemText;
    const char* pauseText;
} StoneInfo;

typedef struct {
    int32_t entranceIndex;
    int16_t sceneId;
    int16_t rotY;
    int8_t roomIndex;
    Vec3f pos;
} StoneWarp;

typedef struct {
    StoneWarp warp[STONE_MAX];
} SavedState;

static const ALIGN_ASSET(2) char sStoneIcons[STONE_MAX][64] = {
    "__OTR__textures/icon_item_custom/gItemIconKokiriEmeraldTex",
    "__OTR__textures/icon_item_custom/gItemIconGoronRubyTex",
    "__OTR__textures/icon_item_custom/gItemIconZoraSapphireTex",
};

static const ALIGN_ASSET(2) char sPassiveOffIcons[STONE_MAX][64] = {
    "__OTR__textures/icon_item_custom/gStonePassiveOff0Tex",
    "__OTR__textures/icon_item_custom/gStonePassiveOff1Tex",
    "__OTR__textures/icon_item_custom/gStonePassiveOff2Tex",
};

static const ALIGN_ASSET(2) char sStoneNames[STONE_MAX][72] = {
    "__OTR__textures/item_name_static/gKokiriEmeraldItemNameENGTex",
    "__OTR__textures/item_name_static/gGoronsRubyItemNameENGTex",
    "__OTR__textures/item_name_static/gZorasSapphireItemNameENGTex",
};

static const ALIGN_ASSET(2) char sOotStoneNames[STONE_MAX][72] = {
    "__OTR__textures/oot_item_name_static/gKokiriEmeraldItemNameENGTex",
    "__OTR__textures/oot_item_name_static/gGoronsRubyItemNameENGTex",
    "__OTR__textures/oot_item_name_static/gZorasSapphireItemNameENGTex",
};

static const ALIGN_ASSET(2) char sGemDLs[STONE_MAX][64] = {
    "__OTR__objects/oot_object_gi_jewel/gGiKokiriEmeraldGemDL",
    "__OTR__objects/oot_object_gi_jewel/gGiGoronRubyGemDL",
    "__OTR__objects/oot_object_gi_jewel/gGiZoraSapphireGemDL",
};

static const ALIGN_ASSET(2) char sSettingDLs[STONE_MAX][64] = {
    "__OTR__objects/oot_object_gi_jewel/gGiKokiriEmeraldSettingDL",
    "__OTR__objects/oot_object_gi_jewel/gGiGoronRubySettingDL",
    "__OTR__objects/oot_object_gi_jewel/gGiZoraSapphireSettingDL",
};

static const Color_RGB8 sGemPrim[STONE_MAX] = {
    { 255, 255, 160 },
    { 255, 170, 255 },
    { 50, 255, 255 },
};

static const Color_RGB8 sGemEnv[STONE_MAX] = {
    { 0, 255, 0 },
    { 255, 0, 100 },
    { 50, 0, 150 },
};

static const ALIGN_ASSET(2) char sOwlStatueDL[] = "__OTR__objects/object_sek/gOwlStatueOpenedDL";

// The statue's display list branches into segment 0x0C for a cull list nothing binds here.
static const Gfx sEmptySegment[4] = {
    gsSPEndDisplayList(),
    gsSPEndDisplayList(),
    gsSPEndDisplayList(),
    gsSPEndDisplayList(),
};

static const StoneInfo sStones[STONE_MAX] = {
    { "nei.spiritual_stone.kokiri", sStoneIcons[STONE_KOKIRI], sStoneNames[STONE_KOKIRI],
      "You got the %gKokiri's Emerald%w!&The forest quickens your step&while you carry it.^"
      "Press %y\xB2%w to choose a new warp&point or return to your saved&owl statue.",
      "%gKokiri's Emerald&%wYou walk faster; %y\xB0%w toggles it.&Press %y\xB2%w for the warp point menu." },
    { "nei.spiritual_stone.goron", sStoneIcons[STONE_GORON], sStoneNames[STONE_GORON],
      "You got the %rGoron's Ruby%w! The&mountain steadies your climb&while you carry it.^"
      "Press %y\xB2%w to choose a new warp&point or return to your saved&owl statue.",
      "%rGoron's Ruby&%wYou climb faster; %y\xB0%w toggles it.&Press %y\xB2%w for the warp point menu." },
    { "nei.spiritual_stone.zora", sStoneIcons[STONE_ZORA], sStoneNames[STONE_ZORA],
      "You got the %bZora's Sapphire%w!&The lake carries you while you&carry it.^"
      "Press %y\xB2%w to choose a new warp&point or return to your saved&owl statue.",
      "%bZora's Sapphire&%wYou swim faster; %y\xB0%w toggles it.&Press %y\xB2%w for the warp point menu." },
};

static const char* const sStoneLabels[STONE_MAX] = { "Kokiri's Emerald", "Goron's Ruby", "Zora's Sapphire" };

static const u16 sItemButtons[S2H_ITEM_BUTTON_COUNT] = { BTN_B,      BTN_CLEFT, BTN_CDOWN, BTN_CRIGHT, 0,
                                                         BTN_DRIGHT, BTN_DLEFT, BTN_DDOWN, BTN_DUP };

static const char* const sRequiredHooks[] = {
    "OnPlayerUpdate", "OnPlayerResolveMotionScale", "OnGameFrameUpdate",
    "OnSceneInit",    "OnLoadFile",                 "OnSceneSpawnActors",
};
static const S2HModRequirements sRequirements = { sizeof(S2HModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const S2HModApi* sApi;
static SavedState sSaved;
static uint8_t sDisabledSpeedMask;
static int16_t sStatueActorId = -1;
static bool sHasOwlStatue;
static bool sHasJewelModel;
static bool sIsVoidDamageWaived;
static Actor* sStatues[STONE_MAX];
static int8_t sPendingWarpStone = -1;
static u16 sWarpPromptTextId;
static const char* sShownIcons[STONE_MAX];
static const char* sBaseIcons[STONE_MAX];

void Player_Action_50(Player* player, PlayState* play);

static bool IsPlayerBusy(Player* player, PlayState* play) {
    return player == NULL || Message_GetState(&play->msgCtx) != TEXT_STATE_NONE ||
           play->pauseCtx.state != PAUSE_STATE_OFF || Player_InCsMode(play) ||
           (player->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_TALKING)) != 0;
}

static void PlaySfxAt(Player* player, u16 sfxId) {
    Audio_PlaySfx_AtPos(&player->actor.projectedPos, sfxId);
}

static void StoreState(void) {
    sApi->StorageSet(MOD_NAME, STORAGE_FIELD, &sSaved, sizeof(sSaved));
}

static void LoadState(void) {
    memset(&sSaved, 0, sizeof(sSaved));
    for (uint8_t stone = 0; stone < STONE_MAX; stone++) {
        sSaved.warp[stone].entranceIndex = -1;
        sSaved.warp[stone].sceneId = -1;
    }
    sApi->StorageGet(MOD_NAME, STORAGE_FIELD, &sSaved, sizeof(sSaved));
    sDisabledSpeedMask = 0;
    sApi->StorageGet(MOD_NAME, STORAGE_PASSIVES_FIELD, &sDisabledSpeedMask, sizeof(sDisabledSpeedMask));
}

static uint16_t FindEquippedButtonMask(const char* key) {
    for (uint8_t button = 0; button < S2H_ITEM_BUTTON_COUNT; button++) {
        const char* equipped = sApi->GetEquippedCustomItem(button);

        if (equipped != NULL && strcmp(equipped, key) == 0) {
            return sItemButtons[button];
        }
    }
    return 0;
}

static int8_t FindStone(const char* key) {
    for (uint8_t stone = 0; stone < STONE_MAX; stone++) {
        if (strcmp(key, sStones[stone].key) == 0) {
            return (int8_t)stone;
        }
    }
    return -1;
}

static bool IsStoneOwned(uint8_t stone) {
    return sApi->IsCustomItemOwned(sStones[stone].key);
}

static bool IsPassiveEnabled(uint8_t stone) {
    return !(sDisabledSpeedMask & (1 << stone));
}

static bool IsPassiveKeyEnabled(const char* key) {
    int8_t stone = FindStone(key);

    return stone < 0 || IsPassiveEnabled((uint8_t)stone);
}

static void TogglePassive(const char* key) {
    int8_t stone = FindStone(key);

    if (stone < 0) {
        return;
    }
    sDisabledSpeedMask ^= (1 << stone);
    sApi->StorageSet(MOD_NAME, STORAGE_PASSIVES_FIELD, &sDisabledSpeedMask, sizeof(sDisabledSpeedMask));
}

static bool IsPassiveOn(uint8_t stone) {
    return IsStoneOwned(stone) && IsPassiveEnabled(stone);
}

// The off icons belong to the pause screen alone; the item buttons keep the stone's own art.
static void ShowPassiveIcons(void) {
    bool isPaused = gPlayState != NULL && gPlayState->pauseCtx.state != PAUSE_STATE_OFF;

    for (uint8_t stone = 0; stone < STONE_MAX; stone++) {
        const char* icon = isPaused && !IsPassiveEnabled(stone) ? sPassiveOffIcons[stone] : sBaseIcons[stone];

        if (icon != sShownIcons[stone] && sApi->SetCustomItemIconPath(sStones[stone].key, icon)) {
            sShownIcons[stone] = icon;
        }
    }
}

static bool IsStoneWarpReady(uint8_t stone) {
    return sHasOwlStatue && sSaved.warp[stone].entranceIndex >= 0;
}

static void SpawnStatue(PlayState* play, uint8_t stone) {
    const StoneWarp* warp = &sSaved.warp[stone];

    if (sStatueActorId < 0 || sStatues[stone] != NULL) {
        return;
    }
    sStatues[stone] = Actor_Spawn(&play->actorCtx, play, sStatueActorId, warp->pos.x, warp->pos.y, warp->pos.z, 0,
                                  warp->rotY, 0, stone);
}

static void RemoveStatue(uint8_t stone) {
    if (sStatues[stone] == NULL) {
        return;
    }
    if (sStatues[stone]->update != NULL) {
        Actor_Kill(sStatues[stone]);
    }
    sStatues[stone] = NULL;
}

static void PlantStatue(PlayState* play, Player* player, uint8_t stone) {
    StoneWarp* warp = &sSaved.warp[stone];

    warp->entranceIndex = gSaveContext.save.entrance;
    warp->sceneId = play->sceneId;
    warp->roomIndex = play->roomCtx.curRoom.num;
    warp->rotY = player->actor.shape.rot.y;
    warp->pos = player->actor.world.pos;
    StoreState();
    RemoveStatue(stone);
    SpawnStatue(play, stone);
    PlaySfxAt(player, NA_SE_SY_GET_ITEM);
}

// respawnFlag 1 is the "you fell in a hole" landing, the only one that honours a hand-written position,
// so the void damage it carries is waived once, for this transition alone.
static void WaiveVoidDamage(bool* should, va_list args) {
    if (!sIsVoidDamageWaived) {
        return;
    }
    sIsVoidDamageWaived = false;
    *should = false;
}

static void WarpToStatue(PlayState* play, uint8_t stone) {
    const StoneWarp* warp = &sSaved.warp[stone];
    RespawnData* respawn = &gSaveContext.respawn[RESPAWN_MODE_DOWN];

    play->nextEntrance = warp->entranceIndex;
    play->transitionTrigger = TRANS_TRIGGER_START;
    play->transitionType = TRANS_TYPE_FADE_BLACK;
    gSaveContext.nextTransitionType = TRANS_TYPE_FADE_BLACK_FAST;
    respawn->entrance = warp->entranceIndex;
    respawn->roomIndex = warp->roomIndex;
    respawn->pos = warp->pos;
    respawn->yaw = warp->rotY;
    respawn->playerParams = 0xDFF;
    gSaveContext.respawnFlag = 1;
    sIsVoidDamageWaived = true;
}

static void RefuseMissingWarp(Player* player, PlayState* play) {
    PlaySfxAt(player, NA_SE_SY_ERROR);
    play->state.input[0].press.button &= ~(BTN_A | BTN_B | BTN_CUP);
    play->state.input[0].cur.button &= ~BTN_B;
}

static void ResolveWarpPrompt(Player* player, PlayState* play) {
    if (sPendingWarpStone < 0) {
        return;
    }
    if (play->msgCtx.currentTextId != sWarpPromptTextId || play->msgCtx.msgMode == MSGMODE_NONE) {
        sPendingWarpStone = -1;
        return;
    }
    if (Message_GetState(&play->msgCtx) != TEXT_STATE_CHOICE || !Message_ShouldAdvance(play)) {
        return;
    }
    uint8_t stone = (uint8_t)sPendingWarpStone;
    uint8_t choice = play->msgCtx.choiceIndex;
    if (choice == WARP_CHOICE_TRAVEL && !IsStoneWarpReady(stone)) {
        RefuseMissingWarp(player, play);
        return;
    }
    Message_CloseTextbox(play);
    sPendingWarpStone = -1;
    if (choice == WARP_CHOICE_PLANT && sHasOwlStatue) {
        PlantStatue(play, player, stone);
    } else if (choice == WARP_CHOICE_TRAVEL) {
        WarpToStatue(play, stone);
    }
}

// Without a saved statue the middle choice stays listed in silver and refuses the press.
static void OpenWarpPrompt(PlayState* play, uint8_t stone) {
    const char* text = IsStoneWarpReady(stone)
                           ? "What do you want to do?\n\xC3%gNew Warp Point\nGo to Warp Point\nNothing%w"
                           : "What do you want to do?\n\xC3%gNew Warp Point\n" SILVER_TEXT
                             "Go to Warp Point\n" GREEN_TEXT "Nothing%w";

    if (!sApi->ShowTextbox(play, text, true)) {
        return;
    }
    sPendingWarpStone = (int8_t)stone;
    sWarpPromptTextId = play->msgCtx.currentTextId;
}

static void TickStoneButtons(PlayState* play) {
    if (sPendingWarpStone >= 0) {
        return;
    }
    for (uint8_t stone = 0; stone < STONE_MAX; stone++) {
        uint16_t button = FindEquippedButtonMask(sStones[stone].key);

        if (button != 0 && IsStoneOwned(stone) && (play->state.input[0].press.button & button)) {
            OpenWarpPrompt(play, stone);
            return;
        }
    }
}

static bool IsClimbing(Player* player) {
    return (player->stateFlags1 & STONE_CLIMBING_STATE) != 0;
}

static void ScaleStoneMovement(Player* player, s32 kind, f32* scale) {
    if (gPlayState == NULL || IsPlayerBusy(player, gPlayState)) {
        return;
    }
    bool isSwimming = (player->stateFlags1 & STONE_SWIMMING_STATE) != 0;
    if (kind == SOH_PLAYER_MOTION_WATER_SPEED && isSwimming && IsPassiveOn(STONE_ZORA)) {
        *scale *= STONE_SWIM_BOOST;
        return;
    }
    bool isWalkingKind = kind == SOH_PLAYER_MOTION_STICK_SPEED || kind == SOH_PLAYER_MOTION_SPEED_CAP;
    if (isWalkingKind && !isSwimming && !IsClimbing(player) && (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) &&
        IsPassiveOn(STONE_KOKIRI)) {
        *scale *= STONE_WALK_BOOST;
    }
}

// The climbing action runs twice in one frame, so Link covers twice the wall.
static void BoostStoneClimb(bool* should, va_list args) {
    Player* player = va_arg(args, Player*);
    PlayState* play = gPlayState;

    if (!*should || play == NULL || player != GET_PLAYER(play) || IsPlayerBusy(player, play) ||
        !IsPassiveOn(STONE_GORON) || !IsClimbing(player) || player->actionFunc != Player_Action_50) {
        return;
    }
    *should = false;
    Player_Action_50(player, play);
    if (player->actionFunc == Player_Action_50 && IsClimbing(player)) {
        Player_Action_50(player, play);
    }
}

// OoT GetItem_DrawJewel. The gem calls segment 0x09 and the setting 0x08: left unbound, both still hold the
// texture paths of Link's face and the renderer runs them as display lists.
static void DrawJewel(PlayState* play, Stone stone) {
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    Color_RGB8 prim = sGemPrim[stone];
    Color_RGB8 env = sGemEnv[stone];

    OPEN_DISPS(gfxCtx);
    Matrix_Push();
    Matrix_RotateZYX(0, -0x4000, 0x4000, MTXMODE_APPLY);
    Gfx_SetupDL25_Xlu(gfxCtx);
    gSPSegment(POLY_XLU_DISP++, 0x09, (uintptr_t)Gfx_TwoTexScroll(gfxCtx, 0, 0, 255, 64, 64, 1, 0, 255, 16, 16));
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, gfxCtx);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 128, prim.r, prim.g, prim.b, 255);
    gDPSetEnvColor(POLY_XLU_DISP++, env.r, env.g, env.b, 255);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sGemDLs[stone]);
    Gfx_SetupDL25_Opa(gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)Gfx_TexScroll(gfxCtx, 0, 0, 16, 16));
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, gfxCtx);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 128, 255, 255, 170, 255);
    gDPSetEnvColor(POLY_OPA_DISP++, 150, 120, 0, 255);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sSettingDLs[stone]);
    Matrix_Pop();
    CLOSE_DISPS(gfxCtx);
}

static void DrawStoneGetItem(PlayState* play, S2HGetItemEntry* entry) {
    DrawJewel(play, (Stone)(entry->drawItemId % STONE_MAX));
}

static void StatueDraw(Actor* thisx, PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x0C, (uintptr_t)sEmptySegment);
    Gfx_DrawDListOpa(play, (Gfx*)sOwlStatueDL);
    CLOSE_DISPS(play->state.gfxCtx);
    if (!sHasJewelModel) {
        return;
    }
    Matrix_Push();
    Matrix_Translate(thisx->world.pos.x, thisx->world.pos.y + STATUE_JEWEL_HEIGHT, thisx->world.pos.z, MTXMODE_NEW);
    Matrix_RotateYS((s16)(play->gameplayFrames * STATUE_JEWEL_SPIN), MTXMODE_APPLY);
    Matrix_Scale(STATUE_JEWEL_SCALE, STATUE_JEWEL_SCALE, STATUE_JEWEL_SCALE, MTXMODE_APPLY);
    DrawJewel(play, (Stone)(thisx->params % STONE_MAX));
    Matrix_Pop();
}

static void StatueInit(Actor* thisx, PlayState* play) {
    Actor_SetScale(thisx, STATUE_SCALE);
    thisx->gravity = 0.0f;
    thisx->shape.shadowDraw = NULL;
}

static void StatueUpdate(Actor* thisx, PlayState* play) {
}

static void PlantStatuesForScene(void) {
    PlayState* play = gPlayState;

    if (play == NULL) {
        return;
    }
    for (uint8_t stone = 0; stone < STONE_MAX; stone++) {
        if (IsStoneWarpReady(stone) && sSaved.warp[stone].sceneId == play->sceneId) {
            SpawnStatue(play, stone);
        }
    }
}

static void ForgetSceneState(s16 sceneId, s8 spawnNum) {
    memset(sStatues, 0, sizeof(sStatues));
    sPendingWarpStone = -1;
}

static void ReloadState(s32 fileNum) {
    LoadState();
}

static void TickMod(Player* player) {
    PlayState* play = gPlayState;

    if (play == NULL || player == NULL || player != GET_PLAYER(play)) {
        return;
    }
    ResolveWarpPrompt(player, play);
    if (IsPlayerBusy(player, play)) {
        return;
    }
    TickStoneButtons(play);
}

static bool RegisterStatueActor(void) {
    S2HActorDefinition definition = { 0 };

    definition.structSize = sizeof(definition);
    definition.key = STATUE_ACTOR_KEY;
    definition.description = "Spiritual Stone Owl Statue";
    definition.category = ACTORCAT_PROP;
    definition.actorFlags = ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    definition.objectId = GAMEPLAY_KEEP;
    definition.instanceSize = 0;
    definition.init = StatueInit;
    definition.update = StatueUpdate;
    definition.draw = StatueDraw;
    sStatueActorId = sApi->RegisterActor(&definition);
    return sStatueActorId >= 0;
}

static bool HasJewelModels(void) {
    for (uint8_t stone = 0; stone < STONE_MAX; stone++) {
        if (!sApi->HasResource(sGemDLs[stone]) || !sApi->HasResource(sSettingDLs[stone])) {
            return false;
        }
    }
    return true;
}

static bool RegisterStone(Stone stone) {
    S2HCustomItemDefinition jewel = Z64Items_Define(sStones[stone].key, sStones[stone].icon, sStones[stone].name);
    const S2HCustomItemRandomizer randomizer = {
        sizeof(S2HCustomItemRandomizer), 0, S2H_CUSTOM_ITEM_TYPE_ITEM, 0, 1, sStones[stone].key, sStones[stone].key, "",
    };

    if (sApi->HasResource(sOotStoneNames[stone])) {
        jewel.namePath = sOotStoneNames[stone];
    }
    if (sHasJewelModel) {
        jewel.getItemEntry.drawFunc = DrawStoneGetItem;
        jewel.getItemEntry.drawItemId = (uint16_t)stone;
    }
    Z64Items_SetButtons(&jewel, S2H_CUSTOM_ITEM_C_BUTTON | S2H_CUSTOM_ITEM_DPAD);
    Z64Items_SetPlacement(&jewel, S2H_ITEM_PAGE_QUEST_VANILLA + 1, (uint8_t)(18 + stone), 0);
    Z64Items_SetTextbox(&jewel, sStones[stone].getItemText);
    Z64Items_SetPauseText(&jewel, sStones[stone].pauseText);
    jewel.flags |= S2H_CUSTOM_ITEM_INSTANT;
    jewel.onPauseToggle = TogglePassive;
    jewel.isPauseActive = IsPassiveKeyEnabled;
    jewel.displayName = sStoneLabels[stone];
    Z64Items_SetLogic(&jewel, &randomizer);
    sBaseIcons[stone] = jewel.iconPath;
    sShownIcons[stone] = jewel.iconPath;
    return Z64Items_Register(sApi, &jewel);
}

S2H_MOD_EXPORT void ModSetApi(const S2HModApi* api) {
    sApi = api;
}

S2H_MOD_EXPORT const S2HModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

S2H_MOD_EXPORT void ModInit(void) {
    if (!S2H_MOD_API_HAS(sApi, HasResource)) {
        return;
    }
    sHasOwlStatue = sApi->HasResource(sOwlStatueDL) != 0 && RegisterStatueActor();
    sHasJewelModel = HasJewelModels();
    for (uint8_t stone = 0; stone < STONE_MAX; stone++) {
        if (!RegisterStone((Stone)stone)) {
            return;
        }
    }
    LoadState();
    sApi->RegisterVB(VB_INFLICT_VOID_DAMAGE, WaiveVoidDamage);
    sApi->RegisterVB(VB_EXECUTE_PLAYER_ACTION_FUNC, BoostStoneClimb);
    S2H_REGISTER_HOOK(sApi, OnPlayerUpdate, TickMod);
    S2H_REGISTER_HOOK(sApi, OnPlayerResolveMotionScale, ScaleStoneMovement);
    S2H_REGISTER_HOOK(sApi, OnSceneInit, ForgetSceneState);
    S2H_REGISTER_HOOK(sApi, OnLoadFile, ReloadState);
    S2H_REGISTER_HOOK(sApi, OnSceneSpawnActors, PlantStatuesForScene);
    if (S2H_MOD_API_HAS(sApi, SetCustomItemIconPath)) {
        S2H_REGISTER_HOOK(sApi, OnGameFrameUpdate, ShowPassiveIcons);
    }
}
