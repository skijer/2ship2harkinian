#include <math.h>
#include <string.h>

#include "z64items.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "overlays/actors/ovl_Door_Ana/z_door_ana.h"

#define SHOVEL_KEY "nei.shovel"
#define SHOVEL_GROTTO_RADIUS 120.0f
#define SHOVEL_ITEM_DROP_CHANCE 15.0f
#define SHOVEL_FAIRY_CHANCE 1.0f
#define SHOVEL_DIG_FRAME 25
#define SHOVEL_GIVE_SCALE 0.2f

static const ALIGN_ASSET(2) char sIconTex[] = "__OTR__textures/icon_item_custom/gItemIconShovelTex";
static const ALIGN_ASSET(2) char sNameTex[] = "__OTR__textures/item_name_custom/gShovelNameTex";
static const ALIGN_ASSET(2) char sDigAnim[] = "__OTR__misc/link_animetion/gPlayerAnim_nei_dampe_dig";
// The one mesh for both hand and pedestal. The held copy is the same geometry exported WITHOUT its Blender
// object transform, so it keeps a 0.16 squash on one axis that the archive copy already bakes in.
static const ALIGN_ASSET(2) char sShovelDL[] = "__OTR__objects/object_nei_shovel/gShovelGiveDL_opaque_dl";

static Color_RGBA8 sDustColor = { 139, 90, 43, 255 };

static const char* const sRequiredHooks[] = { "OnGameFrameUpdate", "OnActorDrawEnd", "OnPlayerUpdate",
                                              "OnPlayerResolveItemActionInit" };
static const S2HModRequirements sRequirements = { sizeof(S2HModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

#define REMOTE_VISUAL_INTERVAL 5u

typedef struct {
    int16_t scene;
    uint8_t active;
    uint8_t adult;
    uint8_t flags[8];
    float values[12];
} RemoteVisualState;

static bool ReadRemoteVisual(Player* player, RemoteVisualState* visual);
static void InitRemoteVisuals(void);

#define SHOVEL_HELD_SCALE 0.11f
#define SHOVEL_HELD_OFFSET_X 5.56f
#define SHOVEL_HELD_OFFSET_Y -1.49f
#define SHOVEL_HELD_OFFSET_Z -10.16f
#define SHOVEL_HELD_ROTATION_X 60.98f
#define SHOVEL_HELD_ROTATION_Y 35.6f
#define SHOVEL_HELD_ROTATION_Z 59.02f

static const S2HModApi* sApi;
static s16 sDigFrame;
static bool sIsDigPending;

extern PlayerAnimationHeader* ResourceMgr_LoadPlayerAnimAsHeader(const char* animPath);
void func_8083B1A0(Player* this, PlayState* play);
void Player_StopHorizontalMovement(Player* this);
s32 Player_SetAction(PlayState* play, Player* this, PlayerActionFunc actionFunc, s32 arg3);
void Player_Anim_PlayOnce(PlayState* play, Player* this, PlayerAnimationHeader* anim);
void DoorAna_WaitOpen(DoorAna* this, PlayState* play);

static void SpawnDust(PlayState* play, Vec3f* pos, u8 count) {
    Vec3f accel = { 0.0f, -1.0f, 0.0f };

    for (u8 i = 0; i < count; i++) {
        Vec3f dustPos = { pos->x + Rand_CenteredFloat(20.0f), pos->y + 3.0f, pos->z + Rand_CenteredFloat(20.0f) };
        Vec3f velocity = { Rand_CenteredFloat(6.0f), 6.0f + Rand_ZeroOne() * 4.0f, Rand_CenteredFloat(6.0f) };
        func_800B0EB0(play, &dustPos, &velocity, &accel, &sDustColor, &sDustColor, 250, 30, 10);
    }
}

static s16 RollDrop(void) {
    f32 roll = Rand_ZeroOne() * 100.0f;

    // Item_DropCollectible turns ITEM00_FLEXIBLE into a healing fairy.
    if (roll < SHOVEL_FAIRY_CHANCE) {
        return ITEM00_FLEXIBLE;
    }
    if (roll < 50.0f) {
        f32 rupeeRoll = Rand_ZeroOne();
        if (rupeeRoll < 0.70f) {
            return ITEM00_RUPEE_GREEN;
        }
        return rupeeRoll < 0.95f ? ITEM00_RUPEE_BLUE : ITEM00_RUPEE_RED;
    }
    if (roll < 75.0f) {
        f32 recoveryRoll = Rand_ZeroOne();
        if (recoveryRoll < 0.50f) {
            return ITEM00_RECOVERY_HEART;
        }
        return recoveryRoll < 0.75f ? ITEM00_MAGIC_JAR_SMALL : ITEM00_MAGIC_JAR_BIG;
    }
    return Rand_ZeroOne() < 0.6f ? ITEM00_BOMBS_A : ITEM00_DEKU_NUTS_1;
}

static void DropTreasure(PlayState* play, Vec3f* digPos) {
    Vec3f dropPos = { digPos->x, digPos->y + 10.0f, digPos->z };

    if (Rand_ZeroOne() * 100.0f > SHOVEL_ITEM_DROP_CHANCE) {
        return;
    }
    Item_DropCollectible(play, &dropPos, RollDrop());
    Audio_PlaySfx_AtPos(&dropPos, NA_SE_SY_GET_ITEM);
}

static void RevealGrotto(PlayState* play, DoorAna* grotto) {
    if (DOORANA_GET_TYPE(&grotto->actor) != DOORANA_TYPE_HIDDEN_BOMB) {
        return;
    }
    Collider_DestroyCylinder(play, &grotto->bombCollider);
    DOORANA_SET_TYPE(&grotto->actor, DOORANA_TYPE_VISIBLE);
    grotto->actionFunc = DoorAna_WaitOpen;
    grotto->actor.flags &= ~ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    Audio_PlaySfx(NA_SE_SY_CORRECT_CHIME);
    SpawnDust(play, &grotto->actor.world.pos, 12);
}

static void UncoverBuriedActors(PlayState* play, Vec3f* digPos) {
    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_ITEMACTION].first; actor != NULL; actor = actor->next) {
        if (actor->update == NULL || actor->id != ACTOR_DOOR_ANA) {
            continue;
        }
        if (Math_Vec3f_DistXYZ(digPos, &actor->world.pos) < SHOVEL_GROTTO_RADIUS) {
            RevealGrotto(play, (DoorAna*)actor);
        }
    }
}

static void Dig(Player* player, PlayState* play) {
    Vec3f digPos = { player->actor.world.pos.x + Math_SinS(player->actor.shape.rot.y) * 30.0f,
                     player->actor.floorHeight + 5.0f,
                     player->actor.world.pos.z + Math_CosS(player->actor.shape.rot.y) * 30.0f };

    SpawnDust(play, &digPos, 8);
    Audio_PlaySfx_AtPos(&digPos, NA_SE_PL_WALK_SAND);
    UncoverBuriedActors(play, &digPos);
    DropTreasure(play, &digPos);
}

static bool CanDig(Player* player, PlayState* play) {
    return !(player->stateFlags1 & PLAYER_STATE1_8000000) && !func_800B7118(player) &&
           (player->meleeWeaponState == PLAYER_MELEE_WEAPON_STATE_0) &&
           (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND);
}

static void DigAction(Player* player, PlayState* play);

static bool IsDigging(Player* player) {
    return player->actionFunc == (PlayerActionFunc)DigAction;
}

static void StopDigging(Player* player, PlayState* play) {
    sIsDigPending = false;
    if (!IsDigging(player)) {
        return;
    }
    sDigFrame = 0;
    func_8083B1A0(player, play);
    Audio_PlaySfx_AtPos(&player->actor.world.pos, NA_SE_PL_CHANGE_ARMS);
}

static void StartDigging(Player* player, PlayState* play) {
    if (IsDigging(player) || !CanDig(player, play)) {
        return;
    }
    PlayerAnimationHeader* digAnim = ResourceMgr_LoadPlayerAnimAsHeader(sDigAnim);
    if (digAnim == NULL) {
        return;
    }
    sDigFrame = 0;
    Player_SetAction(play, player, (PlayerActionFunc)DigAction, 0);
    Player_StopHorizontalMovement(player);
    Player_Anim_PlayOnce(play, player, digAnim);
    Audio_PlaySfx_AtPos(&player->actor.world.pos, NA_SE_PL_CHANGE_ARMS);
}

static void TakeOutShovel(PlayState* play, Player* player) {
    sIsDigPending = true;
}

static bool IsChangingHeldItem(Player* player) {
    return (player->stateFlags3 & PLAYER_STATE3_START_CHANGING_HELD_ITEM) != 0;
}

static void StartPendingDig(Player* player) {
    PlayState* play = gPlayState;

    if (!sIsDigPending || !S2H_MOD_API_HAS(sApi, RegisterCustomItem) || play == NULL) {
        return;
    }
    if (player->heldItemAction != PLAYER_IA_CUSTOM) {
        sIsDigPending = false;
        return;
    }
    if (IsChangingHeldItem(player)) {
        return;
    }
    sIsDigPending = false;
    StartDigging(player, play);
}

static void DigAction(Player* player, PlayState* play) {
    if (PlayerAnimation_Update(play, &player->skelAnime)) {
        StopDigging(player, play);
        return;
    }
    Player_StopHorizontalMovement(player);
    sDigFrame++;
    if (sDigFrame == SHOVEL_DIG_FRAME) {
        Dig(player, play);
    }
}

// Through s32: 180 degrees is 32768, which a float-to-s16 conversion may not represent.
static s16 DegreesToBinang(f32 degrees) {
    return (s16)(s32)(degrees * (32768.0f / 180.0f));
}

static void ApplyHeldTransform(void) {
    Matrix_Translate(SHOVEL_HELD_OFFSET_X, SHOVEL_HELD_OFFSET_Y, SHOVEL_HELD_OFFSET_Z, MTXMODE_APPLY);
    Matrix_RotateZYX(DegreesToBinang(SHOVEL_HELD_ROTATION_X), DegreesToBinang(SHOVEL_HELD_ROTATION_Y),
                     DegreesToBinang(SHOVEL_HELD_ROTATION_Z), MTXMODE_APPLY);
    Matrix_Scale(SHOVEL_HELD_SCALE, SHOVEL_HELD_SCALE, SHOVEL_HELD_SCALE, MTXMODE_APPLY);
}

static bool IsShovelInHands(Player* player, PlayState* play) {
    RemoteVisualState visual;

    if (player == GET_PLAYER(play)) {
        return IsDigging(player);
    }
    return ReadRemoteVisual(player, &visual);
}

static void DrawShovelInHands(Player* player, PlayState* play) {
    if (!IsShovelInHands(player, play)) {
        return;
    }
    Vec3f leftHand = player->bodyPartsPos[PLAYER_BODYPART_LEFT_HAND];
    Vec3f rightHand = player->bodyPartsPos[PLAYER_BODYPART_RIGHT_HAND];
    f32 spanX = leftHand.x - rightHand.x;
    f32 spanY = leftHand.y - rightHand.y;
    f32 spanZ = leftHand.z - rightHand.z;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    Matrix_Push();
    Matrix_Translate((leftHand.x + rightHand.x) * 0.5f, (leftHand.y + rightHand.y) * 0.5f,
                     (leftHand.z + rightHand.z) * 0.5f, MTXMODE_NEW);
    Matrix_RotateYF(Math_Atan2F_XY(spanZ, spanX), MTXMODE_APPLY);
    Matrix_RotateXF(-Math_Atan2F_XY(sqrtf(spanX * spanX + spanZ * spanZ), spanY), MTXMODE_APPLY);
    ApplyHeldTransform();
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sShovelDL);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawAfterPlayer(Actor* actor, PlayState* play) {
    Player* player = (Player*)actor;

    if (player->stateFlags2 & PLAYER_STATE2_20000000) {
        return;
    }
    DrawShovelInHands(player, play);
}

static void DrawGetItem(PlayState* play, S2HGetItemEntry* getItemEntry) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    Matrix_Scale(SHOVEL_GIVE_SCALE, SHOVEL_GIVE_SCALE, SHOVEL_GIVE_SCALE, MTXMODE_APPLY);
    Matrix_RotateYF((s16)(play->gameplayFrames * 2) * 0.01f, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sShovelDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void UseShovel(Player* player, PlayState* play) {
    StartDigging(player, play);
}

static const S2HCustomItemRandomizer sRandomizerItem = {
    sizeof(S2HCustomItemRandomizer), 0, S2H_CUSTOM_ITEM_TYPE_ITEM, 0, 1, SHOVEL_KEY, SHOVEL_KEY, "",
};

static bool sRemoteVisualReady;
static bool sRemoteVisualSent;
static uint32_t sRemoteVisualFrame;
static RemoteVisualState sLastRemoteVisual;

static void ReceiveRemoteVisual(const S2HNetMessage* message) {
    (void)message;
}

static bool ReadRemoteVisual(Player* player, RemoteVisualState* visual) {
    if (!sRemoteVisualReady || gPlayState == NULL || player == GET_PLAYER(gPlayState)) {
        return false;
    }
    uint32_t peerId = sApi->GetNetPeerIdForActor(&player->actor);
    if (peerId == 0 || sApi->GetNetPeerState(peerId, "nei.shovel.visual", visual, sizeof(*visual)) != sizeof(*visual)) {
        return false;
    }
    if (visual->active != 1 || visual->adult > 1 || visual->scene != gPlayState->sceneId ||
        (player->stateFlags2 & PLAYER_STATE2_20000000)) {
        return false;
    }
    for (uint32_t i = 0; i < ARRAY_COUNT(visual->values); i++) {
        if (!isfinite(visual->values[i]) || fabsf(visual->values[i]) > 10000000.0f) {
            return false;
        }
    }
    return true;
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
        visual.active = IsDigging(GET_PLAYER(play));
        if (sRemoteVisualSent && (uint32_t)(play->gameplayFrames - sRemoteVisualFrame) < REMOTE_VISUAL_INTERVAL) {
            return;
        }
    }
    if (sRemoteVisualSent && memcmp(&visual, &sLastRemoteVisual, sizeof(visual)) == 0) {
        return;
    }
    if (sApi->SetNetPeerState("nei.shovel.visual", &visual, sizeof(visual))) {
        sLastRemoteVisual = visual;
        sRemoteVisualSent = true;
        sRemoteVisualFrame = play != NULL ? play->gameplayFrames : 0;
    }
}

static void DrawRemotePlayer(Actor* actor, PlayState* play) {
    if (actor->id == ACTOR_PLAYER || !sRemoteVisualReady || sApi->GetNetPeerIdForActor(actor) == 0) {
        return;
    }
    DrawAfterPlayer(actor, play);
}

static void InitRemoteVisuals(void) {
    if (!S2H_MOD_API_HAS(sApi, RegisterNetChannel) || !S2H_MOD_API_HAS(sApi, SetNetPeerState) ||
        !S2H_MOD_API_HAS(sApi, GetNetPeerState) || !S2H_MOD_API_HAS(sApi, GetNetPeerIdForActor)) {
        return;
    }
    sRemoteVisualReady = sApi->RegisterNetChannel("nei.shovel.visual", 1, ReceiveRemoteVisual);
    if (sRemoteVisualReady) {
        S2H_REGISTER_HOOK(sApi, OnGameFrameUpdate, PublishRemoteVisual);
        S2H_REGISTER_HOOK(sApi, OnActorDrawEnd, DrawRemotePlayer);
    }
}

S2H_MOD_EXPORT void ModSetApi(const S2HModApi* api) {
    sApi = api;
}

S2H_MOD_EXPORT const S2HModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

S2H_MOD_EXPORT void ModInit(void) {
    InitRemoteVisuals();
    S2HCustomItemDefinition shovel = Z64Items_Define(SHOVEL_KEY, sIconTex, sNameTex);

    Z64Items_SetButtons(&shovel, S2H_CUSTOM_ITEM_C_BUTTON | S2H_CUSTOM_ITEM_DPAD);
    Z64Items_SetForms(&shovel, S2H_CUSTOM_ITEM_FORM_BIT(PLAYER_FORM_HUMAN));
    Z64Items_SetPlacement(&shovel, 0, 22, 0);
    shovel.displayName = "Dampe's Shovel";
    Z64Items_SetTextbox(&shovel, "You got %rDampe's Shovel%w! The&old gravekeeper's tool, worn&"
                                 "smooth by years of work.^"
                                 "Press %y\xB2%w to dig. A %yhidden&grotto%w opens where the&"
                                 "ground rings hollow.^"
                                 "Loose earth turns up %grupees%w&and %gbombs%w, and once in a while&a %bfairy%w.");
    Z64Items_SetPauseText(&shovel, "%rDampe's Shovel&%wPress %y\xB2%w to dig. Hidden grottos&"
                                   "and loose soil hide the most.");
    Z64Items_SetCanUse(&shovel, CanDig);
    Z64Items_SetAction(&shovel, TakeOutShovel, NULL);
    Z64Items_SetHeldCallbacks(&shovel, UseShovel, StopDigging, NULL);
    shovel.modelGroup = PLAYER_MODELGROUP_DEFAULT;
    shovel.getItemEntry.drawFunc = DrawGetItem;

    Z64Items_SetLogic(&shovel, &sRandomizerItem);
    if (!Z64Items_Register(sApi, &shovel)) {
        return;
    }
    S2H_REGISTER_HOOK_FOR_ID(sApi, OnActorDrawEnd, ACTOR_PLAYER, DrawAfterPlayer);
    S2H_REGISTER_HOOK(sApi, OnPlayerUpdate, StartPendingDig);
}
