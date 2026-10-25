#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "z64items.h"
#include "z64aiming.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "2s2h/Rando/Types.h"
#include "objects/gameplay_keep/gameplay_keep.h"

#define GARO_MASK_KEY "nei.garo_form_mask"
#define GARO_FORM_KEY "nei.garo"
#define GARO_MASK_PAGE 1
#define GARO_MASK_SLOT 20

#define STICK_FULL 80.0f

#define SPIN_DAMAGE 2
#define SPIN_FRAMES 14
#define SPIN_TURNS 2
#define SPIN_YAW_RATE ((0x10000 * SPIN_TURNS) / SPIN_FRAMES)
#define SPIN_PLAY_SPEED 1.5f
#define SPIN_MOVE_SPEED (9.0f * 1.5f)
// The spin starts on the press and turns into the rod charge if B is still down this many frames later.
#define B_HOLD_FRAMES 9

#define DASH_SPEED 14.0f
#define DASH_DAMAGE 4
#define DASH_TURN 0x400

#define PARRY_DAMAGE 4
#define PARRY_STRIKE_FRAME 6
#define PARRY_FREEZE_FRAMES 60
// A frozen actor never re-registers its AC collider, so both counters thaw their target before landing.
#define PARRY_THAW_LEAD 2
#define BANISH_THAW_LEAD 3
#define RIPOSTE_OFFSET 45.0f
#define GUARD_MELEE_RANGE 160.0f
#define GUARD_HOLD_FRACTION 0.5f
#define GUARD_RETURN_SPEED 1.5f
// Shots are caught in the air: most projectiles delete themselves on impact.
#define GUARD_CATCH_RADIUS 130.0f
#define GUARD_CATCH_HEIGHT 40.0f
#define GUARD_CATCH_MIN_SPEED 1.0f

#define REFLECT_FRAMES 60
#define REFLECT_DAMAGE 4
#define REFLECT_HALF 10.0f
#define REFLECT_MIN_SPEED 8.0f
#define REFLECT_INVULNERABILITY 12
#define REFLECT_PUSH_OUT 35.0f

#define BANISH_COOLDOWN 300
#define BANISH_OFFSET 50.0f
#define BANISH_VANISH_FRAMES 8
#define BANISH_STUN_FRAMES 60
#define BANISH_SHADOW_FRAMES 9
#define BANISH_STUN_RADIUS 80.0f
#define BANISH_NO_TARGET_REACH 60.0f
#define SHADOW_STRIKE_DAMAGE 8
#define SHADOW_STRIKE_FRAME 12
#define SHADOW_STRIKE_WINDOW 4
#define SHADOW_STRIKE_PLAY_SPEED 2.5f

#define LAND_STRIKE_DAMAGE 8
#define LAND_STRIKE_FRAME 12
#define LAND_STRIKE_TAIL 3
#define HOP_MIN_AIR_FRAMES 3
#define JUMP_ATTACK_SLASH_FRAME 6
#define JUMP_GRAVITY (-1.2f)

#define ROD_RELEASE_COOLDOWN 10
#define ROD_CHARGE_MAX 120
#define ROD_CHARGE_TIER2 35
#define ROD_CHARGE_TIER3 85
#define ROD_LEVELS 3
#define ROD_MAGIC_COST 4
#define ROD_L2_DAMAGE 4
#define ROD_L3_DAMAGE 3
#define ROD_ORB_SPEED 12.0f
#define ROD_ORB_LIFETIME 60
#define ROD_BURST_LIFETIME (ROD_ORB_LIFETIME / 2)
#define ROD_BALL_CIRCLE_MAX 0.08f
#define ROD_BALL_SCALE_MAX 7.0f
#define ROD_BALL_AHEAD 55.0f
#define RETICLE_DIST 320.0f
#define RETICLE_SPREAD 26.0f
#define RETICLE_DOT_SCALE 0.55f

// MM has no Ganondorf light-ball models, so every orb is gameplay_keep's light orb at these scales.
#define ORB_BALL_UNIT 0.01f
#define ORB_DOT_UNIT 0.05f
#define ORB_SHADOW_UNIT 0.0125f
#define ORB_SEEKER_DRAW_SCALE 0.03f
#define ORB_STREAK_DRAWN 6

#define ORB_POOL_MAX 12
#define ORB_SEEKERS 4
#define ORB_SEEKER_DAMAGE 2
#define ORB_SEEKER_LIFETIME 70
#define ORB_SEEKER_SPEED 16.0f
#define ORB_SEEKER_TURN 0x1200
#define ORB_SEEKER_FAN 0x2000
#define ORB_BURST_BALLS 8
#define ORB_HOME_RANGE 700.0f
#define ORB_HOME_CONE 0.2f
#define ORB_TURN_RATE 0x0700
#define ORB_WAKE_SCALE 85
#define ORB_TRAIL_LEN 15
#define BIG_MAGIC_RAYS_MAX 6

#define ELEMENT_COUNT 6
#define ELEMENT_FIRE 0
#define ELEMENT_ICE 1
#define ELEMENT_LIGHT 2

#define LAUGH_CHANCE 0.20f
#define ENEMY_SNAPSHOT_MAX 64

#define DEATH_FLAMES 9
#define DEATH_FLAME_RADIUS 20.0f

// The Garo enemy's own 19-bone skeleton: Link's pose drives the body and legs, Garo's idle sways the robe and
// the blades. The enemy is drawn at 0.035, so the subtree takes 3.5 over Link's 0.01.
#define HYBRID_LIMBS 19
#define HYBRID_JOINTS (HYBRID_LIMBS + 1)
#define HYBRID_SUBTREE_SCALE 3.5f
#define HYBRID_JOINT_ROOT 1
#define HYBRID_JOINT_L_SWORD 4
#define HYBRID_JOINT_R_SWORD 6
#define HYBRID_JOINT_ROBE_LEFT 9
#define HYBRID_JOINT_ROBE_RIGHT 10
#define HYBRID_JOINT_R_THIGH 14
#define HYBRID_JOINT_R_FOOT 16
#define HYBRID_JOINT_L_THIGH 17
#define HYBRID_JOINT_L_FOOT 19
#define LEG_THICKNESS 1.3f
#define TRAIL_BLADE_LENGTH 900.0f
#define TRAIL_ELEM_DURATION 8

// MM leaves these bits unnamed: dead, talking, loading, getting an item, carrying, swimming, first person, ledge
// climb, ladder, horse, damage knockback and the textbox freeze.
#define BUSY_STATE1                                                                                          \
    (PLAYER_STATE1_DEAD | PLAYER_STATE1_TALKING | PLAYER_STATE1_200 | PLAYER_STATE1_400 |                    \
     PLAYER_STATE1_CARRYING_ACTOR | PLAYER_STATE1_8000000 | PLAYER_STATE1_100000 | PLAYER_STATE1_4 |         \
     PLAYER_STATE1_200000 | PLAYER_STATE1_800000 | PLAYER_STATE1_4000000 | PLAYER_STATE1_20000000)
#define STATE1_SWIMMING PLAYER_STATE1_8000000
#define STATE1_FIRST_PERSON PLAYER_STATE1_100000
// Shape yaw follows `yaw` instead of the lock-on, or stops following anything.
#define STATE2_NO_Z_TARGET_ROTATION PLAYER_STATE2_20
#define STATE2_NO_ROTATION PLAYER_STATE2_40
#define STATE2_DISABLE_DRAW PLAYER_STATE2_20000000

#define GARO_ANIM_PATH(name) "__OTR__objects/forms/garo/gPlayerAnim_garo_" name
static const ALIGN_ASSET(2) char sIconTex[] = "__OTR__icon_item_static_yar/gItemIconGaroMaskTex";
static const ALIGN_ASSET(2) char sNameTex[] = "__OTR__item_name_static/gItemNameGarosMaskENGTex";
static const ALIGN_ASSET(2) char sGetItemDL[] = "__OTR__objects/object_gi_mask09/gGiGarosMaskFaceDL";
static const ALIGN_ASSET(2) char sMaskDL[] = "__OTR__objects/object_mask_json/object_mask_json_DL_0004C0";
static const ALIGN_ASSET(2) char sHybridSkelPath[] = "__OTR__objects/object_jso/gGaroSkel";
static const ALIGN_ASSET(2) char sHybridIdlePath[] = "__OTR__objects/object_jso/gGaroIdleAnim";

typedef enum {
    ANIM_GUARD,
    ANIM_DASH,
    ANIM_SPIN,
    ANIM_COLLAPSE,
    ANIM_APPEAR,
    ANIM_TAKE_OUT_BOMB,
    ANIM_LAUGH,
    ANIM_APPEAR_DRAW_SWORDS,
    ANIM_BOUNCE,
    ANIM_JUMP_BACK,
    ANIM_SLASH_LOOP,
    ANIM_DRAW_SWORDS,
    ANIM_COUNT,
} GaroAnim;

static const char* const sAnimPaths[ANIM_COUNT] = {
    GARO_ANIM_PATH("guard"),    GARO_ANIM_PATH("dashAttack"),       GARO_ANIM_PATH("spinAttack"),
    GARO_ANIM_PATH("collapse"), GARO_ANIM_PATH("appear"),           GARO_ANIM_PATH("takeOutBomb"),
    GARO_ANIM_PATH("laugh"),    GARO_ANIM_PATH("appearDrawSwords"), GARO_ANIM_PATH("bounce"),
    GARO_ANIM_PATH("jumpBack"), GARO_ANIM_PATH("slashLoop"),        GARO_ANIM_PATH("drawSwords"),
};

typedef enum {
    GARO_MOVE_NONE,
    GARO_MOVE_SPIN,
    GARO_MOVE_ROD_AIM,
    GARO_MOVE_GUARD,
    GARO_MOVE_GUARD_RETURN,
    GARO_MOVE_RIPOSTE,
    GARO_MOVE_DASH,
    GARO_MOVE_BANISH_VANISH,
    GARO_MOVE_BANISH_SHADOW,
    GARO_MOVE_SHADOW_STRIKE,
    GARO_MOVE_LAUGH,
    GARO_MOVE_HOP,
    GARO_MOVE_BACKFLIP,
    GARO_MOVE_JUMP_ATTACK,
    GARO_MOVE_AIR_SLASH,
    GARO_MOVE_LAND_STRIKE,
} GaroMove;

typedef struct {
    bool isActive;
    Vec3f pos;
    s16 yaw;
    s16 pitch;
    s16 timer;
    u8 element;
    u8 damage;
    u32 dmgFlags;
    bool bursts;
    bool isSeeker;
    Actor* target;
    f32 ballCircle;
    f32 ballScale;
    Vec3f trailPos[ORB_TRAIL_LEN];
    s16 trailIndex;
} RodOrb;

// Link limb -> hybrid joint. The shoulders also drive the robe's side panels, done apart in PoseHybrid.
static const s8 sLinkToHybridJoint[PLAYER_LIMB_MAX] = {
    -1, 1, 13, -1, 14, 15, 16, 17, 18, 19, 2, 12, -1, -1, 3, -1, 4, 5, -1, 6, -1, -1,
};

// Sword and robe bones keep Garo's own sway instead of Link's pose.
static bool IsGaroOwnedJoint(s8 joint) {
    return joint == HYBRID_JOINT_L_SWORD || joint == HYBRID_JOINT_R_SWORD || joint == 7 || joint == 8 || joint == 11;
}

// Igos du Ikana's bank (En_Osk): MM gives Garo no player voice, so every Link grunt is paired by tone with one of
// his.
static const u16 sGaroVoiceByAction[0x20] = {
    NA_SE_EN_BOSU_ATTACK,     NA_SE_EN_BOSU_ATTACK_K, NA_SE_EN_BOSU_ATTACK_W, NA_SE_EN_BOSU_HAND,
    NA_SE_EN_BOSU_STAND,      NA_SE_EN_BOSU_DAMAGE,   NA_SE_EN_BOSU_SHOCK,    NA_SE_EN_BOSU_DAMAGE,
    NA_SE_EN_BOSU_DEAD_VOICE, NA_SE_EN_BOSU_SIT,      NA_SE_EN_BOSU_SIT,      NA_SE_EN_BOSU_DEAD,
    NA_SE_EN_BOSU_SHOCK,      NA_SE_EN_BOSU_HAND,     NA_SE_EN_BOSU_SHIT,     NA_SE_EN_BOSU_SIT,
    NA_SE_EN_BOSU_SIT,        NA_SE_EN_BOSU_CYNICAL,  NA_SE_EN_BOSU_SWORD,    NA_SE_EN_BOSU_DAMAGE,
    NA_SE_EN_BOSU_ATTACK_W,   NA_SE_EN_BOSU_ATTACK_K, NA_SE_EN_BOSU_SHOCK,    NA_SE_EN_BOSU_ATTACK_K,
    NA_SE_EN_BOSU_HAND,       NA_SE_EN_BOSU_HAND,     NA_SE_EN_BOSU_DAMAGE,   NA_SE_EN_BOSU_DAMAGE,
    NA_SE_EN_BOSU_ATTACK_K,   NA_SE_EN_BOSU_CYNICAL,  NA_SE_EN_BOSU_ATTACK,   NA_SE_EN_BOSU_LAUGH,
};

// The six SW97 arrow colours, in SW97's order; only fire, ice and light carry a vanilla arrow flag.
static const u8 sOrbPrim[ELEMENT_COUNT][3] = {
    { 255, 200, 0 }, { 170, 255, 255 }, { 255, 255, 255 }, { 200, 120, 255 }, { 255, 255, 170 }, { 170, 255, 255 },
};
static const u8 sOrbEnv[ELEMENT_COUNT][3] = {
    { 255, 0, 0 }, { 0, 0, 255 }, { 170, 170, 170 }, { 100, 0, 255 }, { 255, 255, 0 }, { 0, 255, 0 },
};
static const f32 sRodLevelScale[ROD_LEVELS] = { 0.35f, 0.7f, 1.0f };
static const s16 sRodLevelRays[ROD_LEVELS] = { 0, BIG_MAGIC_RAYS_MAX / 2, BIG_MAGIC_RAYS_MAX };

static const char* const sRequiredHooks[] = {
    "OnPlayerActionHandler",  "OnPlayerResolveLimbDraw", "OnPlayerFilterInput", "OnActorPlaySfx",
    "OnBgCheckRaycastFloor",  "OnSceneInit",             "ShouldActorDraw",     "OnCollisionResolveDamage",
    "OnOcarinaInput",
};
static const S2HModRequirements sRequirements = { sizeof(S2HModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static ColliderQuadInit sEscortQuadInit = {
    { COL_MATERIAL_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_TYPE_PLAYER, COLSHAPE_QUAD },
    { ELEM_MATERIAL_UNK2,
      { 0xFFCFFFFF, 0x00, 0x10 },
      { 0x00000000, 0x00, 0x00 },
      ATELEM_ON | ATELEM_NEAREST | ATELEM_SFX_NORMAL,
      ACELEM_NONE,
      OCELEM_NONE },
    { { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } } },
};

static EffectBlureInit2 sTrailInit = {
    0,
    0,
    0,
    { 120, 60, 200, 200 },
    { 60, 30, 140, 100 },
    { 60, 30, 140, 0 },
    { 60, 30, 140, 0 },
    TRAIL_ELEM_DURATION,
    0,
    EFF_BLURE_DRAW_MODE_SIMPLE,
    0,
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
};

s32 Player_SetAction(PlayState* play, Player* this, PlayerActionFunc actionFunc, s32 arg3);
void func_8083B1A0(Player* player, PlayState* play);
s32 func_808339D4(PlayState* play, Player* player, s32 damage);
bool Player_IsZTargeting(Player* player);
extern PlayerAnimationHeader* ResourceMgr_LoadPlayerAnimAsHeader(const char* animPath);
extern SkeletonHeader* ResourceMgr_LoadSkeletonByName(const char* path, SkelAnime* skelAnime);

static const S2HModApi* sApi;
static PlayerAnimationHeader* sAnims[ANIM_COUNT];

static u8 sMove;
static s16 sMoveTimer;
static s16 sBHeldFrames;
static s16 sSpinEntryYaw;
static s16 sHopAirFrames;
static s16 sHopYaw;
static bool sHasStruck;
static s16 sStrikeTailFrames;
static s16 sBanishCooldown;
static s16 sRodCooldown;
static Actor* sBanishTarget;
static Actor* sParryAttacker;
static Actor* sPendingParry;
static bool sIsParryPending;
static Vec3f sShadowStart;
static Vec3f sShadowEnd;
static Vec3f sShadowPos;
static s16 sShadowTimer;

static s16 sRodCharge;
static u8 sRodElement;
static bool sIsRodChimed;
static f32 sRodBallCircle;
static f32 sRodBallScale;
static s16 sRodBallRays;

static RodOrb sOrbs[ORB_POOL_MAX];
static ColliderQuad sOrbQuads[ORB_POOL_MAX];
static bool sAreOrbQuadsReady;
static Actor* sReflectedShot;
static s16 sReflectTimer;
static ColliderQuad sReflectQuad;
static bool sIsReflectQuadReady;

static s32 sTrailLeft = -1;
static s32 sTrailRight = -1;
static s8 sTrailAxisLeft = -1;
static s8 sTrailAxisRight = -1;

static SkelAnime sHybrid;
static Vec3s sHybridJoints[HYBRID_JOINTS];
static Vec3s sHybridMorph[HYBRID_JOINTS];
static bool sIsHybridReady;

static Actor* sEnemySnapshot[ENEMY_SNAPSHOT_MAX];
static s32 sEnemySnapshotCount;
static bool sIsLaughPending;
static bool sHaveDeathFlamesSpawned;
#define REVEALED_MAX 64
static Actor* sRevealed[REVEALED_MAX];
static s32 sRevealedCount;
static bool sIsOcarinaOut;
static bool sIsInstrumentLent;
static u8 sScaledDamage;

static void GaroAction(Player* player, PlayState* play);

static bool IsGaro(void) {
    return sApi != NULL && sApi->IsFormActive(GARO_FORM_KEY);
}

static bool IsGrounded(Player* player) {
    return (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) != 0;
}

static bool IsGaroBusy(Player* player) {
    return player->actionFunc == GaroAction;
}

static bool CanAct(Player* player) {
    return !(player->stateFlags1 & BUSY_STATE1) && player->csAction == PLAYER_CSACTION_NONE;
}

static bool IsPressed(PlayState* play, u16 button) {
    return CHECK_BTN_ALL(play->state.input[0].press.button, button);
}

static bool IsHeld(PlayState* play, u16 button) {
    return CHECK_BTN_ALL(play->state.input[0].cur.button, button);
}

static f32 GetStickMagnitude(PlayState* play) {
    f32 x = play->state.input[0].cur.stick_x;
    f32 y = play->state.input[0].cur.stick_y;

    return MIN(sqrtf((x * x) + (y * y)) / STICK_FULL, 1.0f);
}

static s16 GetStickYaw(PlayState* play) {
    f32 x = play->state.input[0].cur.stick_x;
    f32 y = play->state.input[0].cur.stick_y;

    return Math_Atan2S(y, -x) + Camera_GetInputDirYaw(GET_ACTIVE_CAM(play));
}

// Freezing the actor also stops its AC from registering, which is why both counters thaw it early.
static void MarkStunned(PlayState* play, Actor* target, s16 frames) {
    Vec3f accel = { 0.0f, -0.05f, 0.0f };
    Color_RGBA8 prim = { 140, 80, 220, 255 };
    Color_RGBA8 env = { 40, 10, 100, 0 };

    target->freezeTimer = frames;
    Actor_SetColorFilter(target, COLORFILTER_COLORFLAG_BLUE, 0xF8, COLORFILTER_BUFFLAG_OPA, frames);
    for (s32 i = 0; i < 8; i++) {
        f32 angle = (f32)i * (M_PIf * 2.0f / 8.0f);
        Vec3f pos = { target->world.pos.x + cosf(angle) * BANISH_STUN_RADIUS, target->world.pos.y + 30.0f,
                      target->world.pos.z + sinf(angle) * BANISH_STUN_RADIUS };
        Vec3f velocity = { cosf(angle) * 0.5f, 1.5f, sinf(angle) * 0.5f };
        EffectSsKirakira_SpawnSmall(play, &pos, &velocity, &accel, &prim, &env);
    }
    Actor_PlaySfx(target, NA_SE_IT_SHIELD_REFLECT_SW);
}

// A counter holds its target by pointer, and the target may have died since.
static bool IsActorAlive(PlayState* play, Actor* actor) {
    if (actor == NULL) {
        return false;
    }
    for (s32 category = 0; category < ACTORCAT_MAX; category++) {
        for (Actor* it = play->actorCtx.actorLists[category].first; it != NULL; it = it->next) {
            if (it == actor) {
                return true;
            }
        }
    }
    return false;
}

// A light-ball sparkle in the orb's element colours, standing in for the Phantom Ganon flash MM lacks.
static void SpawnOrbWake(PlayState* play, Vec3f* pos, Vec3f* velocity, s16 scale, u8 element) {
    Vec3f accel = { 0.0f, 0.0f, 0.0f };
    u8 e = element % ELEMENT_COUNT;
    Color_RGBA8 prim = { sOrbPrim[e][0], sOrbPrim[e][1], sOrbPrim[e][2], 255 };
    Color_RGBA8 env = { sOrbEnv[e][0], sOrbEnv[e][1], sOrbEnv[e][2], 0 };

    EffectSsKirakira_SpawnDispersed(play, pos, velocity, &accel, &prim, &env, scale * 4, 14);
}

// ---- sound ----

static void SpeakWithIgosVoice(Actor* actor, s32 kind, u16* sfxId, bool* handled) {
    u16 action = (u16)(*sfxId - NA_SE_VO_LI_SWORD_N);

    if (!IsGaro() || kind != SOH_ACTOR_SFX_VOICE) {
        return;
    }
    *handled = true;
    if (action < ARRAY_COUNT(sGaroVoiceByAction)) {
        Audio_PlaySfx_AtPos(&actor->projectedPos, sGaroVoiceByAction[action]);
    }
}

static u16 GetChargeSfx(u8 element) {
    if (element == ELEMENT_FIRE) {
        return NA_SE_PL_ARROW_CHARGE_FIRE;
    }
    if (element == ELEMENT_ICE) {
        return NA_SE_PL_ARROW_CHARGE_ICE;
    }
    if (element == ELEMENT_LIGHT) {
        return NA_SE_PL_ARROW_CHARGE_LIGHT;
    }
    return NA_SE_IT_SWORD_CHARGE;
}

// ---- animation ----

static void LoadAnims(void) {
    for (s32 slot = 0; slot < ANIM_COUNT; slot++) {
        sAnims[slot] = ResourceMgr_LoadPlayerAnimAsHeader(sAnimPaths[slot]);
    }
}

// endFrame below zero plays to the last frame; a negative speed runs the clip backwards.
static void PlayGaroAnim(PlayState* play, Player* player, s32 slot, f32 startFrame, f32 endFrame, f32 speed) {
    PlayerAnimationHeader* anim = sAnims[slot];

    if (anim == NULL) {
        return;
    }
    if (endFrame < 0.0f) {
        endFrame = Animation_GetLastFrame(anim);
    }
    PlayerAnimation_Change(play, &player->skelAnime, anim, speed, startFrame, endFrame, ANIMMODE_ONCE, -2.0f);
}

static bool AdvanceGaroAnim(PlayState* play, Player* player) {
    return PlayerAnimation_Update(play, &player->skelAnime);
}

static f32 GetAnimLastFrame(s32 slot) {
    return sAnims[slot] != NULL ? Animation_GetLastFrame(sAnims[slot]) : 0.0f;
}

// ---- strikes ----

// The sword flag is what every sword-vulnerable bumper accepts; the number itself is fixed past the enemy's
// damage table in FixStrikeDamage.
static void Strike(PlayState* play, Player* player, Vec3f* corners, u8 damage) {
    ColliderQuad* quad = &player->meleeWeaponQuads[0];

    Collider_ResetQuadAT(play, &quad->base);
    Collider_SetQuadVertices(quad, &corners[0], &corners[1], &corners[2], &corners[3]);
    quad->base.atFlags = AT_ON | AT_TYPE_PLAYER;
    quad->elem.atDmgInfo.dmgFlags = DMG_SWORD;
    quad->elem.atDmgInfo.damage = damage;
    quad->elem.atElemFlags = ATELEM_ON | ATELEM_NEAREST;
    CollisionCheck_SetAT(play, &play->colChkCtx, &quad->base);
}

static void StopStriking(Player* player) {
    player->meleeWeaponQuads[0].base.atFlags &= ~AT_ON;
}

// A slab in front of Garo, slanting from its near bottom edge to its far top one.
static void StrikeAhead(PlayState* play, Player* player, f32 halfWidth, f32 top, u8 damage) {
    f32 sinYaw = Math_SinS(player->actor.shape.rot.y);
    f32 cosYaw = Math_CosS(player->actor.shape.rot.y);
    Vec3f* pos = &player->actor.world.pos;
    f32 farX = pos->x + sinYaw * 60.0f;
    f32 farZ = pos->z + cosYaw * 60.0f;
    f32 nearX = pos->x + sinYaw * 10.0f;
    f32 nearZ = pos->z + cosYaw * 10.0f;
    Vec3f corners[4] = {
        { farX - cosYaw * halfWidth, pos->y + top, farZ + sinYaw * halfWidth },
        { farX + cosYaw * halfWidth, pos->y + top, farZ - sinYaw * halfWidth },
        { nearX + cosYaw * halfWidth, pos->y, nearZ - sinYaw * halfWidth },
        { nearX - cosYaw * halfWidth, pos->y, nearZ + sinYaw * halfWidth },
    };

    Strike(play, player, corners, damage);
}

// A full-height wall across his facing that marches outward: the plain slab is one thin line at any given
// distance and missed the enemy he had just appeared behind.
static void StrikeWallAhead(PlayState* play, Player* player, s16 liveFrame, u8 damage) {
    f32 distance = 15.0f + (f32)MAX(liveFrame, 0) * 12.0f;
    f32 sinYaw = Math_SinS(player->actor.shape.rot.y);
    f32 cosYaw = Math_CosS(player->actor.shape.rot.y);
    Vec3f* pos = &player->actor.world.pos;
    f32 centerX = pos->x + sinYaw * distance;
    f32 centerZ = pos->z + cosYaw * distance;
    Vec3f corners[4] = {
        { centerX - cosYaw * 45.0f, pos->y + 75.0f, centerZ + sinYaw * 45.0f },
        { centerX + cosYaw * 45.0f, pos->y + 75.0f, centerZ - sinYaw * 45.0f },
        { centerX + cosYaw * 45.0f, pos->y - 10.0f, centerZ - sinYaw * 45.0f },
        { centerX - cosYaw * 45.0f, pos->y - 10.0f, centerZ + sinYaw * 45.0f },
    };

    Strike(play, player, corners, damage);
}

// A quad is flat, so the landing blow turns it 30 degrees a live frame: half a turn covers every direction.
static void StrikeAround(PlayState* play, Player* player, s16 liveFrame, u8 damage) {
    s16 yaw = player->actor.shape.rot.y + (s16)(MAX(liveFrame, 0) * 0x1555);
    f32 dirX = Math_SinS(yaw) * 130.0f;
    f32 dirZ = Math_CosS(yaw) * 130.0f;
    Vec3f* pos = &player->actor.world.pos;
    Vec3f corners[4] = {
        { pos->x + dirX, pos->y + 130.0f, pos->z + dirZ },
        { pos->x - dirX, pos->y + 130.0f, pos->z - dirZ },
        { pos->x - dirX, pos->y - 20.0f, pos->z - dirZ },
        { pos->x + dirX, pos->y - 20.0f, pos->z + dirZ },
    };

    Strike(play, player, corners, damage);
}

// ---- sword trails ----

static void KillTrails(PlayState* play) {
    if (sTrailLeft >= 0) {
        Effect_Destroy(play, sTrailLeft);
    }
    if (sTrailRight >= 0) {
        Effect_Destroy(play, sTrailRight);
    }
    sTrailLeft = -1;
    sTrailRight = -1;
    sTrailAxisLeft = -1;
    sTrailAxisRight = -1;
}

static void SpawnTrails(PlayState* play) {
    if (sTrailLeft >= 0) {
        return;
    }
    Effect_Add(play, &sTrailLeft, EFFECT_BLURE2, 0, 0, &sTrailInit);
    Effect_Add(play, &sTrailRight, EFFECT_BLURE2, 0, 0, &sTrailInit);
}

static bool WantsTrails(void) {
    return sMove == GARO_MOVE_SPIN || sMove == GARO_MOVE_AIR_SLASH || sMove == GARO_MOVE_LAND_STRIKE ||
           sMove == GARO_MOVE_SHADOW_STRIKE || sMove == GARO_MOVE_RIPOSTE;
}

// The right blade's bone is mirrored, so each trail picks, once, the local axis that points down and away from
// Garo in the live pose.
static s8 PickTrailAxis(Player* player, Vec3f* base) {
    static const Vec3f sAxes[6] = { { 0, 1, 0 }, { 0, -1, 0 }, { 1, 0, 0 }, { -1, 0, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
    f32 outX = base->x - player->actor.world.pos.x;
    f32 outZ = base->z - player->actor.world.pos.z;
    f32 outLength = sqrtf(SQ(outX) + SQ(outZ));
    Vec3f want = { 0.0f, -1.0f, 0.0f };
    s8 best = 0;
    f32 bestScore = -1.0e9f;

    if (outLength > 0.001f) {
        want.x = outX / outLength * 0.6f;
        want.z = outZ / outLength * 0.6f;
    }
    for (s8 i = 0; i < 6; i++) {
        Vec3f local = { sAxes[i].x * TRAIL_BLADE_LENGTH, sAxes[i].y * TRAIL_BLADE_LENGTH,
                        sAxes[i].z * TRAIL_BLADE_LENGTH };
        Vec3f tip;
        Matrix_MultVec3f(&local, &tip);
        f32 dx = tip.x - base->x;
        f32 dy = tip.y - base->y;
        f32 dz = tip.z - base->z;
        f32 length = sqrtf(SQ(dx) + SQ(dy) + SQ(dz));
        if (length < 0.001f) {
            continue;
        }
        f32 score = (dx * want.x + dy * want.y + dz * want.z) / length;
        if (score > bestScore) {
            bestScore = score;
            best = i;
        }
    }
    return best;
}

static void FeedTrail(Player* player, s32 effectIndex, s8* axis) {
    static const Vec3f sAxes[6] = { { 0, 1, 0 }, { 0, -1, 0 }, { 1, 0, 0 }, { -1, 0, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
    EffectBlure* trail = (EffectBlure*)Effect_GetByIndex(effectIndex);
    Vec3f origin = { 0.0f, 0.0f, 0.0f };
    Vec3f base;
    Vec3f tip;

    if (trail == NULL) {
        return;
    }
    Matrix_MultVec3f(&origin, &base);
    if (*axis < 0) {
        *axis = PickTrailAxis(player, &base);
    }
    Vec3f local = { sAxes[*axis].x * TRAIL_BLADE_LENGTH, sAxes[*axis].y * TRAIL_BLADE_LENGTH,
                    sAxes[*axis].z * TRAIL_BLADE_LENGTH };
    Matrix_MultVec3f(&local, &tip);
    EffectBlure_AddVertex(trail, &tip, &base);
}

// ---- rod orbs ----

static u32 GetOrbDmgFlags(u8 element) {
    if (element == ELEMENT_FIRE) {
        return DMG_FIRE_ARROW;
    }
    if (element == ELEMENT_ICE) {
        return DMG_ICE_ARROW;
    }
    if (element == ELEMENT_LIGHT) {
        return DMG_LIGHT_ARROW;
    }
    return DMG_NORMAL_ARROW;
}

static u8 GetRodLevel(s16 charge) {
    if (charge < ROD_CHARGE_TIER2) {
        return 1;
    }
    return charge < ROD_CHARGE_TIER3 ? 2 : 3;
}

static void AddOrb(const RodOrb* orb) {
    for (s32 i = 0; i < ORB_POOL_MAX; i++) {
        if (!sOrbs[i].isActive) {
            sOrbs[i] = *orb;
            sOrbs[i].isActive = true;
            return;
        }
    }
}

// Level 1 fires nothing; level 3 flies half as far and breaks into seekers, and costs double for them.
static void FireRodOrb(PlayState* play, Player* player) {
    u8 level = GetRodLevel(sRodCharge);
    bool bursts = level >= 3;
    s16 cost = (s16)(ROD_MAGIC_COST * (bursts ? 2 : 1));
    RodOrb orb;

    sRodCharge = 0;
    sIsRodChimed = false;
    if (level < 2) {
        Player_PlaySfx(player, NA_SE_IT_BOW_FLICK);
        return;
    }
    memset(&orb, 0, sizeof(orb));
    orb.pos = player->bodyPartsPos[PLAYER_BODYPART_LEFT_HAND];
    Z64Aiming_GetDirection(player, &orb.yaw, &orb.pitch);
    orb.element = sRodElement;
    orb.damage = bursts ? ROD_L3_DAMAGE : ROD_L2_DAMAGE;
    orb.dmgFlags = GetOrbDmgFlags(sRodElement);
    orb.bursts = bursts;
    orb.timer = bursts ? ROD_BURST_LIFETIME : ROD_ORB_LIFETIME;
    orb.ballCircle = MAX(sRodBallCircle, ROD_BALL_CIRCLE_MAX * 0.35f);
    orb.ballScale = MAX(sRodBallScale, ROD_BALL_SCALE_MAX * 0.35f);
    if (!Magic_Consume(play, cost, MAGIC_CONSUME_NOW)) {
        orb.damage = 1;
    }
    AddOrb(&orb);
    Player_PlaySfx(player, NA_SE_IT_ARROW_SHOT);
    Player_PlaySfx(player, NA_SE_IT_MAGIC_ARROW_SHOT);
}

static Actor* FindNearestUnclaimed(PlayState* play, Vec3f* origin, Actor** claimed, s32 claimedCount) {
    Actor* best = NULL;
    f32 bestDistSq = SQ(ORB_HOME_RANGE);

    for (Actor* enemy = play->actorCtx.actorLists[ACTORCAT_ENEMY].first; enemy != NULL; enemy = enemy->next) {
        bool isClaimed = false;
        for (s32 i = 0; i < claimedCount; i++) {
            isClaimed = isClaimed || claimed[i] == enemy;
        }
        f32 distSq = Math3D_Vec3fDistSq(&enemy->world.pos, origin);
        if (!isClaimed && enemy->update != NULL && distSq < bestDistSq) {
            bestDistSq = distSq;
            best = enemy;
        }
    }
    return best;
}

// The impact, then fragments fanned out that each claim a different enemy; with fewer enemies than fragments
// the claims start over, so the spares double up instead of flying at nothing.
static void BurstOrb(PlayState* play, RodOrb* source) {
    Actor* claimed[ORB_SEEKERS];
    s32 claimedCount = 0;
    u8 element = source->element % ELEMENT_COUNT;

    EffectSsFhgFlash_SpawnShock(play, NULL, &source->pos, 200, 0);
    for (s32 i = 0; i < ORB_BURST_BALLS; i++) {
        Vec3f velocity = { Rand_CenteredFloat(12.0f), Rand_ZeroFloat(8.0f) + 2.0f, Rand_CenteredFloat(12.0f) };
        SpawnOrbWake(play, &source->pos, &velocity, (s16)(Rand_ZeroOne() * 60.0f) + 110, element);
    }
    Audio_PlaySfx_AtPos(&source->pos, NA_SE_IT_MAGIC_ARROW_SHOT);
    for (s32 i = 0; i < ORB_SEEKERS; i++) {
        Actor* target = FindNearestUnclaimed(play, &source->pos, claimed, claimedCount);
        if (target == NULL && claimedCount > 0) {
            claimedCount = 0;
            target = FindNearestUnclaimed(play, &source->pos, claimed, claimedCount);
        }
        RodOrb fragment;
        memset(&fragment, 0, sizeof(fragment));
        fragment.pos = source->pos;
        fragment.yaw = (s16)(source->yaw + (s16)(i * ORB_SEEKER_FAN) - ORB_SEEKER_FAN);
        fragment.pitch = (s16)(source->pitch - 0x0800);
        fragment.element = source->element;
        fragment.damage = ORB_SEEKER_DAMAGE;
        fragment.dmgFlags = source->dmgFlags;
        fragment.timer = ORB_SEEKER_LIFETIME;
        fragment.isSeeker = true;
        fragment.target = target;
        for (s32 t = 0; t < ORB_TRAIL_LEN; t++) {
            fragment.trailPos[t] = source->pos;
        }
        AddOrb(&fragment);
        if (target != NULL) {
            claimed[claimedCount++] = target;
        }
    }
}

// A fragment wheels onto the enemy it claimed; a plain shot only bends toward what is ahead of it, so one with
// nothing in front flies straight.
static void HomeOrb(PlayState* play, RodOrb* orb) {
    f32 forwardX = Math_SinS(orb->yaw) * Math_CosS(orb->pitch);
    f32 forwardY = -Math_SinS(orb->pitch);
    f32 forwardZ = Math_CosS(orb->yaw) * Math_CosS(orb->pitch);
    Actor* best = NULL;
    f32 bestDistSq = SQ(ORB_HOME_RANGE);

    if (orb->isSeeker && orb->target != NULL && IsActorAlive(play, orb->target)) {
        Vec3f aim = { orb->target->world.pos.x, orb->target->world.pos.y + orb->target->shape.yOffset,
                      orb->target->world.pos.z };
        Math_ScaledStepToS(&orb->yaw, Math_Vec3f_Yaw(&orb->pos, &aim), ORB_SEEKER_TURN);
        Math_ScaledStepToS(&orb->pitch, Math_Vec3f_Pitch(&orb->pos, &aim), ORB_SEEKER_TURN);
        return;
    }
    orb->target = NULL;
    for (Actor* enemy = play->actorCtx.actorLists[ACTORCAT_ENEMY].first; enemy != NULL; enemy = enemy->next) {
        f32 dx = enemy->world.pos.x - orb->pos.x;
        f32 dy = (enemy->world.pos.y + enemy->shape.yOffset) - orb->pos.y;
        f32 dz = enemy->world.pos.z - orb->pos.z;
        f32 distSq = SQ(dx) + SQ(dy) + SQ(dz);
        if (enemy->update == NULL || distSq >= bestDistSq || distSq < 1.0f) {
            continue;
        }
        if (((dx * forwardX + dy * forwardY + dz * forwardZ) / sqrtf(distSq)) < ORB_HOME_CONE) {
            continue;
        }
        bestDistSq = distSq;
        best = enemy;
    }
    if (best == NULL) {
        return;
    }
    Vec3f aim = { best->world.pos.x, best->world.pos.y + best->shape.yOffset, best->world.pos.z };
    s16 turn = orb->isSeeker ? ORB_SEEKER_TURN : ORB_TURN_RATE;
    Math_ScaledStepToS(&orb->yaw, Math_Vec3f_Yaw(&orb->pos, &aim), turn);
    Math_ScaledStepToS(&orb->pitch, Math_Vec3f_Pitch(&orb->pos, &aim), turn);
}

// Quads remember their actor and a new scene brings a new player, so they are rebuilt per scene.
static void PrepareEscortQuads(PlayState* play, Player* player) {
    if (sAreOrbQuadsReady) {
        return;
    }
    for (s32 i = 0; i < ORB_POOL_MAX; i++) {
        Collider_InitQuad(play, &sOrbQuads[i]);
        Collider_SetQuad(play, &sOrbQuads[i], &player->actor, &sEscortQuadInit);
    }
    Collider_InitQuad(play, &sReflectQuad);
    Collider_SetQuad(play, &sReflectQuad, &player->actor, &sEscortQuadInit);
    sAreOrbQuadsReady = true;
    sIsReflectQuadReady = true;
}

static void StampEscortQuad(PlayState* play, ColliderQuad* quad, Vec3f* center, f32 half, u32 dmgFlags, u8 damage) {
    Vec3f a = { center->x - half, center->y + half, center->z };
    Vec3f b = { center->x + half, center->y + half, center->z };
    Vec3f c = { center->x + half, center->y - half, center->z };
    Vec3f d = { center->x - half, center->y - half, center->z };

    Collider_ResetQuadAT(play, &quad->base);
    Collider_SetQuadVertices(quad, &a, &b, &c, &d);
    quad->base.atFlags = AT_ON | AT_TYPE_PLAYER;
    quad->elem.atDmgInfo.dmgFlags = dmgFlags;
    quad->elem.atDmgInfo.damage = damage;
    quad->elem.atElemFlags = ATELEM_ON | ATELEM_NEAREST;
    CollisionCheck_SetAT(play, &play->colChkCtx, &quad->base);
}

static void RecordOrbTrail(RodOrb* orb) {
    orb->trailIndex = (orb->trailIndex + 1) % ORB_TRAIL_LEN;
    orb->trailPos[orb->trailIndex] = orb->pos;
}

// A plain shot pierces; the level-3 ball breaks on its first hit, or at the end of its flight regardless.
static void UpdateOrbs(PlayState* play, Player* player) {
    for (s32 i = 0; i < ORB_POOL_MAX; i++) {
        RodOrb* orb = &sOrbs[i];

        if (!orb->isActive) {
            continue;
        }
        if (sAreOrbQuadsReady && (sOrbQuads[i].base.atFlags & AT_HIT)) {
            sOrbQuads[i].base.atFlags &= ~AT_HIT;
            if (orb->bursts) {
                BurstOrb(play, orb);
                orb->isActive = false;
                continue;
            }
        }
        HomeOrb(play, orb);
        f32 speed = orb->isSeeker ? ORB_SEEKER_SPEED : ROD_ORB_SPEED;
        f32 cosPitch = Math_CosS(orb->pitch);
        orb->pos.x += Math_SinS(orb->yaw) * cosPitch * speed;
        orb->pos.y += -Math_SinS(orb->pitch) * speed;
        orb->pos.z += Math_CosS(orb->yaw) * cosPitch * speed;
        if (orb->isSeeker) {
            RecordOrbTrail(orb);
        }
        if ((orb->timer & 3) == 0) {
            Vec3f zero = { 0.0f, 0.0f, 0.0f };

            SpawnOrbWake(play, &orb->pos, &zero, ORB_WAKE_SCALE, orb->element);
        }
        if (--orb->timer <= 0) {
            if (orb->bursts) {
                BurstOrb(play, orb);
            }
            orb->isActive = false;
            continue;
        }
        PrepareEscortQuads(play, player);
        StampEscortQuad(play, &sOrbQuads[i], &orb->pos, 8.0f, orb->dmgFlags | DMG_SWORD, orb->damage);
    }
}

// ---- reflected shots ----

// There is no "projectile" flag: nearly every shot is something you cannot lock onto.
static bool IsRangedAttacker(Actor* actor) {
    if (actor == NULL) {
        return false;
    }
    return actor->category == ACTORCAT_EXPLOSIVES || !(actor->flags & ACTOR_FLAG_ATTENTION_ENABLED);
}

static void ReflectShot(Player* player, Actor* shot) {
    shot->world.rot.y += 0x8000;
    shot->shape.rot.y = shot->world.rot.y;
    shot->velocity.x = -shot->velocity.x;
    shot->velocity.z = -shot->velocity.z;
    shot->speed = MAX(shot->speed, REFLECT_MIN_SPEED);
    shot->world.pos.x += Math_SinS(shot->world.rot.y) * REFLECT_PUSH_OUT;
    shot->world.pos.z += Math_CosS(shot->world.rot.y) * REFLECT_PUSH_OUT;
    sReflectedShot = shot;
    sReflectTimer = REFLECT_FRAMES;
    player->invincibilityTimer = REFLECT_INVULNERABILITY;
    Audio_PlaySfx_AtPos(&player->actor.world.pos, NA_SE_IT_SHIELD_REFLECT_SW);
}

static bool IsShotIncoming(Actor* actor, Vec3f* chest) {
    f32 dx = chest->x - actor->world.pos.x;
    f32 dy = chest->y - actor->world.pos.y;
    f32 dz = chest->z - actor->world.pos.z;
    f32 velX = actor->velocity.x + Math_SinS(actor->world.rot.y) * actor->speed;
    f32 velZ = actor->velocity.z + Math_CosS(actor->world.rot.y) * actor->speed;

    if (SQ(dx) + SQ(dy) + SQ(dz) > SQ(GUARD_CATCH_RADIUS)) {
        return false;
    }
    return SQ(velX) + SQ(velZ) >= SQ(GUARD_CATCH_MIN_SPEED) && (velX * dx + velZ * dz) > 0.0f;
}

// Every category is swept: a shot can recategorise itself in its own init, as the Octorok's rock does.
static bool TryReflectIncoming(PlayState* play, Player* player) {
    Vec3f chest = { player->actor.world.pos.x, player->actor.world.pos.y + GUARD_CATCH_HEIGHT,
                    player->actor.world.pos.z };

    for (s32 category = 0; category < ACTORCAT_MAX; category++) {
        if (category == ACTORCAT_PLAYER || category == ACTORCAT_BG || category == ACTORCAT_DOOR ||
            category == ACTORCAT_CHEST) {
            continue;
        }
        for (Actor* actor = play->actorCtx.actorLists[category].first; actor != NULL; actor = actor->next) {
            if (actor->update == NULL || actor == sReflectedShot || !IsRangedAttacker(actor) ||
                !IsShotIncoming(actor, &chest)) {
                continue;
            }
            ReflectShot(player, actor);
            return true;
        }
    }
    return false;
}

// The shot keeps its own look and collider; what hurts on the way back is a sword quad escorting it.
static void EscortReflectedShot(PlayState* play, Player* player) {
    if (sReflectTimer <= 0) {
        return;
    }
    sReflectTimer--;
    if (!IsActorAlive(play, sReflectedShot)) {
        sReflectedShot = NULL;
        sReflectTimer = 0;
        return;
    }
    PrepareEscortQuads(play, player);
    StampEscortQuad(play, &sReflectQuad, &sReflectedShot->world.pos, REFLECT_HALF, DMG_SWORD, REFLECT_DAMAGE);
}

// ---- moves ----

static void StartMove(PlayState* play, Player* player, u8 move) {
    if (player->actionFunc != GaroAction) {
        Player_SetAction(play, player, GaroAction, 0);
    }
    sMove = move;
    sMoveTimer = 0;
    sHasStruck = false;
    if (WantsTrails()) {
        SpawnTrails(play);
    }
}

// Ending a free spin keeps the heading he was travelling; one done on the spot hands back the entry facing.
static void SettleSpinFacing(Player* player) {
    player->actor.shape.rot.y = player->speedXZ > 0.5f ? player->yaw : sSpinEntryYaw;
    player->yaw = player->actor.shape.rot.y;
}

static void ClearMove(PlayState* play, Player* player) {
    if (sMove == GARO_MOVE_SPIN) {
        SettleSpinFacing(player);
    }
    if (sMove == GARO_MOVE_ROD_AIM) {
        Z64Aiming_Release(player, play);
        player->upperLimbRot.x = 0;
        player->upperLimbRot.y = 0;
    }
    sMove = GARO_MOVE_NONE;
    sMoveTimer = 0;
    sParryAttacker = NULL;
    sBanishTarget = NULL;
    sStrikeTailFrames = 0;
    sHopAirFrames = 0;
    player->stateFlags2 &= ~(STATE2_DISABLE_DRAW | STATE2_NO_ROTATION);
    player->actor.gravity = JUMP_GRAVITY;
    StopStriking(player);
    KillTrails(play);
}

static void EndMove(PlayState* play, Player* player) {
    ClearMove(play, player);
    player->speedXZ = 0.0f;
    func_8083B1A0(player, play);
}

static void Freeze(Player* player) {
    player->speedXZ = 0.0f;
    player->actor.speed = 0.0f;
    player->actor.velocity.x = 0.0f;
    player->actor.velocity.z = 0.0f;
}

// Airborne moves keep their heading: `yaw` is what the player copies into world.rot.y each frame.
static void KeepAirborne(Player* player) {
    player->actor.gravity = JUMP_GRAVITY;
    player->yaw = sHopYaw;
}

static bool HasLanded(Player* player) {
    sHopAirFrames++;
    return IsGrounded(player) && sHopAirFrames >= HOP_MIN_AIR_FRAMES;
}

// -- spin and rod --

static void StartSpin(PlayState* play, Player* player) {
    sSpinEntryYaw = player->actor.shape.rot.y;
    sBHeldFrames = 0;
    StartMove(play, player, GARO_MOVE_SPIN);
    PlayGaroAnim(play, player, ANIM_SPIN, 0.0f, -1.0f, SPIN_PLAY_SPEED);
    Player_PlaySfx(player, NA_SE_IT_SWORD_SWING_HARD);
}

static void ResetRodCharge(void) {
    sRodCharge = 0;
    sIsRodChimed = false;
    sRodBallCircle = 0.0f;
    sRodBallScale = 0.0f;
    sRodBallRays = 0;
}

static void StartRodAim(PlayState* play, Player* player) {
    StopStriking(player);
    SettleSpinFacing(player);
    ResetRodCharge();
    KillTrails(play);
    sMove = GARO_MOVE_ROD_AIM;
    sMoveTimer = 0;
    player->speedXZ = 0.0f;
    PlayGaroAnim(play, player, ANIM_TAKE_OUT_BOMB, 0.0f, -1.0f, 1.0f);
    Z64Aiming_Request(player, play);
}

// The spin's shape turns on the timer, never as a rate: Z-target keeps dragging it toward the lock-on. Where he
// travels is `yaw`, the field the player copies into world.rot.y.
static void RunSpin(PlayState* play, Player* player) {
    player->stateFlags2 |= STATE2_NO_ROTATION;
    if (IsHeld(play, BTN_B) && ++sBHeldFrames >= B_HOLD_FRAMES) {
        StartRodAim(play, player);
        return;
    }
    sMoveTimer++;
    player->actor.shape.rot.y = (s16)(sSpinEntryYaw + sMoveTimer * SPIN_YAW_RATE);
    f32 stick = GetStickMagnitude(play);
    if (stick > 0.1f) {
        player->yaw = GetStickYaw(play);
        player->speedXZ = stick * SPIN_MOVE_SPEED;
    } else {
        player->speedXZ = 0.0f;
    }
    StrikeAhead(play, player, 35.0f, 55.0f, SPIN_DAMAGE);
    if (AdvanceGaroAnim(play, player)) {
        PlayGaroAnim(play, player, ANIM_SPIN, 0.0f, -1.0f, SPIN_PLAY_SPEED);
    }
    if (sMoveTimer >= SPIN_FRAMES) {
        EndMove(play, player);
    }
}

// The ball steps a size per damage tier, so what it looks like is what the shot will do.
static void GrowRodBall(void) {
    u8 level = GetRodLevel(sRodCharge);
    f32 fraction = sRodLevelScale[level - 1];
    s16 wantRays = sRodLevelRays[level - 1];

    Math_ApproachF(&sRodBallCircle, ROD_BALL_CIRCLE_MAX * fraction, 0.3f, 0.01f);
    Math_ApproachF(&sRodBallScale, ROD_BALL_SCALE_MAX * fraction, 0.3f, 1.0f);
    if ((sMoveTimer & 3) == 0 && sRodBallRays != wantRays) {
        sRodBallRays += sRodBallRays < wantRays ? 1 : -1;
    }
}

static void CycleRodElement(PlayState* play, Player* player) {
    if (IsPressed(play, BTN_L)) {
        sRodElement = (sRodElement + ELEMENT_COUNT - 1) % ELEMENT_COUNT;
        Player_PlaySfx(player, NA_SE_SY_DECIDE);
    }
    if (IsPressed(play, BTN_R)) {
        sRodElement = (sRodElement + 1) % ELEMENT_COUNT;
        Player_PlaySfx(player, NA_SE_SY_DECIDE);
    }
}

// A cancels the aim as it does the Deku bubble's; letting go of B fires.
static void RunRodAim(PlayState* play, Player* player) {
    Z64Aiming_Update(player, play);
    if (IsPressed(play, BTN_A)) {
        sRodCooldown = ROD_RELEASE_COOLDOWN;
        EndMove(play, player);
        return;
    }
    if (!IsHeld(play, BTN_B)) {
        FireRodOrb(play, player);
        sRodCooldown = ROD_RELEASE_COOLDOWN;
        EndMove(play, player);
        return;
    }
    sMoveTimer++;
    if (sRodCharge < ROD_CHARGE_MAX) {
        sRodCharge++;
        Actor_PlaySfx_Flagged(&player->actor, GetChargeSfx(sRodElement));
    }
    GrowRodBall();
    if (!sIsRodChimed && sRodCharge >= ROD_CHARGE_TIER3) {
        Player_PlaySfx(player, NA_SE_SY_SYNTH_MAGIC_ARROW);
        sIsRodChimed = true;
    }
    CycleRodElement(play, player);
    AdvanceGaroAnim(play, player);
}

// -- guard and counter --

static void StartGuardReturn(PlayState* play, Player* player) {
    sMove = GARO_MOVE_GUARD_RETURN;
    sMoveTimer = 0;
    PlayGaroAnim(play, player, ANIM_GUARD, GetAnimLastFrame(ANIM_GUARD) * GUARD_HOLD_FRACTION, 0.0f,
                 -GUARD_RETURN_SPEED);
}

// The attacker freezes, Garo appears behind it from wherever he stood and opens with the jump attack's swing.
static void StartRiposte(PlayState* play, Player* player, Actor* attacker) {
    if (attacker != NULL) {
        s16 yaw = attacker->shape.rot.y;

        MarkStunned(play, attacker, PARRY_FREEZE_FRAMES);
        player->actor.world.pos.x = attacker->world.pos.x - Math_SinS(yaw) * RIPOSTE_OFFSET;
        player->actor.world.pos.z = attacker->world.pos.z - Math_CosS(yaw) * RIPOSTE_OFFSET;
        player->actor.world.pos.y = attacker->world.pos.y;
        player->actor.shape.rot.y = yaw;
        player->yaw = yaw;
    }
    sParryAttacker = attacker;
    StartMove(play, player, GARO_MOVE_RIPOSTE);
    PlayGaroAnim(play, player, ANIM_DRAW_SWORDS, 0.0f, -1.0f, 1.0f);
    Player_PlaySfx(player, NA_SE_PL_MAGIC_WIND_WARP);
}

// A shooter across the room is not what touched him: its shot goes back and the stance ends, with no chase.
static void AnswerHit(PlayState* play, Player* player, Actor* attacker) {
    f32 distSq = attacker == NULL ? 0.0f
                                  : SQ(attacker->world.pos.x - player->actor.world.pos.x) +
                                        SQ(attacker->world.pos.z - player->actor.world.pos.z);

    if (IsRangedAttacker(attacker) || distSq > SQ(GUARD_MELEE_RANGE)) {
        if (IsRangedAttacker(attacker)) {
            ReflectShot(player, attacker);
        }
        StartGuardReturn(play, player);
        return;
    }
    StartRiposte(play, player, attacker);
}

static void RunGuard(PlayState* play, Player* player) {
    Freeze(player);
    AdvanceGaroAnim(play, player);
    if (TryReflectIncoming(play, player)) {
        StartGuardReturn(play, player);
        return;
    }
    if (sIsParryPending) {
        Actor* attacker = IsActorAlive(play, sPendingParry) ? sPendingParry : player->focusActor;

        sIsParryPending = false;
        sPendingParry = NULL;
        AnswerHit(play, player, attacker);
        return;
    }
    sMoveTimer++;
    if (!IsHeld(play, BTN_R)) {
        StartGuardReturn(play, player);
    }
}

static void RunGuardReturn(PlayState* play, Player* player) {
    Freeze(player);
    if (AdvanceGaroAnim(play, player)) {
        EndMove(play, player);
    }
}

static void ThawBeforeStrike(PlayState* play, Actor* target, s16 strikeFrame, s16 lead) {
    if (sMoveTimer >= strikeFrame - lead && IsActorAlive(play, target)) {
        target->freezeTimer = 0;
    }
}

static void RunRiposte(PlayState* play, Player* player) {
    Freeze(player);
    ThawBeforeStrike(play, sParryAttacker, PARRY_STRIKE_FRAME, PARRY_THAW_LEAD);
    if (sMoveTimer >= PARRY_STRIKE_FRAME) {
        StrikeAround(play, player, sMoveTimer - PARRY_STRIKE_FRAME, PARRY_DAMAGE);
        if (!sHasStruck) {
            sHasStruck = true;
            Player_PlaySfx(player, NA_SE_IT_SWORD_SWING_HARD);
        }
    }
    bool isDone = AdvanceGaroAnim(play, player);
    sMoveTimer++;
    if (isDone) {
        EndMove(play, player);
    }
}

// -- dash --

static void RunDash(PlayState* play, Player* player) {
    PlayerAnimationHeader* dash = sAnims[ANIM_DASH];

    if (dash != NULL && player->skelAnime.animation != dash) {
        PlayerAnimation_Change(play, &player->skelAnime, dash, 1.0f, 0.0f, Animation_GetLastFrame(dash), ANIMMODE_LOOP,
                               -4.0f);
    }
    if (GetStickMagnitude(play) > 0.3f) {
        Math_ScaledStepToS(&player->yaw, GetStickYaw(play), DASH_TURN);
    }
    player->actor.shape.rot.y = player->yaw;
    player->speedXZ = DASH_SPEED;
    StrikeAhead(play, player, 25.0f, 60.0f, DASH_DAMAGE);
    AdvanceGaroAnim(play, player);
    sMoveTimer++;
    if ((player->actor.bgCheckFlags & BGCHECKFLAG_WALL) || !IsHeld(play, BTN_A)) {
        EndMove(play, player);
    }
}

// -- banish --

static void StartBanish(PlayState* play, Player* player, Actor* target) {
    StartMove(play, player, GARO_MOVE_BANISH_VANISH);
    PlayGaroAnim(play, player, ANIM_COLLAPSE, 0.0f, -1.0f, 1.0f);
    sBanishTarget = target;
    sBanishCooldown = BANISH_COOLDOWN;
    Player_PlaySfx(player, NA_SE_PL_MAGIC_WIND_WARP);
    if (target != NULL && target->category == ACTORCAT_ENEMY) {
        MarkStunned(play, target, BANISH_STUN_FRAMES);
    }
}

// With nothing locked the shadow ball still crosses a short way ahead, and he reappears where he stood.
static void LaunchShadowBall(PlayState* play, Player* player) {
    Actor* target = sBanishTarget;

    sShadowStart = player->actor.world.pos;
    sShadowStart.y += 30.0f;
    if (IsActorAlive(play, target)) {
        sShadowEnd = target->world.pos;
    } else {
        sShadowEnd = player->actor.world.pos;
        sShadowEnd.x += Math_SinS(player->actor.shape.rot.y) * BANISH_NO_TARGET_REACH;
        sShadowEnd.z += Math_CosS(player->actor.shape.rot.y) * BANISH_NO_TARGET_REACH;
    }
    sShadowEnd.y += 30.0f;
    sShadowTimer = 0;
    sMove = GARO_MOVE_BANISH_SHADOW;
    Player_PlaySfx(player, NA_SE_PL_MAGIC_WIND_WARP);
}

static void RunBanishVanish(PlayState* play, Player* player) {
    Freeze(player);
    if (sMoveTimer < BANISH_VANISH_FRAMES) {
        player->actor.world.pos.y -= 1.0f;
    } else {
        player->stateFlags2 |= STATE2_DISABLE_DRAW;
    }
    bool isDone = AdvanceGaroAnim(play, player);
    sMoveTimer++;
    if (isDone || sMoveTimer >= BANISH_VANISH_FRAMES) {
        LaunchShadowBall(play, player);
    }
}

static void DropShadowWake(PlayState* play) {
    f32 backX = sShadowStart.x - sShadowEnd.x;
    f32 backZ = sShadowStart.z - sShadowEnd.z;
    f32 length = sqrtf(SQ(backX) + SQ(backZ));
    Vec3f zero = { 0.0f, 0.0f, 0.0f };
    Color_RGBA8 prim = { 110, 40, 190, 220 };
    Color_RGBA8 env = { 25, 0, 60, 0 };

    if (length > 0.001f) {
        backX = backX / length * 10.0f;
        backZ = backZ / length * 10.0f;
    }
    for (s32 i = 0; i < 2; i++) {
        Vec3f pos = { sShadowPos.x + backX + Rand_CenteredFloat(8.0f), sShadowPos.y + Rand_CenteredFloat(8.0f),
                      sShadowPos.z + backZ + Rand_CenteredFloat(8.0f) };
        EffectSsDust_Spawn(play, 0, &pos, &zero, &zero, &prim, &env, 90, -6, 8, 0);
    }
}

// The warp hum is a loop, asked for every frame of the crossing so it ends with it.
static void RunBanishShadow(PlayState* play, Player* player) {
    f32 t = MIN((f32)sShadowTimer / (f32)BANISH_SHADOW_FRAMES, 1.0f);

    Freeze(player);
    player->stateFlags2 |= STATE2_DISABLE_DRAW;
    sShadowPos.x = sShadowStart.x + (sShadowEnd.x - sShadowStart.x) * t;
    sShadowPos.y = sShadowStart.y + (sShadowEnd.y - sShadowStart.y) * t;
    sShadowPos.z = sShadowStart.z + (sShadowEnd.z - sShadowStart.z) * t;
    Actor_PlaySfx_Flagged(&player->actor, NA_SE_PL_MAGIC_WIND_WARP);
    DropShadowWake(play);
    if (++sShadowTimer < BANISH_SHADOW_FRAMES) {
        return;
    }
    if (IsActorAlive(play, sBanishTarget)) {
        s16 yaw = sBanishTarget->shape.rot.y;

        player->actor.world.pos.x = sBanishTarget->world.pos.x - Math_SinS(yaw) * BANISH_OFFSET;
        player->actor.world.pos.z = sBanishTarget->world.pos.z - Math_CosS(yaw) * BANISH_OFFSET;
        player->actor.world.pos.y = sBanishTarget->world.pos.y;
        player->actor.shape.rot.y = yaw;
        player->yaw = yaw;
    }
    player->stateFlags2 &= ~STATE2_DISABLE_DRAW;
    Player_PlaySfx(player, NA_SE_PL_MAGIC_WIND_WARP);
    sMove = GARO_MOVE_SHADOW_STRIKE;
    sMoveTimer = 0;
    sHasStruck = false;
    SpawnTrails(play);
    PlayGaroAnim(play, player, ANIM_APPEAR_DRAW_SWORDS, 0.0f, -1.0f, SHADOW_STRIKE_PLAY_SPEED);
}

// He never leaves before the strike has had its window: the fast arrival clip ends before it otherwise.
static void RunShadowStrike(PlayState* play, Player* player) {
    Freeze(player);
    player->invincibilityTimer = MAX(player->invincibilityTimer, 5);
    ThawBeforeStrike(play, sBanishTarget, SHADOW_STRIKE_FRAME, BANISH_THAW_LEAD);
    s16 live = sMoveTimer - SHADOW_STRIKE_FRAME;
    if (live >= 0 && live <= SHADOW_STRIKE_WINDOW) {
        StrikeWallAhead(play, player, live, SHADOW_STRIKE_DAMAGE);
        if (!sHasStruck) {
            sHasStruck = true;
            Player_PlaySfx(player, NA_SE_IT_SWORD_SWING_HARD);
        }
    } else {
        StopStriking(player);
    }
    bool isDone = AdvanceGaroAnim(play, player);
    sMoveTimer++;
    if (isDone && live > SHADOW_STRIKE_WINDOW) {
        EndMove(play, player);
    }
}

// -- leaps --

static void StartLeap(PlayState* play, Player* player, u8 move, s32 anim, s16 yaw, f32 xzSpeed, f32 ySpeed) {
    StartMove(play, player, move);
    PlayGaroAnim(play, player, anim, 0.0f, -1.0f, move == GARO_MOVE_JUMP_ATTACK ? 1.5f : 1.0f);
    sHopYaw = yaw;
    sHopAirFrames = 0;
    player->yaw = yaw;
    player->speedXZ = xzSpeed;
    player->actor.velocity.y = ySpeed;
    player->actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND;
    Player_PlaySfx(player, NA_SE_VO_LI_AUTO_JUMP);
}

static void StartLandStrike(PlayState* play, Player* player) {
    sMove = GARO_MOVE_LAND_STRIKE;
    sMoveTimer = 0;
    sHasStruck = false;
    sStrikeTailFrames = 0;
    SpawnTrails(play);
    PlayGaroAnim(play, player, ANIM_DRAW_SWORDS, 0.0f, -1.0f, 1.0f);
}

static void StartAirSlash(PlayState* play, Player* player) {
    if (player->actionFunc != GaroAction) {
        sHopYaw = player->yaw;
        sHopAirFrames = 0;
    }
    StartMove(play, player, GARO_MOVE_AIR_SLASH);
    PlayGaroAnim(play, player, ANIM_SLASH_LOOP, 0.0f, -1.0f, 1.0f);
    Player_PlaySfx(player, NA_SE_IT_SWORD_SWING);
}

// Hops and the back flip keep facing the lock-on while `yaw` carries them sideways or back.
static void RunHop(PlayState* play, Player* player) {
    player->stateFlags2 |= STATE2_NO_ROTATION;
    KeepAirborne(player);
    AdvanceGaroAnim(play, player);
    if (HasLanded(player)) {
        EndMove(play, player);
    }
}

// The leap turns into the air slash past its apex, and a short hop lands straight into the strike.
static void RunJumpAttack(PlayState* play, Player* player) {
    KeepAirborne(player);
    AdvanceGaroAnim(play, player);
    if (HasLanded(player)) {
        StartLandStrike(play, player);
    } else if (sHopAirFrames >= JUMP_ATTACK_SLASH_FRAME) {
        StartAirSlash(play, player);
    }
}

static void RunAirSlash(PlayState* play, Player* player) {
    KeepAirborne(player);
    if (AdvanceGaroAnim(play, player)) {
        PlayGaroAnim(play, player, ANIM_SLASH_LOOP, 0.0f, -1.0f, 1.0f);
    }
    sMoveTimer++;
    if (IsGrounded(player) && sMoveTimer >= 2) {
        Player_PlaySfx(player, NA_SE_PL_ROLL_DUST);
        StartLandStrike(play, player);
    }
}

static void RunLandStrike(PlayState* play, Player* player) {
    Freeze(player);
    if (sMoveTimer >= LAND_STRIKE_FRAME) {
        StrikeAround(play, player, sMoveTimer - LAND_STRIKE_FRAME, LAND_STRIKE_DAMAGE);
        if (!sHasStruck) {
            sHasStruck = true;
            Player_PlaySfx(player, NA_SE_IT_SWORD_SWING_HARD);
        }
    }
    bool isDone = AdvanceGaroAnim(play, player);
    sMoveTimer++;
    if (isDone && ++sStrikeTailFrames > LAND_STRIKE_TAIL) {
        EndMove(play, player);
    }
}

// -- laugh --

static void RunLaugh(PlayState* play, Player* player) {
    bool isDone = AdvanceGaroAnim(play, player);
    bool wantsToMove =
        GetStickMagnitude(play) > 0.1f || IsPressed(play, BTN_A) || IsPressed(play, BTN_B) || IsPressed(play, BTN_R);

    if (isDone || wantsToMove) {
        EndMove(play, player);
    }
}

static void GaroAction(Player* player, PlayState* play) {
    player->stateFlags2 |= STATE2_NO_Z_TARGET_ROTATION;
    switch (sMove) {
        case GARO_MOVE_SPIN:
            RunSpin(play, player);
            break;
        case GARO_MOVE_ROD_AIM:
            RunRodAim(play, player);
            break;
        case GARO_MOVE_GUARD:
            RunGuard(play, player);
            break;
        case GARO_MOVE_GUARD_RETURN:
            RunGuardReturn(play, player);
            break;
        case GARO_MOVE_RIPOSTE:
            RunRiposte(play, player);
            break;
        case GARO_MOVE_DASH:
            RunDash(play, player);
            break;
        case GARO_MOVE_BANISH_VANISH:
            RunBanishVanish(play, player);
            break;
        case GARO_MOVE_BANISH_SHADOW:
            RunBanishShadow(play, player);
            break;
        case GARO_MOVE_SHADOW_STRIKE:
            RunShadowStrike(play, player);
            break;
        case GARO_MOVE_LAUGH:
            RunLaugh(play, player);
            break;
        case GARO_MOVE_HOP:
        case GARO_MOVE_BACKFLIP:
            RunHop(play, player);
            break;
        case GARO_MOVE_JUMP_ATTACK:
            RunJumpAttack(play, player);
            break;
        case GARO_MOVE_AIR_SLASH:
            RunAirSlash(play, player);
            break;
        case GARO_MOVE_LAND_STRIKE:
            RunLandStrike(play, player);
            break;
        default:
            EndMove(play, player);
            break;
    }
}

// ---- action overrides ----

static bool CanStartMove(Player* player) {
    return IsGaro() && !IsGaroBusy(player) && CanAct(player) && sRodCooldown == 0;
}

static int32_t HandleSlash(PlayState* play, Player* player) {
    if (!IsPressed(play, BTN_B)) {
        return S2H_FORM_ACTION_VANILLA;
    }
    if (!CanStartMove(player) || !IsGrounded(player)) {
        return S2H_FORM_ACTION_BLOCKED;
    }
    StartSpin(play, player);
    return S2H_FORM_ACTION_STARTED;
}

// Z-targeted A falls through to the next handler, which is where the hops live.
static int32_t HandleDash(PlayState* play, Player* player) {
    if (!IsGaro() || !IsPressed(play, BTN_A) || Player_IsZTargeting(player)) {
        return S2H_FORM_ACTION_VANILLA;
    }
    if (!CanStartMove(player) || !IsGrounded(player)) {
        return S2H_FORM_ACTION_BLOCKED;
    }
    StartMove(play, player, GARO_MOVE_DASH);
    return S2H_FORM_ACTION_STARTED;
}

static void StartZTargetMove(PlayState* play, Player* player, s8 direction) {
    s16 facing = player->actor.shape.rot.y;

    if (direction == PLAYER_STICK_DIR_BACKWARD) {
        StartLeap(play, player, GARO_MOVE_BACKFLIP, ANIM_JUMP_BACK, facing + 0x8000, 12.0f, 5.8f);
    } else if (direction == PLAYER_STICK_DIR_LEFT || direction == PLAYER_STICK_DIR_RIGHT) {
        s16 side = direction == PLAYER_STICK_DIR_LEFT ? -0x4000 : 0x4000;
        StartLeap(play, player, GARO_MOVE_HOP, ANIM_BOUNCE, facing + side, 12.75f, 4.5f);
    } else if (direction == PLAYER_STICK_DIR_FORWARD) {
        StartLeap(play, player, GARO_MOVE_JUMP_ATTACK, ANIM_APPEAR, facing, MAX(player->speedXZ, 10.0f), 7.5f);
    } else {
        StartBanish(play, player, player->focusActor);
    }
}

// Standing still with the stick forward, or neutral while the banish recharges, Z+A does nothing, as in NEI.
static int32_t HandleZTargetA(PlayState* play, Player* player) {
    s8 direction = player->controlStickDirections[player->controlStickDataIndex];
    bool isStillForward = direction == PLAYER_STICK_DIR_FORWARD && player->speedXZ <= 0.5f;
    bool isBanishSpent = direction == PLAYER_STICK_DIR_NONE && sBanishCooldown > 0;

    if (!IsGaro() || !IsPressed(play, BTN_A)) {
        return S2H_FORM_ACTION_VANILLA;
    }
    if (!CanStartMove(player) || !IsGrounded(player) || isStillForward || isBanishSpent) {
        return S2H_FORM_ACTION_BLOCKED;
    }
    StartZTargetMove(play, player, direction);
    return S2H_FORM_ACTION_STARTED;
}

// The guard rises halfway through its clip and holds there; the second half plays backwards on release.
static int32_t HandleGuard(PlayState* play, Player* player) {
    if (!IsGaro() || !IsHeld(play, BTN_R) || IsHeld(play, BTN_B)) {
        return S2H_FORM_ACTION_VANILLA;
    }
    if (!CanStartMove(player) || !IsGrounded(player)) {
        return S2H_FORM_ACTION_BLOCKED;
    }
    StartMove(play, player, GARO_MOVE_GUARD);
    PlayGaroAnim(play, player, ANIM_GUARD, 0.0f, GetAnimLastFrame(ANIM_GUARD) * GUARD_HOLD_FRACTION, 1.0f);
    sIsParryPending = false;
    sPendingParry = NULL;
    return S2H_FORM_ACTION_STARTED;
}

// Garo cannot lift anything: an offer with no item attached is a carry, and it is refused.
static int32_t RefuseCarry(PlayState* play, Player* player) {
    if (!IsGaro() || player->interactRangeActor == NULL || player->getItemId != GI_NONE) {
        return S2H_FORM_ACTION_VANILLA;
    }
    return S2H_FORM_ACTION_BLOCKED;
}

// ---- incoming damage ----

// Runs before the player consumes the hit: Garo takes it doubled, and the guard takes it without the knockback
// so its counter can answer.
static void HandleIncomingHit(Player* player, Input* input) {
    u8 damage = player->actor.colChkInfo.damage;

    if (!IsGaro() || damage == 0) {
        sScaledDamage = 0;
        return;
    }
    if (!(player->cylinder.base.acFlags & AC_HIT)) {
        return;
    }
    if (damage != sScaledDamage) {
        sScaledDamage = (u8)MIN(damage * 2, 0xFF);
        player->actor.colChkInfo.damage = sScaledDamage;
    }
    if (sMove != GARO_MOVE_GUARD || gPlayState == NULL) {
        return;
    }
    func_808339D4(gPlayState, player, -player->actor.colChkInfo.damage);
    sPendingParry = player->cylinder.base.ac;
    sIsParryPending = true;
    player->actor.colChkInfo.damage = 0;
    player->cylinder.base.acFlags &= ~AC_HIT;
    sScaledDamage = 0;
}

// Strikes hit for exactly their number whatever the enemy's table says; the orbs keep the table, since their
// element is what finds a weakness.
static void FixStrikeDamage(Actor* victim, ColliderElement* attack, f32* damage) {
    if (!IsGaro() || gPlayState == NULL) {
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    bool isStrike = sMove != GARO_MOVE_NONE && attack == &player->meleeWeaponQuads[0].elem;
    bool isReflect = sIsReflectQuadReady && attack == &sReflectQuad.elem;

    if (isStrike || isReflect) {
        *damage = attack->atDmgInfo.damage;
    }
}

// ---- world ----

// A Garo runs across water: while he is out of it and not rising, the surface is his floor.
static void WalkOnWater(CollisionContext* colCtx, Vec3f* pos, Actor* actor, CollisionPoly** poly, s32* bgId,
                        f32* floorY) {
    PlayState* play = gPlayState;
    WaterBox* waterBox;
    f32 surface;

    if (play == NULL || !IsGaro() || actor != &GET_PLAYER(play)->actor) {
        return;
    }
    Player* player = (Player*)actor;
    if ((player->stateFlags1 & STATE1_SWIMMING) || actor->velocity.y > 0.0f) {
        return;
    }
    if (!WaterBox_GetSurface1(play, colCtx, pos->x, pos->z, &surface, &waterBox) || surface <= *floorY) {
        return;
    }
    *floorY = surface;
}

// He sees what the Lens shows without its circle: lensActive would bring the overlay, which is what masks the
// reveal. In a hiding room the lens actors lose their flag and draw like any other; elsewhere they are fakes.
static void RevealHidden(PlayState* play) {
    if (play->roomCtx.curRoom.lensMode != LENS_MODE_HIDE_ACTORS) {
        return;
    }
    for (s32 category = 0; category < ACTORCAT_MAX; category++) {
        for (Actor* actor = play->actorCtx.actorLists[category].first; actor != NULL; actor = actor->next) {
            if (!(actor->flags & ACTOR_FLAG_REACT_TO_LENS) || sRevealedCount >= REVEALED_MAX) {
                continue;
            }
            actor->flags &= ~ACTOR_FLAG_REACT_TO_LENS;
            sRevealed[sRevealedCount++] = actor;
        }
    }
}

static void ConcealRevealed(PlayState* play) {
    for (s32 i = 0; i < sRevealedCount; i++) {
        if (IsActorAlive(play, sRevealed[i])) {
            sRevealed[i]->flags |= ACTOR_FLAG_REACT_TO_LENS;
        }
    }
    sRevealedCount = 0;
}

// The real Lens still does its own job on top.
static void HideFakes(Actor* actor, bool* shouldDraw) {
    PlayState* play = gPlayState;

    if (!IsGaro() || !(actor->flags & ACTOR_FLAG_REACT_TO_LENS) || play->actorCtx.lensActive ||
        play->roomCtx.curRoom.lensMode == LENS_MODE_HIDE_ACTORS || actor->room != play->roomCtx.curRoom.num) {
        return;
    }
    *shouldDraw = false;
}

static bool IsHoldingOcarina(Player* player) {
    return player->rightHandType == PLAYER_MODELTYPE_RH_INSTRUMENT;
}

static void ReleaseChantVoice(void) {
    if (sIsInstrumentLent) {
        AudioOcarina_SetInstrument(OCARINA_INSTRUMENT_OFF);
        sIsInstrumentLent = false;
    }
}

// Igos du Ikana's chant, the voice MM gives the Elegy. The ocarina stays out across the message session, so the
// latch only lets go once no message is up; the instrument is re-applied while it is open, since the message
// system sets it again around opening.
static void UpdateOcarinaVoice(PlayState* play, Player* player) {
    bool isSessionOpen = play->msgCtx.msgMode != MSGMODE_NONE;

    if (IsHoldingOcarina(player)) {
        sIsOcarinaOut = true;
    } else if (!isSessionOpen) {
        sIsOcarinaOut = false;
    }
    if (!sIsOcarinaOut) {
        ReleaseChantVoice();
        return;
    }
    if (isSessionOpen) {
        AudioOcarina_SetInstrument(OCARINA_INSTRUMENT_IKANA_KING);
        sIsInstrumentLent = true;
    }
}

// Any enemy that vanished since last frame counts as a kill for the taunt: a despawn only costs a dice roll.
static void WatchForKills(PlayState* play) {
    Actor* current[ENEMY_SNAPSHOT_MAX];
    s32 count = 0;

    for (Actor* enemy = play->actorCtx.actorLists[ACTORCAT_ENEMY].first; enemy != NULL && count < ENEMY_SNAPSHOT_MAX;
         enemy = enemy->next) {
        current[count++] = enemy;
    }
    for (s32 i = 0; i < sEnemySnapshotCount && count > 0 && !sIsLaughPending; i++) {
        bool isStillThere = false;
        for (s32 j = 0; j < count; j++) {
            isStillThere = isStillThere || current[j] == sEnemySnapshot[i];
        }
        sIsLaughPending = !isStillThere;
    }
    memcpy(sEnemySnapshot, current, sizeof(Actor*) * count);
    sEnemySnapshotCount = count;
}

static void TryLaugh(PlayState* play, Player* player) {
    if (!sIsLaughPending || IsGaroBusy(player) || !CanAct(player) || !IsGrounded(player)) {
        return;
    }
    sIsLaughPending = false;
    if (Rand_ZeroOne() >= LAUGH_CHANCE || sAnims[ANIM_LAUGH] == NULL) {
        return;
    }
    StartMove(play, player, GARO_MOVE_LAUGH);
    PlayGaroAnim(play, player, ANIM_LAUGH, 0.0f, -1.0f, 1.0f);
    Player_PlaySfx(player, NA_SE_EN_BOSU_LAUGH);
}

// Vanilla's jump reads no item buttons, so the air slash reads B here, off any jump or fall.
static void TryAirSlash(PlayState* play, Player* player) {
    if (IsGaroBusy(player) || IsGrounded(player) || !CanAct(player) || sRodCooldown > 0 || !IsPressed(play, BTN_B)) {
        return;
    }
    StartAirSlash(play, player);
}

// MM's Garo Master burns away in a ring of flames when he falls.
static void BurnOnDeath(PlayState* play, Player* player) {
    Vec3f accel = { 0.0f, 0.1f, 0.0f };

    if (!(player->stateFlags1 & PLAYER_STATE1_DEAD)) {
        sHaveDeathFlamesSpawned = false;
        return;
    }
    if (sHaveDeathFlamesSpawned) {
        return;
    }
    sHaveDeathFlamesSpawned = true;
    for (s32 i = 0; i < DEATH_FLAMES; i++) {
        f32 angle = (f32)i * (M_PIf * 2.0f / (f32)DEATH_FLAMES);
        Vec3f pos = { player->actor.world.pos.x + cosf(angle) * DEATH_FLAME_RADIUS, player->actor.world.pos.y + 5.0f,
                      player->actor.world.pos.z + sinf(angle) * DEATH_FLAME_RADIUS };
        Vec3f velocity = { cosf(angle) * 0.5f, 1.5f, sinf(angle) * 0.5f };
        EffectSsDFire_Spawn(play, &pos, &velocity, &accel, 100, 35, 255, 8, 12, 60);
    }
    Actor_PlaySfx(&player->actor, NA_SE_EN_STAL_DEAD);
}

// ---- hybrid body ----

static bool PrepareHybrid(PlayState* play) {
    if (sIsHybridReady) {
        return true;
    }
    SkeletonHeader* header = ResourceMgr_LoadSkeletonByName(sHybridSkelPath, NULL);
    if (header == NULL || header->limbCount + 1 != HYBRID_JOINTS) {
        return false;
    }
    SkelAnime_InitFlex(play, &sHybrid, (FlexSkeletonHeader*)sHybridSkelPath, (AnimationHeader*)sHybridIdlePath,
                       sHybridJoints, sHybridMorph, HYBRID_JOINTS);
    Animation_PlayLoop(&sHybrid, (AnimationHeader*)sHybridIdlePath);
    sIsHybridReady = true;
    return true;
}

// Garo's idle sways every bone, then Link's pose overwrites the body's; the robe's side panels follow the arms.
static void PoseHybrid(PlayState* play, Player* player) {
    Vec3s* link = player->skelAnime.jointTable;

    if (!PrepareHybrid(play) || link == NULL) {
        return;
    }
    SkelAnime_Update(&sHybrid);
    sHybridJoints[0] = link[0];
    for (s32 limb = 1; limb < PLAYER_LIMB_MAX; limb++) {
        s8 joint = sLinkToHybridJoint[limb];
        if (joint >= 0 && !IsGaroOwnedJoint(joint)) {
            sHybridJoints[joint] = link[limb];
        }
    }
    sHybridJoints[HYBRID_JOINT_ROBE_LEFT] = link[PLAYER_LIMB_LEFT_SHOULDER];
    sHybridJoints[HYBRID_JOINT_ROBE_RIGHT] = link[PLAYER_LIMB_RIGHT_SHOULDER];
}

// Link's root is placed at Link's scale, then the whole Garo subtree takes the enemy's.
static s32 OverrideHybridLimb(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, Actor* actor) {
    if (limbIndex == HYBRID_JOINT_ROOT) {
        Matrix_TranslateRotateZYX(pos, rot);
        Matrix_Scale(HYBRID_SUBTREE_SCALE, HYBRID_SUBTREE_SCALE, HYBRID_SUBTREE_SCALE, MTXMODE_APPLY);
        return true;
    }
    if (limbIndex == HYBRID_JOINT_R_THIGH || limbIndex == HYBRID_JOINT_L_THIGH) {
        Matrix_Scale(1.0f, LEG_THICKNESS, LEG_THICKNESS, MTXMODE_APPLY);
    } else if (limbIndex == HYBRID_JOINT_R_FOOT || limbIndex == HYBRID_JOINT_L_FOOT) {
        Matrix_Scale(1.0f, 1.0f / LEG_THICKNESS, 1.0f / LEG_THICKNESS, MTXMODE_APPLY);
    }
    return false;
}

// The blades only exist on this skeleton, so their trails are fed here, at the bones the player sees.
static void PostHybridLimb(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, Actor* actor) {
    Player* player = (Player*)actor;

    if (sTrailLeft < 0) {
        return;
    }
    if (limbIndex == HYBRID_JOINT_L_SWORD) {
        FeedTrail(player, sTrailLeft, &sTrailAxisLeft);
    } else if (limbIndex == HYBRID_JOINT_R_SWORD) {
        FeedTrail(player, sTrailRight, &sTrailAxisRight);
    }
}

// MM-format display lists index the backface-cull list through segment 0x0C.
static void DrawHybrid(PlayState* play, Player* player) {
    if (!sIsHybridReady) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x0C, (uintptr_t)gCullBackDList);
    Matrix_Push();
    Matrix_Translate(player->actor.world.pos.x, player->actor.world.pos.y, player->actor.world.pos.z, MTXMODE_NEW);
    Matrix_RotateYF(BINANG_TO_RAD(player->actor.shape.rot.y), MTXMODE_APPLY);
    Matrix_Scale(player->actor.scale.x, player->actor.scale.y, player->actor.scale.z, MTXMODE_APPLY);
    SkelAnime_DrawFlexOpa(play, sHybrid.skeleton, sHybrid.jointTable, sHybrid.dListCount, OverrideHybridLimb,
                          PostHybridLimb, &player->actor);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

// Link's own limbs stay hidden, but their post-draw still runs: feet, hands and focus keep their positions.
static void HideLinkBody(Player* player, s32 limbIndex, Gfx** dList, Gfx* limbDList, Vec3f* pos) {
    if (IsGaro()) {
        *dList = NULL;
    }
}

// ---- projectile draws ----

static void DrawBillboardOrb(PlayState* play, Vec3f pos, f32 scale, const u8* prim, const u8* env) {
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Translate(pos.x, pos.y, pos.z, MTXMODE_NEW);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    Matrix_RotateZS((s16)(play->gameplayFrames * 0x400), MTXMODE_APPLY);
    Matrix_Scale(scale, scale, 1.0f, MTXMODE_APPLY);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gLightOrbMaterial1DL);
    gDPSetEnvColor(POLY_XLU_DISP++, env[0], env[1], env[2], 128);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, prim[0], prim[1], prim[2], 255);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gLightOrbModelDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawElementOrb(PlayState* play, Vec3f pos, f32 scale, u8 element) {
    u8 e = element % ELEMENT_COUNT;

    DrawBillboardOrb(play, pos, scale, sOrbPrim[e], sOrbEnv[e]);
}

static void DrawShadowBall(PlayState* play, Vec3f pos, f32 scale) {
    static const u8 sShadowPrim[3] = { 140, 80, 220 };
    static const u8 sShadowEnv[3] = { 40, 10, 100 };

    DrawBillboardOrb(play, pos, scale, sShadowPrim, sShadowEnv);
}

// The ball is the orb at its charge size, wrapped in a ring of small ones, one per damage tier ray.
static void DrawRodBall(PlayState* play, Vec3f pos, f32 circleScale, f32 ballScale, s32 rays, u8 element) {
    if (circleScale <= 0.001f) {
        return;
    }
    DrawElementOrb(play, pos, ballScale * ORB_BALL_UNIT, element);
    for (s32 i = 0; i < MIN(rays, BIG_MAGIC_RAYS_MAX); i++) {
        f32 angle = ((f32)i * (M_PIf * 2.0f / (f32)BIG_MAGIC_RAYS_MAX)) + ((f32)play->gameplayFrames * 0.1f);
        Vec3f ray = { pos.x + cosf(angle) * ballScale * 2.2f, pos.y + sinf(angle) * ballScale * 2.2f, pos.z };

        DrawElementOrb(play, ray, ballScale * ORB_BALL_UNIT * 0.3f, element);
    }
}

// A fragment's lit streak: the last path samples, each a smaller orb than the one before.
static void DrawOrbStreak(PlayState* play, RodOrb* orb) {
    for (s32 i = 0; i < ORB_STREAK_DRAWN; i++) {
        s32 t = ((orb->trailIndex - i) + ORB_TRAIL_LEN) % ORB_TRAIL_LEN;
        f32 taper = 1.0f - ((f32)i / (f32)ORB_STREAK_DRAWN);

        DrawElementOrb(play, orb->trailPos[t], ORB_SEEKER_DRAW_SCALE * taper, orb->element);
    }
}

// The slingshot-style aim draws no crosshair, and Garo's first-person arms are hidden: four dots and a centre
// spread out as the shot charges.
static void DrawChargeAndReticle(PlayState* play, Player* player) {
    Vec3f head = player->actor.focus.pos;
    s16 yaw;
    s16 pitch;

    Z64Aiming_GetDirection(player, &yaw, &pitch);
    f32 cosPitch = Math_CosS(pitch);
    Vec3f forward = { Math_SinS(yaw) * cosPitch, -Math_SinS(pitch), Math_CosS(yaw) * cosPitch };
    Vec3f ball = { head.x + forward.x * ROD_BALL_AHEAD, head.y + forward.y * ROD_BALL_AHEAD,
                   head.z + forward.z * ROD_BALL_AHEAD };
    f32 pulse = 1.0f + 0.06f * Math_SinS(play->gameplayFrames * 0x1000);
    DrawRodBall(play, ball, sRodBallCircle * pulse, sRodBallScale * pulse, sRodBallRays, sRodElement);

    Vec3f center = { head.x + forward.x * RETICLE_DIST, head.y + forward.y * RETICLE_DIST,
                     head.z + forward.z * RETICLE_DIST };
    Vec3f right = { Math_SinS(yaw + 0x4000), 0.0f, Math_CosS(yaw + 0x4000) };
    Vec3f up = { right.y * forward.z - right.z * forward.y, right.z * forward.x - right.x * forward.z,
                 right.x * forward.y - right.y * forward.x };
    f32 spread = RETICLE_SPREAD * (1.0f + MIN((f32)sRodCharge / ROD_CHARGE_MAX, 1.0f) * 0.6f);
    DrawElementOrb(play, center, RETICLE_DOT_SCALE * 0.7f * ORB_DOT_UNIT, sRodElement);
    for (s32 i = 0; i < 4; i++) {
        f32 offsetX = i == 0 ? spread : (i == 1 ? -spread : 0.0f);
        f32 offsetY = i == 2 ? spread : (i == 3 ? -spread : 0.0f);
        Vec3f dot = { center.x + right.x * offsetX + up.x * offsetY, center.y + right.y * offsetX + up.y * offsetY,
                      center.z + right.z * offsetX + up.z * offsetY };
        DrawElementOrb(play, dot, RETICLE_DOT_SCALE * ORB_DOT_UNIT, sRodElement);
    }
}

static void DrawProjectiles(PlayState* play, Player* player) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    CLOSE_DISPS(play->state.gfxCtx);
    if (sMove == GARO_MOVE_ROD_AIM) {
        DrawChargeAndReticle(play, player);
    }
    if (sMove == GARO_MOVE_BANISH_SHADOW) {
        f32 t = MIN((f32)sShadowTimer / (f32)BANISH_SHADOW_FRAMES, 1.0f);
        DrawShadowBall(play, sShadowPos, (3.4f + t * 1.6f) * ORB_SHADOW_UNIT);
    }
    for (s32 i = 0; i < ORB_POOL_MAX; i++) {
        RodOrb* orb = &sOrbs[i];
        if (!orb->isActive) {
            continue;
        }
        if (orb->isSeeker) {
            DrawOrbStreak(play, orb);
        } else {
            DrawRodBall(play, orb->pos, orb->ballCircle, orb->ballScale, BIG_MAGIC_RAYS_MAX, orb->element);
        }
    }
}

static void DrawGaro(PlayState* play, Player* player) {
    bool isBodyVisible =
        !(player->stateFlags2 & STATE2_DISABLE_DRAW) && !(player->stateFlags1 & STATE1_FIRST_PERSON);

    if (isBodyVisible) {
        DrawHybrid(play, player);
    }
    DrawProjectiles(play, player);
}

// ---- lifetime ----

static void ResetState(PlayState* play, Player* player) {
    if (player != NULL && play != NULL && IsGaroBusy(player)) {
        ClearMove(play, player);
    }
    sMove = GARO_MOVE_NONE;
    sIsParryPending = false;
    sPendingParry = NULL;
    sIsLaughPending = false;
    sEnemySnapshotCount = 0;
    sScaledDamage = 0;
    sBanishCooldown = 0;
    sRodCooldown = 0;
    ResetRodCharge();
    memset(sOrbs, 0, sizeof(sOrbs));
    sReflectedShot = NULL;
    sReflectTimer = 0;
}

static void EnterGaro(PlayState* play, Player* player) {
    ResetState(play, player);
    PrepareHybrid(play);
}

static void ExitGaro(PlayState* play, Player* player) {
    bool wasBusy = IsGaroBusy(player);

    ResetState(play, player);
    ConcealRevealed(play);
    ReleaseChantVoice();
    sIsOcarinaOut = false;
    if (wasBusy) {
        func_8083B1A0(player, play);
    }
}

// Damage, a cutscene or the water can take the player from a move without passing through its end.
static void DropInterruptedMove(PlayState* play, Player* player) {
    if (sMove == GARO_MOVE_NONE || IsGaroBusy(player)) {
        return;
    }
    ClearMove(play, player);
}

static void UpdateGaro(PlayState* play, Player* player) {
    DropInterruptedMove(play, player);
    if (!WantsTrails() && sTrailLeft >= 0) {
        KillTrails(play);
    }
    if (sBanishCooldown > 0) {
        sBanishCooldown--;
    }
    if (sRodCooldown > 0) {
        sRodCooldown--;
    }
    UpdateOrbs(play, player);
    EscortReflectedShot(play, player);
    PoseHybrid(play, player);
    RevealHidden(play);
    UpdateOcarinaVoice(play, player);
    BurnOnDeath(play, player);
    WatchForKills(play);
    TryAirSlash(play, player);
    TryLaugh(play, player);
}

static void ForgetScene(s16 sceneId, s8 spawnNum) {
    sAreOrbQuadsReady = false;
    sIsReflectQuadReady = false;
    sIsHybridReady = false;
    sRevealedCount = 0;
    sTrailLeft = -1;
    sTrailRight = -1;
    Z64Aiming_Drop();
    ResetState(NULL, NULL);
}

// ---- mask ----

static bool CanWearMask(Player* player, PlayState* play) {
    return IsGrounded(player) &&
           !(player->stateFlags1 & (STATE1_SWIMMING | STATE1_FIRST_PERSON | PLAYER_STATE1_20000000));
}

static void WearMask(PlayState* play, Player* player) {
    sApi->ToggleForm(GARO_FORM_KEY);
}

// A light ninja's body, the Zora's grip: bushes and bomb flowers, nothing heavier.
static void ResolveGaroStrength(bool* should, va_list args) {
    s32* strength = va_arg(args, s32*);

    if (IsGaro()) {
        *strength = PLAYER_STRENGTH_ZORA;
    }
}

static const S2HCustomItemRandomizer sRandomizerItem = {
    sizeof(S2HCustomItemRandomizer), 0, S2H_CUSTOM_ITEM_TYPE_ITEM, 0, 1, GARO_MASK_KEY, GARO_MASK_KEY, "",
};

static void RegisterMask(void) {
    S2HCustomItemDefinition mask = Z64Items_Define(GARO_MASK_KEY, sIconTex, sNameTex);

    Z64Items_SetButtons(&mask, S2H_CUSTOM_ITEM_C_BUTTON | S2H_CUSTOM_ITEM_DPAD);
    mask.flags |= S2H_CUSTOM_ITEM_INSTANT | S2H_CUSTOM_ITEM_WEARABLE;
    Z64Items_SetPlacement(&mask, GARO_MASK_PAGE, GARO_MASK_SLOT, 0);
    Z64Items_SetReplaces(&mask, ITEM_MASK_GARO);
    Z64Items_SetVanillaMode(&mask, S2H_VANILLA_ITEM_REPLACE, RI_MASK_GARO);
    Z64Items_SetGetItemModel(&mask, sGetItemDL);
    Z64Items_SetTextbox(&mask, "You got %rGaro's Mask%w!&The face of the Garo, Ikana's ninja spies.^Wear it with "
                               "%y\xA1%w to become a %rGaro%w: twin blades, a guard that answers every blow, and a "
                               "rod of elemental magic.");
    Z64Items_SetPauseText(&mask, "%rGaro's Mask&%wPress %y\xA1%w to become a Garo.&%y\xA0%w: spin  Hold %y\xA0%w: rod "
                                 "(%y\xA2%w/%y\xA3%w element)  %y\x9F%w: dash  %y\xA3%w: guard");
    Z64Items_SetCanUse(&mask, CanWearMask);
    Z64Items_SetAction(&mask, WearMask, NULL);
    Z64Items_SetLogic(&mask, &sRandomizerItem);
    Z64Items_Register(sApi, &mask);
}

// ---- form ----

static uint16_t ResolveGaroEquipment(int32_t equipType, uint16_t value) {
    if (equipType == EQUIP_TYPE_SWORD) {
        return EQUIP_VALUE_SWORD_NONE;
    }
    if (equipType == EQUIP_TYPE_SHIELD) {
        return EQUIP_VALUE_SHIELD_NONE;
    }
    return value;
}

static bool IsAllowedCustomItem(const char* customKey) {
    static const char* const sAllowed[] = {
        "nei.keaton_form_mask", "nei.mask_kafei", "nei.gerudo_form_mask", "nei.whip",
        "nei.beetle",           "nei.switch_hook", "nei.somaria",          "nei.rito_form_mask",
    };

    for (u32 i = 0; i < ARRAY_COUNT(sAllowed); i++) {
        if (strcmp(customKey, sAllowed[i]) == 0) {
            return true;
        }
    }
    return false;
}

// The ranged tools of a ninja spy, the transformation masks, bottles and trade items.
static bool AllowsGaroItem(uint16_t item, const char* customKey) {
    if (customKey != NULL) {
        return IsAllowedCustomItem(customKey);
    }
    return item == ITEM_BOW || item == ITEM_ARROW_FIRE || item == ITEM_ARROW_ICE || item == ITEM_ARROW_LIGHT ||
           item == ITEM_OCARINA_FAIRY || item == ITEM_OCARINA_OF_TIME || item == ITEM_HOOKSHOT ||
           item == ITEM_LONGSHOT || (item >= ITEM_MASK_DEKU && item <= ITEM_MASK_FIERCE_DEITY) ||
           (item >= ITEM_BOTTLE && item <= ITEM_OBABA_DRINK) ||
           (item >= ITEM_MOONS_TEAR && item <= ITEM_PENDANT_OF_MEMORIES);
}

static const S2HFormActionOverride sActions[] = {
    { SOH_PLAYER_ACTION_MELEE, HandleSlash },        { SOH_PLAYER_ACTION_ROLL, HandleDash },
    { SOH_PLAYER_ACTION_ZTARGET_A, HandleZTargetA }, { SOH_PLAYER_ACTION_SHIELD, HandleGuard },
    { SOH_PLAYER_ACTION_RECEIVE_ITEM, RefuseCarry },
};

static void RegisterForm(void) {
    S2HFormDefinition garo = { 0 };

    garo.structSize = sizeof(garo);
    garo.key = GARO_FORM_KEY;
    garo.label = "Garo";
    garo.kind = S2H_FORM_KIND_LINK;
    garo.item = GARO_MASK_KEY;
    garo.motionScale = 1.0f;
    garo.resolveEquipment = ResolveGaroEquipment;
    garo.allowsButtonItem = AllowsGaroItem;
    garo.transformMask = sMaskDL;
    garo.transformVoiceSfx = NA_SE_VO_LI_FALL_L;
    garo.actions = sActions;
    garo.actionCount = ARRAY_COUNT(sActions);
    garo.onEnter = EnterGaro;
    garo.onExit = ExitGaro;
    garo.update = UpdateGaro;
    garo.draw = DrawGaro;
    sApi->RegisterForm(&garo);
}

S2H_MOD_EXPORT void ModSetApi(const S2HModApi* api) {
    sApi = api;
}

S2H_MOD_EXPORT const S2HModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

S2H_MOD_EXPORT void ModInit(void) {
    LoadAnims();
    RegisterMask();
    RegisterForm();
    S2H_REGISTER_HOOK(sApi, OnPlayerResolveLimbDraw, HideLinkBody);
    S2H_REGISTER_HOOK(sApi, OnPlayerFilterInput, HandleIncomingHit);
    S2H_REGISTER_HOOK_FOR_ID(sApi, OnActorPlaySfx, ACTOR_PLAYER, SpeakWithIgosVoice);
    S2H_REGISTER_HOOK(sApi, OnBgCheckRaycastFloor, WalkOnWater);
    S2H_REGISTER_HOOK(sApi, ShouldActorDraw, HideFakes);
    S2H_REGISTER_HOOK(sApi, OnCollisionResolveDamage, FixStrikeDamage);
    S2H_REGISTER_HOOK(sApi, OnSceneInit, ForgetScene);
    sApi->RegisterVB(VB_USE_STRENGTH_UPGRADE, ResolveGaroStrength);
}
