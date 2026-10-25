#include <math.h>
#include <string.h>

#include "z64items.h"
#include "z64wheel.h"
#include "z64aiming.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "overlays/actors/ovl_Obj_Ice_Poly/z_obj_ice_poly.h"

#define GUST_JAR_KEY "nei.gust_jar"
#define GUST_JAR_HELD_SCALE 1.5f
#define GUST_JAR_HAND_DROP 2.0f
#define GUST_JAR_NOZZLE_REACH 28.0f
#define GUST_JAR_NOZZLE_HEIGHT 28.0f
#define GUST_JAR_SUCK_LENGTH 260.0f
#define GUST_JAR_SUCK_RADIUS 70.0f
#define GUST_JAR_BLOW_LENGTH 220.0f
#define GUST_JAR_BLOW_RADIUS 90.0f
#define GUST_JAR_PULL_SPEED 8.0f
#define GUST_JAR_ITEM_PULL_SPEED 12.0f
#define GUST_JAR_PUSH_SPEED 14.0f
#define GUST_JAR_DAMAGE 1
#define GUST_JAR_COLLIDER_RADIUS 22
#define GUST_JAR_COLLIDER_HEIGHT 30
#define GUST_JAR_FAIRY_MEMORY 8
// The tornado is modelled 100 units along its own +Y and 47 wide, standing on its base.
#define GUST_JAR_TORNADO_LENGTH 100.0f
#define GUST_JAR_TORNADO_RADIUS 47.0f
#define GUST_JAR_TORNADO_SPIN 0x1800
#define GUST_JAR_RIBBONS 6
#define GUST_JAR_RIBBON_TURNS 0x18000
#define GUST_JAR_RIBBON_STEP 0.06f
#define GUST_JAR_RIBBON_LIFE 8
#define GUST_JAR_SHRUNK_LIMIT 12
#define GUST_JAR_SHRINK_FLOOR 0.15f
#define GUST_JAR_HALF_TURN 3.14159265f
// Frame 8 of the two-handed carry is the pose that holds a jar at chest height.
#define GUST_JAR_POSE_FRAME 8

typedef enum {
    GUST_MODE_SUCTION,
    GUST_MODE_WIND,
    GUST_MODE_FIRE,
    GUST_MODE_ICE,
    GUST_MODE_LIGHT,
    GUST_MODE_SHADOW,
    GUST_MODE_SPIRIT,
    GUST_MODE_MAX,
} GustMode;

typedef struct {
    Color_RGBA8 primary;
    Color_RGBA8 secondary;
    u32 damageFlags;
    const char* medallionKey;
} GustElement;

static const ALIGN_ASSET(2) char sNameTex[] = "__OTR__textures/item_name_custom/gGustJarNameTex";
static const ALIGN_ASSET(2) char sJarDL[] = "__OTR__objects/object_nei_gust_jar/jar_model_dl";
static const ALIGN_ASSET(2) char sJarBodyDL[] = "__OTR__objects/object_nei_gust_jar/jar_body_dl";
static const ALIGN_ASSET(2) char sJarDecorationDL[] = "__OTR__objects/object_nei_gust_jar/jar_decoration_dl";
// The material paints the funnel white, so the element's colour only sticks if it is set after it.
static const ALIGN_ASSET(2) char sTornadoMaterialDL[] = "__OTR__objects/object_nei_tornado/mat_tornado_f3dlite_tornado";
static const ALIGN_ASSET(2) char sTornadoMeshDL[] = "__OTR__objects/object_nei_tornado/tornado_mesh_tri_0";
static const ALIGN_ASSET(2) char sCarryPoseAnim[] = "__OTR__objects/gameplay_keep/gPlayerAnim_link_normal_carryB_free";
static const ALIGN_ASSET(2) char sModeIcons[GUST_MODE_MAX][64] = {
    "__OTR__textures/icon_item_custom/gItemIconGustJarTex",
    "__OTR__textures/icon_item_custom/gItemIconGustJarWindTex",
    "__OTR__textures/icon_item_custom/gItemIconGustJarFireTex",
    "__OTR__textures/icon_item_custom/gItemIconGustJarIceTex",
    "__OTR__textures/icon_item_custom/gItemIconGustJarLightTex",
    "__OTR__textures/icon_item_custom/gItemIconGustJarShadowTex",
    "__OTR__textures/icon_item_custom/gItemIconGustJarSpiritTex",
};

// Suction never damages what it swallows by element, so every blow carries 0 damage: the flags alone
// light torches and fire sun switches, exactly as the matching arrow would. Each breath beyond the first is
// unlocked by owning the matching medallion item.
static const GustElement sElements[GUST_MODE_MAX] = {
    { { 110, 190, 255, 255 }, { 40, 90, 200, 255 }, DMG_SWORD, NULL },
    { { 180, 255, 180, 255 }, { 60, 170, 90, 255 }, DMG_UNBLOCKABLE, "nei.medallion.forest" },
    { { 255, 150, 50, 255 }, { 200, 60, 0, 255 }, DMG_FIRE_ARROW, "nei.medallion.fire" },
    { { 150, 225, 255, 255 }, { 60, 140, 220, 255 }, DMG_ICE_ARROW, "nei.medallion.water" },
    { { 255, 245, 160, 255 }, { 220, 190, 60, 255 }, DMG_LIGHT_ARROW | DMG_LIGHT_RAY, "nei.medallion.light" },
    { { 190, 130, 230, 255 }, { 100, 40, 150, 255 }, DMG_ZORA_BOOMERANG, "nei.medallion.shadow" },
    { { 255, 200, 110, 255 }, { 210, 120, 30, 255 }, DMG_SWORD, "nei.medallion.spirit" },
};

static const Z64WheelEntry sWheelEntries[GUST_MODE_MAX] = {
    { sModeIcons[GUST_MODE_SUCTION], "Suction" }, { sModeIcons[GUST_MODE_WIND], "Wind" },
    { sModeIcons[GUST_MODE_FIRE], "Fire" },       { sModeIcons[GUST_MODE_ICE], "Ice" },
    { sModeIcons[GUST_MODE_LIGHT], "Light" },     { sModeIcons[GUST_MODE_SHADOW], "Shadow" },
    { sModeIcons[GUST_MODE_SPIRIT], "Spirit" },
};

// The small things the bare jar can move. Anything heavier waits for the element that matches it.
static const s16 sSuckableActors[] = {
    ACTOR_EN_KUSA,   ACTOR_OBJ_TSUBO, ACTOR_OBJ_KIBAKO, ACTOR_OBJ_KIBAKO2, ACTOR_OBJ_COMB,
    ACTOR_EN_KANBAN, ACTOR_BG_ICICLE, ACTOR_EN_FIREFLY, ACTOR_EN_TITE,     ACTOR_EN_DEKUNUTS,
    ACTOR_EN_POH,    ACTOR_EN_OKUTA,  ACTOR_EN_PEEHAT,  ACTOR_EN_SB,       ACTOR_EN_BB,
    ACTOR_EN_SKB,    ACTOR_EN_CROW,
};

static ColliderCylinderInit sNozzleColliderInit = {
    {
        COL_MATERIAL_NONE,
        AT_ON | AT_TYPE_PLAYER,
        AC_NONE,
        OC1_NONE,
        OC2_TYPE_PLAYER,
        COLSHAPE_CYLINDER,
    },
    {
        ELEM_MATERIAL_UNK2,
        { DMG_SWORD, 0x00, GUST_JAR_DAMAGE },
        { 0, 0, 0 },
        ATELEM_ON | ATELEM_SFX_NORMAL,
        ACELEM_NONE,
        OCELEM_NONE,
    },
    { GUST_JAR_COLLIDER_RADIUS, GUST_JAR_COLLIDER_HEIGHT, 0, { 0, 0, 0 } },
};

// Indexed by Player.heldItemButton, which keeps a slot for A between the C buttons and the D-pad.
static const u16 sItemButtons[S2H_ITEM_BUTTON_COUNT] = { BTN_B,     BTN_CLEFT, BTN_CDOWN, BTN_CRIGHT, 0,
                                                         BTN_DRIGHT, BTN_DLEFT, BTN_DDOWN,  BTN_DUP };

static const char* const sRequiredHooks[] = { "OnPlayerUpdate", "OnActorDrawEnd", "OnSceneInit", "OnInterfaceDrawEnd",
                                              "OnSaveLoad" };
static const S2HModRequirements sRequirements = { sizeof(S2HModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const S2HModApi* sApi;
static ColliderCylinder sNozzleCollider;
static bool sIsColliderReady;
static bool sIsJarOut;
static bool sIsBlowing;
static s16 sUseTimer;
static Actor* sFairiesPaid[GUST_JAR_FAIRY_MEMORY];
static u8 sFairiesPaidCount;
static s32 sRibbonEffects[GUST_JAR_RIBBONS];
static f32 sRibbonPhase[GUST_JAR_RIBBONS];
static bool sAreRibbonsLive;
static u8 sRibbonMode;
static Actor* sShrunkActors[GUST_JAR_SHRUNK_LIMIT];
static Vec3f sShrunkScales[GUST_JAR_SHRUNK_LIMIT];
static u8 sShrunkCount;
static Vec3s sPoseJoints[PLAYER_LIMB_MAX];

bool Player_UpdateUpperBody(Player* player, PlayState* play);
s32 Player_SetAction(PlayState* play, Player* this, PlayerActionFunc actionFunc, s32 arg3);
void func_8083B1A0(Player* this, PlayState* play);
void Player_StopHorizontalMovement(Player* player);

static void GustAimAction(Player* player, PlayState* play);
static void HoldJarWithBothHands(Player* player, PlayState* play);

static u8 CurrentMode(void) {
    u32 selection = Z64Wheel_GetSelection();

    return selection < GUST_MODE_MAX ? (u8)selection : GUST_MODE_SUCTION;
}

static bool IsBlowMode(u8 mode) {
    return mode != GUST_MODE_SUCTION && mode != GUST_MODE_SPIRIT;
}

static f32 BreathReach(u8 mode) {
    return IsBlowMode(mode) ? GUST_JAR_BLOW_LENGTH : GUST_JAR_SUCK_LENGTH;
}

static void SyncUnlockedModes(void) {
    for (u8 mode = GUST_MODE_WIND; mode < GUST_MODE_MAX; mode++) {
        Z64Wheel_SetEntryUnlocked(mode, sApi->IsCustomItemOwned(sElements[mode].medallionKey));
    }
}

static u16 FindEquippedButtonMask(void) {
    for (u8 button = 0; button < S2H_ITEM_BUTTON_COUNT; button++) {
        const char* equipped = sApi->GetEquippedCustomItem(button);
        if (equipped != NULL && strcmp(equipped, GUST_JAR_KEY) == 0) {
            return sItemButtons[button];
        }
    }
    return 0;
}

static void GetAim(Player* player, s16* yaw, s16* pitch) {
    if (Z64Aiming_IsAiming()) {
        Z64Aiming_GetDirection(player, yaw, pitch);
        return;
    }
    *yaw = player->actor.shape.rot.y;
    *pitch = 0;
}

// A positive aim pitch looks DOWN, the way every vanilla projectile reads it, so the axis drops as it grows.
static void GetNozzle(Player* player, Vec3f* nozzle, Vec3f* axis, s16 yaw, s16 pitch) {
    f32 flat = Math_CosS(pitch);

    axis->x = Math_SinS(yaw) * flat;
    axis->y = -Math_SinS(pitch);
    axis->z = Math_CosS(yaw) * flat;
    nozzle->x = player->actor.world.pos.x + axis->x * GUST_JAR_NOZZLE_REACH;
    nozzle->y = player->actor.world.pos.y + GUST_JAR_NOZZLE_HEIGHT + axis->y * GUST_JAR_NOZZLE_REACH;
    nozzle->z = player->actor.world.pos.z + axis->z * GUST_JAR_NOZZLE_REACH;
}

static bool IsSuckable(Actor* actor, u8 mode) {
    if (actor->update == NULL || (actor->flags & ACTOR_FLAG_TALK) != 0) {
        return false;
    }
    // The Spirit Medallion's pull takes hold of anything that fights back, not just the small fry.
    if (mode == GUST_MODE_SPIRIT && actor->category == ACTORCAT_ENEMY) {
        return true;
    }
    for (u32 index = 0; index < ARRAY_COUNT(sSuckableActors); index++) {
        if (actor->id == sSuckableActors[index]) {
            return true;
        }
    }
    return false;
}

static bool IsInsideCone(Vec3f* nozzle, Vec3f* axis, Vec3f* point, f32 length, f32 radius, f32* distance) {
    f32 toActorX = point->x - nozzle->x;
    f32 toActorY = point->y - nozzle->y;
    f32 toActorZ = point->z - nozzle->z;
    f32 along = toActorX * axis->x + toActorY * axis->y + toActorZ * axis->z;

    if (along < 0.0f || along > length) {
        return false;
    }
    f32 offX = toActorX - axis->x * along;
    f32 offY = toActorY - axis->y * along;
    f32 offZ = toActorZ - axis->z * along;

    *distance = sqrtf(SQ(toActorX) + SQ(toActorY) + SQ(toActorZ));
    return SQ(offX) + SQ(offY) + SQ(offZ) < SQ(radius);
}

static void ShrinkTowardsNozzle(Actor* actor, f32 factor) {
    for (u8 index = 0; index < sShrunkCount; index++) {
        if (sShrunkActors[index] == actor) {
            actor->scale.x = sShrunkScales[index].x * factor;
            actor->scale.y = sShrunkScales[index].y * factor;
            actor->scale.z = sShrunkScales[index].z * factor;
            return;
        }
    }
    if (sShrunkCount >= GUST_JAR_SHRUNK_LIMIT) {
        return;
    }
    sShrunkActors[sShrunkCount] = actor;
    sShrunkScales[sShrunkCount] = actor->scale;
    sShrunkCount++;
}

// Whatever survived the pull gets its own size back; a dead actor's scale is no longer ours to touch.
static void RestoreShrunkActors(void) {
    for (u8 index = 0; index < sShrunkCount; index++) {
        if (sShrunkActors[index] != NULL && sShrunkActors[index]->update != NULL) {
            sShrunkActors[index]->scale = sShrunkScales[index];
        }
    }
    sShrunkCount = 0;
}

static void StepTowards(Actor* actor, Vec3f* target, f32 speed) {
    f32 deltaX = target->x - actor->world.pos.x;
    f32 deltaY = target->y - actor->world.pos.y;
    f32 deltaZ = target->z - actor->world.pos.z;
    f32 length = sqrtf(SQ(deltaX) + SQ(deltaY) + SQ(deltaZ));

    if (length < 1.0f) {
        return;
    }
    actor->world.pos.x += deltaX * speed / length;
    actor->world.pos.y += deltaY * speed / length;
    actor->world.pos.z += deltaZ * speed / length;
}

static bool WasFairyPaid(Actor* actor) {
    for (u8 index = 0; index < sFairiesPaidCount; index++) {
        if (sFairiesPaid[index] == actor) {
            return true;
        }
    }
    return false;
}

static void PaySpiritFairy(PlayState* play, Actor* actor) {
    if (actor->colChkInfo.health != 0 || WasFairyPaid(actor)) {
        return;
    }
    Vec3f dropPos = { actor->world.pos.x, actor->world.pos.y + 20.0f, actor->world.pos.z };

    Item_DropCollectible(play, &dropPos, ITEM00_FLEXIBLE);
    Audio_PlaySfx_AtPos(&dropPos, NA_SE_EV_BUTTERFRY_TO_FAIRY);
    sFairiesPaid[sFairiesPaidCount % GUST_JAR_FAIRY_MEMORY] = actor;
    sFairiesPaidCount = MIN(sFairiesPaidCount + 1, GUST_JAR_FAIRY_MEMORY);
}

static void StopRibbons(PlayState* play) {
    if (!sAreRibbonsLive) {
        return;
    }
    for (u8 index = 0; index < GUST_JAR_RIBBONS; index++) {
        if (sRibbonEffects[index] >= 0) {
            Effect_Destroy(play, sRibbonEffects[index]);
            sRibbonEffects[index] = -1;
        }
    }
    sAreRibbonsLive = false;
}

// Blure colours are fixed when the effect is added, so a new element needs new ribbons.
static void StartRibbons(PlayState* play, u8 mode) {
    Color_RGBA8 primary = sElements[mode].primary;
    Color_RGBA8 secondary = sElements[mode].secondary;
    EffectBlureInit1 init;

    StopRibbons(play);
    init.p1StartColor[0] = primary.r;
    init.p1StartColor[1] = primary.g;
    init.p1StartColor[2] = primary.b;
    init.p1StartColor[3] = 255;
    init.p2StartColor[0] = secondary.r;
    init.p2StartColor[1] = secondary.g;
    init.p2StartColor[2] = secondary.b;
    init.p2StartColor[3] = 0;
    init.p1EndColor[0] = primary.r;
    init.p1EndColor[1] = primary.g;
    init.p1EndColor[2] = primary.b;
    init.p1EndColor[3] = 0;
    init.p2EndColor[0] = secondary.r;
    init.p2EndColor[1] = secondary.g;
    init.p2EndColor[2] = secondary.b;
    init.p2EndColor[3] = 0;
    init.elemDuration = GUST_JAR_RIBBON_LIFE;
    init.unkFlag = 0;
    init.calcMode = 2;

    for (u8 index = 0; index < GUST_JAR_RIBBONS; index++) {
        sRibbonEffects[index] = -1;
        Effect_Add(play, &sRibbonEffects[index], EFFECT_BLURE1, 0, 0, &init);
        sRibbonPhase[index] = (f32)index / (f32)GUST_JAR_RIBBONS;
    }
    sAreRibbonsLive = true;
    sRibbonMode = mode;
}

// Each ribbon rides the funnel's surface: it flares out along the axis while it turns around it.
static void FeedRibbons(PlayState* play, Vec3f* nozzle, Vec3f* axis, s16 yaw, u8 mode, bool isBlow) {
    f32 reach = isBlow ? GUST_JAR_BLOW_LENGTH : GUST_JAR_SUCK_LENGTH;
    f32 spread = isBlow ? GUST_JAR_BLOW_RADIUS : GUST_JAR_SUCK_RADIUS;
    f32 width = spread * 0.16f + 3.0f;
    Vec3f right = { Math_CosS(yaw), 0.0f, -Math_SinS(yaw) };
    Vec3f up = { axis->y * right.z - axis->z * right.y, axis->z * right.x - axis->x * right.z,
                 axis->x * right.y - axis->y * right.x };

    if (!sAreRibbonsLive || sRibbonMode != mode) {
        StartRibbons(play, mode);
    }
    for (u8 index = 0; index < GUST_JAR_RIBBONS; index++) {
        EffectBlure* ribbon = Effect_GetByIndex(sRibbonEffects[index]);
        f32 phase = sRibbonPhase[index];
        f32 along = reach * phase;
        f32 flare = spread * phase * phase;
        s16 turn = (s16)((s32)(play->gameplayFrames * GUST_JAR_TORNADO_SPIN) +
                         (s32)(index * (0x10000 / GUST_JAR_RIBBONS)) + (s32)(phase * GUST_JAR_RIBBON_TURNS));
        f32 turnCos = Math_CosS(turn);
        f32 turnSin = Math_SinS(turn);
        Vec3f base = { nozzle->x + axis->x * along + (right.x * turnCos + up.x * turnSin) * flare,
                       nozzle->y + axis->y * along + (right.y * turnCos + up.y * turnSin) * flare,
                       nozzle->z + axis->z * along + (right.z * turnCos + up.z * turnSin) * flare };
        Vec3f tip = { base.x + axis->x * width, base.y + axis->y * width, base.z + axis->z * width };

        if (ribbon != NULL) {
            EffectBlure_AddVertex(ribbon, &tip, &base);
        }
        f32 nextPhase = phase + GUST_JAR_RIBBON_STEP;

        sRibbonPhase[index] = nextPhase >= 1.0f ? nextPhase - 1.0f : nextPhase;
    }
}

static void SpawnStream(PlayState* play, Vec3f* nozzle, Vec3f* axis, u8 mode, bool isBlow) {
    Vec3f accel = { 0.0f, 0.0f, 0.0f };
    Color_RGBA8 primary = sElements[mode].primary;
    Color_RGBA8 secondary = sElements[mode].secondary;
    f32 reach = isBlow ? GUST_JAR_BLOW_LENGTH : GUST_JAR_SUCK_LENGTH;
    f32 spread = isBlow ? GUST_JAR_BLOW_RADIUS : GUST_JAR_SUCK_RADIUS;

    for (u8 particle = 0; particle < 3; particle++) {
        f32 along = Rand_ZeroFloat(reach);
        Vec3f pos = { nozzle->x + axis->x * along + Rand_CenteredFloat(spread),
                      nozzle->y + axis->y * along + Rand_CenteredFloat(spread * 0.6f),
                      nozzle->z + axis->z * along + Rand_CenteredFloat(spread) };
        f32 flow = isBlow ? 6.0f : -9.0f;
        Vec3f velocity = { axis->x * flow, axis->y * flow + Rand_CenteredFloat(1.0f), axis->z * flow };

        EffectSsKirakira_SpawnFocused(play, &pos, &velocity, &accel, &primary, &secondary, 420, 14);
    }
}

static void StrikeWithNozzle(PlayState* play, Vec3f* nozzle, Vec3f* axis, u8 mode, bool isBlow) {
    f32 reach = isBlow ? GUST_JAR_BLOW_LENGTH * 0.5f : 0.0f;

    sNozzleCollider.dim.pos.x = (s16)(nozzle->x + axis->x * reach);
    sNozzleCollider.dim.pos.y = (s16)(nozzle->y + axis->y * reach);
    sNozzleCollider.dim.pos.z = (s16)(nozzle->z + axis->z * reach);
    sNozzleCollider.dim.radius = isBlow ? (s16)GUST_JAR_BLOW_RADIUS : GUST_JAR_COLLIDER_RADIUS;
    sNozzleCollider.dim.height = GUST_JAR_COLLIDER_HEIGHT;
    sNozzleCollider.dim.yShift = -GUST_JAR_COLLIDER_HEIGHT / 2;
    sNozzleCollider.elem.atDmgInfo.dmgFlags = sElements[mode].damageFlags;
    sNozzleCollider.elem.atDmgInfo.damage = isBlow ? 0 : GUST_JAR_DAMAGE;
    sNozzleCollider.base.atFlags |= AT_ON | AT_TYPE_PLAYER;
    CollisionCheck_SetAT(play, &play->colChkCtx, &sNozzleCollider.base);
    sNozzleCollider.base.atFlags &= ~AT_HIT;
}

static void PullActors(Player* player, PlayState* play, Vec3f* nozzle, Vec3f* axis, u8 mode) {
    static const u8 categories[] = { ACTORCAT_ENEMY, ACTORCAT_PROP, ACTORCAT_MISC, ACTORCAT_EXPLOSIVES };

    for (u8 index = 0; index < ARRAY_COUNT(categories); index++) {
        for (Actor* actor = play->actorCtx.actorLists[categories[index]].first; actor != NULL; actor = actor->next) {
            f32 distance = 0.0f;
            bool isCollectible = actor->id == ACTOR_EN_ITEM00;

            if (!IsInsideCone(nozzle, axis, &actor->world.pos, GUST_JAR_SUCK_LENGTH, GUST_JAR_SUCK_RADIUS,
                              &distance) ||
                (!isCollectible && !IsSuckable(actor, mode))) {
                continue;
            }
            if (isCollectible) {
                StepTowards(actor, &player->actor.world.pos, GUST_JAR_ITEM_PULL_SPEED);
                continue;
            }
            StepTowards(actor, nozzle, GUST_JAR_PULL_SPEED);
            ShrinkTowardsNozzle(actor, MAX(distance / GUST_JAR_SUCK_LENGTH, GUST_JAR_SHRINK_FLOOR));
            if (mode == GUST_MODE_SPIRIT) {
                PaySpiritFairy(play, actor);
            }
        }
    }
}

static void PushActors(PlayState* play, Vec3f* nozzle, Vec3f* axis, u8 mode) {
    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_ENEMY].first; actor != NULL; actor = actor->next) {
        f32 distance = 0.0f;

        if (actor->update == NULL ||
            !IsInsideCone(nozzle, axis, &actor->world.pos, GUST_JAR_BLOW_LENGTH, GUST_JAR_BLOW_RADIUS, &distance)) {
            continue;
        }
        actor->world.pos.x += axis->x * GUST_JAR_PUSH_SPEED;
        actor->world.pos.z += axis->z * GUST_JAR_PUSH_SPEED;
        if (mode == GUST_MODE_WIND) {
            actor->velocity.y = MAX(actor->velocity.y, 4.0f);
        }
    }
}

// Red ice answers to no damage flag at all, so the icy breath melts it itself by setting the switch flag its
// own native shatter reads, which also opens whatever it was guarding.
static void MeltRedIceInCone(PlayState* play, Vec3f* nozzle, Vec3f* axis) {
    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_ITEMACTION].first; actor != NULL; actor = actor->next) {
        ObjIcePoly* ice = (ObjIcePoly*)actor;
        f32 distance = 0.0f;

        if (actor->id != ACTOR_OBJ_ICE_POLY || ice->switchFlag == OBJICEPOLY_SWITCH_FLAG_NONE) {
            continue;
        }
        if (IsInsideCone(nozzle, axis, &actor->world.pos, GUST_JAR_BLOW_LENGTH, GUST_JAR_BLOW_RADIUS * 1.5f,
                         &distance)) {
            Flags_SetSwitch(play, ice->switchFlag);
        }
    }
}

static bool IsButtonHeld(PlayState* play) {
    u16 buttonMask = FindEquippedButtonMask();

    return buttonMask != 0 && (play->state.input[0].cur.button & buttonMask) != 0;
}

static void BreatheThroughJar(Player* player, PlayState* play) {
    u8 mode = CurrentMode();
    s16 yaw;
    s16 pitch;
    Vec3f nozzle;
    Vec3f axis;

    GetAim(player, &yaw, &pitch);
    GetNozzle(player, &nozzle, &axis, yaw, pitch);
    sIsBlowing = IsBlowMode(mode);
    sUseTimer++;
    FeedRibbons(play, &nozzle, &axis, yaw, mode, sIsBlowing);
    SpawnStream(play, &nozzle, &axis, mode, sIsBlowing);
    StrikeWithNozzle(play, &nozzle, &axis, mode, sIsBlowing);
    if (sIsBlowing) {
        PushActors(play, &nozzle, &axis, mode);
        if (mode == GUST_MODE_ICE) {
            MeltRedIceInCone(play, &nozzle, &axis);
        }
    } else {
        PullActors(player, play, &nozzle, &axis, mode);
    }
    if ((sUseTimer % 8) == 1) {
        Audio_PlaySfx_AtPos(&nozzle, sIsBlowing ? NA_SE_PL_MAGIC_WIND_NORMAL : NA_SE_EV_WIND_TRAP - SFX_FLAG);
    }
}

static void StopBreathing(PlayState* play) {
    if (sUseTimer == 0) {
        return;
    }
    StopRibbons(play);
    RestoreShrunkActors();
    sNozzleCollider.base.atFlags &= ~(AT_ON | AT_HIT);
    sUseTimer = 0;
    sIsBlowing = false;
}

static void StopAiming(Player* player, PlayState* play) {
    Z64Aiming_Release(player, play);
    if (player->actionFunc == (PlayerActionFunc)GustAimAction) {
        func_8083B1A0(player, play);
    }
}

static void GustAimAction(Player* player, PlayState* play) {
    Z64Aiming_Update(player, play);
    Player_StopHorizontalMovement(player);
    Player_UpdateUpperBody(player, play);
    HoldJarWithBothHands(player, play);
    // The item's own update stops while this action owns the player, and a stalled item is put away.
    Z64Items_KeepHeld(sApi);
    if (IsButtonHeld(play)) {
        BreatheThroughJar(player, play);
    } else {
        StopBreathing(play);
    }
    if (FindEquippedButtonMask() == 0 || (play->state.input[0].press.button & (BTN_CUP | BTN_A | BTN_B))) {
        StopAiming(player, play);
    }
}

static void StartAiming(Player* player, PlayState* play) {
    if (!Z64Aiming_Request(player, play)) {
        return;
    }
    Player_SetAction(play, player, (PlayerActionFunc)GustAimAction, 1);
    Player_StopHorizontalMovement(player);
}

static void TakeOutJar(PlayState* play, Player* player) {
    if (!sIsColliderReady) {
        Collider_InitCylinder(play, &sNozzleCollider);
        Collider_SetCylinder(play, &sNozzleCollider, &player->actor, &sNozzleColliderInit);
        sIsColliderReady = true;
    }
    // The jar owns C-Up while it is in hand: unclaimed, the press talks to Tatl or beeps over the aim.
    if (S2H_MOD_API_HAS(sApi, BlockPlayerInput)) {
        sApi->BlockPlayerInput(GUST_JAR_KEY, BTN_CUP, false);
    }
    sIsJarOut = true;
    sUseTimer = 0;
    sIsBlowing = false;
    sFairiesPaidCount = 0;
}

// The carry frame is queued here and lands in sPoseJoints when the animation queue runs, so the pose applied is
// the previous frame's. Only the arms are borrowed, so Link keeps walking, running and turning as he was.
static void HoldJarWithBothHands(Player* player, PlayState* play) {
    static const u8 armLimbs[] = { PLAYER_LIMB_LEFT_SHOULDER,  PLAYER_LIMB_LEFT_FOREARM, PLAYER_LIMB_LEFT_HAND,
                                   PLAYER_LIMB_RIGHT_SHOULDER, PLAYER_LIMB_RIGHT_FOREARM, PLAYER_LIMB_RIGHT_HAND };

    AnimTaskQueue_AddLoadPlayerFrame(play, (PlayerAnimationHeader*)sCarryPoseAnim, GUST_JAR_POSE_FRAME, PLAYER_LIMB_MAX,
                                     sPoseJoints);
    for (u8 index = 0; index < ARRAY_COUNT(armLimbs); index++) {
        player->skelAnime.jointTable[armLimbs[index]] = sPoseJoints[armLimbs[index]];
    }
}

static s32 UseJar(Player* player, PlayState* play) {
    HoldJarWithBothHands(player, play);
    if (Z64Wheel_IsOpen() || player->actionFunc == (PlayerActionFunc)GustAimAction) {
        return 0;
    }
    if (play->state.input[0].press.button & BTN_CUP) {
        StartAiming(player, play);
        return 0;
    }
    if (!IsButtonHeld(play)) {
        StopBreathing(play);
        return 0;
    }
    BreatheThroughJar(player, play);
    return 0;
}

static void PutJarAway(Player* player, PlayState* play) {
    StopAiming(player, play);
    StopBreathing(play);
    if (S2H_MOD_API_HAS(sApi, ReleasePlayerInput)) {
        sApi->ReleasePlayerInput(GUST_JAR_KEY);
    }
    sIsJarOut = false;
}

// The jar sits between both hands of the carry pose, laid on its side to face where Link faces, and its band
// takes the colour of the breath: the mouth is the only part of the model that says what the jar is doing.
static void DrawJarInHand(Player* player, PlayState* play) {
    Vec3f* leftHand = &player->bodyPartsPos[PLAYER_BODYPART_LEFT_HAND];
    Vec3f* rightHand = &player->bodyPartsPos[PLAYER_BODYPART_RIGHT_HAND];
    Color_RGBA8 band = sElements[CurrentMode()].primary;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    Matrix_Translate((leftHand->x + rightHand->x) * 0.5f, (leftHand->y + rightHand->y) * 0.5f - GUST_JAR_HAND_DROP,
                     (leftHand->z + rightHand->z) * 0.5f, MTXMODE_NEW);
    Matrix_RotateYF(BINANG_TO_RAD(player->actor.shape.rot.y), MTXMODE_APPLY);
    Matrix_RotateXF(GUST_JAR_HALF_TURN / 2.0f, MTXMODE_APPLY);
    Matrix_Scale(GUST_JAR_HELD_SCALE, GUST_JAR_HELD_SCALE, GUST_JAR_HELD_SCALE, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sJarBodyDL);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, band.r, band.g, band.b, 255);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sJarDecorationDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

// The cone stands on its own base along +Y, so it is laid down onto the aim before it is stretched. The quarter
// turn that lays it down grows with the pitch, because a positive pitch aims below the horizon.
static void DrawTornado(Player* player, PlayState* play) {
    u8 mode = CurrentMode();
    Color_RGBA8 color = sElements[mode].primary;
    f32 reach = BreathReach(mode);
    f32 spread = IsBlowMode(mode) ? GUST_JAR_BLOW_RADIUS : GUST_JAR_SUCK_RADIUS;
    f32 grow = MIN(sUseTimer / 8.0f, 1.0f);
    s16 yaw;
    s16 pitch;
    Vec3f nozzle;
    Vec3f axis;

    GetAim(player, &yaw, &pitch);
    GetNozzle(player, &nozzle, &axis, yaw, pitch);

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sTornadoMaterialDL);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, color.r, color.g, color.b, (u8)(190.0f * grow));
    Matrix_Translate(nozzle.x, nozzle.y, nozzle.z, MTXMODE_NEW);
    Matrix_RotateYF(BINANG_TO_RAD(yaw), MTXMODE_APPLY);
    Matrix_RotateXF(GUST_JAR_HALF_TURN / 2.0f + BINANG_TO_RAD(pitch), MTXMODE_APPLY);
    Matrix_RotateYF(BINANG_TO_RAD((s16)(play->gameplayFrames * GUST_JAR_TORNADO_SPIN)), MTXMODE_APPLY);
    Matrix_Scale(spread / GUST_JAR_TORNADO_RADIUS * grow, reach / GUST_JAR_TORNADO_LENGTH * grow,
                 spread / GUST_JAR_TORNADO_RADIUS * grow, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sTornadoMeshDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawGustJar(Actor* actor, PlayState* play) {
    Player* player = (Player*)actor;

    if (!sIsJarOut) {
        return;
    }
    if (Z64Aiming_IsAiming()) {
        u8 mode = CurrentMode();
        Z64AimingReticleTint tint = IsBlowMode(mode) ? Z64AIMING_TINT_LAUNCH : Z64AIMING_TINT_ATTRACT;

        Z64Aiming_DrawReticle(play, player, BreathReach(mode), (Z64AimingReticleStyle){ Z64AIMING_TOGGLE_BOW, tint });
    } else if (!func_800B7118(player)) {
        DrawJarInHand(player, play);
    }
    if (sUseTimer > 0) {
        DrawTornado(player, play);
    }
}

static void UpdateUnlocks(Player* player) {
    if (gPlayState != NULL) {
        SyncUnlockedModes();
    }
}

// The scene tears down every actor and effect, so the ribbons and the borrowed scales are gone with it.
static void DropAimOnSceneChange(s16 sceneId, s8 spawnNum) {
    Z64Aiming_Drop();
    sIsJarOut = false;
    sUseTimer = 0;
    sAreRibbonsLive = false;
    sShrunkCount = 0;
    sFairiesPaidCount = 0;
}

// Shuffled once by the randomizer; no vanilla logic needs it.
static const S2HCustomItemRandomizer sRandomizerItem = {
    sizeof(S2HCustomItemRandomizer), 0, S2H_CUSTOM_ITEM_TYPE_ITEM, 0, 1, GUST_JAR_KEY, GUST_JAR_KEY, "",
};

S2H_MOD_EXPORT void ModSetApi(const S2HModApi* api) {
    sApi = api;
}

S2H_MOD_EXPORT const S2HModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

S2H_MOD_EXPORT void ModInit(void) {
    if (!S2H_MOD_API_HAS(sApi, StorageSet)) {
        return;
    }

    S2HCustomItemDefinition jar = Z64Items_Define(GUST_JAR_KEY, sModeIcons[GUST_MODE_SUCTION], sNameTex);

    Z64Items_SetButtons(&jar, S2H_CUSTOM_ITEM_C_BUTTON | S2H_CUSTOM_ITEM_DPAD);
    Z64Items_SetForms(&jar, S2H_CUSTOM_ITEM_FORM_BIT(PLAYER_FORM_HUMAN));
    Z64Items_SetPlacement(&jar, 0, 12, 0);
    Z64Items_SetTextbox(&jar, "You got the %rGust Jar%w!&A Minish jar that breathes in&whatever it can swallow.^"
                              "Hold a %yC button%w to pull in grass, pots&and small foes. Anything that&reaches the "
                              "mouth is cut down.^%yC-Up%w aims it. %yL%w turns the&wheel: every %gmedallion%w adds&a "
                              "breath of its own to blow.");
    Z64Items_SetPauseText(&jar, "%rGust Jar&%wHold a %yC button%w to suck things in.&%yC-Up%w aims, %yL%w picks the "
                                "breath.");
    Z64Items_SetAction(&jar, TakeOutJar, UseJar);
    Z64Items_SetHeldCallbacks(&jar, NULL, PutJarAway, NULL);
    Z64Items_SetGetItemModel(&jar, sJarDL);
    jar.getItemScale = GUST_JAR_HELD_SCALE / (0.015f * 30.0f);
    jar.modelGroup = PLAYER_MODELGROUP_DEFAULT;

    Z64Items_SetLogic(&jar, &sRandomizerItem);
    if (!Z64Items_Register(sApi, &jar)) {
        return;
    }
    Z64Wheel_Register(sApi, GUST_JAR_KEY, GUST_JAR_KEY, sWheelEntries, ARRAY_COUNT(sWheelEntries), false);
    S2H_REGISTER_HOOK(sApi, OnPlayerUpdate, UpdateUnlocks);
    S2H_REGISTER_HOOK(sApi, OnSceneInit, DropAimOnSceneChange);
    S2H_REGISTER_HOOK_FOR_ID(sApi, OnActorDrawEnd, ACTOR_PLAYER, DrawGustJar);
}
