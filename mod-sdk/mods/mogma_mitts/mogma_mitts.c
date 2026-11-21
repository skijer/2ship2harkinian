#include <math.h>
#include <stdarg.h>
#include <string.h>

#include "z64items.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"

#define MITTS_KEY "nei.mogma_mitts"
#define MITTS_MAGIC_PER_DRAIN 1
#define MITTS_DRAIN_INTERVAL 10
#define MITTS_GIVE_SCALE 0.5f
#define CLIMB_SHUFFLE_OPTION "oot.shuffle_climb"
#define CLIMB_SKILL_KEY "nei.skill.climb"
#define REMOTE_VISUAL_CHANNEL "nei.mogma_mitts.visual"
#define REMOTE_VISUAL_INTERVAL 5u

static const ALIGN_ASSET(2) char sIconTex[] = "__OTR__textures/icon_item_custom/gItemIconMogmaMittsTex";
static const ALIGN_ASSET(2) char sNameTex[] = "__OTR__textures/item_name_custom/gMogmaMittsNameTex";
static const ALIGN_ASSET(2) char sGiveDL[] = "__OTR__objects/object_nei_mogma_mitts/gMogmaMittsGiveDL";
static const ALIGN_ASSET(2) char sLeftPlateDL[] = "__OTR__objects/oot_object_link_boy/gLinkAdultLeftGauntletPlate1DL";
static const ALIGN_ASSET(2) char sRightPlateDL[] =
    "__OTR__objects/oot_object_link_boy/gLinkAdultRightGauntletPlate1DL";
static const ALIGN_ASSET(2) char sLeftOpenPlateDL[] =
    "__OTR__objects/oot_object_link_boy/gLinkAdultLeftGauntletPlate2DL";
static const ALIGN_ASSET(2) char sLeftFistPlateDL[] =
    "__OTR__objects/oot_object_link_boy/gLinkAdultLeftGauntletPlate3DL";
static const ALIGN_ASSET(2) char sRightOpenPlateDL[] =
    "__OTR__objects/oot_object_link_boy/gLinkAdultRightGauntletPlate2DL";
static const ALIGN_ASSET(2) char sRightFistPlateDL[] =
    "__OTR__objects/oot_object_link_boy/gLinkAdultRightGauntletPlate3DL";

static const char* const sRequiredHooks[] = { "OnGameFrameUpdate", "OnPlayerUpdate", "OnActorDrawEnd", "OnLoadGame" };
static const S2HModRequirements sRequirements = { sizeof(S2HModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

extern HOST_DATA void Player_Action_49(Player* player, PlayState* play);

typedef struct {
    int16_t scene;
    uint8_t active;
    uint8_t adult;
    uint8_t flags[8];
    float values[12];
} RemoteVisualState;

static const S2HModApi* sApi;
static bool sIsGripping;
static bool sIsClimbGrantRegistered;
static s16 sDrainTimer;
static s8 sPreviousInvincibility;
static s8 sHasGauntletModels = -1;
static bool sRemoteVisualReady;
static bool sRemoteVisualSent;
static uint32_t sRemoteVisualFrame;
static RemoteVisualState sLastRemoteVisual;

static bool IsRandoSave(void) {
    return gSaveContext.save.shipSaveInfo.saveType == SAVETYPE_RANDO;
}

// Climbing is only ever shuffled by oot_skills; without it the option reads 0.
static bool HasClimbAbility(void) {
    return !IsRandoSave() || sApi->GetRandoOption(CLIMB_SHUFFLE_OPTION) == 0 ||
           sApi->IsCustomItemOwned(CLIMB_SKILL_KEY);
}

static bool IsEquipped(void) {
    for (u8 button = 0; button < S2H_ITEM_BUTTON_COUNT; button++) {
        const char* equipped = sApi->GetEquippedCustomItem(button);
        if (equipped != NULL && strcmp(equipped, MITTS_KEY) == 0) {
            return true;
        }
    }
    return false;
}

static bool HasMagicToGrip(void) {
    return gSaveContext.save.saveInfo.playerData.isMagicAcquired &&
           gSaveContext.save.saveInfo.playerData.magic >= MITTS_MAGIC_PER_DRAIN;
}

static bool HasGauntletModels(void) {
    if (sHasGauntletModels < 0) {
        sHasGauntletModels = sApi->HasResource(sLeftPlateDL) && sApi->HasResource(sRightPlateDL) &&
                             sApi->HasResource(sLeftOpenPlateDL) && sApi->HasResource(sLeftFistPlateDL) &&
                             sApi->HasResource(sRightOpenPlateDL) && sApi->HasResource(sRightFistPlateDL);
    }
    return sHasGauntletModels;
}

static void SetWorn(bool isWorn) {
    if (S2H_MOD_API_HAS(sApi, SetCustomItemWorn)) {
        sApi->SetCustomItemWorn(MITTS_KEY, isWorn);
    }
}

static void GrantClimbing(bool* should, va_list args) {
    PlayState* play = gPlayState;
    s32* stickX = va_arg(args, s32*);
    s32* stickY = va_arg(args, s32*);

    if (!sIsGripping || play == NULL || HasClimbAbility()) {
        return;
    }
    *stickX = play->state.input[0].rel.stick_x;
    *stickY = play->state.input[0].rel.stick_y;
}

// Registered on first use, after every mod's ModInit, so it runs after the climb restriction it undoes.
static void RegisterClimbGrant(void) {
    if (sIsClimbGrantRegistered) {
        return;
    }
    sApi->RegisterVB(VB_CLIMB, GrantClimbing);
    sIsClimbGrantRegistered = true;
}

static void StartGripping(Player* player) {
    RegisterClimbGrant();
    sIsGripping = true;
    SetWorn(true);
    sDrainTimer = 0;
    sPreviousInvincibility = player->invincibilityTimer;
    Audio_PlaySfx_AtPos(&player->actor.world.pos, NA_SE_SY_LOCK_ON);
}

static void StopGripping(void) {
    sIsGripping = false;
    SetWorn(false);
}

static void ToggleGrip(PlayState* play, Player* player) {
    if (sIsGripping) {
        StopGripping();
        Audio_PlaySfx_AtPos(&player->actor.world.pos, NA_SE_SY_CANCEL);
        return;
    }
    if (!HasMagicToGrip()) {
        Audio_PlaySfx_AtPos(&player->actor.world.pos, NA_SE_SY_ERROR);
        return;
    }
    StartGripping(player);
}

static void GripAnySurface(bool* should, va_list args) {
    if (sIsGripping) {
        *should = true;
    }
}

// A wall climbed past its top has no ledge floor under Link, so the climb-up is held on the ground.
static void KeepGripOnLedge(bool* should, va_list args) {
    Player* player = va_arg(args, Player*);

    if (sIsGripping && gPlayState != NULL && player == GET_PLAYER(gPlayState) &&
        player->actionFunc == Player_Action_49) {
        player->actor.bgCheckFlags |= BGCHECKFLAG_GROUND;
    }
}

static bool WasJustHurt(Player* player) {
    bool isHurt = player->invincibilityTimer > 0 && sPreviousInvincibility == 0;

    sPreviousInvincibility = player->invincibilityTimer;
    return isHurt;
}

static void DrainMagic(Player* player) {
    if (++sDrainTimer < MITTS_DRAIN_INTERVAL) {
        return;
    }
    sDrainTimer = 0;
    if (!HasMagicToGrip()) {
        StopGripping();
        Audio_PlaySfx_AtPos(&player->actor.world.pos, NA_SE_SY_ERROR);
        return;
    }
    gSaveContext.save.saveInfo.playerData.magic -= MITTS_MAGIC_PER_DRAIN;
}

static void UpdateMitts(Player* player) {
    if (!sIsGripping || player == NULL) {
        return;
    }
    if (!IsEquipped() || WasJustHurt(player)) {
        StopGripping();
        return;
    }
    DrainMagic(player);
}

static void StopGrippingOnLoad(s32 fileNum) {
    StopGripping();
}

// OoT's plates follow the forearm and hand matrices, which only human Link's skeleton lays out like OoT's.
static bool AreLocalGauntletsVisible(Player* player) {
    return sIsGripping && player->transformation == PLAYER_FORM_HUMAN && HasGauntletModels() &&
           !(player->stateFlags1 & PLAYER_STATE1_100000);
}

static bool ReadRemoteVisual(Player* player, RemoteVisualState* visual) {
    if (!sRemoteVisualReady || gPlayState == NULL || player == GET_PLAYER(gPlayState)) {
        return false;
    }
    uint32_t peerId = sApi->GetNetPeerIdForActor(&player->actor);
    if (peerId == 0 ||
        sApi->GetNetPeerState(peerId, REMOTE_VISUAL_CHANNEL, visual, sizeof(*visual)) != sizeof(*visual)) {
        return false;
    }
    if (visual->active != 1 || visual->adult > 1 || visual->scene != gPlayState->sceneId) {
        return false;
    }
    for (uint32_t i = 0; i < ARRAY_COUNT(visual->values); i++) {
        if (!isfinite(visual->values[i]) || fabsf(visual->values[i]) > 10000000.0f) {
            return false;
        }
    }
    return HasGauntletModels();
}

static void DrawGauntlets(Actor* actor, PlayState* play) {
    Player* player = (Player*)actor;
    RemoteVisualState visual;
    bool isVisible =
        player == GET_PLAYER(play) ? AreLocalGauntletsVisible(player) : ReadRemoteVisual(player, &visual);

    if (!isVisible || (player->stateFlags2 & PLAYER_STATE2_20000000)) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetEnvColor(POLY_OPA_DISP++, 255, 255, 255, 0);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sLeftPlateDL);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sRightPlateDL);
    gSPDisplayList(POLY_OPA_DISP++,
                   (Gfx*)(player->leftHandType == PLAYER_MODELTYPE_LH_OPEN ? sLeftOpenPlateDL : sLeftFistPlateDL));
    gSPDisplayList(POLY_OPA_DISP++,
                   (Gfx*)(player->rightHandType == PLAYER_MODELTYPE_RH_OPEN ? sRightOpenPlateDL : sRightFistPlateDL));
    CLOSE_DISPS(play->state.gfxCtx);
}

static void ReceiveRemoteVisual(const S2HNetMessage* message) {
    (void)message;
}

static void PublishRemoteVisual(void) {
    if (!sRemoteVisualReady) {
        return;
    }
    RemoteVisualState visual = { 0 };
    PlayState* play = gPlayState;

    visual.scene = -1;
    if (play != NULL) {
        visual.scene = play->sceneId;
        visual.active = AreLocalGauntletsVisible(GET_PLAYER(play));
        if (sRemoteVisualSent && (uint32_t)(play->gameplayFrames - sRemoteVisualFrame) < REMOTE_VISUAL_INTERVAL) {
            return;
        }
    }
    if (sRemoteVisualSent && memcmp(&visual, &sLastRemoteVisual, sizeof(visual)) == 0) {
        return;
    }
    if (sApi->SetNetPeerState(REMOTE_VISUAL_CHANNEL, &visual, sizeof(visual))) {
        sLastRemoteVisual = visual;
        sRemoteVisualSent = true;
        sRemoteVisualFrame = play != NULL ? play->gameplayFrames : 0;
    }
}

static void DrawRemotePlayer(Actor* actor, PlayState* play) {
    if (actor->id == ACTOR_PLAYER || !sRemoteVisualReady || sApi->GetNetPeerIdForActor(actor) == 0) {
        return;
    }
    DrawGauntlets(actor, play);
}

static void InitRemoteVisuals(void) {
    if (!S2H_MOD_API_HAS(sApi, RegisterNetChannel) || !S2H_MOD_API_HAS(sApi, SetNetPeerState) ||
        !S2H_MOD_API_HAS(sApi, GetNetPeerState) || !S2H_MOD_API_HAS(sApi, GetNetPeerIdForActor)) {
        return;
    }
    sRemoteVisualReady = sApi->RegisterNetChannel(REMOTE_VISUAL_CHANNEL, 1, ReceiveRemoteVisual);
    if (sRemoteVisualReady) {
        S2H_REGISTER_HOOK(sApi, OnGameFrameUpdate, PublishRemoteVisual);
        S2H_REGISTER_HOOK(sApi, OnActorDrawEnd, DrawRemotePlayer);
    }
}

static const S2HCustomItemRandomizer sRandomizerItem = {
    sizeof(S2HCustomItemRandomizer), 0, S2H_CUSTOM_ITEM_TYPE_ITEM, 0, 1, MITTS_KEY, MITTS_KEY, "",
};

S2H_MOD_EXPORT void ModSetApi(const S2HModApi* api) {
    sApi = api;
}

S2H_MOD_EXPORT const S2HModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

S2H_MOD_EXPORT void ModInit(void) {
    InitRemoteVisuals();
    if (!S2H_MOD_API_HAS(sApi, RegisterCustomItem)) {
        return;
    }

    S2HCustomItemDefinition mitts = Z64Items_Define(MITTS_KEY, sIconTex, sNameTex);

    Z64Items_SetButtons(&mitts, S2H_CUSTOM_ITEM_C_BUTTON | S2H_CUSTOM_ITEM_DPAD);
    Z64Items_SetPlacement(&mitts, 0, 13, 0);
    Z64Items_SetTextbox(&mitts, "You got the %rMogma Mitts%w!&Digging claws of the Mogma&"
                                "tribe, tough enough to bite&into stone.^"
                                "Press %y\xB2%w to grip. Link climbs where&he couldn't, and grips %yany wall%w.^"
                                "The grip spends %gmagic%w while it&holds.");
    Z64Items_SetPauseText(&mitts, "%rMogma Mitts&%wPress %y\xB2%w to grip any wall.&"
                                  "Press again to stop. Drains magic.");
    Z64Items_SetAction(&mitts, ToggleGrip, NULL);
    Z64Items_SetGetItemModel(&mitts, sGiveDL);
    mitts.getItemScale = MITTS_GIVE_SCALE;
    mitts.flags |= S2H_CUSTOM_ITEM_INSTANT;

    Z64Items_SetLogic(&mitts, &sRandomizerItem);
    if (!Z64Items_Register(sApi, &mitts)) {
        return;
    }
    sApi->RegisterVB(VB_BE_CLIMBABLE_SURFACE, GripAnySurface);
    sApi->RegisterVB(VB_EXECUTE_PLAYER_ACTION_FUNC, KeepGripOnLedge);
    S2H_REGISTER_HOOK(sApi, OnPlayerUpdate, UpdateMitts);
    S2H_REGISTER_HOOK(sApi, OnLoadGame, StopGrippingOnLoad);
    S2H_REGISTER_HOOK_FOR_ID(sApi, OnActorDrawEnd, ACTOR_PLAYER, DrawGauntlets);
}
