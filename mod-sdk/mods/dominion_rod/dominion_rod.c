#include <math.h>
#include <string.h>

#include "z64items.h"
#include "z64aiming.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "objects/object_link_child/object_link_child.h"
#include "overlays/actors/ovl_En_Am/z_en_am.h"
#include "overlays/actors/ovl_En_Egol/z_en_egol.h"
#include "overlays/actors/ovl_En_Vm/z_en_vm.h"

#define DOMROD_KEY "nei.dominion_rod"
#define DOMROD_VISUAL_CHANNEL "nei.dominion_rod.visual"
#define DOMROD_HELD_ROT_Z (-90.0f)
#define DOMROD_HELD_OFFSET_Y 10.0f
#define DOMROD_HELD_SCALE 0.1f
#define DOMROD_GIVE_SCALE 0.2f
#define DOMROD_ORB_SPEED 18.0f
#define DOMROD_ORB_SPEED_HOMING 22.0f
#define DOMROD_ORB_RETURN_SPEED 20.0f
#define DOMROD_ORB_SCALE 5.5f
#define DOMROD_MAX_RANGE 2500.0f
#define DOMROD_SELECT_RANGE 800.0f
#define DOMROD_SELECT_CONE 0x1800
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
#define DOMROD_SNAP_TO_FLOOR 100.0f
#define DOMROD_COMMAND_REACH 800.0f
#define DOMROD_HIDDEN_PLAYER_DISTANCE 10000.0f
#define DOMROD_STEP_EPSILON 0.5f
#define DOMROD_BEAM_FRAMES 30
#define DOMROD_BEAM_COOLDOWN 40
#define DOMROD_BEAM_WIDTH 0.01f
#define DOMROD_BEAM_GROWTH 80.0f
#define DOMROD_BEAM_OVERSHOOT 40.0f
#define DOMROD_BEAMOS_GRAVITY -1.0f
#define DOMROD_POUND_FRAMES 3
#define DOMROD_POUND_COOLDOWN 25
#define DOMROD_EYEGORE_LASER_PITCH_MIN 0x7D0
#define DOMROD_EYEGORE_LASER_PITCH_MAX 0x1F40
#define DOMROD_BORROWED_ATTACKS 3
#define DOMROD_EYEGORE_LASER_OFF 0
#define REMOTE_VISUAL_INTERVAL 5u

typedef enum {
    DOMROD_STOWED,
    DOMROD_IN_HAND,
    DOMROD_AIMING,
    DOMROD_FLYING,
    DOMROD_RETURNING,
    DOMROD_CONTROLLING,
    DOMROD_CATCHING,
} DominionPhase;

// A possessed foe hits for Link: its attack colliders are lent to the player's side and given back on release.
typedef struct {
    u8* atFlags;
    u8 savedAtFlags;
    ColliderElementDamageInfoAT* info;
    ColliderElementDamageInfoAT savedInfo;
} BorrowedAttack;

typedef struct {
    int16_t scene;
    uint8_t active;
    uint8_t adult;
    uint8_t flags[8];
    float values[12];
} RemoteVisualState;

static const ALIGN_ASSET(2) char sIconTex[] = "__OTR__textures/icon_item_custom/gItemIconDominionRodTex";
static const ALIGN_ASSET(2) char sNameTex[] = "__OTR__textures/item_name_custom/gDominionRodNameTex";
static const ALIGN_ASSET(2) char sRodDL[] = "__OTR__objects/object_nei_dominion_rod/gNeiDominionRodDL";
static const ALIGN_ASSET(2) char sRodXluDL[] = "__OTR__objects/object_nei_dominion_rod/gNeiDominionRodXluDL";
static const ALIGN_ASSET(2) char sOrbDL[] = "__OTR__objects/oot_object_fhg/gPhantomEnergyBallDL";

static const u32 sItemButtons[S2H_ITEM_BUTTON_COUNT] = { BTN_B,      BTN_CLEFT, BTN_CDOWN, BTN_CRIGHT, 0,
                                                         BTN_DRIGHT, BTN_DLEFT, BTN_DDOWN, BTN_DUP };
static const u32 sReleaseButtons = BTN_A | BTN_B | BTN_R | BTN_START | BTN_CLEFT | BTN_CDOWN | BTN_CRIGHT | BTN_DUP |
                                   BTN_DDOWN | BTN_DLEFT | BTN_DRIGHT;
static const char* const sSummonKeys[] = { "nei.somaria.statue", "nei.somaria.crate", "nei.somaria.bird",
                                           "nei.somaria.platform" };

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

static ColliderCylinderInit sPoundInit = {
    {
        COL_MATERIAL_NONE,
        AT_ON | AT_TYPE_PLAYER,
        AC_NONE,
        OC1_NONE,
        OC2_NONE,
        COLSHAPE_CYLINDER,
    },
    {
        ELEM_MATERIAL_UNK0,
        { DMG_GORON_POUND, 0x00, 0x08 },
        { 0, 0x00, 0x00 },
        ATELEM_ON | ATELEM_SFX_NORMAL,
        ACELEM_NONE,
        OCELEM_NONE,
    },
    { 100, 45, 0, { 0, 0, 0 } },
};

static const char* const sRequiredHooks[] = {
    "OnGameFrameUpdate",    "OnPlayerUpdate", "OnPlayerActionHandler", "OnPlayerResolveLimbDraw",
    "OnPlayerPostLimbDraw", "OnActorDrawEnd", "OnPlayDrawEnd",         "OnSceneInit",
    "OnActorResolvePlayerRelation",
};
static const S2HModRequirements sRequirements = { sizeof(S2HModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const S2HModApi* sApi;
static ColliderCylinder sCollider;
static bool sIsColliderReady;
static ColliderCylinder sPoundCollider;
static bool sIsPoundReady;
static s16 sPoundFrames;
static Vec3f sPoundPos;
static bool sIsFirePending;
static bool sWasButtonHeld;
static bool sIsDamagePaused;
static bool sWasLinkAirborne;
static bool sHasPossessedMoved;
static bool sIsSlashActive;
static bool sIsTargetACommand;
static u8 sPhase;
static s16 sTimer;
static s16 sAttackCooldown;
static s16 sCommandFrames;
static s16 sLastInvincibility;
static Vec3f sOrbPos;
static Vec3s sOrbRot;
static Vec3f sLaunchPos;
static Vec3f sLastLinkPos;
static Actor* sSelection;
static Actor* sOrbTarget;
static Actor* sPossessed;
static BorrowedAttack sBorrowedAttacks[DOMROD_BORROWED_ATTACKS];
static u8 sBorrowedAttackCount;
static LightNode* sLightNode;
static LightInfo sLightInfo;
static s16 sSummonIds[ARRAY_COUNT(sSummonKeys)] = { -1, -1, -1, -1 };
static MtxF sWeaponHand;
static bool sIsWeaponHandReady;
static bool sRemoteVisualReady;
static bool sRemoteVisualSent;
static uint32_t sRemoteVisualFrame;
static RemoteVisualState sLastRemoteVisual;

bool Player_IsZTargeting(Player* player);
void Player_StopHorizontalMovement(Player* player);
s32 Player_UpperAction_1(Player* player, PlayState* play);

void func_808CC420(EnVm* beamos);
void func_808CCBE4(EnVm* beamos, PlayState* play);
void func_808CCAA4(EnVm* beamos, PlayState* play);
void func_808CCB50(EnVm* beamos, PlayState* play);
void func_808CCCF0(EnVm* beamos, PlayState* play);

void func_808B0358(EnAm* armos);
void func_808B07A8(EnAm* armos, PlayState* play);
void func_808B0820(EnAm* armos);
void func_808B0894(EnAm* armos, PlayState* play);

void EnEgol_SetupStand(EnEgol* eyegore);
void EnEgol_SetupWalk(EnEgol* eyegore);
void EnEgol_SetupLaser(EnEgol* eyegore);
void EnEgol_SetupSlam(EnEgol* eyegore);
void EnEgol_Walk(EnEgol* eyegore, PlayState* play);
void EnEgol_Wait(EnEgol* eyegore, PlayState* play);
void EnEgol_Laser(EnEgol* eyegore, PlayState* play);
void EnEgol_StartDeathCutscene(EnEgol* eyegore, PlayState* play);
void EnEgol_Death(EnEgol* eyegore, PlayState* play);
void EnEgol_FootstepEffects(EnEgol* eyegore, PlayState* play, f32 leftFootFrame, f32 rightFootFrame);

static bool ReadRemoteVisual(Player* player, RemoteVisualState* visual);

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

static bool IsDyingBeamos(Actor* actor) {
    return ((EnVm*)actor)->actionFunc == func_808CCCF0;
}

static bool IsDyingArmos(Actor* actor) {
    return ((EnAm*)actor)->actionFunc == func_808B0894;
}

static bool IsDyingEyegore(Actor* actor) {
    EnEgolActionFunc action = ((EnEgol*)actor)->actionFunc;

    return action == EnEgol_StartDeathCutscene || action == EnEgol_Death;
}

static bool IsPossessable(Actor* actor) {
    if (actor == NULL || actor->update == NULL) {
        return false;
    }
    switch (actor->id) {
        case ACTOR_EN_VM:
            return !IsDyingBeamos(actor);
        case ACTOR_EN_AM:
            return !IsDyingArmos(actor);
        case ACTOR_EN_EGOL:
            return !IsDyingEyegore(actor);
        default:
            return false;
    }
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

// Tall foes such as the Eyegore stand far from their own feet: the orb catches them by the body or the eye.
static f32 GetOrbDistance(Actor* actor) {
    return MIN(Math_Vec3f_DistXYZ(&sOrbPos, &actor->world.pos), Math_Vec3f_DistXYZ(&sOrbPos, &actor->focus.pos));
}

static Actor* FindNearest(PlayState* play, bool (*matches)(Actor*), f32 reach) {
    Actor* nearest = NULL;
    f32 nearestDistance = reach;

    for (s32 category = 0; category < ACTORCAT_MAX; category++) {
        for (Actor* actor = play->actorCtx.actorLists[category].first; actor != NULL; actor = actor->next) {
            if (actor->update == NULL || !matches(actor)) {
                continue;
            }
            f32 distance = GetOrbDistance(actor);

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

static void BorrowAttack(u8* atFlags, u8 lentAtFlags, ColliderElementDamageInfoAT* info, u32 dmgFlags, u8 damage) {
    if (sBorrowedAttackCount >= DOMROD_BORROWED_ATTACKS) {
        return;
    }
    BorrowedAttack* borrowed = &sBorrowedAttacks[sBorrowedAttackCount++];

    borrowed->atFlags = atFlags;
    borrowed->info = info;
    if (atFlags != NULL) {
        borrowed->savedAtFlags = *atFlags;
        *atFlags = lentAtFlags;
    }
    if (info != NULL) {
        borrowed->savedInfo = *info;
        info->dmgFlags = dmgFlags;
        info->damage = damage;
    }
}

static void ReturnBorrowedAttacks(void) {
    for (u8 i = 0; i < sBorrowedAttackCount; i++) {
        BorrowedAttack* borrowed = &sBorrowedAttacks[i];

        if (borrowed->atFlags != NULL) {
            *borrowed->atFlags = borrowed->savedAtFlags & ~(AT_HIT | AT_BOUNCED);
        }
        if (borrowed->info != NULL) {
            *borrowed->info = borrowed->savedInfo;
        }
    }
    sBorrowedAttackCount = 0;
}

static void HaltOwnMovement(Actor* actor) {
    actor->speed = 0.0f;
    actor->velocity.x = 0.0f;
    actor->velocity.z = 0.0f;
}

// Link's steps are copied as a displacement, so the foe keeps its distance and only mirrors the motion.
static void MimicLink(Player* player, Actor* actor, bool canHop) {
    f32 dx = player->actor.world.pos.x - sLastLinkPos.x;
    f32 dz = player->actor.world.pos.z - sLastLinkPos.z;

    actor->world.pos.x += dx;
    actor->world.pos.z += dz;
    sHasPossessedMoved = (SQ(dx) + SQ(dz)) > SQ(DOMROD_STEP_EPSILON);
    if (canHop) {
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

static bool IsCommandTargeting(Player* player) {
    return (player->stateFlags1 & (PLAYER_STATE1_Z_TARGETING | PLAYER_STATE1_PARALLEL)) ||
           Player_IsZTargeting(player) || (gPlayState != NULL && (gPlayState->state.input[0].cur.button & BTN_Z));
}

// The lock-on target, or with Z held and nothing locked, a point straight ahead of Link.
static bool FindCommandPoint(Player* player, Actor* possessed, Vec3f* point) {
    Actor* target = player->focusActor;

    if (target != NULL && target != possessed && target->update != NULL) {
        *point = target->focus.pos;
        return true;
    }
    if (!IsCommandTargeting(player)) {
        return false;
    }
    *point = player->actor.focus.pos;
    point->x += Math_SinS(player->actor.shape.rot.y) * DOMROD_COMMAND_REACH;
    point->z += Math_CosS(player->actor.shape.rot.y) * DOMROD_COMMAND_REACH;
    return true;
}

static void SwitchOffBeam(EnVm* beamos) {
    beamos->unk_210 = 0;
    beamos->unk_220 = 0.0f;
    beamos->unk_224 = 0.0f;
}

// unk_216/unk_218 are the head's pitch and yaw offset; unk_220/unk_224 the beam's width and length. The beam's
// collision triangle is rebuilt from them in EnVm_PostLimbDraw, so aiming the head aims the hitbox.
static void AimBeam(EnVm* beamos, PlayState* play, Vec3f* point) {
    Actor* actor = &beamos->actor;
    f32 length = Math_Vec3f_DistXYZ(&actor->focus.pos, point) + DOMROD_BEAM_OVERSHOOT;

    beamos->unk_216 = Math_Vec3f_Pitch(&actor->focus.pos, point);
    beamos->unk_218 = Math_Vec3f_Yaw(&actor->focus.pos, point) - actor->shape.rot.y;
    beamos->unk_220 = DOMROD_BEAM_WIDTH;
    Math_StepToF(&beamos->unk_224, length, DOMROD_BEAM_GROWTH);
    if (beamos->unk_210 == 0) {
        beamos->unk_210 = 1;
    }
    CollisionCheck_SetAT(play, &play->colChkCtx, &beamos->colliderTris.base);
}

static void ResolveControlledBeam(EnVm* beamos, PlayState* play) {
    Player* player = GET_PLAYER(play);
    Vec3f point;

    beamos->actor.speed = 0.0f;
    Actor_MoveWithGravity(&beamos->actor);
    Actor_UpdateBgCheckInfo(play, &beamos->actor, 30.0f, 20.0f, 40.0f, UPDBGCHECKINFO_FLAG_1 | UPDBGCHECKINFO_FLAG_4);
    if (sCommandFrames <= 0 || sIsDamagePaused || !FindCommandPoint(player, &beamos->actor, &point)) {
        SwitchOffBeam(beamos);
        return;
    }
    AimBeam(beamos, play, &point);
}

static void HopInPlace(EnAm* armos, PlayState* play) {
    armos->actor.speed = 0.0f;
    armos->speed = 0.0f;
    if (sHasPossessedMoved || !(armos->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) {
        SkelAnime_Update(&armos->skelAnime);
    }
}

// The walk cycle only plays while Link moves, so the Eyegore never strides in place.
static void StrideWithLink(EnEgol* eyegore, PlayState* play) {
    eyegore->skelAnime.playSpeed = sHasPossessedMoved ? 1.0f : 0.0f;
    if (sHasPossessedMoved) {
        EnEgol_FootstepEffects(eyegore, play, 9.0f, 24.0f);
    }
}

static void ClearEyegoreLaser(EnEgol* eyegore) {
    eyegore->chargingLaser = false;
    eyegore->chargeLevel = 0;
    eyegore->actionTimer = 0;
    eyegore->laserState = DOMROD_EYEGORE_LASER_OFF;
    Math_Vec3f_Copy(&eyegore->laserScale, &gZeroVec3f);
    Math_Vec3f_Copy(&eyegore->laserScaleTarget, &gZeroVec3f);
    for (s32 i = 0; i < 4; i++) {
        Math_Vec3f_Copy(&eyegore->laserCollider.dim.quad[i], &eyegore->laserBase);
    }
}

static void PossessBeamos(EnVm* beamos) {
    BorrowAttack(&beamos->colliderTris.base.atFlags, AT_ON | AT_TYPE_PLAYER,
                 &beamos->colliderTris.elements[0].base.atDmgInfo, DMG_FIRE_ARROW, 4);
    SwitchOffBeam(beamos);
    beamos->actor.gravity = DOMROD_BEAMOS_GRAVITY;
    beamos->actionFunc = ResolveControlledBeam;
}

// A dormant statue is woken with the same masks and textures EnAm_ApplyEnemyTexture leaves it with.
static void PossessArmos(EnAm* armos) {
    armos->textureBlend = 255;
    armos->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;
    armos->actor.shape.yOffset = 0.0f;
    armos->enemyCollider.elem.acDmgInfo.dmgFlags = 0x81C2C788;
    armos->interactCollider.elem.acDmgInfo.dmgFlags = 0x760D3877;
    BorrowAttack(&armos->enemyCollider.base.atFlags, armos->enemyCollider.base.atFlags & ~AT_ON, NULL, 0, 0);
    Animation_PlayLoopSetSpeed(&armos->skelAnime, (AnimationHeader*)gArmosHopAnim, 4.0f);
    armos->actionFunc = HopInPlace;
}

static void TakeEyegoreReins(EnEgol* eyegore) {
    EnEgol_SetupWalk(eyegore);
    eyegore->actionFunc = StrideWithLink;
}

static void PossessEyegore(EnEgol* eyegore) {
    BorrowAttack(&eyegore->laserCollider.base.atFlags, AT_ON | AT_TYPE_PLAYER, &eyegore->laserCollider.elem.atDmgInfo,
                 DMG_EXPLOSIVES, 8);
    BorrowAttack(&eyegore->bodyCollider.base.atFlags, AT_ON | AT_TYPE_PLAYER,
                 &eyegore->bodyCollider.elements[0].base.atDmgInfo, DMG_GORON_POUND, 8);
    BorrowAttack(NULL, 0, &eyegore->bodyCollider.elements[1].base.atDmgInfo, DMG_GORON_POUND, 8);
    ClearEyegoreLaser(eyegore);
    if (eyegore->actionFunc == EnEgol_Wait) {
        EnEgol_SetupStand(eyegore);
        return;
    }
    TakeEyegoreReins(eyegore);
}

static void PossessActor(Actor* actor) {
    switch (actor->id) {
        case ACTOR_EN_VM:
            PossessBeamos((EnVm*)actor);
            break;
        case ACTOR_EN_AM:
            PossessArmos((EnAm*)actor);
            break;
        case ACTOR_EN_EGOL:
            PossessEyegore((EnEgol*)actor);
            break;
        default:
            break;
    }
}

static void FreeBeamos(EnVm* beamos) {
    SwitchOffBeam(beamos);
    beamos->actor.gravity = 0.0f;
    beamos->actor.velocity.y = 0.0f;
    func_808CC420(beamos);
}

static void FreeArmos(EnAm* armos) {
    armos->enemyCollider.base.atFlags |= AT_ON;
    armos->returnHomeTimer = 300;
    func_808B0358(armos);
}

static void FreeEyegore(EnEgol* eyegore) {
    ClearEyegoreLaser(eyegore);
    EnEgol_SetupWalk(eyegore);
}

// A foe that is already dying or reeling keeps its own action; only the controlled one is handed back.
static void FreePossessed(void) {
    Actor* actor = sPossessed;

    sPossessed = NULL;
    ReturnBorrowedAttacks();
    if (actor == NULL || actor->update == NULL) {
        return;
    }
    switch (actor->id) {
        case ACTOR_EN_VM:
            if (((EnVm*)actor)->actionFunc == ResolveControlledBeam) {
                FreeBeamos((EnVm*)actor);
            }
            break;
        case ACTOR_EN_AM:
            if (((EnAm*)actor)->actionFunc == HopInPlace) {
                FreeArmos((EnAm*)actor);
            }
            break;
        case ACTOR_EN_EGOL:
            if (((EnEgol*)actor)->actionFunc == StrideWithLink) {
                FreeEyegore((EnEgol*)actor);
            }
            break;
        default:
            break;
    }
}

static void Stow(Player* player, PlayState* play) {
    if (sPhase == DOMROD_AIMING) {
        Z64Aiming_Release(player, play);
    }
    RemoveLight(play);
    sCollider.base.atFlags &= ~(AT_ON | AT_HIT);
    FreePossessed();
    sSelection = NULL;
    sOrbTarget = NULL;
    sIsFirePending = false;
    sIsDamagePaused = false;
    sAttackCooldown = 0;
    sCommandFrames = 0;
    AudioSfx_StopById(NA_SE_IT_SWORD_SWING);
}

static void StartReturn(void) {
    FreePossessed();
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

static void LaunchAlong(Player* player, PlayState* play, s16 yaw, s16 pitch) {
    sOrbPos.x = player->actor.world.pos.x + Math_SinS(yaw) * DOMROD_LAUNCH_REACH;
    sOrbPos.y = player->actor.world.pos.y + DOMROD_LAUNCH_HEIGHT;
    sOrbPos.z = player->actor.world.pos.z + Math_CosS(yaw) * DOMROD_LAUNCH_REACH;
    sOrbRot.x = pitch;
    sOrbRot.y = yaw;
    sOrbRot.z = 0;
    sLaunchPos = player->actor.world.pos;
    sTimer = DOMROD_FLIGHT_FRAMES;
    sPhase = DOMROD_FLYING;
    PlayerAnimation_PlayOnce(play, &player->skelAnimeUpper,
                             (PlayerAnimationHeader*)gPlayerAnim_link_fighter_normal_kiru);
    sIsSlashActive = true;
    Collider_SetCylinder(play, &sCollider, &player->actor, &sColliderInit);
    CreateLight(play);
    Audio_PlaySfx_AtPos(&player->actor.world.pos, NA_SE_IT_ARROW_SHOT);
}

static void LaunchAimed(Player* player, PlayState* play) {
    s16 yaw;
    s16 pitch;

    Z64Aiming_GetDirection(player, &yaw, &pitch);
    Z64Aiming_Release(player, play);
    sOrbTarget = NULL;
    LaunchAlong(player, play, yaw, pitch);
}

static void LaunchAtWill(Player* player, PlayState* play) {
    sOrbTarget = (sSelection != NULL && sSelection->update != NULL) ? sSelection : NULL;
    if (sOrbTarget == NULL) {
        LaunchAlong(player, play, player->actor.shape.rot.y, 0);
        return;
    }
    Vec3f origin = { player->actor.world.pos.x, player->actor.world.pos.y + DOMROD_LAUNCH_HEIGHT,
                     player->actor.world.pos.z };

    LaunchAlong(player, play, Math_Vec3f_Yaw(&origin, &sOrbTarget->focus.pos),
                Math_Vec3f_Pitch(&origin, &sOrbTarget->focus.pos));
}

static void StartPossession(Player* player, Actor* target) {
    sPossessed = target;
    sAttackCooldown = 0;
    sCommandFrames = 0;
    sIsDamagePaused = false;
    sHasPossessedMoved = false;
    sLastLinkPos = player->actor.world.pos;
    sWasLinkAirborne = false;
    sOrbPos = target->focus.pos;
    sPhase = DOMROD_CONTROLLING;
    PossessActor(target);
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

    if (!BgCheck_EntityLineTest1(&play->colCtx, previousPos, &sOrbPos, &hitPos, &poly, true, true, true, true, &bgId)) {
        return false;
    }
    sOrbPos = hitPos;
    return true;
}

static Actor* FindHomingTarget(Player* player) {
    Actor* focus = player->focusActor;

    if (Player_IsZTargeting(player) && focus != NULL && focus->update != NULL) {
        return focus;
    }
    if (sOrbTarget != NULL && sOrbTarget->update == NULL) {
        sOrbTarget = NULL;
    }
    return sOrbTarget;
}

static void HomeOn(Actor* target) {
    Math_ScaledStepToS(&sOrbRot.y, Math_Vec3f_Yaw(&sOrbPos, &target->focus.pos), DOMROD_HOMING_TURN);
    Math_ScaledStepToS(&sOrbRot.x, Math_Vec3f_Pitch(&sOrbPos, &target->focus.pos), DOMROD_HOMING_TURN);
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
    Actor* homingTarget = FindHomingTarget(player);
    Vec3f previousPos = sOrbPos;

    if (homingTarget != NULL) {
        HomeOn(homingTarget);
    }
    MoveOrb(homingTarget != NULL ? DOMROD_ORB_SPEED_HOMING : DOMROD_ORB_SPEED);
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
    Actor_PlaySfx_Flagged2(&player->actor, NA_SE_IT_SWORD_SWING);
}

static void UpdateCatching(PlayState* play) {
    if (DECR(sTimer) == 0) {
        RemoveLight(play);
        sPhase = DOMROD_IN_HAND;
    }
}

static bool IsBeamosReeling(EnVm* beamos) {
    return beamos->actionFunc == func_808CCAA4 || beamos->actionFunc == func_808CCB50;
}

static bool DriveBeamos(Player* player, PlayState* play, EnVm* beamos, bool isAttacking) {
    if (IsDyingBeamos(&beamos->actor)) {
        return false;
    }
    if (IsBeamosReeling(beamos)) {
        sLastLinkPos = player->actor.world.pos;
        return true;
    }
    Vec3f point;
    bool hasTarget = FindCommandPoint(player, &beamos->actor, &point);

    beamos->actionFunc = ResolveControlledBeam;
    HaltOwnMovement(&beamos->actor);
    MimicLink(player, &beamos->actor, true);
    if (isAttacking && !hasTarget) {
        func_808CCBE4(beamos, play);
        return false;
    }
    if (isAttacking && sAttackCooldown == 0) {
        sCommandFrames = DOMROD_BEAM_FRAMES;
        sAttackCooldown = DOMROD_BEAM_COOLDOWN;
        Actor_PlaySfx(&beamos->actor, NA_SE_EN_BIMOS_AIM);
    }
    if (sCommandFrames > 0) {
        sCommandFrames--;
    }
    return true;
}

static void PoundAround(PlayState* play, Actor* actor) {
    Vec3f zero = { 0.0f, 0.0f, 0.0f };

    if (!sIsPoundReady) {
        Collider_InitCylinder(play, &sPoundCollider);
        sIsPoundReady = true;
    }
    Collider_SetCylinder(play, &sPoundCollider, actor, &sPoundInit);
    sPoundPos = actor->world.pos;
    sPoundFrames = DOMROD_POUND_FRAMES;
    sAttackCooldown = DOMROD_POUND_COOLDOWN;
    Audio_PlaySfx_AtPos(&actor->world.pos, NA_SE_IT_HAMMER_HIT);
    EffectSsBlast_SpawnWhiteShockwave(play, &actor->world.pos, &zero, &zero);
}

static bool IsArmosReeling(EnAm* armos) {
    return armos->actionFunc == func_808B07A8;
}

static bool DriveArmos(Player* player, PlayState* play, EnAm* armos, bool isAttacking) {
    if (IsDyingArmos(&armos->actor)) {
        return false;
    }
    if (IsArmosReeling(armos)) {
        sLastLinkPos = player->actor.world.pos;
        return true;
    }
    armos->actionFunc = HopInPlace;
    HaltOwnMovement(&armos->actor);
    MimicLink(player, &armos->actor, true);
    if (!isAttacking) {
        return true;
    }
    Vec3f point;

    if (!FindCommandPoint(player, &armos->actor, &point)) {
        armos->actor.colChkInfo.health = 0;
        func_808B0820(armos);
        return false;
    }
    if (sAttackCooldown == 0) {
        PoundAround(play, &armos->actor);
    }
    return true;
}

static void AimEyegoreLaser(EnEgol* eyegore, Vec3f* point) {
    Actor* actor = &eyegore->actor;

    actor->world.rot.y = Math_Vec3f_Yaw(&actor->world.pos, point);
    actor->shape.rot.y = actor->world.rot.y;
    eyegore->laserRot.x = CLAMP(Math_Vec3f_Pitch(&eyegore->laserBase, point), DOMROD_EYEGORE_LASER_PITCH_MIN,
                                DOMROD_EYEGORE_LASER_PITCH_MAX);
}

// The Eyegore's own laser and slam play out untouched; it is taken back once they fall through to its walk.
static bool DriveEyegore(Player* player, PlayState* play, EnEgol* eyegore, bool isAttacking) {
    Vec3f point;
    bool hasTarget = FindCommandPoint(player, &eyegore->actor, &point);

    if (IsDyingEyegore(&eyegore->actor)) {
        return false;
    }
    if (eyegore->actionFunc == EnEgol_Walk) {
        TakeEyegoreReins(eyegore);
    }
    if (eyegore->actionFunc == EnEgol_Laser && hasTarget) {
        AimEyegoreLaser(eyegore, &point);
    }
    if (eyegore->actionFunc != StrideWithLink) {
        sLastLinkPos = player->actor.world.pos;
        sHasPossessedMoved = false;
        return true;
    }
    MimicLink(player, &eyegore->actor, false);
    if (!isAttacking) {
        return true;
    }
    if (!hasTarget) {
        EnEgol_SetupSlam(eyegore);
        return true;
    }
    AimEyegoreLaser(eyegore, &point);
    EnEgol_SetupLaser(eyegore);
    return true;
}

static bool DriveActor(Player* player, PlayState* play, Actor* actor, bool isAttacking) {
    switch (actor->id) {
        case ACTOR_EN_VM:
            return DriveBeamos(player, play, (EnVm*)actor, isAttacking);
        case ACTOR_EN_AM:
            return DriveArmos(player, play, (EnAm*)actor, isAttacking);
        case ACTOR_EN_EGOL:
            return DriveEyegore(player, play, (EnEgol*)actor, isAttacking);
        default:
            return false;
    }
}

// Z + A would otherwise backflip or sidehop: while a foe is held it becomes the attack command.
static void CommandWithTargetA(PlayState* play, Player* player, s32 action, bool* consumed, bool* started) {
    if (sPhase != DOMROD_CONTROLLING || !IsCommandTargeting(player) || !(play->state.input[0].press.button & BTN_A)) {
        return;
    }
    sIsTargetACommand = true;
    *consumed = true;
    *started = true;
}

// A possessed foe must not sense Link: its own AI would turn on him between commands.
static void HidePlayerFromPossessed(Actor* actor, f32* xzDist, f32* yDist, s16* yawTowards) {
    if (actor != sPossessed) {
        return;
    }
    *xzDist = DOMROD_HIDDEN_PLAYER_DISTANCE;
}

static bool ShouldReleasePossessed(Player* player, u32 pressed, u32 rodButton, bool isTargetA) {
    u32 releaseButtons = sReleaseButtons & ~rodButton;

    if (isTargetA) {
        releaseButtons &= ~BTN_A;
    }
    return sPossessed == NULL || sPossessed->update == NULL ||
           Math_Vec3f_DistXYZ(&player->actor.world.pos, &sPossessed->world.pos) > DOMROD_MAX_RANGE ||
           (pressed & releaseButtons);
}

static void UpdateControlling(Player* player, PlayState* play) {
    u32 rodButton = FindEquippedButtonMask();
    u32 pressed = play->state.input[0].press.button;
    bool isTargetA = sIsTargetACommand || (IsCommandTargeting(player) && (pressed & BTN_A));

    sIsTargetACommand = false;
    if (ShouldReleasePossessed(player, pressed, rodButton, isTargetA)) {
        ReleasePossessed();
        return;
    }
    if (sAttackCooldown > 0) {
        sAttackCooldown--;
    }
    sIsDamagePaused = player->invincibilityTimer > 0;
    if (sIsDamagePaused) {
        sLastLinkPos = player->actor.world.pos;
        sHasPossessedMoved = false;
    } else if (!DriveActor(player, play, sPossessed, isTargetA || (pressed & rodButton))) {
        ReleasePossessed();
        return;
    }
    sOrbPos = sPossessed->focus.pos;
    MoveLight();
    SparkleEvery(play, 3, 1);
}

static void UpdateAiming(Player* player, PlayState* play) {
    u32 rodButton = FindEquippedButtonMask();

    Z64Aiming_Update(player, play);
    Player_StopHorizontalMovement(player);
    if (play->state.input[0].press.button & (BTN_B | BTN_A | BTN_CUP)) {
        StopAiming(player, play);
        return;
    }
    if (play->state.input[0].cur.button & rodButton) {
        sWasButtonHeld = true;
        return;
    }
    if (sWasButtonHeld || (play->state.input[0].press.button & rodButton)) {
        LaunchAimed(player, play);
    }
}

// Aiming owns the upper body; otherwise the rod fights as a sword.
static int32_t UpdateRodInHand(Player* player, PlayState* play) {
    if (sPhase == DOMROD_AIMING) {
        return 1;
    }
    return Player_UpperAction_1(player, play);
}

static void TakeOutRod(PlayState* play, Player* player) {
    if (!sIsColliderReady) {
        Collider_InitCylinder(play, &sCollider);
        sIsColliderReady = true;
    }
    sPhase = DOMROD_IN_HAND;
    sIsFirePending = false;
    sLastInvincibility = player->invincibilityTimer;
}

// Acts from the end of the player's update: what the item-button pass sets on the player is undone the same frame.
static void RequestFire(Player* player, PlayState* play) {
    if (sPhase == DOMROD_STOWED || sPhase == DOMROD_IN_HAND) {
        sPhase = DOMROD_IN_HAND;
        sIsFirePending = true;
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

static bool IsChangingHeldItem(Player* player) {
    return (player->stateFlags3 & PLAYER_STATE3_START_CHANGING_HELD_ITEM) != 0;
}

static bool IsReadyInHand(Player* player) {
    return sPhase == DOMROD_IN_HAND && player->heldItemAction == PLAYER_IA_CUSTOM && !IsChangingHeldItem(player);
}

static void RunPendingFire(Player* player, PlayState* play) {
    if (!sIsFirePending || IsChangingHeldItem(player)) {
        return;
    }
    sIsFirePending = false;
    if (IsReadyInHand(player)) {
        LaunchAtWill(player, play);
    }
}

static void AimOnCUp(Player* player, PlayState* play) {
    if (IsReadyInHand(player) && (play->state.input[0].press.button & BTN_CUP)) {
        StartAiming(player, play);
    }
}

static bool IsOrbTarget(Actor* actor) {
    return IsPossessable(actor) || IsSomariaSummon(actor);
}

static Actor* ScanForSelection(Player* player, PlayState* play) {
    Actor* best = NULL;
    s16 bestError = DOMROD_SELECT_CONE;

    for (s32 category = 0; category < ACTORCAT_MAX; category++) {
        for (Actor* actor = play->actorCtx.actorLists[category].first; actor != NULL; actor = actor->next) {
            if (actor->update == NULL || !IsOrbTarget(actor) ||
                Math_Vec3f_DistXZ(&player->actor.world.pos, &actor->world.pos) > DOMROD_SELECT_RANGE) {
                continue;
            }
            s16 error =
                ABS((s16)(Math_Vec3f_Yaw(&player->actor.world.pos, &actor->world.pos) - player->actor.shape.rot.y));

            if (error < bestError) {
                bestError = error;
                best = actor;
            }
        }
    }
    return best;
}

// What is tinted is exactly what the orb will be fired at: the lock-on first, else the best match ahead.
static Actor* PickOrbTarget(Player* player, PlayState* play) {
    Actor* focus = player->focusActor;

    if (Player_IsZTargeting(player) && focus != NULL && focus->update != NULL) {
        return focus;
    }
    return ScanForSelection(player, play);
}

static void MarkSelection(Player* player, PlayState* play) {
    if (!IsReadyInHand(player)) {
        sSelection = NULL;
        return;
    }
    sSelection = PickOrbTarget(player, play);
    if (sSelection != NULL) {
        Actor_SetColorFilter(sSelection, 0, 255, 0, 4);
    }
}

static void UpdatePound(PlayState* play) {
    if (sPoundFrames <= 0) {
        return;
    }
    sPoundFrames--;
    sPoundCollider.dim.pos.x = sPoundPos.x;
    sPoundCollider.dim.pos.y = sPoundPos.y;
    sPoundCollider.dim.pos.z = sPoundPos.z;
    CollisionCheck_SetAT(play, &play->colChkCtx, &sPoundCollider.base);
}

static void UpdateOrb(Player* player, PlayState* play, bool isHurt) {
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

// The item's upper action does not run while Link fights on, so the host's watchdog is told the rod is still out.
static void UpdateRodWorld(Player* player) {
    PlayState* play = gPlayState;

    if (play == NULL || player != GET_PLAYER(play) || sPhase == DOMROD_STOWED) {
        return;
    }
    bool isHurt = WasJustHurt(player);

    if (sPhase == DOMROD_AIMING && isHurt) {
        StopAiming(player, play);
        return;
    }
    RunPendingFire(player, play);
    AimOnCUp(player, play);
    MarkSelection(player, play);
    if (sIsSlashActive) {
        sIsSlashActive = !PlayerAnimation_Update(play, &player->skelAnimeUpper);
    }
    UpdatePound(play);
    if (sPhase == DOMROD_AIMING) {
        UpdateAiming(player, play);
        Z64Items_KeepHeld(sApi);
    }
    if (!IsAway()) {
        return;
    }
    Z64Items_KeepHeld(sApi);
    UpdateOrb(player, play, isHurt);
}

static void ForgetScene(s16 sceneId, s8 spawnNum) {
    Z64Aiming_Drop();
    sLightNode = NULL;
    sIsPoundReady = false;
    sPoundFrames = 0;
    sCommandFrames = 0;
    sIsSlashActive = false;
    sPossessed = NULL;
    sBorrowedAttackCount = 0;
    sSelection = NULL;
    sOrbTarget = NULL;
    sIsFirePending = false;
    sIsWeaponHandReady = false;
    sPhase = DOMROD_STOWED;
}

static void DrawOrb(PlayState* play) {
    s16 spin = (s16)(play->gameplayFrames * 0x1000) + (s16)(Rand_ZeroOne() * 0x4000);

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, 200);
    gDPSetEnvColor(POLY_XLU_DISP++, 255, 215, 50, 0);
    gDPPipeSync(POLY_XLU_DISP++);
    Matrix_Translate(sOrbPos.x, sOrbPos.y, sOrbPos.z, MTXMODE_NEW);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    Matrix_Scale(DOMROD_ORB_SCALE, DOMROD_ORB_SCALE, DOMROD_ORB_SCALE, MTXMODE_APPLY);
    Matrix_RotateZS(spin, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sOrbDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

// Expects the current matrix on the left hand; the hand's own scale is undone so the rod keeps its size.
static void DrawRodInHand(PlayState* play, Player* player) {
    f32 unscale = player->actor.scale.x != 0.0f ? 1.0f / player->actor.scale.x : 1.0f;

    Matrix_Push();
    Matrix_Scale(unscale, unscale, unscale, MTXMODE_APPLY);
    Matrix_RotateZF(DEG_TO_RAD(DOMROD_HELD_ROT_Z), MTXMODE_APPLY);
    Matrix_Translate(0.0f, DOMROD_HELD_OFFSET_Y, 0.0f, MTXMODE_APPLY);
    Matrix_Scale(DOMROD_HELD_SCALE, DOMROD_HELD_SCALE, DOMROD_HELD_SCALE, MTXMODE_APPLY);
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sRodDL);
    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sRodXluDL);
    CLOSE_DISPS(play->state.gfxCtx);
    Matrix_Pop();
}

static bool IsRodShownBy(Player* player) {
    RemoteVisualState visual;

    if (player == GET_PLAYER(gPlayState)) {
        return sPhase != DOMROD_STOWED;
    }
    return ReadRemoteVisual(player, &visual);
}

// With the sword model group the left hand holds a blade; a closed fist grips the rod instead.
static void ReplaceSwordHand(Player* player, s32 limbIndex, Gfx** dList, Gfx* limbDList, Vec3f* pos) {
    if (gPlayState == NULL || limbIndex != PLAYER_LIMB_LEFT_HAND || !IsRodShownBy(player)) {
        return;
    }
    *dList = (Gfx*)gLinkHumanLeftHandClosedDL;
}

static uint32_t CaptureRemoteLimbEnv(PlayState* play) {
    Gfx* begin = play->state.gfxCtx->polyOpa.start;
    Gfx* cursor = play->state.gfxCtx->polyOpa.p;

    while (cursor > begin) {
        cursor--;
        if (((cursor->words.w0 >> 24) & 255) == G_SETENVCOLOR) {
            return (uint32_t)cursor->words.w1;
        }
    }
    return UINT32_MAX;
}

static void RestoreRemoteLimbEnv(PlayState* play, uint32_t color) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, 255);
    gDPSetEnvColor(POLY_OPA_DISP++, color >> 24, (color >> 16) & 255, (color >> 8) & 255, color & 255);
    gSPLoadGeometryMode(POLY_OPA_DISP++, G_ZBUFFER | G_SHADE | G_CULL_BACK | G_LIGHTING | G_SHADING_SMOOTH);
    CLOSE_DISPS(play->state.gfxCtx);
}

// The local rod is drawn after Link so his remaining limbs keep their render state; a remote one is drawn in place.
static void DrawRodAtHand(Player* player, s32 limbIndex) {
    PlayState* play = gPlayState;
    RemoteVisualState visual;

    if (play == NULL || limbIndex != PLAYER_LIMB_LEFT_HAND) {
        return;
    }
    if (player == GET_PLAYER(play)) {
        Matrix_Get(&sWeaponHand);
        sIsWeaponHandReady = sPhase != DOMROD_STOWED;
        return;
    }
    if (!ReadRemoteVisual(player, &visual)) {
        return;
    }
    uint32_t savedEnv = CaptureRemoteLimbEnv(play);

    DrawRodInHand(play, player);
    RestoreRemoteLimbEnv(play, savedEnv);
}

static void DrawHeldRod(Actor* actor, PlayState* play) {
    Player* player = (Player*)actor;

    if (sPhase == DOMROD_STOWED || !sIsWeaponHandReady || (player->stateFlags2 & PLAYER_STATE2_20000000)) {
        return;
    }
    Matrix_Push();
    Matrix_Put(&sWeaponHand);
    DrawRodInHand(play, player);
    Matrix_Pop();
}

static void DrawRodEffects(void) {
    PlayState* play = gPlayState;

    if (play == NULL) {
        return;
    }
    if (sPhase == DOMROD_AIMING) {
        Z64Aiming_DrawReticle(play, GET_PLAYER(play), DOMROD_MAX_RANGE,
                              (Z64AimingReticleStyle){ Z64AIMING_TOGGLE_HOOKSHOT, Z64AIMING_TINT_CONTROL });
        return;
    }
    if (sPhase == DOMROD_FLYING || sPhase == DOMROD_RETURNING || sPhase == DOMROD_CONTROLLING) {
        DrawOrb(play);
    }
}

static void DrawGetItem(PlayState* play, S2HGetItemEntry* entry) {
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_RotateYF((s16)(play->gameplayFrames * 2) * 0.01f, MTXMODE_APPLY);
    Matrix_Scale(DOMROD_GIVE_SCALE, DOMROD_GIVE_SCALE, DOMROD_GIVE_SCALE, MTXMODE_APPLY);
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sRodDL);
    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sRodXluDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static const S2HCustomItemRandomizer sRandomizerItem = {
    sizeof(S2HCustomItemRandomizer), 0, S2H_CUSTOM_ITEM_TYPE_ITEM, 0, 1, DOMROD_KEY, DOMROD_KEY, "",
};

static void ReceiveRemoteVisual(const S2HNetMessage* message) {
    (void)message;
}

static bool ReadRemoteVisual(Player* player, RemoteVisualState* visual) {
    if (!sRemoteVisualReady || gPlayState == NULL || player == GET_PLAYER(gPlayState)) {
        return false;
    }
    uint32_t peerId = sApi->GetNetPeerIdForActor(&player->actor);

    if (peerId == 0 ||
        sApi->GetNetPeerState(peerId, DOMROD_VISUAL_CHANNEL, visual, sizeof(*visual)) != sizeof(*visual)) {
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
        visual.active = (sPhase != DOMROD_STOWED);
        if (sRemoteVisualSent && (uint32_t)(play->gameplayFrames - sRemoteVisualFrame) < REMOTE_VISUAL_INTERVAL) {
            return;
        }
    }
    if (sRemoteVisualSent && memcmp(&visual, &sLastRemoteVisual, sizeof(visual)) == 0) {
        return;
    }
    if (sApi->SetNetPeerState(DOMROD_VISUAL_CHANNEL, &visual, sizeof(visual))) {
        sLastRemoteVisual = visual;
        sRemoteVisualSent = true;
        sRemoteVisualFrame = play != NULL ? play->gameplayFrames : 0;
    }
}

static void InitRemoteVisuals(void) {
    if (!S2H_MOD_API_HAS(sApi, RegisterNetChannel) || !S2H_MOD_API_HAS(sApi, SetNetPeerState) ||
        !S2H_MOD_API_HAS(sApi, GetNetPeerState) || !S2H_MOD_API_HAS(sApi, GetNetPeerIdForActor)) {
        return;
    }
    sRemoteVisualReady = sApi->RegisterNetChannel(DOMROD_VISUAL_CHANNEL, 1, ReceiveRemoteVisual);
    if (sRemoteVisualReady) {
        S2H_REGISTER_HOOK(sApi, OnGameFrameUpdate, PublishRemoteVisual);
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
    if (!S2H_MOD_API_HAS(sApi, GetEquippedCustomItem) || !S2H_MOD_API_HAS(sApi, GetActorId)) {
        return;
    }

    S2HCustomItemDefinition rod = Z64Items_Define(DOMROD_KEY, sIconTex, sNameTex);

    Z64Items_SetButtons(&rod, S2H_CUSTOM_ITEM_C_BUTTON | S2H_CUSTOM_ITEM_DPAD);
    Z64Items_SetForms(&rod, S2H_CUSTOM_ITEM_FORM_BIT(PLAYER_FORM_HUMAN));
    Z64Items_SetPlacement(&rod, 0, 22, -1);
    Z64Items_SetTextbox(&rod, "You got the %pDominion Rod%w!&Ready it with %y\xB2%w and press %y\xB2%w&"
                              "to fire a control orb at the&foe that glows blue. %y\xB6%w aims.^"
                              "Possess %rBeamos%w, %yArmos%w or&%bEyegores%w. Hold %y\xB5%w and press %y\xB0%w&"
                              "to command an attack, even&without a locked target.^"
                              "Press %y\xB2%w with no target: Beamos&and Armos blow up, and an&"
                              "Eyegore slams the ground.");
    Z64Items_SetPauseText(&rod, "%pDominion Rod&%wPress %y\xB2%w to fire at the blue foe.&"
                                "%y\xB6%w aims. A possessed foe copies&your steps.");
    Z64Items_SetAction(&rod, TakeOutRod, UpdateRodInHand);
    Z64Items_SetHeldCallbacks(&rod, RequestFire, PutRodAway, NULL);
    Z64Items_SetHeldModel(&rod, NULL, PLAYER_MODELGROUP_ONE_HAND_SWORD);
    Z64Items_SetMeleeWeapon(&rod, Z64ITEMS_MELEE_KOKIRI_SWORD);
    rod.getItemEntry.drawFunc = DrawGetItem;

    Z64Items_SetLogic(&rod, &sRandomizerItem);
    if (!Z64Items_Register(sApi, &rod)) {
        return;
    }
    S2H_REGISTER_HOOK_FOR_ID(sApi, OnPlayerActionHandler, SOH_PLAYER_ACTION_ZTARGET_A, CommandWithTargetA);
    S2H_REGISTER_HOOK(sApi, OnPlayerUpdate, UpdateRodWorld);
    S2H_REGISTER_HOOK(sApi, OnActorResolvePlayerRelation, HidePlayerFromPossessed);
    S2H_REGISTER_HOOK(sApi, OnPlayDrawEnd, DrawRodEffects);
    S2H_REGISTER_HOOK(sApi, OnPlayerResolveLimbDraw, ReplaceSwordHand);
    S2H_REGISTER_HOOK(sApi, OnPlayerPostLimbDraw, DrawRodAtHand);
    S2H_REGISTER_HOOK_FOR_ID(sApi, OnActorDrawEnd, ACTOR_PLAYER, DrawHeldRod);
    S2H_REGISTER_HOOK(sApi, OnSceneInit, ForgetScene);
}
