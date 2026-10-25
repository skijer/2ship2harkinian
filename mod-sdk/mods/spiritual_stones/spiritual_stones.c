// The three spiritual stones as C-button items: each gives a passive speed buff while owned, and holding its
// button plants an owl statue that a tap later warps back to.

#include <string.h>

#include "z64items.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"

#define MOD_NAME "spiritual_stones"
#define STATUE_ACTOR_KEY "spiritual_stones.statue"
#define STORAGE_FIELD "warps"

#define STONE_HOLD_FRAMES 60
#define STATUE_SCALE 0.005f
#define STATUE_GLOW_HEIGHT 62.0f
#define STATUE_GLOW_SCALE 0.006f
#define STONE_WALK_BOOST 1.5f
#define STONE_SWIM_BOOST 2.0f
#define STONE_CLIMB_BOOST 2.0f

typedef enum {
    STONE_KOKIRI,
    STONE_GORON,
    STONE_ZORA,
    STONE_MAX,
} Stone;

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

static const ALIGN_ASSET(2) char sStoneNames[STONE_MAX][72] = {
    "__OTR__textures/item_name_static/gKokiriEmeraldItemNameENGTex",
    "__OTR__textures/item_name_static/gGoronsRubyItemNameENGTex",
    "__OTR__textures/item_name_static/gZorasSapphireItemNameENGTex",
};

static const ALIGN_ASSET(2) char sFlashDL[] = "__OTR__objects/gameplay_keep/gEffFlash1DL";
static const ALIGN_ASSET(2) char sOwlStatueDL[] = "__OTR__objects/object_sek/gOwlStatueOpenedDL";

// The statue's display list branches into segment 0x0C for a cull list nothing binds here, so it is pointed at a
// dead end.
static const Gfx sEmptySegment[4] = {
    gsSPEndDisplayList(),
    gsSPEndDisplayList(),
    gsSPEndDisplayList(),
    gsSPEndDisplayList(),
};

static const StoneInfo sStones[STONE_MAX] = {
    { "nei.spiritual_stone.kokiri", sStoneIcons[STONE_KOKIRI], sStoneNames[STONE_KOKIRI],
      "You got the %gKokiri's Emerald%w!\nThe forest quickens your step\nwhile you carry it.\x10%rHold%w its "
      "%yC button%w to plant an owl statue\nhere, and tap it to return to it.",
      "%gKokiri's Emerald\n%wA toggles walking speed. Hold its %yC button%w to\nplant a warp, tap it to return." },
    { "nei.spiritual_stone.goron", sStoneIcons[STONE_GORON], sStoneNames[STONE_GORON],
      "You got the %rGoron's Ruby%w!\nThe mountain steadies your climb\nwhile you carry it.\x10%rHold%w its "
      "%yC button%w to plant an owl statue\nhere, and tap it to return to it.",
      "%rGoron's Ruby\n%wA toggles climbing speed. Hold its %yC button%w to\nplant a warp, tap it to return." },
    { "nei.spiritual_stone.zora", sStoneIcons[STONE_ZORA], sStoneNames[STONE_ZORA],
      "You got the %bZora's Sapphire%w!\nThe lake carries you while you\ncarry it.\x10%rHold%w its %yC button%w to "
      "plant an owl\nstatue here, and tap it to return to it.",
      "%bZora's Sapphire\n%wA toggles swimming speed. Hold its %yC button%w to\nplant a warp, tap it to return." },
};

static const Color_RGB8 sStoneGlow[STONE_MAX] = {
    { 0, 255, 100 },
    { 255, 60, 60 },
    { 60, 160, 255 },
};

static const u16 sItemButtons[S2H_ITEM_BUTTON_COUNT] = { BTN_B,   BTN_CLEFT, BTN_CDOWN, BTN_CRIGHT, 0,
                                                         BTN_DRIGHT, BTN_DLEFT, BTN_DDOWN, BTN_DUP };

static const char* const sRequiredHooks[] = {
    "OnPlayerUpdate",
    "OnPlayerResolveMotionScale",
    "OnSceneInit",
    "OnLoadFile",
    "OnSceneSpawnActors",
};
static const S2HModRequirements sRequirements = { sizeof(S2HModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const S2HModApi* sApi;
static SavedState sSaved;
static uint8_t sDisabledSpeedMask;
static int16_t sStatueActorId = -1;
static bool sHasOwlStatue;
static bool sIsVoidDamageWaived;
static uint8_t sStoneHoldFrames[STONE_MAX];
static Actor* sStatues[STONE_MAX];
static int8_t sPendingWarpStone = -1;

static bool IsPlayerBusy(Player* player, PlayState* play) {
    return player == NULL || Message_GetState(&play->msgCtx) != TEXT_STATE_NONE ||
           play->pauseCtx.state != PAUSE_STATE_OFF || Player_InCsMode(play) ||
           (player->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_TALKING)) != 0;
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
    sDisabledSpeedMask=0;
    sApi->StorageGet(MOD_NAME,"speed-effects-disabled",&sDisabledSpeedMask,sizeof(sDisabledSpeedMask));
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

// MM has no quest flag for the stones, so the registry's owned state is the only record of them.
static bool IsStoneOwned(uint8_t stone) {
    return sApi->IsCustomItemOwned(sStones[stone].key);
}

static bool StoneSpeedActive(const char* key) {
    for(uint8_t i=0;i<STONE_MAX;++i)if(strcmp(key,sStones[i].key)==0)return !(sDisabledSpeedMask & (1<<i));
    return true;
}
static void ToggleStoneSpeed(const char* key) {
    for(uint8_t i=0;i<STONE_MAX;++i)if(strcmp(key,sStones[i].key)==0) {
        sDisabledSpeedMask ^= (1<<i);
        sApi->StorageSet(MOD_NAME,"speed-effects-disabled",&sDisabledSpeedMask,sizeof(sDisabledSpeedMask));
        break;
    }
}
static bool IsStoneSpeedOn(uint8_t stone) {return IsStoneOwned(stone) && StoneSpeedActive(sStones[stone].key);}

static bool IsStoneWarpReady(uint8_t stone) {
    return sHasOwlStatue && sSaved.warp[stone].entranceIndex >= 0;
}

// Replanting moves the one waypoint that stone has, so the statue standing on the old spot goes with it.
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
    Audio_PlaySfx_AtPos(&player->actor.world.pos, NA_SE_SY_GET_ITEM);
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

static void ResolveWarpPrompt(PlayState* play) {
    if (sPendingWarpStone < 0 || Message_GetState(&play->msgCtx) != TEXT_STATE_CHOICE ||
        !Message_ShouldAdvance(play)) {
        return;
    }
    uint8_t stone = (uint8_t)sPendingWarpStone;

    Message_CloseTextbox(play);
    if (play->msgCtx.choiceIndex == 0) {
        WarpToStatue(play, stone);
    }
    sPendingWarpStone = -1;
}

static void OpenWarpPrompt(PlayState* play, uint8_t stone) {
    if (!sApi->ShowTextbox(play, "Return to your waypoint?\n\xC2%gYes\nNo%w", true)) {
        return;
    }
    sPendingWarpStone = (int8_t)stone;
}

static void TickStoneButtons(Player* player, PlayState* play) {
    for (uint8_t stone = 0; stone < STONE_MAX; stone++) {
        uint16_t button = FindEquippedButtonMask(sStones[stone].key);

        if (button == 0 || !IsStoneOwned(stone)) {
            sStoneHoldFrames[stone] = 0;
            continue;
        }
        if (play->state.input[0].cur.button & button) {
            if (sStoneHoldFrames[stone] < STONE_HOLD_FRAMES && ++sStoneHoldFrames[stone] == STONE_HOLD_FRAMES &&
                sHasOwlStatue) {
                PlantStatue(play, player, stone);
            }
            continue;
        }
        if (sStoneHoldFrames[stone] > 0 && sStoneHoldFrames[stone] < STONE_HOLD_FRAMES && sPendingWarpStone < 0 &&
            IsStoneWarpReady(stone)) {
            OpenWarpPrompt(play, stone);
        }
        sStoneHoldFrames[stone] = 0;
    }
}

// Scale the fresh movement target, never the velocity retained from the previous frame.
static void ScaleStoneMovement(Player* player, s32 kind, f32* scale) {
    if (kind != SOH_PLAYER_MOTION_STICK_SPEED || gPlayState == NULL || IsPlayerBusy(player, gPlayState)) return;
    if (IsStoneSpeedOn(STONE_ZORA) && (player->stateFlags1 & PLAYER_STATE1_8000000)) {
        *scale *= STONE_SWIM_BOOST;
    } else if (IsStoneSpeedOn(STONE_KOKIRI) && (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) {
        *scale *= STONE_WALK_BOOST;
    }
}

static void ApplyStoneBuffs(Player* player) {
    if (IsStoneSpeedOn(STONE_GORON) && (player->stateFlags1 & PLAYER_STATE1_200000)) {
        f32 speed = player->skelAnime.playSpeed;
        player->skelAnime.playSpeed = CLAMP(speed * STONE_CLIMB_BOOST, -STONE_CLIMB_BOOST, STONE_CLIMB_BOOST);
    }
}

static void StatueDraw(Actor* thisx, PlayState* play) {
    Color_RGB8 glow = sStoneGlow[thisx->params % STONE_MAX];

    OPEN_DISPS(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x0C, (uintptr_t)sEmptySegment);
    Gfx_DrawDListOpa(play, (Gfx*)sOwlStatueDL);
    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    gDPSetPrimColor(POLY_XLU_DISP++, 0x80, 0x80, glow.r, glow.g, glow.b, 200);
    gDPSetEnvColor(POLY_XLU_DISP++, glow.r / 2, glow.g / 2, glow.b / 2, 255);
    Matrix_Translate(thisx->world.pos.x, thisx->world.pos.y + STATUE_GLOW_HEIGHT, thisx->world.pos.z, MTXMODE_NEW);
    Matrix_RotateYS((int16_t)(play->gameplayFrames * 0x400), MTXMODE_APPLY);
    Matrix_Scale(STATUE_GLOW_SCALE, STATUE_GLOW_SCALE, STATUE_GLOW_SCALE, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sFlashDL);
    CLOSE_DISPS(play->state.gfxCtx);
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
    memset(sStoneHoldFrames, 0, sizeof(sStoneHoldFrames));
    memset(sStatues, 0, sizeof(sStatues));
    sPendingWarpStone = -1;
}

static void ReloadState(s32 fileNum) {
    LoadState();
}

static void TickMod(Player* player) {
    PlayState* play = gPlayState;

    if (play == NULL || player == NULL) {
        return;
    }
    ResolveWarpPrompt(play);
    if (IsPlayerBusy(player, play)) {
        return;
    }
    TickStoneButtons(player, play);
    ApplyStoneBuffs(player);
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

static bool RegisterStone(Stone stone) {
    S2HCustomItemDefinition jewel = Z64Items_Define(sStones[stone].key, sStones[stone].icon, sStones[stone].name);

    static const char* const loaderIcons[] = {
        "__OTR__textures/oot_item_icons/gItemIconKokiriEmeraldTex",
        "__OTR__textures/oot_item_icons/gItemIconGoronRubyTex",
        "__OTR__textures/oot_item_icons/gItemIconZoraSapphireTex",
    };
    if (sApi->HasResource(loaderIcons[stone])) jewel.iconPath = loaderIcons[stone];
    static const char* const loaderNames[] = {
        "__OTR__textures/oot_item_name_static/gKokiriEmeraldItemNameENGTex",
        "__OTR__textures/oot_item_name_static/gGoronsRubyItemNameENGTex",
        "__OTR__textures/oot_item_name_static/gZorasSapphireItemNameENGTex",
    };
    static const char* const loaderModels[] = {
        "__OTR__objects/oot_loader_models/gKokiriEmeraldDL",
        "__OTR__objects/oot_loader_models/gGoronRubyDL",
        "__OTR__objects/oot_loader_models/gZoraSapphireDL",
    };
    if (sApi->HasResource(loaderNames[stone])) {
        jewel.namePath = loaderNames[stone];
        Z64Items_SetGetItemModel(&jewel, loaderModels[stone]);
    }
    Z64Items_SetButtons(&jewel, S2H_CUSTOM_ITEM_C_BUTTON | S2H_CUSTOM_ITEM_DPAD);
    Z64Items_SetPlacement(&jewel, S2H_ITEM_PAGE_QUEST_VANILLA + 1, (uint8_t)(18 + stone), 0);
    Z64Items_SetTextbox(&jewel, sStones[stone].getItemText);
    Z64Items_SetPauseText(&jewel, sStones[stone].pauseText);
    jewel.flags |= S2H_CUSTOM_ITEM_INSTANT;
    jewel.onPauseToggle=ToggleStoneSpeed;
    jewel.isPauseActive=StoneSpeedActive;
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
    for (uint8_t stone = 0; stone < STONE_MAX; stone++) {
        if (!RegisterStone((Stone)stone)) {
            return;
        }
    }
    LoadState();
    sApi->RegisterVB(VB_INFLICT_VOID_DAMAGE, WaiveVoidDamage);
    S2H_REGISTER_HOOK(sApi, OnPlayerUpdate, TickMod);
    S2H_REGISTER_HOOK(sApi, OnPlayerResolveMotionScale, ScaleStoneMovement);
    S2H_REGISTER_HOOK(sApi, OnSceneInit, ForgetSceneState);
    S2H_REGISTER_HOOK(sApi, OnLoadFile, ReloadState);
    S2H_REGISTER_HOOK(sApi, OnSceneSpawnActors, PlantStatuesForScene);
}
