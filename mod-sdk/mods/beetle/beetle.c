#include <math.h>
#include <string.h>

#include "z64items.h"
#include "z64logic.h"
void Player_AnimSfx_PlayVoice(Player* player, u16 sfxId);
#include "z64aiming.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "overlays/actors/ovl_Obj_Switch/z_obj_switch.h"

#define BEETLE_KEY "nei.beetle"
#define BEETLE_SPEED 12.0f
#define BEETLE_BOOST_SCALE 1.8f
#define BEETLE_RETURN_SPEED 18.0f
#define BEETLE_MAX_DISTANCE 800.0f
#define BEETLE_CATCH_DISTANCE 50.0f
#define BEETLE_LAUNCH_HEIGHT 40.0f
#define BEETLE_LAUNCH_REACH 30.0f
#define BEETLE_COLLIDER_RADIUS 15
#define BEETLE_COLLIDER_HEIGHT 18
#define BEETLE_GRAB_REACH 40.0f
#define BEETLE_FLIGHT_FRAMES 600
#define BEETLE_TURN_RATE 0x360
#define BEETLE_MAX_PITCH 0x3000
#define BEETLE_TARGET_RANGE 700.0f
#define BEETLE_HOMING_STEP 0x180
#define BEETLE_HOMING_STEP_BOOSTED 0x500
#define BEETLE_HOMING_STEP_CHARGING 0x800
#define BEETLE_CAMERA_DISTANCE 120.0f
#define BEETLE_CAMERA_HEIGHT 30.0f
#define BEETLE_MODEL_SCALE 0.06f
#define BEETLE_HELD_SCALE 0.048f
#define BEETLE_HELD_REACH 8.0f
#define BEETLE_WING_BEAT 0.7f
#define BEETLE_WING_MIN 0.3f
#define BEETLE_WING_MAX 1.0f
#define BEETLE_GIVE_SCALE (0.35f * 0.8f)

typedef enum {
    BEETLE_STOWED,
    BEETLE_IN_HAND,
    BEETLE_AIMING,
    BEETLE_PILOTED,
    BEETLE_LOOSE,
    BEETLE_RETURNING,
} BeetlePhase;

static const ALIGN_ASSET(2) char sIconTex[] = "__OTR__textures/icon_item_custom/gItemIconBeetleTex";
static const ALIGN_ASSET(2) char sNameTex[] = "__OTR__textures/item_name_custom/gBeetleNameTex";
static const ALIGN_ASSET(2) char sBodyDL[] = "__OTR__objects/object_nei_beetle/g_beetle_body_dl";
static const ALIGN_ASSET(2) char sWingsDL[] = "__OTR__objects/object_nei_beetle/g_beetle_wings_dl";
static const ALIGN_ASSET(2) char sWholeDL[] = "__OTR__objects/object_nei_beetle/g_beetle_dl";
static const ALIGN_ASSET(2) char sWaitAnim[] = "__OTR__objects/gameplay_keep/gPlayerAnim_link_boom_throw_waitR";
static const ALIGN_ASSET(2) char sThrowAnim[] = "__OTR__objects/oot_gameplay_keep/gPlayerAnim_link_boom_throwR";
static const ALIGN_ASSET(2) char sCatchAnim[] = "__OTR__objects/oot_gameplay_keep/gPlayerAnim_link_boom_catch";

static const u32 sItemButtons[S2H_ITEM_BUTTON_COUNT] = { BTN_B,      BTN_CLEFT, BTN_CDOWN, BTN_CRIGHT, 0,
                                                         BTN_DRIGHT, BTN_DLEFT, BTN_DDOWN, BTN_DUP };

static ColliderCylinderInit sColliderInit = {
    {
        COL_MATERIAL_NONE,
        AT_ON | AT_TYPE_PLAYER,
        AC_NONE,
        OC1_ON | OC1_TYPE_ALL,
        OC2_TYPE_PLAYER,
        COLSHAPE_CYLINDER,
    },
    {
        ELEM_MATERIAL_UNK2,
        { DMG_ZORA_BOOMERANG, 0x00, 0x01 },
        { 0, 0, 0 },
        ATELEM_ON | ATELEM_NEAREST | ATELEM_SFX_NORMAL,
        ACELEM_NONE,
        OCELEM_ON,
    },
    { BEETLE_COLLIDER_RADIUS, BEETLE_COLLIDER_HEIGHT, 0, { 0, 0, 0 } },
};

static const char* const sRequiredHooks[] = { "OnGameFrameUpdate",   "OnActorDestroy",
                                              "OnActorUpdate",       "OnPlayDrawEnd",
                                              "OnPlayerUpdate",      "OnActorDrawEnd",
                                              "OnSceneInit",         "OnPlayerFilterInput",
                                              "OnPlayerResolveItemActionInit" };
static const S2HModRequirements sRequirements = { sizeof(S2HModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const S2HModApi* sApi;

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

static ColliderCylinder sCollider;
static bool sIsColliderReady;
static bool sIsAimPending;
static bool sWasButtonHeld;
static u8 sPhase;
static u8 sAnimatedPhase;
static s16 sFlightTimer;
static Vec3f sBeetlePos;
static Vec3f sLaunchPos;
static Vec3s sBeetleRot;
static f32 sWingSpread = BEETLE_WING_MAX;
static s8 sWingBeat = -1;
static Actor* sCargo;
static Actor* sTarget;
static Actor* sCandidate;
static bool sIsCharging;
static bool sIsAutonomous;
static s16 sCameraId = CAM_ID_NONE;

static u32 FindEquippedButtonMask(void) {
    for (u8 button = 0; button < S2H_ITEM_BUTTON_COUNT; button++) {
        const char* equipped = sApi->GetEquippedCustomItem(button);
        if (equipped != NULL && strcmp(equipped, BEETLE_KEY) == 0) {
            return sItemButtons[button];
        }
    }
    return 0;
}

static bool IsButtonHeld(PlayState* play) {
    return (play->state.input[0].cur.button & FindEquippedButtonMask()) != 0;
}

static bool WasButtonPressed(PlayState* play) {
    return (play->state.input[0].press.button & FindEquippedButtonMask()) != 0;
}

static bool IsAway(void) {
    return sPhase >= BEETLE_PILOTED;
}

static void DestroyFlightCamera(PlayState* play) {
    if (sCameraId == CAM_ID_NONE) {
        return;
    }
    Camera_ChangeMode(Play_GetCamera(play, CAM_ID_MAIN), CAM_MODE_NORMAL);
    Play_ChangeCameraStatus(play, CAM_ID_MAIN, CAM_STATUS_ACTIVE);
    Play_ClearCamera(play, sCameraId);
    sCameraId = CAM_ID_NONE;
}

static void CreateFlightCamera(PlayState* play) {
    if (sCameraId != CAM_ID_NONE) {
        return;
    }
    sCameraId = Play_CreateSubCamera(play);
    Play_ChangeCameraStatus(play, CAM_ID_MAIN, CAM_STATUS_WAIT);
    Play_ChangeCameraStatus(play, sCameraId, CAM_STATUS_ACTIVE);
}

static void FollowBeetleWithCamera(PlayState* play) {
    f32 pitchScale = Math_CosS(sBeetleRot.x);
    Vec3f eye;

    if (sCameraId == CAM_ID_NONE) {
        return;
    }
    eye.x = sBeetlePos.x - Math_SinS(sBeetleRot.y) * pitchScale * BEETLE_CAMERA_DISTANCE;
    eye.y = sBeetlePos.y - BEETLE_CAMERA_HEIGHT + Math_SinS(sBeetleRot.x) * BEETLE_CAMERA_DISTANCE;
    eye.z = sBeetlePos.z - Math_CosS(sBeetleRot.y) * pitchScale * BEETLE_CAMERA_DISTANCE;
    Play_SetCameraAtEye(play, sCameraId, &sBeetlePos, &eye);
}

static void BeatWings(void) {
    sWingSpread += sWingBeat * BEETLE_WING_BEAT;
    if (sWingSpread >= BEETLE_WING_MAX) {
        sWingSpread = BEETLE_WING_MAX;
        sWingBeat = -1;
    } else if (sWingSpread <= BEETLE_WING_MIN) {
        sWingSpread = BEETLE_WING_MIN;
        sWingBeat = 1;
    }
}

static void CarryCargo(void) {
    if (sCargo == NULL) {
        return;
    }
    if (sCargo->update == NULL) {
        sCargo = NULL;
        return;
    }
    Math_Vec3f_Copy(&sCargo->world.pos, &sBeetlePos);
}

static void DropCargo(Player* player) {
    if (sCargo == NULL) {
        return;
    }
    Math_Vec3f_Copy(&sCargo->world.pos, &player->actor.world.pos);
    sCargo->flags &= ~ACTOR_FLAG_HOOKSHOT_ATTACHED;
    sCargo->gravity = -0.9f;
    sCargo->bgCheckFlags &= ~(BGCHECKFLAG_GROUND | BGCHECKFLAG_GROUND_TOUCH);
    sCargo = NULL;
}

static void Land(Player* player, PlayState* play) {
    DestroyFlightCamera(play);
    DropCargo(player);
    sCollider.base.atFlags &= ~(AT_ON | AT_HIT);
    player->stateFlags2 &= ~PLAYER_STATE2_20;
    sTarget = NULL;
    sCandidate = NULL;
    sIsCharging = false;
    sIsAutonomous = false;
    sPhase = BEETLE_IN_HAND;
}

static void StartReturn(Player* player, PlayState* play) {
    DestroyFlightCamera(play);
    player->stateFlags2 &= ~PLAYER_STATE2_20;
    player->focusActor = NULL;
    sTarget = NULL;
    sCandidate = NULL;
    sIsCharging = false;
    sPhase = BEETLE_RETURNING;
    Audio_PlaySfx_AtPos(&sBeetlePos, NA_SE_PL_CHANGE_ARMS);
}

static void Launch(Player* player, PlayState* play) {
    s16 yaw;
    s16 pitch;

    Z64Aiming_GetDirection(player, &yaw, &pitch);
    Z64Aiming_Release(player, play);
    sBeetlePos.x = player->actor.world.pos.x + Math_SinS(yaw) * BEETLE_LAUNCH_REACH;
    sBeetlePos.y = player->actor.world.pos.y + BEETLE_LAUNCH_HEIGHT;
    sBeetlePos.z = player->actor.world.pos.z + Math_CosS(yaw) * BEETLE_LAUNCH_REACH;
    sBeetleRot.x = pitch;
    sBeetleRot.y = yaw;
    sBeetleRot.z = 0;
    sLaunchPos = player->actor.world.pos;
    sFlightTimer = BEETLE_FLIGHT_FRAMES;
    sCargo = NULL;
    sTarget = NULL;
    sIsCharging = false;
    sPhase = BEETLE_PILOTED;
    Collider_SetCylinder(play, &sCollider, &player->actor, &sColliderInit);
    CreateFlightCamera(play);
    Audio_PlaySfx_AtPos(&player->actor.world.pos, NA_SE_IT_SWORD_SWING);
    Player_AnimSfx_PlayVoice(player, NA_SE_VO_LI_SWORD_N);
}

static void StartAiming(Player* player, PlayState* play) {
    if (!Z64Aiming_Request(player, play)) {
        return;
    }
    sWasButtonHeld = false;
    sPhase = BEETLE_AIMING;
}

static void StopAiming(Player* player, PlayState* play) {
    Z64Aiming_Release(player, play);
    sPhase = BEETLE_IN_HAND;
}

static void Fly(f32 speed) {
    f32 pitchScale = Math_CosS(sBeetleRot.x);

    sBeetlePos.x += Math_SinS(sBeetleRot.y) * pitchScale * speed;
    sBeetlePos.y -= Math_SinS(sBeetleRot.x) * speed;
    sBeetlePos.z += Math_CosS(sBeetleRot.y) * pitchScale * speed;
}

static void SteerWithStick(PlayState* play) {
    f32 stickX = play->state.input[0].rel.stick_x * GameInteractor_InvertControl(GI_INVERT_FIRST_PERSON_AIM_X);
    f32 stickY = -play->state.input[0].rel.stick_y * GameInteractor_InvertControl(GI_INVERT_FIRST_PERSON_AIM_Y);

    if (sqrtf(SQ(stickX) + SQ(stickY)) <= 20.0f) {
        return;
    }
    sBeetleRot.y -= (s16)((stickX / 60.0f) * BEETLE_TURN_RATE);
    sBeetleRot.x =
        CLAMP((s32)(sBeetleRot.x + (s16)((stickY / 60.0f) * BEETLE_TURN_RATE)), -BEETLE_MAX_PITCH, BEETLE_MAX_PITCH);
}

static bool IsCollectible(Actor* actor) {
    return actor->id == ACTOR_EN_ITEM00 || actor->id == ACTOR_EN_SI ||
           (actor->id == ACTOR_EN_ELFORG && actor->draw != NULL && (actor->params & 0xF) != 1 &&
            (actor->params & 0xF) != 8);
}

static void Grab(Actor* actor) {
    sCargo = actor;
    actor->flags |= ACTOR_FLAG_HOOKSHOT_ATTACHED;
}

static void GrabWhatItPasses(PlayState* play) {
    static const u8 sCollectibleCategories[] = { ACTORCAT_ITEMACTION, ACTORCAT_PROP, ACTORCAT_MISC };

    if (sCargo != NULL) {
        return;
    }
    for (u32 i = 0; i < ARRAY_COUNT(sCollectibleCategories); i++) {
        for (Actor* actor = play->actorCtx.actorLists[sCollectibleCategories[i]].first; actor != NULL;
             actor = actor->next) {
            if (actor->update != NULL && IsCollectible(actor) &&
                Math_Vec3f_DistXYZ(&sBeetlePos, &actor->world.pos) < BEETLE_GRAB_REACH) {
                Grab(actor);
                return;
            }
        }
    }
}

static bool HasStruckSomething(void) {
    Actor* hit = (sCollider.base.atFlags & AT_HIT) ? sCollider.base.at : NULL;

    sCollider.base.atFlags &= ~AT_HIT;
    if (hit == NULL) {
        return false;
    }
    if (IsCollectible(hit)) {
        Grab(hit);
        return false;
    }
    Audio_PlaySfx_AtPos(&sBeetlePos, NA_SE_IT_SHIELD_BOUND);
    return true;
}

static bool HasHitGeometry(PlayState* play, Vec3f* previousPos) {
    CollisionPoly* poly = NULL;
    s32 bgId = BGCHECK_SCENE;
    Vec3f hitPos;

    if (!BgCheck_EntityLineTest1(&play->colCtx, previousPos, &sBeetlePos, &hitPos, &poly, true, true, true, true,
                                 &bgId)) {
        return false;
    }
    sBeetlePos = hitPos;
    Audio_PlaySfx_AtPos(&sBeetlePos, NA_SE_IT_SHIELD_BOUND);
    return true;
}

static bool IsTargetable(Actor* actor) {
    if (actor->category == ACTORCAT_ENEMY || actor->category == ACTORCAT_BOSS) {
        return (actor->flags & ACTOR_FLAG_ATTENTION_ENABLED) != 0;
    }
    if (IsCollectible(actor)) {
        return true;
    }
    if (actor->id == ACTOR_OBJ_SWITCH) {
        u8 kind = actor->params & 7;

        return kind == OBJSWITCH_TYPE_EYE || kind == OBJSWITCH_TYPE_CRYSTAL ||
               kind == OBJSWITCH_TYPE_CRYSTAL_TARGETABLE;
    }
    return false;
}

static Actor* FindTarget(PlayState* play) {
    static const u8 sTargetCategories[] = { ACTORCAT_ENEMY, ACTORCAT_BOSS,   ACTORCAT_ITEMACTION, ACTORCAT_PROP,
                                            ACTORCAT_MISC,  ACTORCAT_SWITCH, ACTORCAT_BG };
    Actor* nearest = NULL;
    f32 nearestDistance = BEETLE_TARGET_RANGE;

    for (u32 i = 0; i < ARRAY_COUNT(sTargetCategories); i++) {
        for (Actor* actor = play->actorCtx.actorLists[sTargetCategories[i]].first; actor != NULL; actor = actor->next) {
            if (actor->update == NULL || !IsTargetable(actor)) {
                continue;
            }
            f32 distance = Math_Vec3f_DistXYZ(&sBeetlePos, &actor->world.pos);

            if (distance < nearestDistance) {
                nearestDistance = distance;
                nearest = actor;
            }
        }
    }
    return nearest;
}

static void HomeToTarget(s16 step) {
    Math_SmoothStepToS(&sBeetleRot.y, Math_Vec3f_Yaw(&sBeetlePos, &sTarget->world.pos), 4, step, 0x10);
    Math_SmoothStepToS(&sBeetleRot.x, Math_Vec3f_Pitch(&sBeetlePos, &sTarget->world.pos), 4, step, 0x10);
}

static void DropLostTarget(void) {
    if (sTarget != NULL && sTarget->update == NULL) {
        sTarget = NULL;
        sIsCharging = false;
    }
}

static void HoldTarget(Player* player, PlayState* play) {
    if (play->state.input[0].press.button & BTN_Z) {
        sTarget = sTarget != NULL ? NULL : FindTarget(play);
        sIsCharging = false;
    }
    if (sTarget == NULL) {
        return;
    }
    player->focusActor = sTarget;
    player->zTargetActiveTimer = 15;
}

static bool HasFlownTooFar(void) {
    return Math_Vec3f_DistXYZ(&sBeetlePos, &sLaunchPos) > BEETLE_MAX_DISTANCE || DECR(sFlightTimer) == 0;
}

static void AdvanceFlight(Player* player, PlayState* play, f32 speed) {
    Vec3f previousPos = sBeetlePos;
    bool hasStruck = HasStruckSomething();

    GrabWhatItPasses(play);
    Fly(speed);
    sCollider.dim.pos.x = (s16)sBeetlePos.x;
    sCollider.dim.pos.y = (s16)(sBeetlePos.y - BEETLE_COLLIDER_HEIGHT / 2);
    sCollider.dim.pos.z = (s16)sBeetlePos.z;
    sCollider.base.atFlags |= AT_ON | AT_TYPE_PLAYER;
    CollisionCheck_SetAT(play, &play->colChkCtx, &sCollider.base);
    BeatWings();
    CarryCargo();
    Actor_PlaySfx_Flagged2(&player->actor, NA_SE_EN_BIRI_FLY);
    if (hasStruck || HasHitGeometry(play, &previousPos) || HasFlownTooFar()) {
        StartReturn(player, play);
    }
}

static void UpdatePiloted(Player* player, PlayState* play) {
    bool isBoosting = (play->state.input[0].cur.button & BTN_A) != 0;

    DropLostTarget();
    Player_StopHorizontalMovement(player);
    player->stateFlags2 |= PLAYER_STATE2_20;
    if (WasButtonPressed(play)) {
        StartReturn(player, play);
        return;
    }
    if (play->state.input[0].press.button & BTN_B) {
        DestroyFlightCamera(play);
        player->stateFlags2 &= ~PLAYER_STATE2_20;
        player->focusActor = NULL;
        sCandidate = NULL;
        sIsCharging = sTarget != NULL;
        sIsAutonomous = true;
        sAnimatedPhase = BEETLE_STOWED;
        sPhase = BEETLE_LOOSE;
        return;
    }
    HoldTarget(player, play);
    sCandidate = sTarget == NULL ? FindTarget(play) : NULL;
    SteerWithStick(play);
    if (sTarget != NULL) {
        HomeToTarget(isBoosting ? BEETLE_HOMING_STEP_BOOSTED : BEETLE_HOMING_STEP);
    }
    AdvanceFlight(player, play, isBoosting ? BEETLE_SPEED * BEETLE_BOOST_SCALE : BEETLE_SPEED);
    FollowBeetleWithCamera(play);
}

static void UpdateLoose(Player* player, PlayState* play) {
    DropLostTarget();
    if (!sIsCharging || sTarget == NULL) {
        StartReturn(player, play);
        return;
    }
    HomeToTarget(BEETLE_HOMING_STEP_CHARGING);
    AdvanceFlight(player, play, BEETLE_SPEED * BEETLE_BOOST_SCALE);
}

static void UpdateReturning(Player* player, PlayState* play) {
    Vec3f hand = { player->actor.world.pos.x, player->actor.world.pos.y + BEETLE_LAUNCH_HEIGHT,
                   player->actor.world.pos.z };
    f32 distance = Math_Vec3f_DistXYZ(&sBeetlePos, &hand);

    BeatWings();
    CarryCargo();
    if (distance <= BEETLE_CATCH_DISTANCE) {
        if (!sIsAutonomous) {
            PlayerAnimation_PlayOnce(play, &player->skelAnimeUpper, (PlayerAnimationHeader*)sCatchAnim);
            sAnimatedPhase = BEETLE_RETURNING;
        }
        Audio_PlaySfx_AtPos(&player->actor.world.pos, NA_SE_PL_CATCH_BOOMERANG);
        Land(player, play);
        return;
    }
    f32 step = BEETLE_RETURN_SPEED / distance;

    sBeetlePos.x += (hand.x - sBeetlePos.x) * step;
    sBeetlePos.y += (hand.y - sBeetlePos.y) * step;
    sBeetlePos.z += (hand.z - sBeetlePos.z) * step;
    sBeetleRot.y = Math_Vec3f_Yaw(&sBeetlePos, &hand);
    sBeetleRot.x = Math_Vec3f_Pitch(&sBeetlePos, &hand);
    Actor_PlaySfx_Flagged2(&player->actor, NA_SE_EN_BIRI_FLY);
}

static void AnimateArm(Player* player, PlayState* play) {
    if (sAnimatedPhase == sPhase) {
        PlayerAnimation_Update(play, &player->skelAnimeUpper);
        return;
    }
    sAnimatedPhase = sPhase;
    if (sPhase == BEETLE_AIMING) {
        PlayerAnimation_PlayLoop(play, &player->skelAnimeUpper, (PlayerAnimationHeader*)sWaitAnim);
    } else {
        PlayerAnimation_PlayOnce(play, &player->skelAnimeUpper, (PlayerAnimationHeader*)sThrowAnim);
    }
}

static void UpdateAiming(Player* player, PlayState* play) {
    Z64Aiming_Update(player, play);
    Player_StopHorizontalMovement(player);
    if (play->state.input[0].press.button & (BTN_B | BTN_A)) {
        StopAiming(player, play);
        return;
    }
    if (IsButtonHeld(play)) {
        sWasButtonHeld = true;
        return;
    }
    if (sWasButtonHeld || WasButtonPressed(play)) {
        Launch(player, play);
    }
}

static int32_t UpdateBeetleInHand(Player* player, PlayState* play) {
    if (sPhase == BEETLE_IN_HAND || sPhase == BEETLE_STOWED || sIsAutonomous) {
        return 0;
    }
    if (sPhase == BEETLE_AIMING) {
        UpdateAiming(player, play);
    }
    AnimateArm(player, play);
    return 1;
}

static void TakeOutBeetle(PlayState* play, Player* player) {
    if (!sIsColliderReady) {
        Collider_InitCylinder(play, &sCollider);
        sIsColliderReady = true;
    }
    sPhase = BEETLE_IN_HAND;
    sAnimatedPhase = BEETLE_STOWED;
    sCargo = NULL;
    sIsAimPending = true;
    Audio_PlaySfx_AtPos(&player->actor.world.pos, NA_SE_PL_CHANGE_ARMS);
}

static void RequestAim(Player* player, PlayState* play) {
    sIsAimPending = true;
}

static void PutBeetleAway(Player* player, PlayState* play) {
    if (sPhase == BEETLE_AIMING) {
        Z64Aiming_Release(player, play);
    }
    if (IsAway()) {
        Land(player, play);
    }
    DestroyFlightCamera(play);
    sCollider.base.atFlags &= ~(AT_ON | AT_HIT);
    sIsAimPending = false;
    sPhase = BEETLE_STOWED;
    AudioSfx_StopById(NA_SE_EN_BIRI_FLY);
}

static bool IsChangingHeldItem(Player* player) {
    return (player->stateFlags3 & PLAYER_STATE3_START_CHANGING_HELD_ITEM) != 0;
}

static void RunPendingAim(Player* player, PlayState* play) {
    if (!sIsAimPending || IsChangingHeldItem(player)) {
        return;
    }
    sIsAimPending = false;
    if (player->heldItemAction == PLAYER_IA_CUSTOM && sPhase == BEETLE_IN_HAND) {
        StartAiming(player, play);
    }
}

static bool IsFlightInterrupted(Player* player) {
    return player->invincibilityTimer < 0 || (player->stateFlags1 & PLAYER_STATE1_8000000);
}

static void UpdateBeetleWorld(Player* player) {
    PlayState* play = gPlayState;

    if (play == NULL) {
        return;
    }
    RunPendingAim(player, play);
    if (sPhase == BEETLE_AIMING) {
        Z64Aiming_Update(player, play);
        Z64Items_KeepHeld(sApi);
    }
    if (!IsAway()) {
        return;
    }
    Z64Items_KeepHeld(sApi);
    if (IsFlightInterrupted(player)) {
        StartReturn(player, play);
        return;
    }
    switch (sPhase) {
        case BEETLE_PILOTED:
            UpdatePiloted(player, play);
            break;
        case BEETLE_LOOSE:
            UpdateLoose(player, play);
            break;
        case BEETLE_RETURNING:
            UpdateReturning(player, play);
            break;
        default:
            Land(player, play);
            break;
    }
}

static void HoldPilotInput(Player* player, Input* input) {
    if (sPhase != BEETLE_PILOTED) {
        return;
    }
    input->cur.button &= ~(BTN_A | BTN_B | BTN_R | BTN_Z);
    input->press.button &= ~(BTN_A | BTN_B | BTN_R | BTN_Z);
    input->cur.stick_x = 0;
    input->cur.stick_y = 0;
    input->rel.stick_x = 0;
    input->rel.stick_y = 0;
}

static void ForgetScene(s16 sceneId, s8 spawnNum) {
    sCameraId = CAM_ID_NONE;
    sCargo = NULL;
    sTarget = NULL;
    sCandidate = NULL;
    sIsAimPending = false;
    sIsCharging = false;
    sIsAutonomous = false;
    sPhase = BEETLE_STOWED;
}

static void DrawBeetleAt(PlayState* play, Vec3f* pos, Vec3s* rot, f32 scale) {
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Translate(pos->x, pos->y, pos->z, MTXMODE_NEW);
    Matrix_RotateYF(BINANG_TO_RAD(rot->y), MTXMODE_APPLY);
    Matrix_RotateXF(BINANG_TO_RAD(rot->x), MTXMODE_APPLY);
    Matrix_RotateYF(M_PIf / 2.0f, MTXMODE_APPLY);
    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sBodyDL);
    Matrix_Scale(1.0f, sWingSpread, 1.0f, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sWingsDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

// Actor_UpdateAll fills arrowHoverActor from a search around Link; written here so the arrow follows the beetle.
static void OfferCandidate(PlayState* play) {
    if (sCandidate != NULL && sCandidate->update != NULL) {
        play->actorCtx.attention.arrowHoverActor = sCandidate;
    }
}

static bool IsBinang(f32 value) {
    return value >= -32768.0f && value <= 32767.0f;
}

static void DrawRemoteBeetle(PlayState* play, Player* player) {
    RemoteVisualState visual;

    if (!ReadRemoteVisual(player, &visual)) {
        return;
    }
    if (!IsBinang(visual.values[3]) || !IsBinang(visual.values[4]) || !IsBinang(visual.values[5]) ||
        visual.values[6] < 0.0f || visual.values[6] > 4.0f) {
        return;
    }
    Vec3f position = { visual.values[0], visual.values[1], visual.values[2] };
    Vec3s rotation = { visual.values[3], visual.values[4], visual.values[5] };
    f32 wingSpread = sWingSpread;

    sWingSpread = visual.values[6];
    if (!visual.flags[0]) {
        rotation = (Vec3s){ 0, player->actor.shape.rot.y, 0 };
        position = player->bodyPartsPos[PLAYER_BODYPART_LEFT_HAND];
        position.x += Math_SinS(rotation.y) * BEETLE_HELD_REACH;
        position.z += Math_CosS(rotation.y) * BEETLE_HELD_REACH;
    }
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    DrawBeetleAt(play, &position, &rotation, visual.flags[0] ? BEETLE_MODEL_SCALE : BEETLE_HELD_SCALE);
    sWingSpread = wingSpread;
}

static void DrawBeetle(Actor* actor, PlayState* play) {
    Player* player = (Player*)actor;

    if (player != GET_PLAYER(play)) {
        DrawRemoteBeetle(play, player);
        return;
    }
    if (sPhase == BEETLE_STOWED || (player->stateFlags2 & PLAYER_STATE2_20000000)) {
        return;
    }
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    if (IsAway()) {
        OfferCandidate(play);
        DrawBeetleAt(play, &sBeetlePos, &sBeetleRot, BEETLE_MODEL_SCALE);
        return;
    }
    Vec3s handRot = { 0, player->actor.shape.rot.y, 0 };
    Vec3f hand = player->bodyPartsPos[PLAYER_BODYPART_LEFT_HAND];

    hand.x += Math_SinS(handRot.y) * BEETLE_HELD_REACH;
    hand.z += Math_CosS(handRot.y) * BEETLE_HELD_REACH;
    DrawBeetleAt(play, &hand, &handRot, BEETLE_HELD_SCALE);
}

static void DrawAimReticle(void) {
    PlayState* play = gPlayState;
    if (play == NULL || sPhase != BEETLE_AIMING || play->pauseCtx.state != 0) {
        return;
    }
    Z64Aiming_DrawReticle(play, GET_PLAYER(play), BEETLE_MAX_DISTANCE,
                          (Z64AimingReticleStyle){ Z64AIMING_TOGGLE_BOOMERANG, Z64AIMING_TINT_CONTROL });
}

static void KeepFairyCargo(Actor* actor) {
    if (actor != sCargo || actor->id != ACTOR_EN_ELFORG) {
        return;
    }
    actor->world.pos = sBeetlePos;
    actor->velocity.y = 0.0f;
    actor->speed = 0.0f;
}

static void ForgetDestroyedActor(Actor* actor) {
    if (sCargo == actor) {
        sCargo = NULL;
    }
    if (sTarget == actor) {
        sTarget = NULL;
    }
    if (sCandidate == actor) {
        sCandidate = NULL;
    }
}

static void DrawGetItem(PlayState* play, S2HGetItemEntry* getItemEntry) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    Matrix_Scale(BEETLE_GIVE_SCALE, BEETLE_GIVE_SCALE, BEETLE_GIVE_SCALE, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sWholeDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static const S2HCustomItemRandomizer sRandomizerItem = {
    sizeof(S2HCustomItemRandomizer), 0, S2H_CUSTOM_ITEM_TYPE_ITEM, 0, 1, BEETLE_KEY, BEETLE_KEY, "",
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
    if (peerId == 0 || sApi->GetNetPeerState(peerId, "nei.beetle.visual", visual, sizeof(*visual)) != sizeof(*visual)) {
        return false;
    }
    if (visual->active != 1 || visual->adult > 1 || visual->scene != gPlayState->sceneId || visual->flags[0] > 1 ||
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
        visual.active = (sPhase != BEETLE_STOWED);
        if (visual.active) {
            visual.flags[0] = IsAway();
            visual.values[0] = sBeetlePos.x;
            visual.values[1] = sBeetlePos.y;
            visual.values[2] = sBeetlePos.z;
            visual.values[3] = sBeetleRot.x;
            visual.values[4] = sBeetleRot.y;
            visual.values[5] = sBeetleRot.z;
            visual.values[6] = sWingSpread;
        }
        if (sRemoteVisualSent && (uint32_t)(play->gameplayFrames - sRemoteVisualFrame) < REMOTE_VISUAL_INTERVAL) {
            return;
        }
    }
    if (sRemoteVisualSent && memcmp(&visual, &sLastRemoteVisual, sizeof(visual)) == 0) {
        return;
    }
    if (sApi->SetNetPeerState("nei.beetle.visual", &visual, sizeof(visual))) {
        sLastRemoteVisual = visual;
        sRemoteVisualSent = true;
        sRemoteVisualFrame = play != NULL ? play->gameplayFrames : 0;
    }
}

static void DrawRemotePlayer(Actor* actor, PlayState* play) {
    if (actor->id == ACTOR_PLAYER || !sRemoteVisualReady || sApi->GetNetPeerIdForActor(actor) == 0) {
        return;
    }
    DrawBeetle(actor, play);
}

static void InitRemoteVisuals(void) {
    if (!S2H_MOD_API_HAS(sApi, RegisterNetChannel) || !S2H_MOD_API_HAS(sApi, SetNetPeerState) ||
        !S2H_MOD_API_HAS(sApi, GetNetPeerState) || !S2H_MOD_API_HAS(sApi, GetNetPeerIdForActor)) {
        return;
    }
    sRemoteVisualReady = sApi->RegisterNetChannel("nei.beetle.visual", 1, ReceiveRemoteVisual);
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

static void GrantBeetleLogic(void) {
    static const Z64LogicSource sBeetle = { BEETLE_KEY };
    static const char* const sFlying[] = { "CanUseProjectile", "HookshotOrBoomerang", "CanHitSwitch",
                                           "CanAttack",        "CanDamage",           NULL };

    if (Z64Logic_Init(sApi)) {
        Z64Logic_GrantCapabilities(&sBeetle, sFlying);
    }
}

S2H_MOD_EXPORT void ModInit(void) {
    GrantBeetleLogic();
    InitRemoteVisuals();
    if (!S2H_MOD_API_HAS(sApi, GetEquippedCustomItem)) {
        return;
    }

    S2HCustomItemDefinition beetle = Z64Items_Define(BEETLE_KEY, sIconTex, sNameTex);

    Z64Items_SetButtons(&beetle, S2H_CUSTOM_ITEM_C_BUTTON | S2H_CUSTOM_ITEM_DPAD);
    Z64Items_SetPlacement(&beetle, 0, 8, 0);
    Z64Items_SetTextbox(&beetle, "You got the %rBeetle%w! A&clockwork flier from the sky&"
                                 "above the clouds, and it&answers to your hand.^"
                                 "Hold %y\xB2%w to aim and let go to fly&it: steer with the stick, hurry&"
                                 "it with %y\xB0%w, lock on with %g\xB5%w, or&"
                                 "leave it to finish alone with %y\xB1%w.^"
                                 "It carries %grupees%w home and&knocks the wind out of what it&hits.");
    Z64Items_SetPauseText(&beetle, "%rBeetle&%wHold %y\xB2%w to aim, release to fly.&"
                                   "Steer with the stick, %y\xB0%w to hurry, %g\xB5%w&to lock on.");
    Z64Items_SetAction(&beetle, TakeOutBeetle, UpdateBeetleInHand);
    Z64Items_SetHeldCallbacks(&beetle, RequestAim, PutBeetleAway, NULL);
    beetle.modelGroup = PLAYER_MODELGROUP_DEFAULT;
    beetle.getItemEntry.drawFunc = DrawGetItem;

    Z64Items_SetLogic(&beetle, &sRandomizerItem);
    if (!Z64Items_Register(sApi, &beetle)) {
        return;
    }
    S2H_REGISTER_HOOK(sApi, OnPlayerUpdate, UpdateBeetleWorld);
    S2H_REGISTER_HOOK(sApi, OnPlayDrawEnd, DrawAimReticle);
    S2H_REGISTER_HOOK_FOR_ID(sApi, OnActorUpdate, ACTOR_EN_ELFORG, KeepFairyCargo);
    S2H_REGISTER_HOOK(sApi, OnActorDestroy, ForgetDestroyedActor);
    S2H_REGISTER_HOOK(sApi, OnSceneInit, ForgetScene);
    S2H_REGISTER_HOOK(sApi, OnPlayerFilterInput, HoldPilotInput);
    S2H_REGISTER_HOOK_FOR_ID(sApi, OnActorDrawEnd, ACTOR_PLAYER, DrawBeetle);
}
