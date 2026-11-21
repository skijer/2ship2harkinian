#include <math.h>
#include <string.h>

#include "z64items.h"
#include "z64logic.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "z64sound_source.h"
#include "overlays/actors/ovl_En_Ishi/z_en_ishi.h"

#define SPINNER_KEY "nei.spinner"
#define SPINNER_CHARGE_MAX 60
#define SPINNER_SPEED_MIN 12.0f
#define SPINNER_SPEED_MAX 25.0f
#define SPINNER_SPEED_HOMING 30.0f
#define SPINNER_HOVER_HEIGHT 17.0f
#define SPINNER_RIDE_FRAMES 120
#define SPINNER_ATTACK_FRAMES 20
#define SPINNER_RECOIL_FRAMES 15
#define SPINNER_HOMING_WINDUP_FRAMES 10
#define SPINNER_HOMING_AIM_FRAMES 5
#define SPINNER_HOMING_FLIGHT_FRAMES 30
#define SPINNER_HOMING_ARC 120.0f
#define SPINNER_ATTACK_RADIUS 30
#define SPINNER_COLLIDER_HEIGHT 16
#define SPINNER_ATTACK_DAMAGE 2
#define SPINNER_HOMING_DAMAGE 4
#define SPINNER_ROCK_REACH 40.0f
#define SPINNER_MODEL_SCALE 0.2f
#define SPINNER_GIVE_SCALE 0.3f
#define SPINNER_PLAYER_GRAVITY -1.2f

typedef enum {
    SPINNER_STOWED,
    SPINNER_IN_HAND,
    SPINNER_CHARGING,
    SPINNER_RIDING,
    SPINNER_ATTACKING,
    SPINNER_HOMING_WINDUP,
    SPINNER_HOMING_AIM,
    SPINNER_HOMING_LAUNCH,
    SPINNER_RECOIL,
} SpinnerPhase;

static const ALIGN_ASSET(2) char sIconTex[] = "__OTR__textures/icon_item_custom/gItemIconSpinnerTex";
static const ALIGN_ASSET(2) char sNameTex[] = "__OTR__textures/item_name_custom/gSpinnerNameTex";
static const ALIGN_ASSET(2) char sSpinnerDL[] = "__OTR__objects/object_nei_spinner_tp/spinner_dl";

static const u32 sItemButtons[S2H_ITEM_BUTTON_COUNT] = { BTN_B,      BTN_CLEFT, BTN_CDOWN, BTN_CRIGHT, 0,
                                                         BTN_DRIGHT, BTN_DLEFT, BTN_DDOWN, BTN_DUP };

static ColliderCylinderInit sAttackColliderInit = {
    {
        COL_MATERIAL_NONE,
        AT_ON | AT_TYPE_PLAYER,
        AC_NONE,
        OC1_NONE,
        OC2_NONE,
        COLSHAPE_CYLINDER,
    },
    {
        ELEM_MATERIAL_UNK2,
        { DMG_GORON_POUND | DMG_SWORD, 0x00, SPINNER_ATTACK_DAMAGE },
        { 0, 0, 0 },
        ATELEM_ON | ATELEM_SFX_NORMAL,
        ACELEM_NONE,
        OCELEM_NONE,
    },
    { SPINNER_ATTACK_RADIUS, SPINNER_COLLIDER_HEIGHT, 0, { 0, 0, 0 } },
};

static const char* const sRequiredHooks[] = { "OnGameFrameUpdate", "OnPlayerUpdate", "OnActorDrawEnd",
                                              "OnPassPlayerInputs", "OnSceneInit" };
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

static ColliderCylinder sAttackCollider;
static bool sIsColliderReady;
static bool sIsMountPending;
static u8 sPhase;
static s16 sCharge;
static s16 sTimer;
static s16 sAngle;
static f32 sSpeed;
static Actor* sTarget;

void Player_StopHorizontalMovement(Player* player);
void Player_Anim_PlayLoop(PlayState* play, Player* player, PlayerAnimationHeader* anim);
PlayerAnimationHeader* Player_GetIdleAnim(Player* player);

static void SpawnFloorShockwave(Actor* actor, PlayState* play) {
    Vec3f zero = { 0.0f, 0.0f, 0.0f };
    Vec3f pos = { actor->world.pos.x, actor->floorHeight + 2.0f, actor->world.pos.z };

    EffectSsBlast_SpawnWhiteCustomScale(play, &pos, &zero, &zero, 60, 150, 8);
}

static bool IsRiding(void) {
    return sPhase >= SPINNER_CHARGING;
}

static u32 FindEquippedButtonMask(void) {
    for (u8 button = 0; button < S2H_ITEM_BUTTON_COUNT; button++) {
        const char* equipped = sApi->GetEquippedCustomItem(button);
        if (equipped != NULL && strcmp(equipped, SPINNER_KEY) == 0) {
            return sItemButtons[button];
        }
    }
    return 0;
}

static bool IsButtonHeld(PlayState* play) {
    return (play->state.input[0].cur.button & FindEquippedButtonMask()) != 0;
}

static void SilenceSpinner(void) {
    AudioSfx_StopById(NA_SE_EV_ROCK_SLIDE);
    AudioSfx_StopById(NA_SE_IT_SHIELD_BOUND);
    AudioSfx_StopById(NA_SE_IT_SWORD_SWING);
    AudioSfx_StopById(NA_SE_IT_HAMMER_SWING);
}

// Riding borrows the spin-attack flag, the one vanilla uses to keep the camera and turning off Link's back.
static void HoldRidingFlags(Player* player) {
    player->stateFlags1 |= PLAYER_STATE1_CHARGING_SPIN_ATTACK;
    player->stateFlags2 |= PLAYER_STATE2_20;
}

static void RestoreIdleAnimation(Player* player, PlayState* play) {
    player->skelAnime.playSpeed = 1.0f;
    Player_Anim_PlayLoop(play, player, Player_GetIdleAnim(player));
}

static void Dismount(Player* player, PlayState* play) {
    RestoreIdleAnimation(player, play);
    sPhase = SPINNER_IN_HAND;
    sTarget = NULL;
    sAttackCollider.base.atFlags &= ~(AT_ON | AT_HIT);
    player->actor.gravity = SPINNER_PLAYER_GRAVITY;
    player->actor.shape.rot.x = 0;
    player->actor.shape.rot.z = 0;
    player->stateFlags1 &= ~PLAYER_STATE1_CHARGING_SPIN_ATTACK;
    player->stateFlags2 &= ~PLAYER_STATE2_20;
    SilenceSpinner();
    Audio_PlaySfx_AtPos(&player->actor.world.pos, NA_SE_PL_LAND_GRASS);
}

// Riding hovers off the floor, so the ground check only guards mounting: without this the engine would
// reject the second press and the spin attack would never fire.
static bool CanUseSpinner(Player* player, PlayState* play) {
    if (IsRiding()) {
        return true;
    }
    return player->transformation == PLAYER_FORM_HUMAN && (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) &&
           player->meleeWeaponState == PLAYER_MELEE_WEAPON_STATE_0 && !(player->stateFlags1 & PLAYER_STATE1_8000000);
}

static void Mount(Player* player, PlayState* play) {
    if (IsRiding() || !CanUseSpinner(player, play)) {
        return;
    }
    Collider_SetCylinder(play, &sAttackCollider, &player->actor, &sAttackColliderInit);
    sAttackCollider.base.atFlags &= ~AT_ON;
    sPhase = SPINNER_CHARGING;
    sCharge = 0;
    player->actor.velocity.y = 8.0f;
    Audio_PlaySfx_AtPos(&player->actor.world.pos, NA_SE_IT_SWORD_SWING);
}

static void PlaySpinPose(Player* player, PlayState* play, f32 speed) {
    PlayerAnimationHeader* pose = (PlayerAnimationHeader*)dgPlayerAnim_link_fighter_Lpower_kiru_wait;

    if (!BEN_ANIM_EQUAL(player->skelAnime.animation, dgPlayerAnim_link_fighter_Lpower_kiru_wait)) {
        PlayerAnimation_Change(play, &player->skelAnime, pose, speed, 0.0f, Animation_GetLastFrame(pose), ANIMMODE_LOOP,
                               -4.0f);
    }
    player->skelAnime.playSpeed = speed;
}

static void HoverAboveFloor(Player* player, f32 lift) {
    Math_StepToF(&player->actor.world.pos.y, player->actor.floorHeight + SPINNER_HOVER_HEIGHT + lift, 4.0f);
    player->actor.velocity.y = 0.0f;
    player->actor.gravity = 0.0f;
}

static void MoveAlong(Player* player, s16 angle, f32 speed) {
    player->actor.world.pos.x += Math_SinS(angle) * speed;
    player->actor.world.pos.z += Math_CosS(angle) * speed;
}

static void BounceOffWalls(Player* player) {
    if (player->actor.bgCheckFlags & BGCHECKFLAG_WALL) {
        sAngle += 0x8000;
        Audio_PlaySfx_AtPos(&player->actor.world.pos, NA_SE_IT_SHIELD_BOUND);
    }
}

static void SteerWithStick(PlayState* play, f32 deadZone, f32 rate) {
    f32 stickX = play->state.input[0].rel.stick_x;

    if (fabsf(stickX) > deadZone) {
        sAngle += (s16)(stickX * rate);
    }
}

static void LeaveSpinnerAt(Player* player, PlayState* play, Actor* cucco) {
    s16 awayYaw = Math_Vec3f_Yaw(&cucco->world.pos, &player->actor.world.pos);

    Dismount(player, play);
    MoveAlong(player, awayYaw, 50.0f);
    player->actor.velocity.y = 8.0f;
    Actor_PlaySfx(&player->actor, NA_SE_PL_DAMAGE);
    Actor_PlaySfx(cucco, NA_SE_EV_CHICKEN_CRY_M);
}

static bool IsWithinXZ(Actor* actor, Player* player, f32 radius) {
    f32 dx = actor->world.pos.x - player->actor.world.pos.x;
    f32 dz = actor->world.pos.z - player->actor.world.pos.z;

    return SQ(dx) + SQ(dz) < SQ(radius);
}

static bool KnockedOffByCucco(Player* player, PlayState* play) {
    static const u8 sCuccoCategories[] = { ACTORCAT_PROP, ACTORCAT_ENEMY };

    for (u32 i = 0; i < ARRAY_COUNT(sCuccoCategories); i++) {
        for (Actor* actor = play->actorCtx.actorLists[sCuccoCategories[i]].first; actor != NULL; actor = actor->next) {
            if (actor->update != NULL && (actor->id == ACTOR_EN_NIW || actor->id == ACTOR_EN_ATTACK_NIW) &&
                IsWithinXZ(actor, player, SPINNER_ATTACK_RADIUS + 30.0f)) {
                LeaveSpinnerAt(player, play, actor);
                return true;
            }
        }
    }
    return false;
}

static void ShatterRollingBoulders(Player* player, PlayState* play) {
    Actor* next;

    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_PROP].first; actor != NULL; actor = next) {
        next = actor->next;
        if (actor->update == NULL || actor->id != ACTOR_EN_GOROIWA ||
            !IsWithinXZ(actor, player, SPINNER_ATTACK_RADIUS + SPINNER_ROCK_REACH)) {
            continue;
        }
        func_800BBFB0(play, &actor->world.pos, 140.0f, 6, 180, 90, 1);
        func_800BBFB0(play, &actor->world.pos, 140.0f, 12, 80, 90, 1);
        SoundSource_PlaySfxAtFixedWorldPos(play, &actor->world.pos, 40, NA_SE_EV_WALL_BROKEN);
        Actor_Kill(actor);
    }
}

#ifdef _WIN32
// Compare the engine's function address, not this DLL's import trampoline.
__declspec(dllimport) void EnIshi_Idle(EnIshi* rock, PlayState* play);
#else
void EnIshi_Idle(EnIshi* rock, PlayState* play);
#endif
void EnIshi_SpawnDebrisBoulder(Actor* rock, PlayState* play);
void EnIshi_SpawnDustBoulder(Actor* rock, PlayState* play);

static bool IsSilverBoulderInReach(Player* player, EnIshi* rock) {
    f32 bottom = rock->actor.world.pos.y + rock->collider.dim.yShift;
    f32 attackBottom = sAttackCollider.dim.pos.y;

    return rock->actionFunc == EnIshi_Idle && !(rock->flags & ISHI_FLAG_CUTSCENE_ROCK) &&
           IsWithinXZ(&rock->actor, player, sAttackCollider.dim.radius + rock->collider.dim.radius) &&
           attackBottom + sAttackCollider.dim.height >= bottom && attackBottom <= bottom + rock->collider.dim.height;
}

// Breaks the way a Goron's throw does: the boulder's switch flag keeps it gone.
static void ShatterSilverBoulders(Player* player, PlayState* play) {
    Actor* next;

    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_PROP].first; actor != NULL; actor = next) {
        next = actor->next;
        if (actor->update == NULL || actor->id != ACTOR_EN_ISHI || actor->parent != NULL ||
            ENISHI_GET_SIZE_FLAG(actor) != ISHI_SIZE_SILVER_BOULDER || !IsSilverBoulderInReach(player, (EnIshi*)actor)) {
            continue;
        }
        Flags_SetSwitch(play, ENISHI_GET_SWITCH_FLAG(actor));
        EnIshi_SpawnDebrisBoulder(actor, play);
        EnIshi_SpawnDustBoulder(actor, play);
        SoundSource_PlaySfxAtFixedWorldPos(play, &actor->world.pos, 40, NA_SE_EV_WALL_BROKEN);
        Actor_Kill(actor);
    }
}

static bool StrikeAround(Player* player, PlayState* play, u8 damage) {
    sAttackCollider.dim.pos.x = player->actor.world.pos.x;
    sAttackCollider.dim.pos.y = player->actor.world.pos.y - 25.0f + SPINNER_HOVER_HEIGHT;
    sAttackCollider.dim.pos.z = player->actor.world.pos.z;
    sAttackCollider.elem.atDmgInfo.damage = damage;
    sAttackCollider.base.atFlags |= AT_ON | AT_TYPE_PLAYER;
    if (sAttackCollider.base.atFlags & AT_HIT) {
        sAttackCollider.base.atFlags &= ~AT_HIT;
        Audio_PlaySfx_AtPos(&player->actor.world.pos, NA_SE_IT_HAMMER_HIT);
    }
    CollisionCheck_SetAT(play, &play->colChkCtx, &sAttackCollider.base);
    ShatterRollingBoulders(player, play);
    ShatterSilverBoulders(player, play);
    return KnockedOffByCucco(player, play);
}

static void Launch(Player* player) {
    sSpeed = SPINNER_SPEED_MIN + (SPINNER_SPEED_MAX - SPINNER_SPEED_MIN) * ((f32)sCharge / SPINNER_CHARGE_MAX);
    player->actor.velocity.y = 6.0f;
    if (player->focusActor != NULL) {
        sTarget = player->focusActor;
        sAngle = Math_Vec3f_Yaw(&player->actor.world.pos, &sTarget->world.pos);
        sTimer = 0;
        sPhase = SPINNER_HOMING_WINDUP;
        Audio_PlaySfx_AtPos(&player->actor.world.pos, NA_SE_IT_SWORD_PUTAWAY);
        return;
    }
    sAngle = player->actor.shape.rot.y;
    sTimer = SPINNER_RIDE_FRAMES;
    sPhase = SPINNER_RIDING;
    Audio_PlaySfx_AtPos(&player->actor.world.pos, NA_SE_IT_HAMMER_SWING);
}

static void UpdateCharging(Player* player, PlayState* play) {
    if (IsButtonHeld(play)) {
        sCharge = MIN(sCharge + 1, SPINNER_CHARGE_MAX);
    }
    HoverAboveFloor(player, 0.0f);
    PlaySpinPose(player, play, 1.0f);
    if ((play->gameplayFrames % 20) == 0) {
        Audio_PlaySfx_AtPos(&player->actor.world.pos, NA_SE_IT_SHIELD_BOUND);
    }
    if (!IsButtonHeld(play)) {
        Launch(player);
    }
}

static void StartAttack(Player* player, PlayState* play) {
    sPhase = SPINNER_ATTACKING;
    sTimer = SPINNER_ATTACK_FRAMES;
    Player_PlaySfx(player, NA_SE_VO_LI_MAGIC_ATTACK);
    Audio_PlaySfx_AtPos(&player->actor.world.pos, NA_SE_IT_HAMMER_SWING);
    SpawnFloorShockwave(&player->actor, play);
}

static void UpdateRiding(Player* player, PlayState* play) {
    PlaySpinPose(player, play, 1.0f);
    SteerWithStick(play, 10.0f, 6.0f);
    BounceOffWalls(player);
    MoveAlong(player, sAngle, sSpeed);
    player->yaw = player->actor.world.rot.y = player->actor.shape.rot.y = sAngle;
    HoverAboveFloor(player, 0.0f);
    if ((play->gameplayFrames % 20) == 0) {
        Audio_PlaySfx_AtPos(&player->actor.world.pos, NA_SE_EV_ROCK_SLIDE);
    }
    if (--sTimer <= 0) {
        Dismount(player, play);
    }
}

static void UpdateAttacking(Player* player, PlayState* play) {
    if (--sTimer <= 0) {
        sAttackCollider.base.atFlags &= ~AT_ON;
        sTimer = SPINNER_RIDE_FRAMES;
        sPhase = SPINNER_RIDING;
        return;
    }
    PlaySpinPose(player, play, 3.0f);
    SteerWithStick(play, 15.0f, 3.0f);
    BounceOffWalls(player);
    MoveAlong(player, sAngle, sSpeed * 1.3f);
    player->yaw = player->actor.world.rot.y = player->actor.shape.rot.y = sAngle;
    HoverAboveFloor(player, 10.0f);
    if ((play->gameplayFrames % 8) == 0) {
        Audio_PlaySfx_AtPos(&player->actor.world.pos, NA_SE_IT_SWORD_SWING);
    }
    StrikeAround(player, play, SPINNER_ATTACK_DAMAGE);
}

static void TrackTarget(Player* player) {
    if (sTarget != NULL && sTarget->update != NULL) {
        sAngle = Math_Vec3f_Yaw(&player->actor.world.pos, &sTarget->world.pos);
    }
}

static void UpdateHomingWindup(Player* player, PlayState* play) {
    HoverAboveFloor(player, 0.0f);
    if ((play->gameplayFrames % 8) == 0) {
        Audio_PlaySfx_AtPos(&player->actor.world.pos, NA_SE_IT_SHIELD_BOUND);
    }
    if (++sTimer >= SPINNER_HOMING_WINDUP_FRAMES) {
        sTimer = 0;
        sPhase = SPINNER_HOMING_AIM;
        Audio_PlaySfx_AtPos(&player->actor.world.pos, NA_SE_IT_SWORD_SWING_HARD);
    }
}

static void UpdateHomingAim(Player* player, PlayState* play) {
    HoverAboveFloor(player, 0.0f);
    TrackTarget(player);
    if (++sTimer >= SPINNER_HOMING_AIM_FRAMES) {
        sTimer = 0;
        sSpeed = SPINNER_SPEED_HOMING;
        sPhase = SPINNER_HOMING_LAUNCH;
        Audio_PlaySfx_AtPos(&player->actor.world.pos, NA_SE_IT_HAMMER_SWING);
    }
}

static bool HasReachedTarget(Player* player) {
    return sTarget != NULL && sTarget->update != NULL &&
           Math_Vec3f_DistXYZ(&player->actor.world.pos, &sTarget->world.pos) < SPINNER_ATTACK_RADIUS;
}

static void StartRecoil(void) {
    sTarget = NULL;
    sTimer = SPINNER_RECOIL_FRAMES;
    sPhase = SPINNER_RECOIL;
}

static void UpdateHomingLaunch(Player* player, PlayState* play) {
    f32 progress = MIN(++sTimer / (f32)SPINNER_HOMING_FLIGHT_FRAMES, 1.0f);

    HoverAboveFloor(player, Math_SinS((s16)(progress * 0x8000)) * SPINNER_HOMING_ARC);
    PlaySpinPose(player, play, 3.0f);
    TrackTarget(player);
    MoveAlong(player, sAngle, sSpeed);
    if (StrikeAround(player, play, SPINNER_HOMING_DAMAGE)) {
        return;
    }
    if (HasReachedTarget(player)) {
        SpawnFloorShockwave(sTarget, play);
        Audio_PlaySfx_AtPos(&sTarget->world.pos, NA_SE_IT_HAMMER_HIT);
        StartRecoil();
        return;
    }
    if (sTimer > SPINNER_HOMING_FLIGHT_FRAMES) {
        StartRecoil();
        return;
    }
    if ((play->gameplayFrames % 6) == 0) {
        Audio_PlaySfx_AtPos(&player->actor.world.pos, NA_SE_EV_ROCK_SLIDE);
    }
}

static void UpdateRecoil(Player* player, PlayState* play) {
    if (--sTimer <= 0) {
        Dismount(player, play);
        return;
    }
    sAttackCollider.base.atFlags &= ~AT_ON;
    PlaySpinPose(player, play, 0.5f);
    sSpeed *= 0.9f;
    MoveAlong(player, sAngle + 0x8000, sSpeed * 0.3f);
    HoverAboveFloor(player, 0.0f);
}

static bool IsRideInterrupted(Player* player) {
    return player->invincibilityTimer < 0 || (player->stateFlags1 & PLAYER_STATE1_8000000) ||
           player->meleeWeaponState != PLAYER_MELEE_WEAPON_STATE_0;
}

static bool WasDismountPressed(PlayState* play) {
    return (play->state.input[0].press.button & (BTN_A | BTN_B)) != 0;
}

// The return value is Link's "upper body is busy" flag: claiming it while merely holding the spinner would block rolls.
static int32_t UpdateSpinnerInHand(Player* player, PlayState* play) {
    if (!IsRiding()) {
        return 0;
    }
    if (IsRideInterrupted(player) || WasDismountPressed(play)) {
        Dismount(player, play);
        return 1;
    }
    HoldRidingFlags(player);
    Player_StopHorizontalMovement(player);
    switch (sPhase) {
        case SPINNER_CHARGING:
            UpdateCharging(player, play);
            break;
        case SPINNER_RIDING:
            UpdateRiding(player, play);
            break;
        case SPINNER_ATTACKING:
            UpdateAttacking(player, play);
            break;
        case SPINNER_HOMING_WINDUP:
            UpdateHomingWindup(player, play);
            break;
        case SPINNER_HOMING_AIM:
            UpdateHomingAim(player, play);
            break;
        case SPINNER_HOMING_LAUNCH:
            UpdateHomingLaunch(player, play);
            break;
        case SPINNER_RECOIL:
            UpdateRecoil(player, play);
            break;
        default:
            Dismount(player, play);
            break;
    }
    return 1;
}

static void TakeOutSpinner(PlayState* play, Player* player) {
    if (!sIsColliderReady) {
        Collider_InitCylinder(play, &sAttackCollider);
        sIsColliderReady = true;
    }
    sPhase = SPINNER_IN_HAND;
    sIsMountPending = true;
}

static void RideOrAttack(Player* player, PlayState* play) {
    if (sPhase == SPINNER_IN_HAND) {
        Mount(player, play);
        return;
    }
    if (sPhase == SPINNER_RIDING) {
        StartAttack(player, play);
    }
}

static void PutSpinnerAway(Player* player, PlayState* play) {
    if (IsRiding()) {
        Dismount(player, play);
    }
    sIsMountPending = false;
    sPhase = SPINNER_STOWED;
}

static bool IsChangingHeldItem(Player* player) {
    return (player->stateFlags3 & PLAYER_STATE3_START_CHANGING_HELD_ITEM) != 0;
}

static void MountWhenReady(Player* player) {
    PlayState* play = gPlayState;

    if (play == NULL || !sIsMountPending || IsChangingHeldItem(player)) {
        return;
    }
    sIsMountPending = false;
    if (player->heldItemAction == PLAYER_IA_CUSTOM && sPhase == SPINNER_IN_HAND) {
        Mount(player, play);
    }
}

static void KeepRideButtons(Input* input) {
    if (!IsRiding()) {
        return;
    }
    input->cur.button &= ~(BTN_A | BTN_B | BTN_R);
    input->press.button &= ~(BTN_A | BTN_B | BTN_R);
}

static void ForgetScene(s16 sceneId, s8 spawnNum) {
    sIsMountPending = false;
    sPhase = SPINNER_STOWED;
}

static bool IsSpinnerUnder(Player* player, PlayState* play) {
    RemoteVisualState visual;

    if (player == GET_PLAYER(play)) {
        return IsRiding();
    }
    return ReadRemoteVisual(player, &visual);
}

static void DrawSpinner(Actor* actor, PlayState* play) {
    Player* player = (Player*)actor;

    if (!IsSpinnerUnder(player, play) || player->actor.scale.y < 0.0f ||
        (player->stateFlags2 & PLAYER_STATE2_20000000)) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    Matrix_Translate(player->actor.world.pos.x, player->actor.world.pos.y, player->actor.world.pos.z, MTXMODE_NEW);
    Matrix_RotateYF(BINANG_TO_RAD((s16)(play->gameplayFrames * 0x800)), MTXMODE_APPLY);
    Matrix_Scale(SPINNER_MODEL_SCALE, SPINNER_MODEL_SCALE, SPINNER_MODEL_SCALE, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sSpinnerDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawGetItem(PlayState* play, S2HGetItemEntry* getItemEntry) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    Matrix_Scale(SPINNER_GIVE_SCALE, SPINNER_GIVE_SCALE, SPINNER_GIVE_SCALE, MTXMODE_APPLY);
    Matrix_RotateYF((s16)(play->gameplayFrames * 2) * 0.01f, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sSpinnerDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static const S2HCustomItemRandomizer sRandomizerItem = {
    sizeof(S2HCustomItemRandomizer), 0, S2H_CUSTOM_ITEM_TYPE_ITEM, 0, 1, SPINNER_KEY, SPINNER_KEY, "",
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
    if (peerId == 0 ||
        sApi->GetNetPeerState(peerId, "nei.spinner.visual", visual, sizeof(*visual)) != sizeof(*visual)) {
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
        visual.active = IsRiding();
        if (sRemoteVisualSent && (uint32_t)(play->gameplayFrames - sRemoteVisualFrame) < REMOTE_VISUAL_INTERVAL) {
            return;
        }
    }
    if (sRemoteVisualSent && memcmp(&visual, &sLastRemoteVisual, sizeof(visual)) == 0) {
        return;
    }
    if (sApi->SetNetPeerState("nei.spinner.visual", &visual, sizeof(visual))) {
        sLastRemoteVisual = visual;
        sRemoteVisualSent = true;
        sRemoteVisualFrame = play != NULL ? play->gameplayFrames : 0;
    }
}

static void DrawRemotePlayer(Actor* actor, PlayState* play) {
    if (actor->id == ACTOR_PLAYER || !sRemoteVisualReady || sApi->GetNetPeerIdForActor(actor) == 0) {
        return;
    }
    DrawSpinner(actor, play);
}

static void InitRemoteVisuals(void) {
    if (!S2H_MOD_API_HAS(sApi, RegisterNetChannel) || !S2H_MOD_API_HAS(sApi, SetNetPeerState) ||
        !S2H_MOD_API_HAS(sApi, GetNetPeerState) || !S2H_MOD_API_HAS(sApi, GetNetPeerIdForActor)) {
        return;
    }
    sRemoteVisualReady = sApi->RegisterNetChannel("nei.spinner.visual", 1, ReceiveRemoteVisual);
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

static void GrantSpinnerLogic(void) {
    static const Z64LogicSource sSpinner = { SPINNER_KEY };
    static const char* const sRamming[] = { "BlastOrSmash", "CanHitSwitch", "CanAttack", "CanDamage", NULL };

    if (Z64Logic_Init(sApi)) {
        Z64Logic_GrantCapabilities(&sSpinner, sRamming);
        Z64Logic_GrantBoulderSmashing(&sSpinner);
    }
}

S2H_MOD_EXPORT void ModInit(void) {
    GrantSpinnerLogic();
    InitRemoteVisuals();
    if (!S2H_MOD_API_HAS(sApi, RegisterCustomItem)) {
        return;
    }

    S2HCustomItemDefinition spinner = Z64Items_Define(SPINNER_KEY, sIconTex, sNameTex);

    Z64Items_SetButtons(&spinner, S2H_CUSTOM_ITEM_C_BUTTON | S2H_CUSTOM_ITEM_DPAD);
    Z64Items_SetForms(&spinner, S2H_CUSTOM_ITEM_FORM_BIT(PLAYER_FORM_HUMAN));
    Z64Items_SetPlacement(&spinner, 0, 1, 0);
    Z64Items_SetTextbox(&spinner, "You got the %rSpinner%w! An&ancient spinning top that rides&"
                                  "on its own grinding force.^"
                                  "Hold %y\xB2%w to wind it up, then let&go to ride. Press %y\xB2%w again to&"
                                  "%rspin attack%w.^"
                                  "Let go while %y\xB5%w-targeting and it&homes in on your foe.");
    Z64Items_SetPauseText(&spinner, "%rSpinner&%wHold %y\xB2%w to charge, release to ride.&"
                                    "Press %y\xB2%w to spin attack.");
    Z64Items_SetCanUse(&spinner, CanUseSpinner);
    Z64Items_SetAction(&spinner, TakeOutSpinner, UpdateSpinnerInHand);
    Z64Items_SetHeldCallbacks(&spinner, RideOrAttack, PutSpinnerAway, NULL);
    spinner.modelGroup = PLAYER_MODELGROUP_DEFAULT;
    spinner.getItemEntry.drawFunc = DrawGetItem;

    Z64Items_SetLogic(&spinner, &sRandomizerItem);
    if (!Z64Items_Register(sApi, &spinner)) {
        return;
    }
    S2H_REGISTER_HOOK(sApi, OnPlayerUpdate, MountWhenReady);
    S2H_REGISTER_HOOK(sApi, OnPassPlayerInputs, KeepRideButtons);
    S2H_REGISTER_HOOK(sApi, OnSceneInit, ForgetScene);
    S2H_REGISTER_HOOK_FOR_ID(sApi, OnActorDrawEnd, ACTOR_PLAYER, DrawSpinner);
}
