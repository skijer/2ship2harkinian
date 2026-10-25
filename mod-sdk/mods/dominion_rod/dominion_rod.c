/**
 * Dominion Rod: hold the button to aim, let go to fire a golden orb. It possesses a Beamos or an Armos,
 * which then copies Link's steps; the rod's button makes it attack, and anything else lets it go.
 * An orb that reaches a Cane of Somaria summon swaps Link with it.
 */

#include <string.h>

#include "z64items.h"
#include "z64aiming.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "overlays/actors/ovl_En_Bom/z_en_bom.h"

#define DOMROD_KEY "nei.dominion_rod"
#define DOMROD_ORB_SPEED 18.0f
#define DOMROD_ORB_SPEED_HOMING 22.0f
#define DOMROD_ORB_RETURN_SPEED 20.0f
#define DOMROD_MAX_RANGE 2500.0f
#define DOMROD_CATCH_DISTANCE 60.0f
#define DOMROD_LAUNCH_HEIGHT 45.0f
#define DOMROD_LAUNCH_REACH 30.0f
#define DOMROD_FLIGHT_FRAMES 600
#define DOMROD_CATCH_FRAMES 12
#define DOMROD_HOMING_TURN 0x400
#define DOMROD_COLLIDER_RADIUS 35
#define DOMROD_COLLIDER_HEIGHT 50
#define DOMROD_POSSESS_REACH (DOMROD_COLLIDER_RADIUS + 40.0f)
#define DOMROD_SWAP_REACH (DOMROD_COLLIDER_RADIUS + 30.0f)
#define DOMROD_LIGHT_RADIUS 200
#define DOMROD_HOP_VELOCITY 12.0f
#define DOMROD_BEAMOS_COOLDOWN 305
#define DOMROD_SNAP_TO_FLOOR 100.0f

typedef enum {
    DOMROD_STOWED,
    DOMROD_IN_HAND,
    DOMROD_AIMING,
    DOMROD_FLYING,
    DOMROD_RETURNING,
    DOMROD_CONTROLLING,
    DOMROD_CATCHING,
} DominionPhase;

static const ALIGN_ASSET(2) char sIconTex[] = "__OTR__textures/icon_item_custom/gItemIconDominionRodTex";
static const ALIGN_ASSET(2) char sNameTex[] = "__OTR__textures/item_name_custom/gDominionRodNameTex";
static const ALIGN_ASSET(2) char sRodDL[] = "__OTR__objects/object_nei_dominion_rod/gNeiDominionRodDL";

static const u32 sItemButtons[S2H_ITEM_BUTTON_COUNT] = { BTN_B,   BTN_CLEFT, BTN_CDOWN, BTN_CRIGHT, 0,
                                                         BTN_DRIGHT, BTN_DLEFT, BTN_DDOWN, BTN_DUP };
static const u32 sReleaseButtons = BTN_A | BTN_B | BTN_R | BTN_START | BTN_CLEFT | BTN_CDOWN | BTN_CRIGHT |
                                   BTN_DUP | BTN_DDOWN | BTN_DLEFT | BTN_DRIGHT;
static const char* const sSummonKeys[] = { "nei.somaria.statue", "nei.somaria.crate", "nei.somaria.bird",
                                           "nei.somaria.platform" };

// Damage 0: the orb takes what it passes over by distance, and the cylinder only reports the rest.
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
        { 0, 0x00, 0x00 },
        { 0, 0x00, 0x00 },
        ATELEM_ON | ATELEM_NEAREST,
        ACELEM_NONE,
        OCELEM_ON,
    },
    { DOMROD_COLLIDER_RADIUS, DOMROD_COLLIDER_HEIGHT, 0, { 0, 0, 0 } },
};

static const char* const sRequiredHooks[] = { "OnPlayerUpdate", "OnPlayDrawEnd", "OnSceneInit" };
static const S2HModRequirements sRequirements = { sizeof(S2HModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const S2HModApi* sApi;
static ColliderCylinder sCollider;
static bool sIsColliderReady;
static bool sIsAimPending;
static bool sWasButtonHeld;
static bool sIsDamagePaused;
static bool sWasLinkAirborne;
static u8 sPhase;
static s16 sTimer;
static s16 sAttackCooldown;
static s16 sLastInvincibility;
static Vec3f sOrbPos;
static Vec3s sOrbRot;
static Vec3f sLaunchPos;
static Vec3f sLastLinkPos;
static Actor* sPossessed;
static LightNode* sLightNode;
static LightInfo sLightInfo;
static s16 sSummonIds[ARRAY_COUNT(sSummonKeys)] = { -1, -1, -1, -1 };

void Player_StopHorizontalMovement(Player* player);
bool Player_IsZTargeting(Player* player);

static u32 FindEquippedButtonMask(void) {
    for (u8 button = 0; button < S2H_ITEM_BUTTON_COUNT; button++) {
        const char* equipped = sApi->GetEquippedCustomItem(button);
        if (equipped != NULL && strcmp(equipped, DOMROD_KEY) == 0) {
            return sItemButtons[button];
        }
    }
    return 0;
}

static bool IsAway(void) {
    return sPhase >= DOMROD_FLYING;
}

static bool IsLinkAirborne(Player* player) {
    return !(player->actor.bgCheckFlags & BGCHECKFLAG_GROUND);
}

static void CreateLight(PlayState* play) {
    Lights_PointNoGlowSetInfo(&sLightInfo, sOrbPos.x, sOrbPos.y, sOrbPos.z, 255, 215, 50, DOMROD_LIGHT_RADIUS);
    if (sLightNode == NULL) {
        sLightNode = LightContext_InsertLight(play, &play->lightCtx, &sLightInfo);
    }
}

static void MoveLight(void) {
    if (sLightNode != NULL) {
        Lights_PointNoGlowSetInfo(&sLightInfo, sOrbPos.x, sOrbPos.y, sOrbPos.z, 255, 215, 50, DOMROD_LIGHT_RADIUS);
    }
}

static void RemoveLight(PlayState* play) {
    if (sLightNode != NULL) {
        LightContext_RemoveLight(play, &play->lightCtx, sLightNode);
        sLightNode = NULL;
    }
}

// The orb is its light and a cloud of golden sparks.
static void SpawnOrbSparkle(PlayState* play, s32 count) {
    Vec3f velocity = { 0.0f, 0.0f, 0.0f };
    Vec3f accel = { 0.0f, -0.08f, 0.0f };
    Color_RGBA8 prim = { 255, 235, 120, 255 };
    Color_RGBA8 env = { 255, 190, 40, 255 };

    for (s32 i = 0; i < count; i++) {
        Vec3f pos = sOrbPos;

        pos.x += Rand_CenteredFloat(15.0f);
        pos.y += Rand_CenteredFloat(15.0f);
        pos.z += Rand_CenteredFloat(15.0f);
        EffectSsKirakira_SpawnFocused(play, &pos, &velocity, &accel, &prim, &env, (s16)(Rand_ZeroOne() * 300.0f) + 500,
                                      12);
    }
}

static void SparkleEvery(PlayState* play, u32 period, s32 count) {
    if ((play->gameplayFrames % period) == 0) {
        SpawnOrbSparkle(play, count);
    }
}

static bool IsPossessable(Actor* actor) {
    return actor != NULL && actor->update != NULL && (actor->id == ACTOR_EN_VM || actor->id == ACTOR_EN_AM);
}

static bool IsSomariaSummon(Actor* actor) {
    for (u32 i = 0; i < ARRAY_COUNT(sSummonKeys); i++) {
        if (sSummonIds[i] < 0) {
            sSummonIds[i] = sApi->GetActorId(sSummonKeys[i]);
        }
        if (sSummonIds[i] >= 0 && actor->id == sSummonIds[i]) {
            return true;
        }
    }
    return false;
}

static Actor* FindNearest(PlayState* play, bool (*matches)(Actor*), f32 reach) {
    Actor* nearest = NULL;
    f32 nearestDistance = reach;

    for (s32 category = 0; category < ACTORCAT_MAX; category++) {
        for (Actor* actor = play->actorCtx.actorLists[category].first; actor != NULL; actor = actor->next) {
            if (actor->update == NULL || !matches(actor)) {
                continue;
            }
            f32 distance = Math_Vec3f_DistXYZ(&sOrbPos, &actor->world.pos);
            if (distance < nearestDistance) {
                nearestDistance = distance;
                nearest = actor;
            }
        }
    }
    return nearest;
}

static Actor* FindPossessionTarget(PlayState* play) {
    Actor* hit = (sCollider.base.atFlags & AT_HIT) ? sCollider.base.at : NULL;

    sCollider.base.atFlags &= ~AT_HIT;
    if (IsPossessable(hit)) {
        return hit;
    }
    return FindNearest(play, IsPossessable, DOMROD_POSSESS_REACH);
}

static void Stow(Player* player, PlayState* play) {
    if (sPhase == DOMROD_AIMING) {
        Z64Aiming_Release(player, play);
    }
    RemoveLight(play);
    sCollider.base.atFlags &= ~(AT_ON | AT_HIT);
    sPossessed = NULL;
    sIsAimPending = false;
    sIsDamagePaused = false;
    sAttackCooldown = 0;
    AudioSfx_StopById(NA_SE_IT_SWORD_SWING);
}

static void StartReturn(void) {
    sPossessed = NULL;
    sPhase = DOMROD_RETURNING;
    Audio_PlaySfx_AtPos(&sOrbPos, NA_SE_IT_SHIELD_BOUND);
}

static void ReleasePossessed(void) {
    Audio_PlaySfx_AtPos(&sOrbPos, NA_SE_EV_BOMB_DROP_WATER);
    StartReturn();
}

static void StartAiming(Player* player, PlayState* play) {
    if (!Z64Aiming_Request(player, play)) {
        return;
    }
    sWasButtonHeld = false;
    sPhase = DOMROD_AIMING;
    Audio_PlaySfx_AtPos(&player->actor.world.pos, NA_SE_PL_CHANGE_ARMS);
}

static void StopAiming(Player* player, PlayState* play) {
    Z64Aiming_Release(player, play);
    sPhase = DOMROD_IN_HAND;
    Audio_PlaySfx_AtPos(&player->actor.world.pos, NA_SE_PL_CHANGE_ARMS);
}

static void Launch(Player* player, PlayState* play) {
    s16 yaw;
    s16 pitch;

    Z64Aiming_GetDirection(player, &yaw, &pitch);
    Z64Aiming_Release(player, play);
    sOrbPos.x = player->actor.world.pos.x + Math_SinS(yaw) * DOMROD_LAUNCH_REACH;
    sOrbPos.y = player->actor.world.pos.y + DOMROD_LAUNCH_HEIGHT;
    sOrbPos.z = player->actor.world.pos.z + Math_CosS(yaw) * DOMROD_LAUNCH_REACH;
    sOrbRot.x = pitch;
    sOrbRot.y = yaw;
    sOrbRot.z = 0;
    sLaunchPos = player->actor.world.pos;
    sTimer = DOMROD_FLIGHT_FRAMES;
    sPhase = DOMROD_FLYING;
    Collider_SetCylinder(play, &sCollider, &player->actor, &sColliderInit);
    CreateLight(play);
    Audio_PlaySfx_AtPos(&player->actor.world.pos, NA_SE_IT_ARROW_SHOT);
}

static void StartPossession(Player* player, Actor* target) {
    sPossessed = target;
    sAttackCooldown = 0;
    sIsDamagePaused = false;
    sLastLinkPos = player->actor.world.pos;
    sWasLinkAirborne = false;
    sOrbPos = target->focus.pos;
    sPhase = DOMROD_CONTROLLING;
    Audio_PlaySfx_AtPos(&sOrbPos, NA_SE_IT_SHIELD_REFLECT_SW);
}

static void SwapWithSummon(Player* player, PlayState* play, Actor* summon) {
    Vec3f linkPos = player->actor.world.pos;
    Vec3f zero = { 0.0f, 0.0f, 0.0f };
    CollisionPoly* floorPoly = NULL;
    s32 bgId;

    player->actor.world.pos = summon->world.pos;
    player->actor.prevPos = summon->world.pos;
    summon->world.pos = linkPos;
    f32 floor = BgCheck_EntityRaycastFloor5(&play->colCtx, &floorPoly, &bgId, &player->actor, &player->actor.world.pos);
    if (floor > BGCHECK_Y_MIN && player->actor.world.pos.y - floor < DOMROD_SNAP_TO_FLOOR) {
        player->actor.world.pos.y = floor;
        player->actor.bgCheckFlags |= BGCHECKFLAG_GROUND;
    }
    Vec3f flash = player->actor.world.pos;
    flash.y += 30.0f;
    EffectSsBlast_SpawnWhiteShockwave(play, &flash, &zero, &zero);
    flash = summon->world.pos;
    flash.y += 30.0f;
    EffectSsBlast_SpawnWhiteShockwave(play, &flash, &zero, &zero);
    Audio_PlaySfx_AtPos(&player->actor.world.pos, NA_SE_PL_MAGIC_WIND_WARP);
    StartReturn();
}

static void MoveOrb(f32 speed) {
    f32 pitchScale = Math_CosS(sOrbRot.x);

    sOrbPos.x += Math_SinS(sOrbRot.y) * pitchScale * speed;
    sOrbPos.y -= Math_SinS(sOrbRot.x) * speed;
    sOrbPos.z += Math_CosS(sOrbRot.y) * pitchScale * speed;
}

static bool HasHitGeometry(PlayState* play, Vec3f* previousPos) {
    CollisionPoly* poly = NULL;
    s32 bgId = BGCHECK_SCENE;
    Vec3f hitPos;

    if (!BgCheck_EntityLineTest1(&play->colCtx, previousPos, &sOrbPos, &hitPos, &poly, true, true, true, true,
                                 &bgId)) {
        return false;
    }
    sOrbPos = hitPos;
    return true;
}

static void HomeOnFocus(Player* player) {
    Actor* focus = player->focusActor;

    Math_ScaledStepToS(&sOrbRot.y, Math_Vec3f_Yaw(&sOrbPos, &focus->focus.pos), DOMROD_HOMING_TURN);
    Math_ScaledStepToS(&sOrbRot.x, Math_Vec3f_Pitch(&sOrbPos, &focus->focus.pos), DOMROD_HOMING_TURN);
}

static void UpdateFlying(Player* player, PlayState* play) {
    Actor* summon = FindNearest(play, IsSomariaSummon, DOMROD_SWAP_REACH);
    if (summon != NULL) {
        SwapWithSummon(player, play, summon);
        return;
    }
    Actor* target = FindPossessionTarget(play);
    if (target != NULL) {
        StartPossession(player, target);
        return;
    }
    bool isHoming = Player_IsZTargeting(player) && player->focusActor != NULL && player->focusActor->update != NULL;
    Vec3f previousPos = sOrbPos;

    if (isHoming) {
        HomeOnFocus(player);
    }
    MoveOrb(isHoming ? DOMROD_ORB_SPEED_HOMING : DOMROD_ORB_SPEED);
    sCollider.dim.pos.x = sOrbPos.x;
    sCollider.dim.pos.y = sOrbPos.y;
    sCollider.dim.pos.z = sOrbPos.z;
    sCollider.base.atFlags |= AT_ON | AT_TYPE_PLAYER;
    CollisionCheck_SetAT(play, &play->colChkCtx, &sCollider.base);
    CollisionCheck_SetOC(play, &play->colChkCtx, &sCollider.base);
    MoveLight();
    SparkleEvery(play, 1, 2);
    if (HasHitGeometry(play, &previousPos) || Math_Vec3f_DistXYZ(&sOrbPos, &sLaunchPos) > DOMROD_MAX_RANGE ||
        DECR(sTimer) == 0) {
        StartReturn();
        return;
    }
    Actor_PlaySfx_Flagged2(&player->actor, NA_SE_IT_SWORD_SWING);
}

static void UpdateReturning(Player* player, PlayState* play) {
    Vec3f hand = { player->actor.world.pos.x, player->actor.world.pos.y + DOMROD_LAUNCH_HEIGHT,
                   player->actor.world.pos.z };
    f32 distance = Math_Vec3f_DistXYZ(&sOrbPos, &hand);

    if (distance <= DOMROD_CATCH_DISTANCE) {
        sTimer = DOMROD_CATCH_FRAMES;
        sPhase = DOMROD_CATCHING;
        Audio_PlaySfx_AtPos(&player->actor.world.pos, NA_SE_PL_CATCH_BOOMERANG);
        return;
    }
    f32 step = DOMROD_ORB_RETURN_SPEED / distance;

    sOrbPos.x += (hand.x - sOrbPos.x) * step;
    sOrbPos.y += (hand.y - sOrbPos.y) * step;
    sOrbPos.z += (hand.z - sOrbPos.z) * step;
    sOrbRot.y = Math_Vec3f_Yaw(&sOrbPos, &hand);
    sOrbRot.x = Math_Vec3f_Pitch(&sOrbPos, &hand);
    MoveLight();
    SparkleEvery(play, 2, 2);
}

static void UpdateCatching(PlayState* play) {
    if (DECR(sTimer) == 0) {
        RemoveLight(play);
        sPhase = DOMROD_IN_HAND;
    }
}

// The possessed actor keeps its own update; its steering is Link's step this frame, laid on top of it.
static void MimicLink(Player* player, Actor* actor, bool followsHeight, bool hops) {
    actor->world.pos.x += player->actor.world.pos.x - sLastLinkPos.x;
    actor->world.pos.z += player->actor.world.pos.z - sLastLinkPos.z;
    if (followsHeight) {
        actor->world.pos.y += player->actor.world.pos.y - sLastLinkPos.y;
    }
    if (hops) {
        bool isAirborne = IsLinkAirborne(player);

        if (isAirborne && !sWasLinkAirborne && (actor->bgCheckFlags & BGCHECKFLAG_GROUND)) {
            actor->velocity.y = DOMROD_HOP_VELOCITY;
            Audio_PlaySfx_AtPos(&actor->world.pos, NA_SE_EN_STALKID_WALK);
        }
        sWasLinkAirborne = isAirborne;
    }
    actor->shape.rot.y = player->actor.shape.rot.y;
    actor->world.rot.y = player->actor.world.rot.y;
    sLastLinkPos = player->actor.world.pos;
}

static void HaltOwnMovement(Actor* actor) {
    actor->speed = 0.0f;
    actor->velocity.x = 0.0f;
    actor->velocity.z = 0.0f;
}

static void CountDownAttack(void) {
    if (sAttackCooldown > 0) {
        sAttackCooldown--;
    }
}

static void ControlBeamos(Player* player, Actor* actor, bool isAttacking) {
    HaltOwnMovement(actor);
    MimicLink(player, actor, false, true);
    if (isAttacking && sAttackCooldown == 0) {
        Audio_PlaySfx_AtPos(&actor->world.pos, NA_SE_EN_BIMOS_LAZER);
        sAttackCooldown = DOMROD_BEAMOS_COOLDOWN;
    }
    CountDownAttack();
}

static bool ControlArmos(Player* player, PlayState* play, Actor* actor, bool isAttacking) {
    HaltOwnMovement(actor);
    MimicLink(player, actor, false, true);
    if (!isAttacking) {
        return true;
    }
    EnBom* bomb = (EnBom*)Actor_Spawn(&play->actorCtx, play, ACTOR_EN_BOM, actor->world.pos.x, actor->world.pos.y,
                                      actor->world.pos.z, 0, 0, 0x6FF, BOMB_TYPE_BODY);
    if (bomb != NULL) {
        bomb->timer = 0;
    }
    Audio_PlaySfx_AtPos(&actor->world.pos, NA_SE_IT_BOMB_EXPLOSION);
    Actor_Kill(actor);
    return false;
}

static bool DriveActor(Player* player, PlayState* play, Actor* actor, bool isAttacking) {
    switch (actor->id) {
        case ACTOR_EN_VM:
            ControlBeamos(player, actor, isAttacking);
            return true;
        case ACTOR_EN_AM:
            return ControlArmos(player, play, actor, isAttacking);
        default:
            return false;
    }
}

// A hit only pauses the possession: the actor holds still until Link can act again.
static void UpdateControlling(Player* player, PlayState* play) {
    u32 rodButton = FindEquippedButtonMask();
    u32 pressed = play->state.input[0].press.button;

    if (sPossessed == NULL || sPossessed->update == NULL ||
        Math_Vec3f_DistXYZ(&player->actor.world.pos, &sPossessed->world.pos) > DOMROD_MAX_RANGE ||
        (pressed & (sReleaseButtons & ~rodButton))) {
        ReleasePossessed();
        return;
    }
    sIsDamagePaused = player->invincibilityTimer > 0;
    if (!sIsDamagePaused) {
        if (!DriveActor(player, play, sPossessed, (pressed & rodButton) != 0)) {
            ReleasePossessed();
            return;
        }
        sOrbPos = sPossessed->focus.pos;
    } else {
        sLastLinkPos = player->actor.world.pos;
    }
    MoveLight();
    SparkleEvery(play, 3, 1);
}

// Holding the button aims, letting go fires. The release only counts once the button was seen held, or the
// press that took the rod out would fire it during the item-change animation.
static void UpdateAiming(Player* player, PlayState* play) {
    u32 rodButton = FindEquippedButtonMask();

    Z64Aiming_Update(player, play);
    Player_StopHorizontalMovement(player);
    if (play->state.input[0].press.button & (BTN_B | BTN_A)) {
        StopAiming(player, play);
        return;
    }
    if (play->state.input[0].cur.button & rodButton) {
        sWasButtonHeld = true;
        return;
    }
    if (sWasButtonHeld || (play->state.input[0].press.button & rodButton)) {
        Launch(player, play);
    }
}

// Busy only while aiming: with the orb away Link keeps his hands, since a possessed foe copies his steps.
static int32_t UpdateRodInHand(Player* player, PlayState* play) {
    if (sPhase != DOMROD_AIMING) {
        return 0;
    }
    UpdateAiming(player, play);
    return 1;
}

static void TakeOutRod(PlayState* play, Player* player) {
    if (!sIsColliderReady) {
        Collider_InitCylinder(play, &sCollider);
        sIsColliderReady = true;
    }
    sPhase = DOMROD_IN_HAND;
    sIsAimPending = true;
    sLastInvincibility = player->invincibilityTimer;
}

// Pressing the button acts from the end of the player's update: the aim claims the camera, and what is set
// from the item-button pass is undone the same frame. A rod still in hand after a scene change reads as stowed.
static void RequestAim(Player* player, PlayState* play) {
    if (sPhase == DOMROD_STOWED || sPhase == DOMROD_IN_HAND) {
        sPhase = DOMROD_IN_HAND;
        sIsAimPending = true;
    }
}

static void PutRodAway(Player* player, PlayState* play) {
    Stow(player, play);
    sPhase = DOMROD_STOWED;
}

static bool WasJustHurt(Player* player) {
    bool isHurt = player->invincibilityTimer > 0 && sLastInvincibility <= 0;

    sLastInvincibility = player->invincibilityTimer;
    return isHurt;
}

static void RunPendingAim(Player* player, PlayState* play) {
    if (!sIsAimPending || (player->stateFlags3 & PLAYER_STATE3_START_CHANGING_HELD_ITEM)) {
        return;
    }
    sIsAimPending = false;
    if (player->heldItemAction == PLAYER_IA_CUSTOM && sPhase == DOMROD_IN_HAND) {
        StartAiming(player, play);
    }
}

// The orb and the possession run here, after the action func, so they have the last word of the frame; the
// item's upper action does not run while Link fights on, so the host's watchdog is told the rod is still out.
static void UpdateRodWorld(Player* player) {
    PlayState* play = gPlayState;

    if (play == NULL || sPhase == DOMROD_STOWED) {
        return;
    }
    bool isHurt = WasJustHurt(player);

    if (sPhase == DOMROD_AIMING && isHurt) {
        StopAiming(player, play);
        return;
    }
    RunPendingAim(player, play);
    if (sPhase == DOMROD_AIMING) {
        Z64Aiming_Update(player, play);
        Z64Items_KeepHeld(sApi);
    }
    if (!IsAway()) {
        return;
    }
    Z64Items_KeepHeld(sApi);
    switch (sPhase) {
        case DOMROD_FLYING:
            if (isHurt) {
                StartReturn();
            } else {
                UpdateFlying(player, play);
            }
            break;
        case DOMROD_RETURNING:
            UpdateReturning(player, play);
            break;
        case DOMROD_CONTROLLING:
            UpdateControlling(player, play);
            break;
        case DOMROD_CATCHING:
            UpdateCatching(play);
            break;
        default:
            sPhase = DOMROD_IN_HAND;
            break;
    }
}

// The light list, the camera and the possessed actor all belong to the scene that is ending.
static void ForgetScene(s16 sceneId, s8 spawnNum) {
    Z64Aiming_Drop();
    sLightNode = NULL;
    sPossessed = NULL;
    sIsAimPending = false;
    sPhase = DOMROD_STOWED;
}

// Drawn from the play's end, not Link's: in first person he is not drawn, and the reticle must be.
static void DrawRodEffects(void) {
    PlayState* play = gPlayState;

    if (play != NULL && sPhase == DOMROD_AIMING) {
        Z64Aiming_DrawReticle(play, GET_PLAYER(play), DOMROD_MAX_RANGE,
                              (Z64AimingReticleStyle){ Z64AIMING_TOGGLE_BOW, Z64AIMING_TINT_CONTROL });
    }
}

S2H_MOD_EXPORT void ModSetApi(const S2HModApi* api) {
    sApi = api;
}

S2H_MOD_EXPORT const S2HModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

// Shuffled once by the randomizer; no vanilla logic needs it.
static const S2HCustomItemRandomizer sRandomizerItem = {
    sizeof(S2HCustomItemRandomizer), 0, S2H_CUSTOM_ITEM_TYPE_ITEM, 0, 1, DOMROD_KEY, DOMROD_KEY, "",
};

S2H_MOD_EXPORT void ModInit(void) {
    if (!S2H_MOD_API_HAS(sApi, GetEquippedCustomItem) || !S2H_MOD_API_HAS(sApi, GetActorId)) {
        return;
    }

    S2HCustomItemDefinition rod = Z64Items_Define(DOMROD_KEY, sIconTex, sNameTex);

    Z64Items_SetButtons(&rod, S2H_CUSTOM_ITEM_C_BUTTON | S2H_CUSTOM_ITEM_DPAD);
    // Shares the Shovel's cell, after it.
    Z64Items_SetPlacement(&rod, 0, 22, -1);
    Z64Items_SetTextbox(&rod, "You got the %pDominion Rod%w!&An ancient artifact that can&possess and control "
                              "enemies.^Hold a %yC button%w to aim and let go to fire&a golden orb. It possesses "
                              "%rBeamos%w and %yArmos%w.^A possessed enemy %gcopies your steps%w. The same button "
                              "makes it attack,&any other lets it go.");
    Z64Items_SetPauseText(&rod, "%pDominion Rod&%wHold a %yC button%w to aim, release to fire.&A possessed foe copies "
                                "your steps.");
    Z64Items_SetAction(&rod, TakeOutRod, UpdateRodInHand);
    Z64Items_SetHeldCallbacks(&rod, RequestAim, PutRodAway, NULL);
    Z64Items_SetGetItemModel(&rod, sRodDL);
    rod.modelGroup = PLAYER_MODELGROUP_DEFAULT;

    Z64Items_SetLogic(&rod, &sRandomizerItem);
    if (!Z64Items_Register(sApi, &rod)) {
        return;
    }
    S2H_REGISTER_HOOK(sApi, OnPlayerUpdate, UpdateRodWorld);
    S2H_REGISTER_HOOK(sApi, OnPlayDrawEnd, DrawRodEffects);
    S2H_REGISTER_HOOK(sApi, OnSceneInit, ForgetScene);
}
