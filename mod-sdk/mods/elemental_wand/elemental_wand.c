#include <math.h>
#include <string.h>

#include "z64items.h"
#include "z64logic.h"
#include "z64wheel.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "regs.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "overlays/actors/ovl_En_Bom/z_en_bom.h"

#define WAND_KEY "nei.elemental_wand"
#define WAND_SHUFFLE_OPTION "nei.wand_shuffle"
#define WAND_VISUAL_CHANNEL "nei.elemental_wand.visual"
#define WAND_PI 3.14159265f
#define WAND_DEGREES_TO_RADIANS (WAND_PI / 180.0f)

// Order: rotation Y, X, Z, then the offsets along the shaft's own axes, then scale. The staff is 350 units tall.
#define WAND_HELD_ROT_Y 180.0f
#define WAND_HELD_ROT_X 91.034f
#define WAND_HELD_ROT_Z 111.724f
#define WAND_HELD_OFFSET_Y 7.356f
#define WAND_HELD_OFFSET_Z (-3.218f)
#define WAND_HELD_SCALE 0.12f
#define WAND_GIVE_SCALE 0.3f

#define WAND_CAST_FRACTION 0.35f
#define WAND_GLOW_FRAMES 20
#define REMOTE_VISUAL_INTERVAL 5u

typedef enum {
    WAND_SAND,
    WAND_TORNADO,
    WAND_WATER,
    WAND_METEOR,
    WAND_STORM,
    WAND_SCEPTER,
    WAND_MODE_MAX,
} WandMode;

typedef enum {
    WAND_SHUFFLE_MEDALLIONS,
    WAND_SHUFFLE_SINGLE,
    WAND_SHUFFLE_RODS,
} WandShuffle;

typedef enum {
    WAND_SWING_NONE,
    WAND_SWING_FORE,
    WAND_SWING_BACK,
    WAND_SWING_THROW,
} WandSwingClip;

typedef struct {
    const char* opaModel;
    const char* xluModel;
    const char* medallionKey;
    s16 magicCost;
} WandRod;

static const ALIGN_ASSET(2) char sNameTex[] = "__OTR__textures/item_name_custom/gSandRodNameTex";
static const ALIGN_ASSET(2) char sIconSand[] = "__OTR__textures/icon_item_custom/gItemIconSandRodTex";
static const ALIGN_ASSET(2) char sIconTornado[] = "__OTR__textures/icon_item_custom/gItemIconTornadoRodTex";
static const ALIGN_ASSET(2) char sIconWater[] = "__OTR__textures/icon_item_custom/gItemIconWaterRodTex";
static const ALIGN_ASSET(2) char sIconMeteor[] = "__OTR__textures/icon_item_custom/gItemIconMeteorRodTex";
static const ALIGN_ASSET(2) char sIconStorm[] = "__OTR__textures/icon_item_custom/gItemIconStormRodTex";
static const ALIGN_ASSET(2) char sIconScepter[] = "__OTR__textures/icon_item_custom/gItemIconShadowScepterTex";

static const ALIGN_ASSET(2) char sSandOpa[] = "__OTR__objects/object_nei_wand_sand_rod/gNeiSandRodDL";
static const ALIGN_ASSET(2) char sSandXlu[] = "__OTR__objects/object_nei_wand_sand_rod/gNeiSandRodXluDL";
static const ALIGN_ASSET(2) char sTornadoOpa[] = "__OTR__objects/object_nei_wand_tornado_rod/gNeiTornadoRodDL";
static const ALIGN_ASSET(2) char sTornadoXlu[] = "__OTR__objects/object_nei_wand_tornado_rod/gNeiTornadoRodXluDL";
static const ALIGN_ASSET(2) char sWaterOpa[] = "__OTR__objects/object_nei_wand_water_rod/gNeiWaterRodDL";
static const ALIGN_ASSET(2) char sWaterXlu[] = "__OTR__objects/object_nei_wand_water_rod/gNeiWaterRodXluDL";
static const ALIGN_ASSET(2) char sMeteorOpa[] = "__OTR__objects/object_nei_wand_meteor_rod/gNeiMeteorRodDL";
static const ALIGN_ASSET(2) char sMeteorXlu[] = "__OTR__objects/object_nei_wand_meteor_rod/gNeiMeteorRodXluDL";
static const ALIGN_ASSET(2) char sStormOpa[] = "__OTR__objects/object_nei_wand_storm_rod/gNeiStormRodDL";
static const ALIGN_ASSET(2) char sStormXlu[] = "__OTR__objects/object_nei_wand_storm_rod/gNeiStormRodXluDL";
static const ALIGN_ASSET(2) char sScepterOpa[] = "__OTR__objects/object_nei_wand_shadow_scepter/gNeiShadowScepterDL";
static const ALIGN_ASSET(2) char sScepterXlu[] = "__OTR__objects/object_nei_wand_shadow_scepter/gNeiShadowScepterXluDL";

static const ALIGN_ASSET(2) char sTornadoMaterialDL[] = "__OTR__objects/object_nei_tornado/mat_tornado_f3dlite_tornado";
static const ALIGN_ASSET(2) char sTornadoMeshDL[] = "__OTR__objects/object_nei_tornado/tornado_mesh_tri_0";
static const ALIGN_ASSET(2) char sSlabDL[] = "__OTR__objects/object_d_lift/gDampeGraveBrownElevatorDL";
static const ALIGN_ASSET(2) char sWandHoverAnim[] = "__OTR__misc/link_animetion/nei_wand_hover";

#define FLIGHT_COST 12
#define FLIGHT_FRAMES 240
#define FLIGHT_STARTUP_FRAMES 4
#define FLIGHT_LIFT 35.0f
#define FLIGHT_RISE 3.0f
#define FLIGHT_SPEED 8.0f
#define FLIGHT_BOOST_SPEED 12.0f
#define SHADOW_COST 6
#define SHADOW_FRAMES 200
#define SHADOW_STEP 6.0f
#define SHADOW_FLOOR_REACH 40.0f

static const WandRod sRods[WAND_MODE_MAX] = {
    { sSandOpa, sSandXlu, "nei.medallion.spirit", 2 },
    { sTornadoOpa, sTornadoXlu, "nei.medallion.forest", FLIGHT_COST },
    { sWaterOpa, sWaterXlu, "nei.medallion.water", 2 },
    { sMeteorOpa, sMeteorXlu, "nei.medallion.fire", 3 },
    { sStormOpa, sStormXlu, "nei.medallion.light", 6 },
    { sScepterOpa, sScepterXlu, "nei.medallion.shadow", SHADOW_COST },
};

static const Z64WheelEntry sWheelEntries[WAND_MODE_MAX] = {
    { sIconSand, "Sand Rod" },     { sIconTornado, "Tornado Rod" }, { sIconWater, "Water Rod" },
    { sIconMeteor, "Meteor Rod" }, { sIconStorm, "Storm Rod" },     { sIconScepter, "Shadow Scepter" },
};

static const char* const sRodPickupKeys[WAND_MODE_MAX] = {
    "nei.wand.sand", "nei.wand.tornado", "nei.wand.water", "nei.wand.meteor", "nei.wand.storm", "nei.wand.shadow",
};
static const char* const sVariantNames[WAND_MODE_MAX] = { "sand", "tornado", "water", "meteor", "storm", "shadow" };
static const char* const sRodTexts[WAND_MODE_MAX] = {
    "You got the %ySand Rod%w! Its&sand raises a road beneath&you.",
    "You got the %gTornado Rod%w!&Start flight for %g12 MP%w. Move&"
    "with the stick; %y\xB0%w boosts. %y\xB1%w exits&with a jump slash.^Press the rod to rise higher.&"
    "Boosting and rising spend your&flight timer faster.",
    "You got the %bWater Rod%w! Call&up a column of water.",
    "You got the %rMeteor Rod%w!&Create a meteor bomb in your&left hand and throw it ahead.",
    "You got the %yStorm Rod%w!&Target a foe to strike it with&lightning from the sky.",
    "You got the %pShadow Scepter%w!&Become a shadow and cross&walls. Press again to return to&normal.^"
    "A hit or unsafe floor returns&you to where you started.",
};

static const char* const sShuffleValues[] = { "Medallions", "One item", "A rod per pickup" };

static const u16 sItemButtons[S2H_ITEM_BUTTON_COUNT] = { BTN_B,      BTN_CLEFT, BTN_CDOWN, BTN_CRIGHT, 0,
                                                         BTN_DRIGHT, BTN_DLEFT, BTN_DDOWN, BTN_DUP };

#define SAND_MAX_SLABS 8
#define SAND_SLAB_SCALE 0.05f
#define SAND_HOLD_INTERVAL 6
#define SAND_STEP_FRACTION 0.85f
#define SAND_FALLBACK_DIST 40.0f
#define SAND_CRUMBLE_FRAMES 24
#define SAND_DUST_EVERY 4
#define SAND_SFX_TICKS 20
#define SAND_ENV_R 214
#define SAND_ENV_G 178
#define SAND_ENV_B 112
// ObjLift reads a scene switch flag out of its params, and with params 0 that is flag 0.
#define SAND_SWITCH_FLAG 0
// The crumble countdown rides on home.rot.x: once the update below is installed only ObjLift's destroy runs.
#define SAND_CRUMBLE(actor) ((actor)->home.rot.x)

#define WATER_SPAWN_DIST 70.0f
#define WATER_RIDE_HEIGHT 240.0f
#define WATER_REST_HEIGHT 10.0f
#define WATER_STEP_SCALE 0.8f
#define WATER_STEP_MAX 3.0f
#define WATER_STEP_MIN 0.01f
#define WATER_BOB_RATE 0x800
#define WATER_BOB_AMPLITUDE 4.0f
#define WATER_ENV_R 60
#define WATER_ENV_G 150
#define WATER_ENV_B 225

#define METEOR_SPAWN_DIST 30.0f
#define METEOR_SPAWN_HEIGHT 28.0f
#define METEOR_LAUNCH_SPEED 28.0f
#define METEOR_LAUNCH_RISE 6.0f
#define METEOR_FUSE_FRAMES 170
#define METEOR_ARM_FRAMES 5
#define METEOR_HAND_FRAMES 8
#define METEOR_HAND_LIFT 5.0f
// The AT collider only exists while the bomb is ALREADY exploding, so enemy contact is proximity.
#define METEOR_ENEMY_TRIGGER_RADIUS 42.0f
#define METEOR_BOMB_SCALE 0.01f
#define METEOR_HOP_PERIOD 16.0f
#define METEOR_HOP_HEIGHT 46.0f
#define METEOR_HOP_DECAY 0.3f
#define METEOR_STEER_RATE 0x900
#define METEOR_TINT_R 235
#define METEOR_TINT_G 45
#define METEOR_TINT_B 30
#define METEOR_TINT_MIX 255
#define METEOR_HOP_TIME(actor) ((actor)->home.rot.x)
#define METEOR_AGE(actor) ((actor)->home.rot.z)
#define METEOR_AIRBORNE (-1)

#define STORM_OKARINA_PARAMS 1
#define STORM_SPAWN_Y_OFFSET (-30.0f)
#define STORM_RAY_SPEED 18.0f
#define STORM_RAY_LIFE 40
#define STORM_RAY_SKY_HEIGHT 120.0f
#define STORM_RAY_RADIUS 22
#define STORM_RAY_HEIGHT 30
#define STORM_RAY_DAMAGE 2
#define STORM_BOLT_SCALE 210
#define STORM_BOLT_LIFE 6
#define STORM_BOLT_BRANCHES 2
#define STORM_BOLT_EVERY 2
// The effect's yaw is a SCREEN-SPACE roll applied after billboarding, not a world direction, so the ray's own
// yaw left the sprite near upright. 0x4000 lays it along the ray instead.
#define STORM_BOLT_ROLL 0x4000
#define STORM_BOLT_ROLL_JITTER 4096.0f

static const u8 sEnemyCategories[2] = { ACTORCAT_ENEMY, ACTORCAT_BOSS };

typedef struct {
    int16_t scene;
    uint8_t active;
    uint8_t adult;
    uint8_t flags[8];
    float values[12];
} RemoteVisualState;

static const S2HModApi* sApi;
static bool sIsWandOut;
static bool sIsPressPending;
static bool sWasWandOut;
static MtxF sHandMatrix;
static bool sHasHandMatrix;
static s16 sLastScene = -1;
static s16 sRodGlow;
static Input sWandModeInput;

static u8 sSwingClip;
static u8 sSwingMode;
static bool sHasSwingFired;

static Actor* sSandSlabs[SAND_MAX_SLABS];
static u8 sSandNextSlot;
static s16 sSandHoldTimer;
static f32 sSandTopOffset;
static f32 sSandReach;
static bool sIsSandMeasured;

static bool sIsFlying;
static s16 sFlightTime;
static s16 sFlightStartupFrames;
static f32 sFlightGravity;

static Actor* sWaterGeyser;
static f32 sWaterBaseY;
static f32 sWaterCurrentHeight;
static f32 sWaterTargetHeight;

static ActorFunc sMeteorBombUpdate;
static ActorFunc sMeteorBombDraw;
static Actor* sHandMeteor;
static s16 sHandMeteorFrames;

static struct {
    Vec3f pos;
    Vec3f vel;
    s16 life;
    bool isActive;
} sStormRay;
static ColliderCylinder sStormRayCollider;
static bool sIsStormColliderReady;

static struct {
    bool isActive;
    s16 time;
    s16 hurtTimer;
    u8 warpFrames;
    f32 gravity;
    Vec3f returnPos;
    Vec3f pos;
} sShadow;

static Color_RGBA8 sStormBoltPrim = { 255, 255, 255, 255 };
static Color_RGBA8 sStormBoltEnv = { 255, 255, 60, 255 };
static Color_RGBA8 sSandDust = { SAND_ENV_R, SAND_ENV_G, SAND_ENV_B, 255 };

static ColliderCylinderInit sStormRayColliderInit = {
    { COL_MATERIAL_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_TYPE_PLAYER, COLSHAPE_CYLINDER },
    { ELEM_MATERIAL_UNK2,
      { DMG_ZORA_BOOMERANG, 0x00, STORM_RAY_DAMAGE },
      { 0, 0, 0 },
      ATELEM_ON | ATELEM_SFX_NORMAL,
      ACELEM_NONE,
      OCELEM_NONE },
    { STORM_RAY_RADIUS, STORM_RAY_HEIGHT, 0, { 0, 0, 0 } },
};

extern PlayerAnimationHeader* ResourceMgr_LoadPlayerAnimAsHeader(const char* animPath);
s32 Player_SetAction(PlayState* play, Player* player, PlayerActionFunc actionFunc, s32 arg3);
void Player_SetAction_PreserveItemAction(PlayState* play, Player* player, PlayerActionFunc actionFunc, s32 arg3);
void func_8083B1A0(Player* player, PlayState* play);
void func_808395F0(PlayState* play, Player* player, PlayerMeleeWeaponAnimation meleeWeaponAnim, f32 linearVelocity,
                   f32 yVelocity);
bool Player_IsZTargeting(Player* player);

static bool ReadRemoteVisual(Player* player, RemoteVisualState* visual);

static bool IsWandKey(const char* key) {
    return key != NULL && strcmp(key, WAND_KEY) == 0;
}

static u8 CurrentMode(void) {
    uint32_t selection = Z64Wheel_GetSelection();
    return selection < WAND_MODE_MAX ? (u8)selection : WAND_SAND;
}

static u8 ShuffleRule(void) {
    return S2H_MOD_API_HAS(sApi, GetRandoOption) ? sApi->GetRandoOption(WAND_SHUFFLE_OPTION) : WAND_SHUFFLE_MEDALLIONS;
}

static bool IsRodPickupOwned(u8 mode) {
    return sApi->IsCustomItemOwned(sRodPickupKeys[mode]);
}

static void SyncUnlockedRods(void) {
    u8 rule = ShuffleRule();
    bool hasStaff = rule == WAND_SHUFFLE_SINGLE && sApi->IsCustomItemOwned(WAND_KEY);
    for (u8 mode = 0; mode < WAND_MODE_MAX; mode++) {
        bool isOwned;
        switch (rule) {
            case WAND_SHUFFLE_SINGLE:
                isOwned = hasStaff || IsRodPickupOwned(mode);
                break;
            case WAND_SHUFFLE_RODS:
                isOwned = IsRodPickupOwned(mode);
                break;
            default:
                isOwned = sApi->IsCustomItemOwned(sRods[mode].medallionKey) || IsRodPickupOwned(mode);
                break;
        }
        Z64Wheel_SetEntryUnlocked(mode, isOwned);
    }
}

// The host already owns both the pickup and the staff; this only lights the rod and tunes the staff to it.
static void ReceiveRod(const char* itemKey, const char* variantKey) {
    for (u8 mode = 0; mode < WAND_MODE_MAX; mode++) {
        if (strcmp(variantKey, sVariantNames[mode]) == 0) {
            Z64Wheel_SetEntryUnlocked(mode, true);
            Z64Wheel_SetSelection(mode);
            return;
        }
    }
}

static void LightOwnedRods(const char* key) {
    SyncUnlockedRods();
}

static u16 FindButtonMask(void) {
    if (gPlayState != NULL) {
        u8 held = GET_PLAYER(gPlayState)->heldItemButton;
        if (held < S2H_ITEM_BUTTON_COUNT && IsWandKey(sApi->GetEquippedCustomItem(held))) {
            return sItemButtons[held];
        }
    }
    for (u8 button = 0; button < S2H_ITEM_BUTTON_COUNT; button++) {
        if (IsWandKey(sApi->GetEquippedCustomItem(button))) {
            return sItemButtons[button];
        }
    }
    return 0;
}

static bool IsButtonHeld(PlayState* play) {
    u16 mask = FindButtonMask();
    return mask != 0 && (play->state.input[0].cur.button & mask) != 0;
}

static bool HasLockOn(Player* player) {
    return Player_IsZTargeting(player) && player->focusActor != NULL && player->focusActor->update != NULL;
}

static bool SpendMagic(PlayState* play, s16 cost) {
    if (cost <= 0) {
        return true;
    }
    if (!Magic_Consume(play, cost, MAGIC_CONSUME_NOW)) {
        return false;
    }
    Magic_Reset(play);
    return true;
}

static bool HasMagic(PlayState* play, s16 cost) {
    return cost <= 0 || gSaveContext.save.saveInfo.playerData.magic >= cost;
}

static Actor* FindNearestEnemy(PlayState* play, Vec3f* pos, f32 range) {
    f32 rangeSq = SQ(range);
    for (u8 index = 0; index < ARRAY_COUNT(sEnemyCategories); index++) {
        for (Actor* actor = play->actorCtx.actorLists[sEnemyCategories[index]].first; actor != NULL;
             actor = actor->next) {
            if (actor->update != NULL && Math3D_Vec3fDistSq(&actor->world.pos, pos) < rangeSq) {
                return actor;
            }
        }
    }
    return NULL;
}

static void SpawnDust(PlayState* play, Vec3f* center, f32 minRadius, f32 maxRadius, u8 count, Color_RGBA8* color) {
    Vec3f velocity = { 0.0f, 0.6f, 0.0f };
    Vec3f accel = { 0.0f, -0.02f, 0.0f };
    Color_RGBA8 fade = { color->r, color->g, color->b, 0 };
    for (u8 index = 0; index < count; index++) {
        f32 angle = Rand_ZeroOne() * (2.0f * WAND_PI);
        f32 radius = minRadius + Rand_ZeroOne() * (maxRadius - minRadius);
        Vec3f pos = { center->x + cosf(angle) * radius, center->y, center->z + sinf(angle) * radius };
        func_800B0DE0(play, &pos, &velocity, &accel, color, &fade, 60, 12);
    }
}

static void MeasureSlab(PlayState* play, Actor* slab) {
    s32 bgId = ((DynaPolyActor*)slab)->bgId;
    if (sIsSandMeasured || bgId < 0 || bgId >= BG_ACTOR_MAX) {
        return;
    }
    CollisionHeader* header = play->colCtx.dyna.bgActors[bgId].colHeader;
    if (header == NULL) {
        return;
    }
    f32 halfX = (f32)(header->maxBounds.x - header->minBounds.x) * 0.5f * SAND_SLAB_SCALE;
    f32 halfZ = (f32)(header->maxBounds.z - header->minBounds.z) * 0.5f * SAND_SLAB_SCALE;
    sSandTopOffset = (f32)header->maxBounds.y * SAND_SLAB_SCALE;
    sSandReach = MIN(halfX, halfZ);
    sIsSandMeasured = true;
}

static void NextSlabSpot(Player* player, Vec3f* out) {
    s16 yaw = player->actor.shape.rot.y;
    f32 distance = sIsSandMeasured ? sSandReach * SAND_STEP_FRACTION : SAND_FALLBACK_DIST;
    out->x = player->actor.world.pos.x + Math_SinS(yaw) * distance;
    out->y = player->actor.world.pos.y - sSandTopOffset;
    out->z = player->actor.world.pos.z + Math_CosS(yaw) * distance;
}

static bool IsSpotCovered(Vec3f* spot) {
    for (u8 index = 0; index < SAND_MAX_SLABS; index++) {
        Actor* slab = sSandSlabs[index];
        if (slab == NULL || SAND_CRUMBLE(slab) != 0) {
            continue;
        }
        if (SQ(slab->world.pos.x - spot->x) + SQ(slab->world.pos.z - spot->z) < SQ(sSandReach)) {
            return true;
        }
    }
    return false;
}

static void ForgetSlab(Actor* slab) {
    for (u8 index = 0; index < SAND_MAX_SLABS; index++) {
        if (sSandSlabs[index] == slab) {
            sSandSlabs[index] = NULL;
        }
    }
}

// The countdown never touches actor->scale, only the draw: the dynapoly is built from the actor's transform.
static void UpdateSlab(Actor* thisx, PlayState* play) {
    if (SAND_CRUMBLE(thisx) == 0) {
        if (!DynaPolyActor_IsPlayerOnTop((DynaPolyActor*)thisx)) {
            return;
        }
        SAND_CRUMBLE(thisx) = SAND_CRUMBLE_FRAMES;
        SoundSource_PlaySfxAtFixedWorldPos(play, &thisx->world.pos, SAND_SFX_TICKS, NA_SE_EV_FALL_DOWN_DIRT);
    }
    SAND_CRUMBLE(thisx)--;
    if ((SAND_CRUMBLE(thisx) % SAND_DUST_EVERY) == 0) {
        SpawnDust(play, &thisx->world.pos, 10.0f, 40.0f, 3, &sSandDust);
    }
    if (SAND_CRUMBLE(thisx) <= 0) {
        ForgetSlab(thisx);
        Actor_Kill(thisx);
    }
}

static void DrawSlab(Actor* thisx, PlayState* play) {
    f32 remaining = SAND_CRUMBLE(thisx) <= 0 ? 1.0f : (f32)SAND_CRUMBLE(thisx) / (f32)SAND_CRUMBLE_FRAMES;
    OPEN_DISPS(play->state.gfxCtx);
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetEnvColor(POLY_OPA_DISP++, SAND_ENV_R, SAND_ENV_G, SAND_ENV_B, 255);
    CLOSE_DISPS(play->state.gfxCtx);
    Matrix_Scale(remaining, remaining, remaining, MTXMODE_APPLY);
    Gfx_DrawDListOpa(play, (Gfx*)sSlabDL);
}

static void ForgetSandSlabs(void) {
    memset(sSandSlabs, 0, sizeof(sSandSlabs));
    sSandNextSlot = 0;
    sSandHoldTimer = 0;
}

static void EvictSlab(u8 slot) {
    Actor* slab = sSandSlabs[slot];
    if (slab != NULL && SAND_CRUMBLE(slab) == 0) {
        SAND_CRUMBLE(slab) = SAND_CRUMBLE_FRAMES;
    }
    sSandSlabs[slot] = NULL;
}

// ObjLift_Init kills itself when the flag is already set, and nothing runs between the unset and the spawn.
static Actor* SpawnLift(PlayState* play, Vec3f* pos, s16 yaw) {
    bool wasFlagSet = Flags_GetSwitch(play, SAND_SWITCH_FLAG) != 0;
    if (wasFlagSet) {
        Flags_UnsetSwitch(play, SAND_SWITCH_FLAG);
    }
    Actor* lift = Actor_Spawn(&play->actorCtx, play, ACTOR_OBJ_LIFT, pos->x, pos->y, pos->z, 0, yaw, 0, 0);
    if (wasFlagSet) {
        Flags_SetSwitch(play, SAND_SWITCH_FLAG);
    }
    if (lift == NULL || lift->update == NULL) {
        return NULL;
    }
    lift->room = -1;
    return lift;
}

static bool EnsureLiftObject(PlayState* play) {
    if (Object_GetSlot(&play->objectCtx, OBJECT_D_LIFT) >= 0) {
        return true;
    }
    Object_SpawnPersistent(&play->objectCtx, OBJECT_D_LIFT);
    return false;
}

static bool CastSand(Player* player, PlayState* play) {
    Vec3f pos;
    if (!EnsureLiftObject(play)) {
        return false;
    }
    NextSlabSpot(player, &pos);
    Actor* slab = SpawnLift(play, &pos, player->actor.shape.rot.y);
    if (slab == NULL) {
        return false;
    }
    // destroy is left alone: ObjLift_Destroy is what unregisters the dynapoly.
    slab->update = UpdateSlab;
    slab->draw = DrawSlab;
    Actor_SetScale(slab, SAND_SLAB_SCALE);
    SAND_CRUMBLE(slab) = 0;
    MeasureSlab(play, slab);
    if (sIsSandMeasured) {
        NextSlabSpot(player, &pos);
        slab->world.pos = pos;
    }
    EvictSlab(sSandNextSlot);
    sSandSlabs[sSandNextSlot] = slab;
    sSandNextSlot = (u8)((sSandNextSlot + 1) % SAND_MAX_SLABS);
    SpawnDust(play, &pos, 8.0f, 30.0f, 4, &sSandDust);
    SoundSource_PlaySfxAtFixedWorldPos(play, &pos, SAND_SFX_TICKS, NA_SE_EV_LAND_DIRT);
    return true;
}

static bool ShouldLaySlab(Player* player, bool isHeld) {
    Vec3f spot;
    if (!isHeld) {
        sSandHoldTimer = 0;
        return false;
    }
    if (--sSandHoldTimer > 0) {
        return false;
    }
    sSandHoldTimer = SAND_HOLD_INTERVAL;
    NextSlabSpot(player, &spot);
    return !IsSpotCovered(&spot);
}

static void UpdateGeyser(Actor* thisx, PlayState* play) {
    f32 bob = Math_SinS((s16)(play->gameplayFrames * WATER_BOB_RATE)) * WATER_BOB_AMPLITUDE;
    Math_SmoothStepToF(&sWaterCurrentHeight, sWaterTargetHeight, WATER_STEP_SCALE, WATER_STEP_MAX, WATER_STEP_MIN);
    thisx->world.pos.y = sWaterBaseY + sWaterCurrentHeight + bob;
    Actor_PlaySfx_Flagged2(thisx, NA_SE_EV_FOUNTAIN - SFX_FLAG);
}

static void DrawGeyser(Actor* thisx, PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetEnvColor(POLY_OPA_DISP++, WATER_ENV_R, WATER_ENV_G, WATER_ENV_B, 255);
    CLOSE_DISPS(play->state.gfxCtx);
    Gfx_DrawDListOpa(play, (Gfx*)sSlabDL);
}

static bool CastWater(Player* player, PlayState* play) {
    s16 yaw = player->actor.shape.rot.y;
    if (sWaterGeyser != NULL && sWaterGeyser->update == UpdateGeyser) {
        sWaterTargetHeight = sWaterTargetHeight > WATER_REST_HEIGHT ? WATER_REST_HEIGHT : WATER_RIDE_HEIGHT;
        return true;
    }
    if (!EnsureLiftObject(play)) {
        return false;
    }
    Vec3f pos = { player->actor.world.pos.x + Math_SinS(yaw) * WATER_SPAWN_DIST, player->actor.world.pos.y,
                  player->actor.world.pos.z + Math_CosS(yaw) * WATER_SPAWN_DIST };
    sWaterGeyser = SpawnLift(play, &pos, yaw);
    if (sWaterGeyser == NULL) {
        return false;
    }
    sWaterGeyser->update = UpdateGeyser;
    sWaterGeyser->draw = DrawGeyser;
    Actor_SetScale(sWaterGeyser, SAND_SLAB_SCALE);
    sWaterBaseY = pos.y;
    sWaterCurrentHeight = 0.0f;
    sWaterTargetHeight = WATER_REST_HEIGHT;
    return true;
}

// Wrapped, not replaced: EnBom_Draw also billboards and updates the spheres that give the explosion its hitbox.
static void DrawTintedBomb(Actor* thisx, PlayState* play) {
    if (sMeteorBombDraw == NULL) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    gDPSetGrayscaleColor(POLY_OPA_DISP++, METEOR_TINT_R, METEOR_TINT_G, METEOR_TINT_B, METEOR_TINT_MIX);
    gSPGrayscale(POLY_OPA_DISP++, true);
    gDPSetGrayscaleColor(POLY_XLU_DISP++, METEOR_TINT_R, METEOR_TINT_G, METEOR_TINT_B, METEOR_TINT_MIX);
    gSPGrayscale(POLY_XLU_DISP++, true);
    CLOSE_DISPS(play->state.gfxCtx);
    sMeteorBombDraw(thisx, play);
    OPEN_DISPS(play->state.gfxCtx);
    gSPGrayscale(POLY_OPA_DISP++, false);
    gSPGrayscale(POLY_XLU_DISP++, false);
    CLOSE_DISPS(play->state.gfxCtx);
}

static Actor* MeteorTarget(PlayState* play) {
    Player* player = GET_PLAYER(play);
    return HasLockOn(player) ? player->focusActor : NULL;
}

static bool IsMeteorTouchingEnemy(Actor* thisx, PlayState* play) {
    Vec3f shadow = { thisx->world.pos.x, thisx->floorHeight, thisx->world.pos.z };
    if (thisx->floorHeight <= BGCHECK_Y_MIN) {
        shadow.y = thisx->world.pos.y;
    }
    return FindNearestEnemy(play, &shadow, METEOR_ENEMY_TRIGGER_RADIUS) != NULL;
}

// speed is re-stamped, not just seeded: EnBom_Move brakes while grounded, and a bomb pinned near the floor
// is grounded on almost every frame.
static void HopMeteor(Actor* thisx) {
    f32 phase = (f32)METEOR_HOP_TIME(thisx) / METEOR_HOP_PERIOD;
    s16 hopIndex = (s16)((f32)METEOR_HOP_TIME(thisx) / METEOR_HOP_PERIOD);
    f32 falloff = 1.0f / (1.0f + METEOR_HOP_DECAY * (f32)hopIndex);
    thisx->world.pos.y = thisx->floorHeight + fabsf(sinf(WAND_PI * phase)) * METEOR_HOP_HEIGHT * falloff;
    thisx->velocity.y = 0.0f;
    thisx->speed = METEOR_LAUNCH_SPEED * falloff;
    METEOR_HOP_TIME(thisx)++;
}

static void UpdateMeteor(Actor* thisx, PlayState* play) {
    if (thisx == sHandMeteor && sHandMeteorFrames > 0) {
        return;
    }
    // Read before the vanilla update: EnBom_Move consumes both bits as it bounces.
    bool hasHitWall = (thisx->bgCheckFlags & BGCHECKFLAG_WALL) != 0;
    bool hasLanded = (thisx->bgCheckFlags & BGCHECKFLAG_GROUND_TOUCH) != 0;
    bool isBody = thisx->params == BOMB_TYPE_BODY;
    if (isBody) {
        if (METEOR_AGE(thisx) < METEOR_ARM_FRAMES) {
            METEOR_AGE(thisx)++;
        } else if (hasHitWall || IsMeteorTouchingEnemy(thisx, play)) {
            ((EnBom*)thisx)->timer = 0;
        }
        if (hasLanded && METEOR_HOP_TIME(thisx) == METEOR_AIRBORNE) {
            METEOR_HOP_TIME(thisx) = 0;
        }
        Actor* target = MeteorTarget(play);
        if (target != NULL) {
            Math_ScaledStepToS(&thisx->world.rot.y, Math_Vec3f_Yaw(&thisx->world.pos, &target->focus.pos),
                               METEOR_STEER_RATE);
        }
    }
    sMeteorBombUpdate(thisx, play);
    if (isBody && METEOR_HOP_TIME(thisx) != METEOR_AIRBORNE && thisx->floorHeight > BGCHECK_Y_MIN) {
        HopMeteor(thisx);
    }
}

static void PlaceMeteorInHand(Player* player) {
    sHandMeteor->world.pos = player->bodyPartsPos[PLAYER_BODYPART_LEFT_HAND];
    sHandMeteor->world.pos.y += METEOR_HAND_LIFT;
}

static void PlayThrow(PlayState* play, Player* player) {
    PlayerAnimationHeader* throwAnim = ResourceMgr_LoadPlayerAnimAsHeader(gPlayerAnim_link_normal_throw_free);
    if (throwAnim == NULL) {
        return;
    }
    PlayerAnimation_PlayOnce(play, &player->skelAnimeUpper, throwAnim);
    sSwingClip = WAND_SWING_THROW;
}

// The bomb is born in the left hand and held there for a few frames before the throw lets it go.
static bool CastMeteor(Player* player, PlayState* play) {
    Actor* target = MeteorTarget(play);
    s16 yaw = target != NULL ? Math_Vec3f_Yaw(&player->actor.world.pos, &target->focus.pos) : player->actor.shape.rot.y;
    Actor* bomb =
        Actor_Spawn(&play->actorCtx, play, ACTOR_EN_BOM, player->actor.world.pos.x + Math_SinS(yaw) * METEOR_SPAWN_DIST,
                    player->actor.world.pos.y + METEOR_SPAWN_HEIGHT,
                    player->actor.world.pos.z + Math_CosS(yaw) * METEOR_SPAWN_DIST, 0, yaw, 0, BOMB_TYPE_BODY);
    if (bomb == NULL) {
        return false;
    }
    if (sMeteorBombUpdate == NULL) {
        sMeteorBombUpdate = bomb->update;
        sMeteorBombDraw = bomb->draw;
    }
    bomb->update = UpdateMeteor;
    bomb->draw = DrawTintedBomb;
    ((EnBom*)bomb)->timer = METEOR_FUSE_FRAMES;
    Actor_SetScale(bomb, METEOR_BOMB_SCALE);
    METEOR_HOP_TIME(bomb) = METEOR_AIRBORNE;
    METEOR_AGE(bomb) = 0;
    bomb->speed = 0.0f;
    bomb->velocity.y = 0.0f;
    bomb->world.rot.y = yaw;
    sHandMeteor = bomb;
    sHandMeteorFrames = METEOR_HAND_FRAMES;
    PlaceMeteorInHand(player);
    PlayThrow(play, player);
    return true;
}

static void ThrowHandMeteor(Player* player) {
    if (sHandMeteor == NULL) {
        return;
    }
    if (sHandMeteor->update == NULL) {
        sHandMeteor = NULL;
        return;
    }
    if (sHandMeteorFrames > 0) {
        sHandMeteorFrames--;
        PlaceMeteorInHand(player);
        sHandMeteor->speed = 0.0f;
        sHandMeteor->velocity.y = 0.0f;
        ((EnBom*)sHandMeteor)->timer = METEOR_FUSE_FRAMES;
        return;
    }
    sHandMeteor->speed = METEOR_LAUNCH_SPEED;
    sHandMeteor->velocity.y = METEOR_LAUNCH_RISE;
    sHandMeteor->world.rot.y = player->actor.shape.rot.y;
    Audio_PlaySfx_AtPos(&sHandMeteor->world.pos, NA_SE_PL_THROW);
    sHandMeteor = NULL;
}

static void ForgetMeteorActor(Actor* actor) {
    if (actor == sHandMeteor) {
        sHandMeteor = NULL;
        sHandMeteorFrames = 0;
    }
}

static void TickStormRay(PlayState* play, Player* player) {
    if (!sStormRay.isActive) {
        return;
    }
    if (--sStormRay.life <= 0 || (sStormRayCollider.base.atFlags & AT_HIT)) {
        sStormRayCollider.base.atFlags &= ~AT_HIT;
        sStormRay.isActive = false;
        return;
    }
    sStormRay.pos.x += sStormRay.vel.x;
    sStormRay.pos.y += sStormRay.vel.y;
    sStormRay.pos.z += sStormRay.vel.z;
    if ((sStormRay.life % STORM_BOLT_EVERY) == 0) {
        s16 roll = STORM_BOLT_ROLL + (s16)Rand_CenteredFloat(STORM_BOLT_ROLL_JITTER);
        EffectSsLightning_Spawn(play, &sStormRay.pos, &sStormBoltPrim, &sStormBoltEnv, STORM_BOLT_SCALE, roll,
                                STORM_BOLT_LIFE, STORM_BOLT_BRANCHES);
    }
    if (!sIsStormColliderReady) {
        Collider_InitCylinder(play, &sStormRayCollider);
        Collider_SetCylinder(play, &sStormRayCollider, &player->actor, &sStormRayColliderInit);
        sIsStormColliderReady = true;
    }
    Collider_UpdateCylinder(&player->actor, &sStormRayCollider);
    sStormRayCollider.dim.pos.x = (s16)sStormRay.pos.x;
    sStormRayCollider.dim.pos.y = (s16)sStormRay.pos.y;
    sStormRayCollider.dim.pos.z = (s16)sStormRay.pos.z;
    CollisionCheck_SetAT(play, &play->colChkCtx, &sStormRayCollider.base);
}

// The bolt falls from the sky straight onto the target instead of flying out of the rod.
static bool StrikeTarget(Player* player, Actor* target) {
    if (sStormRay.isActive) {
        return false;
    }
    sStormRay.pos = target->focus.pos;
    sStormRay.pos.y += STORM_RAY_SKY_HEIGHT;
    sStormRay.vel.x = 0.0f;
    sStormRay.vel.y = -STORM_RAY_SPEED;
    sStormRay.vel.z = 0.0f;
    sStormRay.life = STORM_RAY_LIFE;
    sStormRay.isActive = true;
    Audio_PlaySfx_AtPos(&player->actor.world.pos, NA_SE_IT_MAGIC_ARROW_SHOT);
    return true;
}

// En_Okarina_Effect kills itself on Init while it is raining, so a NULL there is a legitimate "not now" and
// the cast reports failure rather than eating the magic.
static bool CallStorm(Player* player, PlayState* play) {
    Actor* storm = Actor_Spawn(&play->actorCtx, play, ACTOR_EN_OKARINA_EFFECT, player->actor.world.pos.x,
                               player->actor.world.pos.y + STORM_SPAWN_Y_OFFSET, player->actor.world.pos.z, 0, 0, 0,
                               STORM_OKARINA_PARAMS);
    return storm != NULL;
}

static bool CastStorm(Player* player, PlayState* play) {
    if (HasLockOn(player)) {
        return StrikeTarget(player, player->focusActor);
    }
    return CallStorm(player, play);
}

static void EnsureHoverPose(PlayState* play, Player* player) {
    PlayerAnimationHeader* hover = ResourceMgr_LoadPlayerAnimAsHeader(sWandHoverAnim);
    if (hover != NULL && player->skelAnime.animation != (void*)hover) {
        PlayerAnimation_PlayLoop(play, &player->skelAnime, hover);
    }
}

static void EndFlight(PlayState* play, Player* player, bool slash) {
    sIsFlying = false;
    sFlightStartupFrames = 0;
    player->actor.gravity = sFlightGravity;
    func_8083B1A0(player, play);
    if (slash) {
        func_808395F0(play, player, PLAYER_MWA_JUMPSLASH_START, 5.0f, 5.0f);
    }
}

static Vec3f GetStickStep(PlayState* play, Input* input, f32 speed) {
    f32 x = input->cur.stick_x / 80.0f;
    f32 y = input->cur.stick_y / 80.0f;
    f32 magnitude = sqrtf(SQ(x) + SQ(y));
    if (magnitude > 1.0f) {
        x /= magnitude;
        y /= magnitude;
    }
    s16 cameraYaw = Camera_GetInputDirYaw(GET_ACTIVE_CAM(play));
    Vec3f step = { (-x * Math_CosS(cameraYaw) + y * Math_SinS(cameraYaw)) * speed, 0.0f,
                   (y * Math_CosS(cameraYaw) + x * Math_SinS(cameraYaw)) * speed };
    return step;
}

static bool IsFlightInterrupted(Player* player) {
    return !sIsFlying || sFlightTime <= 0 ||
           (player->stateFlags1 &
            (PLAYER_STATE1_DEAD | PLAYER_STATE1_4000000 | PLAYER_STATE1_20000000 | PLAYER_STATE1_8000000));
}

// Reads the unfiltered buttons: A and B are taken from vanilla for the whole flight.
static void FlightAction(Player* player, PlayState* play) {
    Input* input = &sWandModeInput;
    bool isBoosting = (input->cur.button & BTN_A) != 0;
    Z64Items_KeepHeld(sApi);
    EnsureHoverPose(play, player);
    if (IsFlightInterrupted(player)) {
        EndFlight(play, player, false);
        return;
    }
    if (input->press.button & BTN_B) {
        EndFlight(play, player, true);
        return;
    }
    sFlightTime -= isBoosting ? 3 : 1;
    player->actor.velocity.y = 0.0f;
    player->actor.gravity = 0.0f;
    if (input->cur.button & FindButtonMask()) {
        player->actor.world.pos.y += FLIGHT_RISE * R_UPDATE_RATE / 3.0f;
        sFlightTime -= 2;
    }
    Vec3f step = GetStickStep(play, input, (isBoosting ? FLIGHT_BOOST_SPEED : FLIGHT_SPEED) * R_UPDATE_RATE * 0.5f);
    player->actor.world.pos.x += step.x;
    player->actor.world.pos.z += step.z;
    player->speedXZ = 0.0f;
    player->actor.speed = 0.0f;
    PlayerAnimation_Update(play, &player->skelAnime);
}

static bool StartFlight(Player* player, PlayState* play) {
    if (sIsFlying) {
        return true;
    }
    PlayerAnimationHeader* hover = ResourceMgr_LoadPlayerAnimAsHeader(sWandHoverAnim);
    if (hover == NULL || !HasMagic(play, FLIGHT_COST) || !SpendMagic(play, FLIGHT_COST)) {
        return false;
    }
    sFlightGravity = player->actor.gravity;
    sIsFlying = true;
    sFlightTime = FLIGHT_FRAMES;
    sRodGlow = WAND_GLOW_FRAMES;
    sFlightStartupFrames = FLIGHT_STARTUP_FRAMES;
    player->actor.world.pos.y += FLIGHT_LIFT;
    player->actor.prevPos = player->actor.world.pos;
    player->actor.velocity.y = 0.0f;
    player->actor.gravity = 0.0f;
    player->actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND;
    player->stateFlags3 |= PLAYER_STATE3_2;
    player->itemAction = player->heldItemAction;
    Player_SetAction_PreserveItemAction(play, player, FlightAction, 0);
    PlayerAnimation_PlayLoop(play, &player->skelAnime, hover);
    return true;
}

// The first frames re-assert the held staff: changing the action can drop the custom item from the hand.
static void KeepFlightStarted(PlayState* play, Player* player) {
    sFlightStartupFrames--;
    player->heldItemId = ITEM_CUSTOM;
    player->heldItemAction = PLAYER_IA_CUSTOM;
    player->itemAction = PLAYER_IA_CUSTOM;
    if (CustomItemRegistry_GetHeldKey() == NULL) {
        CustomItemRegistry_InitHeld(play, player);
    }
    Player_SetAction_PreserveItemAction(play, player, FlightAction, 0);
    Z64Items_KeepHeld(sApi);
    player->stateFlags3 |= PLAYER_STATE3_2;
    player->actor.velocity.y = 0.0f;
    player->actor.gravity = 0.0f;
    EnsureHoverPose(play, player);
}

static void TickFlight(PlayState* play, Player* player) {
    Z64Items_KeepHeld(sApi);
    if (sFlightStartupFrames > 0) {
        KeepFlightStarted(play, player);
        return;
    }
    if (player->actionFunc != FlightAction) {
        EndFlight(play, player, false);
        return;
    }
    player->actor.gravity = 0.0f;
    player->actor.velocity.y = 0.0f;
    player->actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND;
}

static bool FindShadowFloor(PlayState* play, Player* player, Vec3f* position, f32* floorY) {
    CollisionPoly* floor = NULL;
    s32 bgId;
    Vec3f probe = *position;
    probe.y += SHADOW_FLOOR_REACH;
    *floorY = BgCheck_EntityRaycastFloor5_2(play, &play->colCtx, &floor, &bgId, &player->actor, &probe);
    return floor != NULL && *floorY > BGCHECK_Y_MIN && fabsf(position->y - *floorY) <= SHADOW_FLOOR_REACH;
}

static bool IsShadowExitClear(PlayState* play, Player* player, Vec3f* position, f32* floorY) {
    if (!FindShadowFloor(play, player, position, floorY)) {
        return false;
    }
    Vec3f origin = *position;
    origin.y = *floorY + 35.0f;
    for (s32 i = 0; i < 4; i++) {
        Vec3f end = origin;
        Vec3f hit;
        CollisionPoly* poly = NULL;
        s32 bgId;
        end.x += Math_SinS((s16)(i * 0x4000)) * 18.0f;
        end.z += Math_CosS((s16)(i * 0x4000)) * 18.0f;
        if (BgCheck_AnyLineTest3(&play->colCtx, &origin, &end, &hit, &poly, true, false, false, true, &bgId)) {
            return false;
        }
    }
    return true;
}

// A manual exit keeps the spot the shadow reached when Link fits there; anything else sends him back.
static void EndShadow(Player* player, PlayState* play, bool manual) {
    f32 floorY;
    Vec3f destination = sShadow.returnPos;
    if (manual && IsShadowExitClear(play, player, &sShadow.pos, &floorY)) {
        destination = sShadow.pos;
        destination.y = floorY;
    }
    sShadow.isActive = false;
    sShadow.warpFrames = 2;
    player->actor.world.pos = destination;
    player->actor.prevPos = destination;
    player->actor.velocity = (Vec3f){ 0.0f, 0.0f, 0.0f };
    player->speedXZ = 0.0f;
    player->actor.speed = 0.0f;
    player->actor.gravity = sShadow.gravity;
    player->fallStartHeight = (s16)destination.y;
    player->fallDistance = 0;
    func_8083B1A0(player, play);
}

static bool ToggleShadow(Player* player, PlayState* play) {
    f32 floorY;
    if (sShadow.isActive) {
        EndShadow(player, play, true);
        return true;
    }
    if (!HasMagic(play, SHADOW_COST) || !FindShadowFloor(play, player, &player->actor.world.pos, &floorY) ||
        !SpendMagic(play, SHADOW_COST)) {
        return false;
    }
    sShadow.returnPos = player->actor.world.pos;
    sShadow.pos = player->actor.world.pos;
    sShadow.hurtTimer = player->invincibilityTimer;
    sShadow.gravity = player->actor.gravity;
    sShadow.isActive = true;
    sShadow.time = SHADOW_FRAMES;
    player->speedXZ = 0.0f;
    player->actor.speed = 0.0f;
    return true;
}

// The real actor follows the shadow so native cameras, room transitions and focus keep tracking it.
static void TickShadowMode(Player* player, PlayState* play) {
    Input* input = &sWandModeInput;
    f32 floorY;
    Z64Items_KeepHeld(sApi);
    if (--sShadow.time <= 0 || player->invincibilityTimer > sShadow.hurtTimer ||
        (player->stateFlags1 & (PLAYER_STATE1_4000000 | PLAYER_STATE1_8000000))) {
        EndShadow(player, play, false);
        return;
    }
    if (input->press.button & FindButtonMask()) {
        EndShadow(player, play, true);
        return;
    }
    Vec3f step = GetStickStep(play, input, SHADOW_STEP);
    Vec3f desired = { sShadow.pos.x + step.x, sShadow.pos.y, sShadow.pos.z + step.z };
    if (!FindShadowFloor(play, player, &desired, &floorY)) {
        EndShadow(player, play, false);
        return;
    }
    desired.y = floorY;
    sShadow.pos = desired;
    player->actor.world.pos = desired;
    player->actor.prevPos = desired;
    player->actor.focus.pos = desired;
    player->actor.focus.pos.y += 40.0f;
    player->actor.velocity = (Vec3f){ 0.0f, 0.0f, 0.0f };
    player->actor.gravity = 0.0f;
    player->speedXZ = 0.0f;
    player->actor.speed = 0.0f;
}

static void FilterWandModeInput(Player* player, Input* input) {
    const u16 takenButtons = BTN_A | BTN_B | BTN_R;
    sWandModeInput = *input;
    if (sIsFlying || sShadow.isActive) {
        input->cur.button &= ~takenButtons;
        input->press.button &= ~takenButtons;
    }
    if (sShadow.isActive) {
        input->cur.stick_x = 0;
        input->cur.stick_y = 0;
        input->rel.stick_x = 0;
        input->rel.stick_y = 0;
    }
}

// The shadow slips through walls: wall checks stay off while it moves and for the frames that settle the exit.
static void ShadowWarpCollision(Actor* actor, s32* flags) {
    if (gPlayState == NULL || actor != &GET_PLAYER(gPlayState)->actor ||
        (!sShadow.isActive && sShadow.warpFrames == 0)) {
        return;
    }
    *flags &= ~UPDBGCHECKINFO_FLAG_1;
    if (sShadow.warpFrames > 0) {
        sShadow.warpFrames--;
    }
}

static void SpawnRodGlow(PlayState* play, Player* player) {
    if (sRodGlow <= 0) {
        return;
    }
    sRodGlow--;
    Vec3f pos = player->bodyPartsPos[PLAYER_BODYPART_RIGHT_HAND];
    Vec3f zero = { 0.0f, 0.0f, 0.0f };
    Color_RGBA8 prim = { 255, 255, 150, 255 };
    Color_RGBA8 env = { 255, 160, 0, 0 };
    pos.y += 25.0f;
    EffectSsKirakira_SpawnDispersed(play, &pos, &zero, &zero, &prim, &env, 800, 8);
}

static bool CastRod(Player* player, PlayState* play, u8 mode) {
    bool didCast;
    switch (mode) {
        case WAND_TORNADO:
            return StartFlight(player, play);
        case WAND_SCEPTER:
            return ToggleShadow(player, play);
        case WAND_SAND:
            didCast = CastSand(player, play);
            break;
        case WAND_WATER:
            didCast = CastWater(player, play);
            break;
        case WAND_METEOR:
            didCast = CastMeteor(player, play);
            break;
        case WAND_STORM:
            didCast = CastStorm(player, play);
            break;
        default:
            return false;
    }
    if (didCast) {
        sRodGlow = WAND_GLOW_FRAMES;
        SpendMagic(play, sRods[mode].magicCost);
    }
    return didCast;
}

static bool StartSwingClip(PlayState* play, Player* player, u8 clip) {
    PlayerAnimationHeader* anim =
        ResourceMgr_LoadPlayerAnimAsHeader(clip == WAND_SWING_FORE ? gPlayerAnim_link_fighter_Lside_kiru
                                                                   : gPlayerAnim_link_fighter_Rside_kiru);
    if (anim == NULL) {
        return false;
    }
    PlayerAnimation_PlayOnce(play, &player->skelAnimeUpper, anim);
    sSwingClip = clip;
    return true;
}

static bool StartSwing(PlayState* play, Player* player, u8 mode) {
    if (player->stateFlags1 & PLAYER_STATE1_CARRYING_ACTOR) {
        return false;
    }
    sSwingMode = mode;
    sHasSwingFired = false;
    return StartSwingClip(play, player, WAND_SWING_FORE);
}

static s32 SwingWandUpper(Player* player, PlayState* play) {
    sIsWandOut = true;
    Z64Items_KeepHeld(sApi);
    if (sIsFlying) {
        return 1;
    }
    if (sSwingClip == WAND_SWING_NONE) {
        return 0;
    }
    if (sSwingClip == WAND_SWING_BACK && !sHasSwingFired &&
        player->skelAnimeUpper.curFrame >= player->skelAnimeUpper.endFrame * WAND_CAST_FRACTION) {
        sHasSwingFired = true;
        CastRod(player, play, sSwingMode);
    }
    if (PlayerAnimation_Update(play, &player->skelAnimeUpper)) {
        if (sSwingClip != WAND_SWING_FORE || !StartSwingClip(play, player, WAND_SWING_BACK)) {
            sSwingClip = WAND_SWING_NONE;
        }
    }
    return 1;
}

static void TakeOutWand(PlayState* play, Player* player) {
    sIsWandOut = true;
    sHasHandMatrix = false;
    sIsPressPending = false;
    sSwingClip = WAND_SWING_NONE;
    Z64Items_KeepHeld(sApi);
}

static void PressWand(Player* player, PlayState* play) {
    sIsPressPending = true;
}

static void PutWandAway(Player* player, PlayState* play) {
    if (sIsFlying && sFlightStartupFrames > 0) {
        sIsWandOut = true;
        Z64Items_KeepHeld(sApi);
        return;
    }
    if (sIsFlying) {
        EndFlight(play, player, false);
    }
    if (sShadow.isActive) {
        EndShadow(player, play, false);
    }
    sIsWandOut = false;
    sSwingClip = WAND_SWING_NONE;
}

static bool CanUseWand(Player* player, PlayState* play) {
    return !func_800B7118(player) && Z64Wheel_CountUnlocked() > 0;
}

// The scene already destroyed every actor and effect: the pointers are dropped, never freed.
static void ForgetSceneEffects(s16 sceneId) {
    sLastScene = sceneId;
    ForgetSandSlabs();
    sWaterGeyser = NULL;
    sStormRay.isActive = false;
    sIsStormColliderReady = false;
    sIsFlying = false;
    sFlightStartupFrames = 0;
    sShadow.isActive = false;
    sHandMeteor = NULL;
    sSwingClip = WAND_SWING_NONE;
}

static void FireWand(PlayState* play, Player* player, u8 mode) {
    bool didFire;
    if (mode == WAND_TORNADO || mode == WAND_SCEPTER) {
        didFire = CastRod(player, play, mode);
    } else {
        didFire = HasMagic(play, sRods[mode].magicCost) && StartSwing(play, player, mode);
    }
    if (!didFire) {
        Audio_PlaySfx_AtPos(&player->actor.world.pos, NA_SE_SY_ERROR);
    }
}

static void UpdateWand(Player* player) {
    PlayState* play = gPlayState;
    if (play == NULL || player != GET_PLAYER(play)) {
        return;
    }
    if (sLastScene != play->sceneId) {
        ForgetSceneEffects(play->sceneId);
    }
    SyncUnlockedRods();
    // Scene changes clear this mod's effects, but can preserve the host's held item.
    if (IsWandKey(CustomItemRegistry_GetHeldKey())) {
        sIsWandOut = true;
        Z64Items_KeepHeld(sApi);
    }
    bool isHeld = IsButtonHeld(play);
    SpawnRodGlow(play, player);
    ThrowHandMeteor(player);
    if (sShadow.isActive) {
        TickShadowMode(player, play);
        sIsPressPending = false;
        return;
    }
    if (sIsFlying) {
        TickFlight(play, player);
        sIsPressPending = false;
        return;
    }
    TickStormRay(play, player);
    if (!sIsWandOut) {
        sSwingClip = WAND_SWING_NONE;
        sWasWandOut = false;
        sIsPressPending = false;
        return;
    }
    u8 mode = CurrentMode();
    if (mode == WAND_SAND && sSwingClip == WAND_SWING_NONE && ShouldLaySlab(player, isHeld) &&
        HasMagic(play, sRods[WAND_SAND].magicCost)) {
        CastRod(player, play, WAND_SAND);
    }
    if (sIsPressPending && sWasWandOut && sSwingClip == WAND_SWING_NONE) {
        FireWand(play, player, mode);
    }
    sIsPressPending = false;
    sWasWandOut = true;
}

// Nothing touches the matrix between the hand limb and the end of the post-limb pass, so this is the hand bone's own.
static void CaptureHandMatrix(Player* player, s32 limbIndex) {
    if (limbIndex != PLAYER_LIMB_RIGHT_HAND || gPlayState == NULL || player != GET_PLAYER(gPlayState)) {
        return;
    }
    Matrix_Get(&sHandMatrix);
    sHasHandMatrix = true;
}

static void DrawWandInHand(Player* player, PlayState* play, u8 mode) {
    f32 unscale = player->actor.scale.x != 0.0f ? 1.0f / player->actor.scale.x : 1.0f;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    Matrix_Put(&sHandMatrix);
    // That matrix carries Link's own body scale.
    Matrix_Scale(unscale, unscale, unscale, MTXMODE_APPLY);
    Matrix_RotateYF(WAND_HELD_ROT_Y * WAND_DEGREES_TO_RADIANS, MTXMODE_APPLY);
    Matrix_RotateXF(WAND_HELD_ROT_X * WAND_DEGREES_TO_RADIANS, MTXMODE_APPLY);
    Matrix_RotateZF(WAND_HELD_ROT_Z * WAND_DEGREES_TO_RADIANS, MTXMODE_APPLY);
    Matrix_Translate(0.0f, WAND_HELD_OFFSET_Y, WAND_HELD_OFFSET_Z, MTXMODE_APPLY);
    Matrix_Scale(WAND_HELD_SCALE, WAND_HELD_SCALE, WAND_HELD_SCALE, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sRods[mode].opaModel);
    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sRods[mode].xluModel);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawWandTimer(PlayState* play) {
    if (!sIsFlying && !sShadow.isActive) {
        return;
    }
    s32 remaining = MAX(sIsFlying ? sFlightTime : sShadow.time, 0);
    s32 maximum = sIsFlying ? FLIGHT_FRAMES : SHADOW_FRAMES;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL39_Overlay(play->state.gfxCtx);
    gDPSetCombineMode(OVERLAY_DISP++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 30, 30, 40, 200);
    gDPFillRectangle(OVERLAY_DISP++, SCREEN_WIDTH / 2 - 41, SCREEN_HEIGHT - 33, SCREEN_WIDTH / 2 + 41,
                     SCREEN_HEIGHT - 25);
    gDPPipeSync(OVERLAY_DISP++);
    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, sShadow.isActive ? 160 : 80, sShadow.isActive ? 100 : 220, 255, 255);
    gDPFillRectangle(OVERLAY_DISP++, SCREEN_WIDTH / 2 - 40, SCREEN_HEIGHT - 32,
                     SCREEN_WIDTH / 2 - 40 + 80 * remaining / maximum, SCREEN_HEIGHT - 26);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawFlightTornadoFeet(Actor* actor, PlayState* play) {
    if (!sIsFlying || actor != &GET_PLAYER(play)->actor) {
        return;
    }
    Player* player = (Player*)actor;
    OPEN_DISPS(play->state.gfxCtx);
    for (s32 foot = 0; foot < 2; foot++) {
        Vec3f pos = player->bodyPartsPos[foot != 0 ? PLAYER_BODYPART_RIGHT_FOOT : PLAYER_BODYPART_LEFT_FOOT];
        Matrix_Push();
        Matrix_Translate(pos.x, pos.y - 14.0f, pos.z, MTXMODE_NEW);
        Matrix_RotateYF(play->gameplayFrames * (foot != 0 ? -0.25f : 0.25f), MTXMODE_APPLY);
        Matrix_Scale(0.24f, 0.24f, 0.24f, MTXMODE_APPLY);
        Gfx_SetupDL25_Xlu(play->state.gfxCtx);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sTornadoMaterialDL);
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 70, 255, 110, 180);
        gDPSetEnvColor(POLY_XLU_DISP++, 10, 90, 25, 128);
        MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sTornadoMeshDL);
        Matrix_Pop();
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawOnlyShadow(PlayState* play) {
    Matrix_Push();
    Matrix_Translate(sShadow.pos.x, sShadow.pos.y + 0.2f, sShadow.pos.z, MTXMODE_NEW);
    Matrix_Scale(0.5f, 1.0f, 0.5f, MTXMODE_APPLY);
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL44_Xlu(play->state.gfxCtx);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 0, 0, 0, 180);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gCircleShadowDL);
    CLOSE_DISPS(play->state.gfxCtx);
    Matrix_Pop();
}

// Runs right before Link draws: the shadow hides him, and the sand cast lifts the casting arm.
static void PoseActivation(Actor* actor, bool* should) {
    if (gPlayState == NULL || actor != &GET_PLAYER(gPlayState)->actor) {
        return;
    }
    Player* player = (Player*)actor;
    sHasHandMatrix = false;
    if (sShadow.isActive) {
        *should = false;
        return;
    }
    if (sRodGlow > 0 && CurrentMode() == WAND_SAND) {
        player->skelAnime.jointTable[PLAYER_LIMB_RIGHT_SHOULDER].z -= 0x0800;
    }
}

static void DrawWand(Actor* actor, PlayState* play) {
    if (actor != &GET_PLAYER(play)->actor) {
        return;
    }
    if (sShadow.isActive) {
        DrawOnlyShadow(play);
        return;
    }
    Player* player = (Player*)actor;
    if (!sIsWandOut || !sHasHandMatrix || (player->stateFlags2 & PLAYER_STATE2_20000000)) {
        return;
    }
    DrawWandInHand(player, play, CurrentMode());
}

static void DrawRodGetItem(PlayState* play, u8 mode) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    Matrix_Scale(WAND_GIVE_SCALE, WAND_GIVE_SCALE, WAND_GIVE_SCALE, MTXMODE_APPLY);
    Matrix_RotateYF((s16)(play->gameplayFrames * 2) * 0.01f, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sRods[mode].opaModel);
    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sRods[mode].xluModel);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawGetItem(PlayState* play, S2HGetItemEntry* entry) {
    DrawRodGetItem(play, CurrentMode());
}

static void DrawSandGetItem(PlayState* play, S2HGetItemEntry* entry) {
    DrawRodGetItem(play, WAND_SAND);
}

static void DrawTornadoGetItem(PlayState* play, S2HGetItemEntry* entry) {
    DrawRodGetItem(play, WAND_TORNADO);
}

static void DrawWaterGetItem(PlayState* play, S2HGetItemEntry* entry) {
    DrawRodGetItem(play, WAND_WATER);
}

static void DrawMeteorGetItem(PlayState* play, S2HGetItemEntry* entry) {
    DrawRodGetItem(play, WAND_METEOR);
}

static void DrawStormGetItem(PlayState* play, S2HGetItemEntry* entry) {
    DrawRodGetItem(play, WAND_STORM);
}

static void DrawScepterGetItem(PlayState* play, S2HGetItemEntry* entry) {
    DrawRodGetItem(play, WAND_SCEPTER);
}

// MM's registry keeps no runtime id in the entry, so each rod pickup gets its own draw.
static const S2HGetItemDrawFunc sRodGetItemDraws[WAND_MODE_MAX] = {
    DrawSandGetItem, DrawTornadoGetItem, DrawWaterGetItem, DrawMeteorGetItem, DrawStormGetItem, DrawScepterGetItem,
};

static void ForgetSceneState(s16 sceneId, s8 spawnNum) {
    ForgetSceneEffects(sceneId);
    sIsWandOut = false;
}

static void ReloadRods(s16 fileNum) {
    SyncUnlockedRods();
}

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
        sApi->GetNetPeerState(peerId, WAND_VISUAL_CHANNEL, visual, sizeof(*visual)) != sizeof(*visual)) {
        return false;
    }
    if (visual->active != 1 || visual->scene != gPlayState->sceneId || visual->flags[0] >= WAND_MODE_MAX ||
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
        visual.active = sIsWandOut && !sShadow.isActive;
        if (visual.active) {
            visual.flags[0] = CurrentMode();
        }
        if (sRemoteVisualSent && (uint32_t)(play->gameplayFrames - sRemoteVisualFrame) < REMOTE_VISUAL_INTERVAL) {
            return;
        }
    }
    if (sRemoteVisualSent && memcmp(&visual, &sLastRemoteVisual, sizeof(visual)) == 0) {
        return;
    }
    if (sApi->SetNetPeerState(WAND_VISUAL_CHANNEL, &visual, sizeof(visual))) {
        sLastRemoteVisual = visual;
        sRemoteVisualSent = true;
        sRemoteVisualFrame = play != NULL ? play->gameplayFrames : 0;
    }
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

static void DrawRemoteLimb(Player* player, s32 limb) {
    PlayState* play = gPlayState;
    RemoteVisualState visual;
    if (limb != PLAYER_LIMB_RIGHT_HAND || !ReadRemoteVisual(player, &visual)) {
        return;
    }
    uint32_t savedRemoteEnv = CaptureRemoteLimbEnv(play);
    MtxF localHand = sHandMatrix;
    Matrix_Push();
    Matrix_Get(&sHandMatrix);
    DrawWandInHand(player, play, visual.flags[0]);
    sHandMatrix = localHand;
    Matrix_Pop();
    RestoreRemoteLimbEnv(play, savedRemoteEnv);
}

static void InitRemoteVisuals(void) {
    if (!S2H_MOD_API_HAS(sApi, RegisterNetChannel) || !S2H_MOD_API_HAS(sApi, SetNetPeerState) ||
        !S2H_MOD_API_HAS(sApi, GetNetPeerState) || !S2H_MOD_API_HAS(sApi, GetNetPeerIdForActor)) {
        return;
    }
    sRemoteVisualReady = sApi->RegisterNetChannel(WAND_VISUAL_CHANNEL, 1, ReceiveRemoteVisual);
    if (sRemoteVisualReady) {
        S2H_REGISTER_HOOK(sApi, OnGameFrameUpdate, PublishRemoteVisual);
        S2H_REGISTER_HOOK(sApi, OnPlayerPostLimbDraw, DrawRemoteLimb);
    }
}

static const S2HCustomItemRandomizer sRandomizerItem = {
    sizeof(S2HCustomItemRandomizer), 0, S2H_CUSTOM_ITEM_TYPE_ITEM, 0, 1, WAND_KEY, WAND_KEY, "",
};

static const char* const sRequiredHooks[] = {
    "OnGameFrameUpdate",  "OnPlayerUpdate",          "OnPlayerFilterInput", "OnActorResolveBgCheckFlags",
    "OnActorDrawEnd",     "ShouldActorDraw",         "OnActorDestroy",      "OnSceneInit",
    "OnSaveLoad",         "OnPlayerPostLimbDraw",    "OnInterfaceDrawEnd",
};
static const S2HModRequirements sRequirements = { sizeof(S2HModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

S2H_MOD_EXPORT void ModSetApi(const S2HModApi* api) {
    sApi = api;
}

S2H_MOD_EXPORT const S2HModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

static bool CanCallMeteorForLogic(void* userData) {
    bool isLit;
    switch (ShuffleRule()) {
        case WAND_SHUFFLE_SINGLE:
            isLit = Z64Logic_HasKey(WAND_KEY) || Z64Logic_HasKey(sRodPickupKeys[WAND_METEOR]);
            break;
        case WAND_SHUFFLE_RODS:
            isLit = Z64Logic_HasKey(sRodPickupKeys[WAND_METEOR]);
            break;
        default:
            isLit = Z64Logic_HasKey(sRods[WAND_METEOR].medallionKey) || Z64Logic_HasKey(sRodPickupKeys[WAND_METEOR]);
            break;
    }
    return isLit && sApi->LogicHasCapability("HasMagic");
}

static void GrantWandLogic(void) {
    static const char* const sBombing[] = { "HasExplosives", "BlastOrSmash", "CanHitSwitch", "CanAttack", "CanDamage" };
    if (!Z64Logic_Init(sApi)) {
        return;
    }
    for (u32 i = 0; i < ARRAY_COUNT(sBombing); i++) {
        sApi->GrantLogicCapability(sRodPickupKeys[WAND_METEOR], sBombing[i], CanCallMeteorForLogic, NULL);
    }
}

static bool RegisterRodPickups(void) {
    for (u8 mode = 0; mode < WAND_MODE_MAX; mode++) {
        S2HCustomPickupVariant pickup = Z64Items_DefinePickup(WAND_KEY, sVariantNames[mode], sRodPickupKeys[mode]);
        S2HCustomItemRandomizer logic = sRandomizerItem;
        logic.flags |= S2H_CUSTOM_ITEM_RANDO_ADVANCEMENT;
        logic.logicKey = sRodPickupKeys[mode];
        logic.hintKey = sRodPickupKeys[mode];
        logic.poolModeOption = WAND_SHUFFLE_OPTION;
        logic.poolModeValue = WAND_SHUFFLE_RODS;
        pickup.iconPath = sWheelEntries[mode].iconPath;
        pickup.getItemModelPath = sRods[mode].opaModel;
        pickup.getItemText = sRodTexts[mode];
        pickup.onReceive = ReceiveRod;
        pickup.randomizer = &logic;
        pickup.drawFunc = sRodGetItemDraws[mode];
        if (!Z64Items_RegisterPickup(sApi, &pickup)) {
            return false;
        }
    }
    return true;
}

S2H_MOD_EXPORT void ModInit(void) {
    InitRemoteVisuals();
    S2HModRandoOption shuffle = { sizeof(S2HModRandoOption),
                                  WAND_SHUFFLE_OPTION,
                                  "Elemental Wand Rods",
                                  "Which of the six rods the staff can be tuned to: one per medallion, all "
                                  "six from the staff itself, or one more rod for every staff you find.",
                                  sShuffleValues,
                                  ARRAY_COUNT(sShuffleValues),
                                  WAND_SHUFFLE_MEDALLIONS,
                                  S2H_MOD_RANDO_WIDGET_COMBOBOX };
    S2HCustomItemDefinition wand = Z64Items_Define(WAND_KEY, sIconSand, sNameTex);
    if (!S2H_MOD_API_HAS(sApi, GetRandoOption)) {
        return;
    }
    Z64Items_SetButtons(&wand, S2H_CUSTOM_ITEM_C_BUTTON | S2H_CUSTOM_ITEM_DPAD);
    Z64Items_SetForms(&wand, S2H_CUSTOM_ITEM_FORM_BIT(PLAYER_FORM_HUMAN));
    Z64Items_SetPlacement(&wand, 0, 3, 0);
    Z64Items_SetTextbox(&wand, "You got the %rElemental Wand%w!&A staff that answers to six&"
                               "elements, one at a time.^"
                               "Press %y\xB2%w to swing it and cast&the element it is tuned to.&"
                               "Hold %y\xB3%w to turn the wheel.^"
                               "Every %gmedallion%w you carry adds&a rod the staff can become.");
    Z64Items_SetPauseText(&wand, "%rElemental Wand&%wSix rods in one staff. %y\xB2%w casts, %y\xB3%w&"
                                 "picks the rod.");
    Z64Items_SetAction(&wand, TakeOutWand, SwingWandUpper);
    Z64Items_SetHeldCallbacks(&wand, PressWand, PutWandAway, NULL);
    Z64Items_SetCanUse(&wand, CanUseWand);
    Z64Items_SetGetItemModel(&wand, sSandOpa);
    wand.modelGroup = PLAYER_MODELGROUP_DEFAULT;
    wand.getItemEntry.drawFunc = DrawGetItem;
    wand.onReceive = LightOwnedRods;

    S2HCustomItemRandomizer wandLogic = sRandomizerItem;
    wandLogic.flags |= S2H_CUSTOM_ITEM_RANDO_ADVANCEMENT;
    wandLogic.poolModeOption = WAND_SHUFFLE_OPTION;
    wandLogic.poolModeValue = WAND_SHUFFLE_RODS;
    wandLogic.poolModeInvert = true;
    Z64Items_SetLogic(&wand, &wandLogic);
    if (!Z64Items_Register(sApi, &wand) || !RegisterRodPickups()) {
        return;
    }
    sApi->RegisterRandoOption(&shuffle);
    GrantWandLogic();
    Z64Wheel_Register(sApi, WAND_KEY, WAND_KEY, sWheelEntries, ARRAY_COUNT(sWheelEntries), false);
    S2H_REGISTER_HOOK(sApi, OnPlayerFilterInput, FilterWandModeInput);
    S2H_REGISTER_HOOK(sApi, OnActorResolveBgCheckFlags, ShadowWarpCollision);
    S2H_REGISTER_HOOK(sApi, OnPlayerUpdate, UpdateWand);
    S2H_REGISTER_HOOK_FOR_ID(sApi, OnActorDrawEnd, ACTOR_PLAYER, DrawFlightTornadoFeet);
    S2H_REGISTER_HOOK(sApi, OnInterfaceDrawEnd, DrawWandTimer);
    S2H_REGISTER_HOOK(sApi, OnActorDestroy, ForgetMeteorActor);
    S2H_REGISTER_HOOK_FOR_ID(sApi, ShouldActorDraw, ACTOR_PLAYER, PoseActivation);
    S2H_REGISTER_HOOK(sApi, OnSceneInit, ForgetSceneState);
    S2H_REGISTER_HOOK(sApi, OnSaveLoad, ReloadRods);
    S2H_REGISTER_HOOK(sApi, OnPlayerPostLimbDraw, CaptureHandMatrix);
    S2H_REGISTER_HOOK_FOR_ID(sApi, OnActorDrawEnd, ACTOR_PLAYER, DrawWand);
}
