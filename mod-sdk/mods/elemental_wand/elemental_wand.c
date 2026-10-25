/**
 * elemental_wand: six rods in one staff (Skijer's NEI).
 *
 * One item, one cell, one wheel: Sand, Tornado, Water, Meteor, Storm and the Shadow Scepter share the staff
 * and which one is live is the wheel's selection. Casting is a double sword swing over the walk, and the rod
 * itself fires on a frame of the second slash.
 *
 * Which rods are yours is a randomizer setting (Randomizer > Mods): the six medallions, one item lighting
 * all six, or a rod per pickup.
 */

#include <math.h>
#include <string.h>

#include "z64items.h"
#include "z64wheel.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "z64quake.h"
#include "overlays/actors/ovl_En_Bom/z_en_bom.h"
#include "overlays/actors/ovl_En_Light/z_en_light.h"

static Gfx sEmptyDL[] = { gsSPEndDisplayList() };

#define WAND_KEY "nei.elemental_wand"
#define WAND_SHUFFLE_OPTION "nei.wand_shuffle"
#define WAND_RODS_FIELD "rods"
#define WAND_PI 3.14159265f
#define WAND_DEGREES_TO_RADIANS (WAND_PI / 180.0f)

// Dialled in game and baked by NEI. The staff measures 350 units tall, which is why its scale is not a
// rod's. Order: rotation Y, X, Z, then the offsets along the shaft's own axes, then scale.
#define WAND_HELD_ROT_Y 180.0f
#define WAND_HELD_ROT_X 91.034f
#define WAND_HELD_ROT_Z 111.724f
#define WAND_HELD_OFFSET_Y 7.356f
#define WAND_HELD_OFFSET_Z (-3.218f)
#define WAND_HELD_SCALE 0.12f

// How far into the second slash the rod goes off: the summon belongs to the animation, not to the button.
#define WAND_CAST_FRACTION 0.35f

typedef enum {
    WAND_SAND,
    WAND_TORNADO,
    WAND_WATER,
    WAND_METEOR,
    WAND_STORM,
    WAND_SCEPTER,
    WAND_MODE_MAX,
} WandMode;

// The randomizer treatments, in the order the dropdown lists them.
typedef enum {
    WAND_SHUFFLE_MEDALLIONS,
    WAND_SHUFFLE_SINGLE,
    WAND_SHUFFLE_RODS,
} WandShuffle;

typedef enum {
    WAND_SWING_NONE,
    WAND_SWING_FORE,
    WAND_SWING_BACK,
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
static const ALIGN_ASSET(2) char sSwingForeAnim[] = "__OTR__objects/gameplay_keep/gPlayerAnim_link_fighter_Lside_kiru";
static const ALIGN_ASSET(2) char sSwingBackAnim[] = "__OTR__objects/gameplay_keep/gPlayerAnim_link_fighter_Rside_kiru";
static const ALIGN_ASSET(2) char sSwimAnim[] = "__OTR__objects/gameplay_keep/gPlayerAnim_link_swimer_swim";
static const ALIGN_ASSET(2) char sLandingRollAnim[] =
    "__OTR__objects/gameplay_keep/gPlayerAnim_link_normal_landing_roll";

static const ALIGN_ASSET(2) char sBoeMaterialDL[] = "__OTR__objects/object_mkk/gBlackBoeBodyMaterialDL";
static const ALIGN_ASSET(2) char sBoeModelDL[] = "__OTR__objects/object_mkk/gBlackBoeBodyModelDL";
static const ALIGN_ASSET(2) char sBoeEndDL[] = "__OTR__objects/object_mkk/gBlackBoeEndDL";

// Sand and Water undercut the rest because they are movement and get spammed; Storm costs the most because
// it fires world flags. The Tornado is 0 on purpose: it bills itself while the column climbs.
static const WandRod sRods[WAND_MODE_MAX] = {
    { sSandOpa, sSandXlu, "nei.medallion.spirit", 2 },  { sTornadoOpa, sTornadoXlu, "nei.medallion.forest", 0 },
    { sWaterOpa, sWaterXlu, "nei.medallion.water", 2 }, { sMeteorOpa, sMeteorXlu, "nei.medallion.fire", 3 },
    { sStormOpa, sStormXlu, "nei.medallion.light", 6 }, { sScepterOpa, sScepterXlu, "nei.medallion.shadow", 3 },
};

static const Z64WheelEntry sWheelEntries[WAND_MODE_MAX] = {
    { sIconSand, "Sand Rod" },     { sIconTornado, "Tornado Rod" }, { sIconWater, "Water Rod" },
    { sIconMeteor, "Meteor Rod" }, { sIconStorm, "Storm Rod" },     { sIconScepter, "Shadow Scepter" },
};

static const char* const sSeparateKeys[WAND_MODE_MAX] = {
    "nei.wand.sand", "nei.wand.tornado", "nei.wand.water",
    "nei.wand.meteor", "nei.wand.storm", "nei.wand.shadow"
};
static bool IsWandKey(const char* key) {
    return key != NULL && strcmp(key, WAND_KEY) == 0;
}
static const char* const sVariantNames[WAND_MODE_MAX] = { "sand", "tornado", "water", "meteor", "storm", "shadow" };
static void ReceiveWandFunction(const char* itemKey, const char* variantKey) {
    for (u8 mode = 0; mode < WAND_MODE_MAX; mode++) {
        if (strcmp(variantKey, sVariantNames[mode]) == 0) {
            Z64Wheel_SetEntryUnlocked(mode, true);
            Z64Wheel_SetSelection(mode);
            break;
        }
    }
}

static const char* const sShuffleValues[] = { "Medallions", "One item", "Function pickups" };

// Indexed by Player.heldItemButton, which keeps a slot for A between the C buttons and the D-pad.
static const u16 sItemButtons[S2H_ITEM_BUTTON_COUNT] = { BTN_B,     BTN_CLEFT, BTN_CDOWN, BTN_CRIGHT, 0,
                                                         BTN_DRIGHT, BTN_DLEFT, BTN_DDOWN,  BTN_DUP };

static const char* const sRequiredHooks[] = { "OnPlayerUpdate", "OnActorDrawEnd",       "OnSceneInit",
                                              "OnSaveLoad",     "OnPlayerPostLimbDraw", "OnInterfaceDrawEnd" };
static const S2HModRequirements sRequirements = { sizeof(S2HModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

#define SAND_MAX_SLABS 8
#define SAND_SLAB_SCALE 0.05f
#define SAND_HOLD_INTERVAL 6
// Under 1.0 so consecutive slabs overlap instead of leaving a seam to fall through.
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
// The crumble countdown rides on home.rot.x: once the update below is installed no ObjLift code ever runs on
// this actor again except its destroy, which only reads dyna.bgId.
#define SAND_CRUMBLE(actor) ((actor)->home.rot.x)

// The player update runs at 20 Hz, so a full column is three seconds.
#define WIND_CHARGE_MIN 15
#define WIND_CHARGE_FULL 60
#define WIND_LAUNCH_MIN 8.0f
#define WIND_LAUNCH_MAX 32.0f
// Eight ticks fall inside the climb, so a full gale costs exactly 8 and a partial one costs its share.
#define WIND_DRAIN_INTERVAL 7
#define WIND_FLUTTER_INTERVAL 4
#define WIND_BOOST 1.55f
#define WIND_BOOST_FRAMES 12
#define WIND_HEIGHT 82.0f
#define WIND_RADIUS 30.0f
#define WIND_SPIN 0x0900
#define WIND_ALPHA 70
#define WIND_GROW_MIN 0.35f
#define WIND_FADE_FRAMES 14.0f
#define WIND_FLAME_MAX 8
#define WIND_FLAME_KEEP 4
#define WIND_FLAME_BURST_FRAMES 7
#define WIND_FLAME_RADIUS 17.0f
#define WIND_FLAME_BURST_RADIUS 40.0f
#define WIND_FLAME_HEIGHT 12.0f
#define WIND_FLAME_DRIFT 0x0700
// Index 6 of En_Light's table is the green flame, and it must be POSITIVE: bit 15 is the small candle, whose
// draw hardcodes orange, and an even index also skips its Y flip.
#define WIND_LIGHT_PARAMS 6
#define WIND_LIGHT_SCALE 0.0010f
#define WIND_QUAKE_SPEED 32000
#define WIND_QUAKE_AMPLITUDE 6
#define WIND_QUAKE_FRAMES 11
// A continuous id, only ever played MINUS the flag and re-issued every frame, which is also what stops it.
#define WIND_LOOP_SFX (NA_SE_EV_WIND_TRAP - SFX_FLAG)
#define WIND_RIBBONS 5
#define WIND_RIBBON_SPREAD 2.0f
#define WIND_RIBBON_LIFE 8
#define WIND_RIBBON_STEP 0.06f
#define WIND_RIBBON_TURNS 0x18000

// Link's ~60 units are about 1.6 m and the update runs at 20 Hz, so the gale is roughly twelve metres of
// ground: a long jump rather than a flight.
#define GALE_ANTICIPATE_FRAMES 2
#define GALE_IMPULSE_FRAMES 2
#define GALE_FLIGHT_FRAMES 18
#define GALE_SPEED_XZ 26.0f
#define GALE_RISE 12.0f
#define GALE_GRAVITY (-1.6f)
#define GALE_ROLL_FRAMES 10
#define GALE_LIFTOFF_FRAMES 3
#define GALE_COST 4
#define GALE_FALL_LIMIT 60
#define GALE_TURN_RATE 0x800
#define GALE_QUAKE_SPEED 25000
#define GALE_QUAKE_AMPLITUDE 8
#define GALE_QUAKE_FRAMES 9
#define GALE_VANILLA_GRAVITY (-1.2f)
#define GALE_MORPH_FRAMES (-4.0f)

typedef enum {
    GALE_OFF,
    GALE_ANTICIPATE,
    GALE_IMPULSE,
    GALE_FLIGHT,
    GALE_FALL,
    GALE_ROLL,
} WandGalePhase;

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
// Nothing detonates it for these first frames: thrown from hand height it is still inside Link's own space.
#define METEOR_ARM_FRAMES 5
// The AT collider only exists while the bomb is ALREADY exploding, so enemy contact is proximity.
#define METEOR_ENEMY_TRIGGER_RADIUS 42.0f
#define METEOR_BOMB_SCALE 0.01f
// Peak and speed both fall as 1/(1 + k*n): slow, where vanilla's restitution dies in two hops.
#define METEOR_HOP_PERIOD 16.0f
#define METEOR_HOP_HEIGHT 46.0f
#define METEOR_HOP_DECAY 0.3f
#define METEOR_STEER_RATE 0x900
#define METEOR_TINT_R 235
#define METEOR_TINT_G 45
#define METEOR_TINT_B 30
#define METEOR_TINT_MIX 255
// Both on home.rot, which En_Bom never touches. Hop time is AIRBORNE until the throw lands.
#define METEOR_HOP_TIME(actor) ((actor)->home.rot.x)
#define METEOR_AGE(actor) ((actor)->home.rot.z)
#define METEOR_AIRBORNE (-1)

// En_Okarina_Effect's own storm is the whole cast; the song's magic reset is not part of it.
#define STORM_OKARINA_PARAMS 1
#define STORM_SPAWN_Y_OFFSET (-30.0f)
#define STORM_RAY_SPEED 18.0f
#define STORM_RAY_LIFE 40
#define STORM_RAY_SPAWN_HEIGHT 30.0f
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

#define SHADOW_SEEK_RANGE 460.0f
#define SHADOW_SPAWN_DIST 30.0f
#define SHADOW_SPAWN_HEIGHT 25.0f
#define SHADOW_SPEED 11.0f
#define SHADOW_HIT_RADIUS 22.0f
#define SHADOW_LIFE_FRAMES 90
#define SHADOW_STUN_FRAMES 120
#define SHADOW_SCALE 0.014f
#define SHADOW_FILTER_STRENGTH 0xF8

static const u8 sEnemyCategories[2] = { ACTORCAT_ENEMY, ACTORCAT_BOSS };

static const S2HModApi* sApi;
static bool sIsWandOut;
static bool sIsPressPending;
static bool sWasWandOut;
static MtxF sHandMatrix;
static bool sHasHandMatrix;
static s16 sLastScene = -1;

static u8 sSwingClip;
static u8 sSwingMode;
static bool sHasSwingFired;

static Actor* sSandSlabs[SAND_MAX_SLABS];
static u8 sSandNextSlot;
static s16 sSandHoldTimer;
static f32 sSandTopOffset;
static f32 sSandReach;
static bool sIsSandMeasured;

static Actor* sWindFlames[WIND_FLAME_MAX];
static f32 sWindFlamePhase[WIND_FLAME_MAX];
static s32 sWindRibbons[WIND_RIBBONS];
static f32 sWindRibbonPhase[WIND_RIBBONS];
static bool sAreWindRibbonsLive;
static Vec3f sWindOrigin;
static f32 sWindFade;
static f32 sWindGrow;
static s32 sWindAge;
static s16 sWindSpin;
static bool sIsWindUp;
static s16 sWindCharge;
static s16 sWindDrainTimer;
static s16 sWindBoostTimer;
static bool sIsWindCharging;

static u8 sGalePhase;
static s16 sGaleTimer;
static s16 sGaleYaw;

static Actor* sWaterGeyser;
static f32 sWaterBaseY;
static f32 sWaterCurrentHeight;
static f32 sWaterTargetHeight;

static ActorFunc sMeteorBombUpdate;
static ActorFunc sMeteorBombDraw;

static struct {
    Vec3f pos;
    Vec3f vel;
    s16 life;
    bool isActive;
} sStormRay;
static ColliderCylinder sStormRayCollider;
static bool sIsStormColliderReady;

static struct {
    Vec3f pos;
    Actor* target;
    s16 yaw;
    s16 life;
    bool isActive;
} sShadowBolt;

static Color_RGBA8 sStormBoltPrim = { 255, 255, 255, 255 };
static Color_RGBA8 sStormBoltEnv = { 255, 255, 60, 255 };
static Color_RGBA8 sSandDust = { SAND_ENV_R, SAND_ENV_G, SAND_ENV_B, 255 };
static Color_RGBA8 sGaleDust = { 200, 210, 190, 255 };

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
void Player_StopHorizontalMovement(Player* player);
s32 Player_SetAction(PlayState* play, Player* player, PlayerActionFunc actionFunc, s32 arg3);
void Player_Anim_PlayLoop(PlayState* play, Player* player, PlayerAnimationHeader* anim);
PlayerAnimationHeader* Player_GetIdleAnim(Player* player);
void func_8083B1A0(Player* player, PlayState* play);
void func_80834CD0(Player* player, f32 velocityY, u16 sfxId);
bool Player_IsZTargeting(Player* player);

static void ChargeWindAction(Player* player, PlayState* play);
static void RideGaleAction(Player* player, PlayState* play);

static u8 CurrentMode(void) {
    uint32_t selection = Z64Wheel_GetSelection();

    return selection < WAND_MODE_MAX ? (u8)selection : WAND_SAND;
}

static u8 ShuffleRule(void) {
    return S2H_MOD_API_HAS(sApi, GetRandoOption) ? sApi->GetRandoOption(WAND_SHUFFLE_OPTION) : WAND_SHUFFLE_MEDALLIONS;
}

static u8 OwnedRodBits(void) {
    u8 rods = 0;

    sApi->StorageGet(WAND_KEY, WAND_RODS_FIELD, &rods, sizeof(rods));
    return rods;
}

// Which rods answer. The three treatments differ only in what lights an entry, never in what the staff is.
static void SyncUnlockedRods(void) {
    u8 rule = ShuffleRule();
    bool hasStaff = rule == WAND_SHUFFLE_SINGLE && sApi->IsCustomItemOwned(WAND_KEY);

    for (u8 mode = 0; mode < WAND_MODE_MAX; mode++) {
        bool isOwned;

        switch (rule) {
            case WAND_SHUFFLE_SINGLE:
                isOwned = hasStaff;
                break;
            case WAND_SHUFFLE_RODS:
                isOwned = sApi->IsCustomItemOwned(sSeparateKeys[mode]);
                break;
            default:
                isOwned = sApi->IsCustomItemOwned(sRods[mode].medallionKey) || sApi->IsCustomItemOwned(sSeparateKeys[mode]);
                break;
        }
        Z64Wheel_SetEntryUnlocked(mode, isOwned);
    }
}

// Ownership and the selected distribution mode decide which wheel entries are available.
static void LightNextRod(const char* key) {
    SyncUnlockedRods();
}

static u16 FindButtonMask(void) {
    if (gPlayState != NULL) {
        u8 held = GET_PLAYER(gPlayState)->heldItemButton;
        if (held < S2H_ITEM_BUTTON_COUNT && IsWandKey(sApi->GetEquippedCustomItem(held))) return sItemButtons[held];
    }
    for (u8 button = 0; button < S2H_ITEM_BUTTON_COUNT; button++) {
        const char* equipped = sApi->GetEquippedCustomItem(button);

        if (IsWandKey(equipped)) {
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

// Failing to pay costs nothing else: the rod that declined reports it and the swing is never started.
static bool SpendMagic(PlayState* play, s16 cost) {
    if (cost <= 0) {
        return true;
    }
    if (!Magic_Consume(play, cost, MAGIC_CONSUME_NOW)) {
        return false;
    }
    // Consuming leaves the meter flashing and refusing every later request until the spell resets it.
    Magic_Reset(play);
    return true;
}

static bool HasMagic(PlayState* play, s16 cost) {
    return cost <= 0 || gSaveContext.save.saveInfo.playerData.magic >= cost;
}

static Actor* FindNearestEnemy(PlayState* play, Vec3f* pos, f32 range) {
    f32 rangeSq = SQ(range);

    for (u8 index = 0; index < ARRAY_COUNT(sEnemyCategories); index++) {
        Actor* actor = play->actorCtx.actorLists[sEnemyCategories[index]].first;

        while (actor != NULL) {
            if (actor->update != NULL) {
                f32 distanceSq =
                    SQ(actor->world.pos.x - pos->x) + SQ(actor->world.pos.y - pos->y) + SQ(actor->world.pos.z - pos->z);

                if (distanceSq < rangeSq) {
                    return actor;
                }
            }
            actor = actor->next;
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

// The slab is MEASURED off its own registered collision: world units from the actor origin up to the surface
// Link stands on, and out to its nearest edge. Both are constant, so the first slab pays for it.
static void MeasureSlab(PlayState* play, Actor* slab) {
    s32 bgId = ((DynaPolyActor*)slab)->bgId;
    CollisionHeader* header;
    f32 halfX;
    f32 halfZ;

    if (sIsSandMeasured || bgId < 0 || bgId >= BG_ACTOR_MAX) {
        return;
    }
    header = play->colCtx.dyna.bgActors[bgId].colHeader;
    if (header == NULL) {
        return;
    }
    halfX = (f32)(header->maxBounds.x - header->minBounds.x) * 0.5f * SAND_SLAB_SCALE;
    halfZ = (f32)(header->maxBounds.z - header->minBounds.z) * 0.5f * SAND_SLAB_SCALE;
    sSandTopOffset = (f32)header->maxBounds.y * SAND_SLAB_SCALE;
    sSandReach = halfX < halfZ ? halfX : halfZ;
    sIsSandMeasured = true;
}

// One step ahead of Link, with its walking surface at his feet. The player's own origin sits on the floor,
// which is why the slab is dropped by its top offset rather than by a number picked to look right.
static void NextSlabSpot(Player* player, Vec3f* out) {
    s16 yaw = player->actor.shape.rot.y;
    f32 distance = sIsSandMeasured ? sSandReach * SAND_STEP_FRACTION : SAND_FALLBACK_DIST;

    out->x = player->actor.world.pos.x + Math_SinS(yaw) * distance;
    out->y = player->actor.world.pos.y - sSandTopOffset;
    out->z = player->actor.world.pos.z + Math_CosS(yaw) * distance;
}

// A slab that has started crumbling does not count: it is on its way out.
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

/**
 * The slab's whole life. No shake and no fall: standing on it starts a countdown, and the countdown eats it.
 *
 * The countdown never touches actor->scale, only the draw: the dynapoly is built from the actor's transform,
 * so scaling the actor would shrink the floor out from under Link while he is still standing on it.
 */
static void UpdateSlab(Actor* thisx, PlayState* play) {
    if (SAND_CRUMBLE(thisx) == 0) {
        if (!DynaPolyActor_IsPlayerOnTop((DynaPolyActor*)thisx)) {
            return;
        }
        SAND_CRUMBLE(thisx) = SAND_CRUMBLE_FRAMES;
        // A fixed world position, not the actor's: the slab is killed partway through the sound.
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

    // Applied to the matrix Actor_Draw already set up: this shrinks the model and leaves the collision alone.
    Matrix_Scale(remaining, remaining, remaining, MTXMODE_APPLY);
    Gfx_DrawDListOpa(play, (Gfx*)sSlabDL);
}

static void ForgetSandSlabs(void) {
    for (u8 index = 0; index < SAND_MAX_SLABS; index++) {
        sSandSlabs[index] = NULL;
    }
    sSandNextSlot = 0;
    sSandHoldTimer = 0;
}

// Overflow only: the oldest is told to start crumbling and then let go of, so a held button cannot fill the
// actor list. It finishes on its own; the countdown lives on the actor.
static void EvictSlab(u8 slot) {
    Actor* slab = sSandSlabs[slot];

    if (slab != NULL && SAND_CRUMBLE(slab) == 0) {
        SAND_CRUMBLE(slab) = SAND_CRUMBLE_FRAMES;
    }
    sSandSlabs[slot] = NULL;
}

// ObjLift_Init kills itself when the flag is already set, and nothing runs between the two flag writes.
static Actor* SpawnLift(PlayState* play, Vec3f* pos, s16 yaw) {
    bool wasFlagSet = Flags_GetSwitch(play, SAND_SWITCH_FLAG) != 0;
    Actor* lift;

    if (wasFlagSet) {
        Flags_UnsetSwitch(play, SAND_SWITCH_FLAG);
    }
    lift = Actor_Spawn(&play->actorCtx, play, ACTOR_OBJ_LIFT, pos->x, pos->y, pos->z, 0, yaw, 0, 0);
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
    s16 yaw = player->actor.shape.rot.y;
    Actor* slab;

    if (!EnsureLiftObject(play)) {
        return false;
    }
    // Placed blind, with no raycast on purpose: that is what lets a slab hang in mid-air over a gap.
    NextSlabSpot(player, &pos);
    slab = SpawnLift(play, &pos, yaw);
    if (slab == NULL) {
        return false;
    }

    // destroy is left alone: ObjLift_Destroy is what unregisters the dynapoly.
    slab->update = UpdateSlab;
    slab->draw = DrawSlab;
    Actor_SetScale(slab, SAND_SLAB_SCALE);
    SAND_CRUMBLE(slab) = 0;

    // The collision only exists once Init has registered it, so the first slab is the one that can be
    // measured, and once measured it is moved to where it should have gone.
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

// What makes the rod a road: it only lays a slab when the step ahead has nothing standable under it, so
// holding the button and walking keeps producing ground, and standing still stops costing magic.
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

// Frozen on purpose: EnLight_Update is the only thing that plays its torch sound and the only thing that
// restores the light's radius each frame, so replacing it keeps these motes silent and dark.
static void KeepFlameStill(Actor* thisx, PlayState* play) {
}

static Actor* SpawnFlame(PlayState* play, Vec3f* origin) {
    Actor* flame =
        Actor_Spawn(&play->actorCtx, play, ACTOR_EN_LIGHT, origin->x, origin->y, origin->z, 0, 0, 0, WIND_LIGHT_PARAMS);

    if (flame != NULL) {
        Actor_SetScale(flame, WIND_LIGHT_SCALE);
        // Re-seated as a no-glow point of radius 0: EnLight_Init made it a glowing one, which is the wide
        // green disc on the floor. The node still points here, so EnLight_Destroy has something valid.
        Lights_PointNoGlowSetInfo(&((EnLight*)flame)->lightInfo, (s16)origin->x, (s16)origin->y, (s16)origin->z, 0, 0,
                                  0, 0);
        flame->update = KeepFlameStill;
    }
    return flame;
}

static void StopWindRibbons(PlayState* play) {
    if (!sAreWindRibbonsLive) {
        return;
    }
    for (u8 index = 0; index < WIND_RIBBONS; index++) {
        if (sWindRibbons[index] >= 0) {
            Effect_Destroy(play, sWindRibbons[index]);
            sWindRibbons[index] = -1;
        }
    }
    sAreWindRibbonsLive = false;
}

static void StartWindRibbons(PlayState* play) {
    EffectBlureInit1 init;

    StopWindRibbons(play);
    init.p1StartColor[0] = 190;
    init.p1StartColor[1] = 255;
    init.p1StartColor[2] = 200;
    init.p1StartColor[3] = 255;
    init.p2StartColor[0] = 120;
    init.p2StartColor[1] = 220;
    init.p2StartColor[2] = 150;
    init.p2StartColor[3] = 0;
    init.p1EndColor[0] = 190;
    init.p1EndColor[1] = 255;
    init.p1EndColor[2] = 200;
    init.p1EndColor[3] = 0;
    init.p2EndColor[0] = 120;
    init.p2EndColor[1] = 220;
    init.p2EndColor[2] = 150;
    init.p2EndColor[3] = 0;
    init.elemDuration = WIND_RIBBON_LIFE;
    init.unkFlag = 0;
    init.calcMode = 2;

    for (u8 index = 0; index < WIND_RIBBONS; index++) {
        sWindRibbons[index] = -1;
        Effect_Add(play, &sWindRibbons[index], EFFECT_BLURE1, 0, 0, &init);
        sWindRibbonPhase[index] = (f32)index / (f32)WIND_RIBBONS;
    }
    sAreWindRibbonsLive = true;
}

// Each streak climbs the column's surface, flaring out as it turns around it.
static void FeedWindRibbons(PlayState* play) {
    f32 reach = WIND_HEIGHT * sWindGrow;
    f32 spread = WIND_RADIUS * sWindGrow * WIND_RIBBON_SPREAD;
    f32 width = spread * 0.16f + 3.0f;

    if (!sAreWindRibbonsLive) {
        StartWindRibbons(play);
    }
    for (u8 index = 0; index < WIND_RIBBONS; index++) {
        EffectBlure* ribbon = Effect_GetByIndex(sWindRibbons[index]);
        f32 phase = sWindRibbonPhase[index];
        f32 along = reach * phase;
        f32 flare = spread * phase * phase;
        s16 turn = (s16)((s32)(sWindSpin) + (s32)(index * (0x10000 / WIND_RIBBONS)) + (s32)(phase * WIND_RIBBON_TURNS));
        Vec3f base = { sWindOrigin.x + Math_CosS(turn) * flare, sWindOrigin.y + along,
                       sWindOrigin.z + Math_SinS(turn) * flare };
        Vec3f tip = { base.x, base.y + width, base.z };
        f32 nextPhase = phase + WIND_RIBBON_STEP;

        if (ribbon != NULL) {
            EffectBlure_AddVertex(ribbon, &tip, &base);
        }
        sWindRibbonPhase[index] = nextPhase >= 1.0f ? nextPhase - 1.0f : nextPhase;
    }
}

static void StopUpdraft(PlayState* play) {
    if (!sIsWindUp) {
        return;
    }
    sIsWindUp = false;
    StopWindRibbons(play);
    // En_Light has no lifetime of its own, so whoever spawns one owns it until they say otherwise.
    for (u8 index = 0; index < WIND_FLAME_MAX; index++) {
        if (sWindFlames[index] != NULL) {
            Actor_Kill(sWindFlames[index]);
            sWindFlames[index] = NULL;
        }
    }
}

// The scene already destroyed every actor and effect: killing a pointer into its arena is how that becomes a
// crash, so the pointers are dropped rather than freed.
static void ForgetUpdraft(void) {
    memset(sWindFlames, 0, sizeof(sWindFlames));
    memset(sWindRibbons, 0, sizeof(sWindRibbons));
    sAreWindRibbonsLive = false;
    sIsWindUp = false;
    sWindFade = 0.0f;
    sWindGrow = WIND_GROW_MIN;
    sWindAge = 0;
}

static void ShakeCamera(PlayState* play, s16 speed, s16 amplitude, s16 frames) {
    s16 quake = Quake_Request(Play_GetCamera(play, CAM_ID_MAIN), QUAKE_TYPE_3);

    Quake_SetSpeed(quake, speed);
    Quake_SetPerturbations(quake, amplitude, 0, 0, 0);
    Quake_SetDuration(quake, frames);
}

static void StartUpdraft(PlayState* play, Vec3f* origin) {
    if (sIsWindUp) {
        return;
    }
    sWindAge = 0;
    sIsWindUp = true;
    sWindFade = 1.0f;
    sWindGrow = WIND_GROW_MIN;
    sWindOrigin = *origin;

    // Everything erupts at once; the tick culls it back when the burst is over.
    for (u8 index = 0; index < WIND_FLAME_MAX; index++) {
        sWindFlamePhase[index] = (f32)index * (2.0f * WAND_PI / WIND_FLAME_MAX) + Rand_ZeroFloat(0.6f);
        sWindFlames[index] = SpawnFlame(play, origin);
    }
    ShakeCamera(play, WIND_QUAKE_SPEED, WIND_QUAKE_AMPLITUDE, WIND_QUAKE_FRAMES);
}

static void TickUpdraft(PlayState* play, bool isHolding, f32 fill) {
    bool isBursting;

    if (!sIsWindUp) {
        return;
    }
    isBursting = sWindAge < WIND_FLAME_BURST_FRAMES;
    sWindAge++;

    // The wind only holds while it is being built; let go and what is left dies away instead of cutting out.
    if (isHolding) {
        sWindFade = 1.0f;
        sWindGrow = WIND_GROW_MIN + (1.0f - WIND_GROW_MIN) * CLAMP(fill, 0.0f, 1.0f);
    } else {
        sWindFade -= 1.0f / WIND_FADE_FRAMES;
        if (sWindFade <= 0.0f) {
            StopUpdraft(play);
            return;
        }
    }
    sWindSpin += WIND_SPIN;
    Audio_PlaySfx_AtPos(&sWindOrigin, WIND_LOOP_SFX);
    FeedWindRibbons(play);

    for (u8 index = 0; index < WIND_FLAME_MAX; index++) {
        f32 angle = sWindFlamePhase[index] + BINANG_TO_RAD(WIND_FLAME_DRIFT) * sWindAge;
        f32 radius = isBursting ? WIND_FLAME_BURST_RADIUS : WIND_FLAME_RADIUS;

        if (sWindFlames[index] == NULL) {
            continue;
        }
        // Once the burst is spent only a few stay, gathered in close around the column.
        if (!isBursting && index >= WIND_FLAME_KEEP) {
            Actor_Kill(sWindFlames[index]);
            sWindFlames[index] = NULL;
            continue;
        }
        sWindFlames[index]->world.pos = sWindOrigin;
        sWindFlames[index]->world.pos.x += sinf(angle) * radius;
        sWindFlames[index]->world.pos.z += cosf(angle) * radius;
        sWindFlames[index]->world.pos.y += WIND_FLAME_HEIGHT;
        Actor_SetScale(sWindFlames[index], WIND_LIGHT_SCALE * sWindFade);
    }
}

// The cone is modelled standing on its base along +Y, which is where the column wants it anyway.
static void DrawUpdraft(PlayState* play) {
    f32 height = WIND_HEIGHT * sWindGrow;
    f32 spread = WIND_RADIUS * sWindGrow;

    if (!sIsWindUp) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sTornadoMaterialDL);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 190, 255, 200, (u8)(WIND_ALPHA * sWindFade));
    Matrix_Translate(sWindOrigin.x, sWindOrigin.y, sWindOrigin.z, MTXMODE_NEW);
    Matrix_RotateYF(BINANG_TO_RAD(sWindSpin), MTXMODE_APPLY);
    Matrix_Scale(spread / 47.0f, height / 100.0f, spread / 47.0f, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sTornadoMeshDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void EndWindCharge(Player* player, PlayState* play) {
    sIsWindCharging = false;
    sWindCharge = 0;
    if (player->actionFunc == (PlayerActionFunc)ChargeWindAction) {
        func_8083B1A0(player, play);
    }
}

// The charge decides the height; the boost below then keeps the next hop or slash strong.
static void LaunchOnColumn(PlayState* play, Player* player) {
    f32 fill = (f32)sWindCharge / (f32)WIND_CHARGE_FULL;

    func_80834CD0(player, WIND_LAUNCH_MIN + (WIND_LAUNCH_MAX - WIND_LAUNCH_MIN) * CLAMP(fill, 0.0f, 1.0f), NA_SE_NONE);
    sWindBoostTimer = WIND_BOOST_FRAMES;
    Player_PlaySfx(player, NA_SE_PL_ROLL);
}

/**
 * Rooted while the column builds. An action func of its own rather than a paused one: anything that takes
 * the player (damage, a cutscene, a textbox) replaces it, and the charge notices next frame that it is no
 * longer the one driving.
 */
static void ChargeWindAction(Player* player, PlayState* play) {
    Player_StopHorizontalMovement(player);
    Player_Anim_PlayLoop(play, player, Player_GetIdleAnim(player));
}

static bool StartWindCharge(Player* player, PlayState* play) {
    if (sIsWindCharging || !(player->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) {
        return false;
    }
    sIsWindCharging = true;
    sWindCharge = 0;
    sWindDrainTimer = WIND_DRAIN_INTERVAL;
    StartUpdraft(play, &player->actor.world.pos);
    Player_SetAction(play, player, (PlayerActionFunc)ChargeWindAction, 0);
    return true;
}

static void TickWindCharge(PlayState* play, Player* player, bool isHeld) {
    if (sWindBoostTimer > 0) {
        sWindBoostTimer--;
    }
    if (!sIsWindCharging) {
        TickUpdraft(play, false, 0.0f);
        return;
    }
    // Anything that took the stance away drops the charge without launching him.
    if (player->actionFunc != (PlayerActionFunc)ChargeWindAction || !(player->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) {
        EndWindCharge(player, play);
        TickUpdraft(play, false, 0.0f);
        return;
    }

    // Only the CLIMB is billed and only the climb flutters: sitting on a full column is silent and free, so
    // holding it is a question of when to leave, never of how much magic is left.
    if (sWindCharge < WIND_CHARGE_FULL) {
        sWindCharge++;
        if ((sWindCharge % WIND_FLUTTER_INTERVAL) == 0) {
            Player_PlaySfx(player, NA_SE_EN_KAICHO_FLUTTER);
        }
        if (--sWindDrainTimer <= 0) {
            sWindDrainTimer = WIND_DRAIN_INTERVAL;
            if (!SpendMagic(play, 1)) {
                EndWindCharge(player, play);
                TickUpdraft(play, false, 0.0f);
                return;
            }
        }
    }
    if (isHeld) {
        Z64Items_KeepHeld(sApi);
        TickUpdraft(play, true, (f32)sWindCharge / (f32)WIND_CHARGE_FULL);
        return;
    }
    if (sWindCharge >= WIND_CHARGE_MIN) {
        LaunchOnColumn(play, player);
    }
    EndWindCharge(player, play);
    TickUpdraft(play, false, 0.0f);
}

// Multiplies whatever vertical launch just happened for a short window after a gale, so a charge that ends in
// a side hop becomes real distance.
static void BoostNextJump(Player* player) {
    if (sWindBoostTimer <= 0 || player->actor.velocity.y <= 0.0f) {
        return;
    }
    player->actor.velocity.y *= WIND_BOOST;
}

static void EndGale(Player* player, PlayState* play) {
    sGalePhase = GALE_OFF;
    sGaleTimer = 0;
    player->actor.gravity = GALE_VANILLA_GRAVITY;
    if (player->actionFunc == (PlayerActionFunc)RideGaleAction) {
        func_8083B1A0(player, play);
    }
}

// A lock-on gives the BEARING only: the power is fixed, so overshooting is part of the tool.
static bool CastGale(Player* player, PlayState* play) {
    if (sGalePhase != GALE_OFF || !(player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) ||
        !SpendMagic(play, GALE_COST)) {
        return false;
    }
    sGaleYaw = HasLockOn(player) ? Math_Vec3f_Yaw(&player->actor.world.pos, &player->focusActor->focus.pos)
                                 : player->actor.shape.rot.y;
    sGalePhase = GALE_ANTICIPATE;
    sGaleTimer = GALE_ANTICIPATE_FRAMES;
    Player_SetAction(play, player, (PlayerActionFunc)RideGaleAction, 0);
    return true;
}

static void PlayGaleAnim(PlayState* play, Player* player, const char* animPath, bool doLoop) {
    PlayerAnimationHeader* anim = ResourceMgr_LoadPlayerAnimAsHeader(animPath);

    if (anim == NULL) {
        return;
    }
    PlayerAnimation_Change(play, &player->skelAnime, anim, 1.0f, 0.0f, Animation_GetLastFrame(anim),
                           doLoop ? ANIMMODE_LOOP : ANIMMODE_ONCE, GALE_MORPH_FRAMES);
}

static void KickOffGale(PlayState* play, Player* player) {
    player->actor.velocity.y = GALE_RISE;
    player->actor.gravity = 0.0f; // the arc is integrated below, not by the engine
    player->actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND;
    player->stateFlags1 |= PLAYER_STATE1_40000;
    player->fallStartHeight = (s16)player->actor.world.pos.y;
    PlayGaleAnim(play, player, sSwimAnim, true);

    ShakeCamera(play, GALE_QUAKE_SPEED, GALE_QUAKE_AMPLITUDE, GALE_QUAKE_FRAMES);
    SpawnDust(play, &player->actor.world.pos, 20.0f, 70.0f, 6, &sGaleDust);
    Player_PlaySfx(player, NA_SE_PL_ROLL);
}

static void FlyGale(Player* player) {
    Math_ScaledStepToS(&player->actor.shape.rot.y, sGaleYaw, GALE_TURN_RATE);
    player->actor.world.rot.y = player->actor.shape.rot.y;
    player->yaw = player->actor.shape.rot.y;
    player->speedXZ = GALE_SPEED_XZ;
    player->actor.speed = GALE_SPEED_XZ;
    player->actor.velocity.y += GALE_GRAVITY;
    player->actor.gravity = 0.0f;
}

// Anything solid ends the flight. The floor is exempt for the first frames because the launch velocity is
// only applied on the NEXT engine move, so Link is still standing on the ground he just left.
static bool HasGaleHitSomething(Player* player, s16 flown) {
    s16 solid = BGCHECKFLAG_WALL | BGCHECKFLAG_GROUND_TOUCH;

    if (flown >= GALE_LIFTOFF_FRAMES) {
        solid |= BGCHECKFLAG_GROUND;
    }
    return (player->actor.bgCheckFlags & solid) != 0;
}

// The flight is driven from the mod's tick, which runs after the action func: the body is the last word of
// the frame that way, and the action func itself is only there to keep vanilla out of the wheel.
static void RideGaleAction(Player* player, PlayState* play) {
    PlayerAnimation_Update(play, &player->skelAnime);
}

static void TickGale(PlayState* play, Player* player) {
    if (sGalePhase == GALE_OFF) {
        return;
    }
    if (player->actionFunc != (PlayerActionFunc)RideGaleAction) {
        EndGale(player, play);
        return;
    }
    Z64Items_KeepHeld(sApi);
    sGaleTimer--;

    switch (sGalePhase) {
        case GALE_ANTICIPATE:
            Player_StopHorizontalMovement(player);
            if (sGaleTimer <= 0) {
                sGalePhase = GALE_IMPULSE;
                sGaleTimer = GALE_IMPULSE_FRAMES;
            }
            break;

        case GALE_IMPULSE:
            Player_StopHorizontalMovement(player);
            if (sGaleTimer <= 0) {
                KickOffGale(play, player);
                sGalePhase = GALE_FLIGHT;
                sGaleTimer = GALE_FLIGHT_FRAMES;
            }
            break;

        case GALE_FLIGHT:
            FlyGale(player);
            if (sGaleTimer > 0 && !HasGaleHitSomething(player, GALE_FLIGHT_FRAMES - sGaleTimer)) {
                break;
            }
            if (player->actor.bgCheckFlags & (BGCHECKFLAG_WALL | BGCHECKFLAG_GROUND_TOUCH)) {
                Player_StopHorizontalMovement(player); // hitting something kills the run; a timeout keeps it
            }
            sGalePhase = GALE_FALL;
            sGaleTimer = GALE_FALL_LIMIT;
            player->actor.gravity = GALE_VANILLA_GRAVITY;
            break;

        case GALE_FALL:
            if (sGaleTimer > 0 && !(player->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) {
                break;
            }
            sGalePhase = GALE_ROLL;
            sGaleTimer = GALE_ROLL_FRAMES;
            Player_StopHorizontalMovement(player);
            PlayGaleAnim(play, player, sLandingRollAnim, false);
            Player_PlaySfx(player, NA_SE_PL_BOUND_DIRT);
            break;

        default: // GALE_ROLL: hand the body back and let the engine stand him up itself
            if (sGaleTimer <= 0) {
                EndGale(player, play);
            }
            break;
    }
}

// No Majora's Mask actor is a water geyser, so the column is a slab that rises and sinks under a water tint.
// The vanilla oscillation is kept: it is what sells the column as water rather than a lift.
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

static void ForgetGeyser(void) {
    sWaterGeyser = NULL;
}

static bool CastWater(Player* player, PlayState* play) {
    Vec3f pos;
    s16 yaw = player->actor.shape.rot.y;

    // Already up: the cast is the lift control, so it just flips which way the column is going.
    if (sWaterGeyser != NULL && sWaterGeyser->update == UpdateGeyser) {
        sWaterTargetHeight = sWaterTargetHeight > WATER_REST_HEIGHT ? WATER_REST_HEIGHT : WATER_RIDE_HEIGHT;
        return true;
    }
    if (!EnsureLiftObject(play)) {
        return false;
    }
    pos.x = player->actor.world.pos.x + Math_SinS(yaw) * WATER_SPAWN_DIST;
    pos.y = player->actor.world.pos.y;
    pos.z = player->actor.world.pos.z + Math_CosS(yaw) * WATER_SPAWN_DIST;

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

// Wrapped, not replaced: EnBom_Draw also billboards and runs Collider_UpdateSpheres, which is what gives the
// explosion its hitbox.
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
    gSPGrayscale(POLY_OPA_DISP++, false); // both buffers, or the rest of the frame comes out red too
    gSPGrayscale(POLY_XLU_DISP++, false);
    CLOSE_DISPS(play->state.gfxCtx);
}

// Read live rather than captured at launch, so switching target mid-flight redirects the bomb and dropping Z
// lets it run straight on.
static Actor* MeteorTarget(PlayState* play) {
    Player* player = GET_PLAYER(play);

    return HasLockOn(player) ? player->focusActor : NULL;
}

// Measured from the bomb's SHADOW: an enemy's world.pos sits on the floor, so a bomb mid-arc is a whole hop
// height away from a target it is passing straight through.
static bool IsMeteorTouchingEnemy(Actor* thisx, PlayState* play) {
    Vec3f shadow = { thisx->world.pos.x, thisx->floorHeight, thisx->world.pos.z };

    if (thisx->floorHeight <= BGCHECK_Y_MIN) {
        shadow.y = thisx->world.pos.y; // over a pit there is no shadow; it is falling, so use itself
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
    thisx->velocity.y = 0.0f; // or the vanilla floor bounce fights the curve
    thisx->speed = METEOR_LAUNCH_SPEED * falloff;
    METEOR_HOP_TIME(thisx)++;
}

static void UpdateMeteor(Actor* thisx, PlayState* play) {
    // Read before the vanilla update: EnBom_Move consumes both bits as it bounces.
    bool hasHitWall = (thisx->bgCheckFlags & BGCHECKFLAG_WALL) != 0;
    bool hasLanded = (thisx->bgCheckFlags & BGCHECKFLAG_GROUND_TOUCH) != 0;
    Actor* target;

    if (thisx->params == BOMB_TYPE_BODY) {
        if (METEOR_AGE(thisx) < METEOR_ARM_FRAMES) {
            METEOR_AGE(thisx)++;
        } else if (hasHitWall || IsMeteorTouchingEnemy(thisx, play)) {
            // Anything solid sets it off. Only the floor may be hit over and over: that is the skip.
            ((EnBom*)thisx)->timer = 0;
        }
        // The throw itself is vanilla gravity out of Link's hand; the skip begins where it lands.
        if (hasLanded && METEOR_HOP_TIME(thisx) == METEOR_AIRBORNE) {
            METEOR_HOP_TIME(thisx) = 0;
        }
        target = MeteorTarget(play);
        if (target != NULL) {
            // world.rot.y is the heading the bomb's speed is driven along, so turning it steers.
            Math_ScaledStepToS(&thisx->world.rot.y, Math_Vec3f_Yaw(&thisx->world.pos, &target->focus.pos),
                               METEOR_STEER_RATE);
        }
    }

    sMeteorBombUpdate(thisx, play);

    if (thisx->params == BOMB_TYPE_BODY && METEOR_HOP_TIME(thisx) != METEOR_AIRBORNE &&
        thisx->floorHeight > BGCHECK_Y_MIN) {
        HopMeteor(thisx);
    }
}

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
    bomb->speed = METEOR_LAUNCH_SPEED;
    bomb->velocity.y = METEOR_LAUNCH_RISE;
    bomb->world.rot.y = yaw;

    Audio_PlaySfx_AtPos(&bomb->world.pos, NA_SE_PL_THROW);
    return true;
}

static void ForgetStormRay(void) {
    sStormRay.isActive = false;
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

    // The bolt IS the visual: the effect draws and animates itself, so the rod owns no draw at all.
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

static bool FireStormRay(Player* player, PlayState* play, Actor* target) {
    Vec3f localVel = { 0.0f, 0.0f, STORM_RAY_SPEED };
    s16 yaw;
    s16 pitch;

    if (sStormRay.isActive) {
        return false; // one ray at a time
    }
    sStormRay.pos = player->actor.world.pos;
    sStormRay.pos.y += STORM_RAY_SPAWN_HEIGHT;

    if (target != NULL) {
        // focus.pos, not world.pos: that is the point the game itself considers "where you aimed".
        yaw = Math_Vec3f_Yaw(&sStormRay.pos, &target->focus.pos);
        pitch = Math_Vec3f_Pitch(&sStormRay.pos, &target->focus.pos);
    } else {
        yaw = player->actor.shape.rot.y;
        pitch = 0;
    }
    // Rebuilding this from Math_SinS by hand gets the pitch sign backwards, which is why it goes through the
    // matrix stack the way the elemental rods' own conversion does.
    Matrix_Push();
    Matrix_RotateYS(yaw, MTXMODE_NEW);
    Matrix_RotateXS(pitch, MTXMODE_APPLY);
    Matrix_MultVec3f(&localVel, &sStormRay.vel);
    Matrix_Pop();

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
        return FireStormRay(player, play, player->focusActor);
    }
    return CallStorm(player, play);
}

// A frozen enemy is the Deku Nut effect: the freeze stops its update and the colour filter is what makes it
// read as stunned.
static void StunEnemy(Actor* enemy) {
    enemy->freezeTimer = SHADOW_STUN_FRAMES;
    Actor_SetColorFilter(enemy, COLORFILTER_COLORFLAG_BLUE, SHADOW_FILTER_STRENGTH, COLORFILTER_BUFFLAG_OPA,
                         SHADOW_STUN_FRAMES);
}

static void ForgetShadowBolt(void) {
    sShadowBolt.isActive = false;
    sShadowBolt.target = NULL;
}

// Re-acquired every frame rather than trusted: the enemy the bolt left chasing can die before it arrives.
static void TickShadowBolt(PlayState* play) {
    Actor* hit;

    if (!sShadowBolt.isActive) {
        return;
    }
    if (--sShadowBolt.life <= 0) {
        ForgetShadowBolt();
        return;
    }
    if (sShadowBolt.target != NULL && sShadowBolt.target->update != NULL) {
        sShadowBolt.yaw = Math_Vec3f_Yaw(&sShadowBolt.pos, &sShadowBolt.target->world.pos);
    }
    sShadowBolt.pos.x += Math_SinS(sShadowBolt.yaw) * SHADOW_SPEED;
    sShadowBolt.pos.z += Math_CosS(sShadowBolt.yaw) * SHADOW_SPEED;

    hit = FindNearestEnemy(play, &sShadowBolt.pos, SHADOW_HIT_RADIUS);
    if (hit != NULL) {
        StunEnemy(hit);
        Audio_PlaySfx_AtPos(&sShadowBolt.pos, NA_SE_EN_FANTOM_DAMAGE);
        ForgetShadowBolt();
    }
}

static void DrawShadowBolt(PlayState* play) {
    if (!sShadowBolt.isActive) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    Matrix_Translate(sShadowBolt.pos.x, sShadowBolt.pos.y, sShadowBolt.pos.z, MTXMODE_NEW);
    // The Boe is a flat billboard: its own draw replaces the rotation the same way.
    Matrix_ReplaceRotation(&play->billboardMtxF);
    Matrix_Scale(SHADOW_SCALE, SHADOW_SCALE, SHADOW_SCALE, MTXMODE_APPLY);

    // The Boe's material lists reach for segment 8; pointing it at an empty list is what keeps them from
    // running whatever the scene happened to leave there.
    gSPSegment(POLY_XLU_DISP++, 0x08, (uintptr_t)sEmptyDL);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sBoeMaterialDL);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sBoeModelDL);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sBoeEndDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static bool CastShadow(Player* player, PlayState* play) {
    s16 yaw = player->actor.shape.rot.y;

    if (sShadowBolt.isActive) {
        return false; // one bolt at a time
    }
    sShadowBolt.pos.x = player->actor.world.pos.x + Math_SinS(yaw) * SHADOW_SPAWN_DIST;
    sShadowBolt.pos.y = player->actor.world.pos.y + SHADOW_SPAWN_HEIGHT;
    sShadowBolt.pos.z = player->actor.world.pos.z + Math_CosS(yaw) * SHADOW_SPAWN_DIST;

    // No enemy in reach is not a failed cast: the bolt flies off Link's nose and fades on its timer.
    sShadowBolt.target = FindNearestEnemy(play, &player->actor.world.pos, SHADOW_SEEK_RANGE);
    sShadowBolt.yaw = yaw;
    sShadowBolt.life = SHADOW_LIFE_FRAMES;
    sShadowBolt.isActive = true;

    Audio_PlaySfx_AtPos(&player->actor.world.pos, NA_SE_IT_SHIELD_REFLECT_MG);
    return true;
}

// Magic is spent only on success, so a rod that declines (no room for the actor, a geyser already up) costs
// nothing and reports the error itself.
static bool CastRod(Player* player, PlayState* play, u8 mode) {
    bool didCast;

    switch (mode) {
        case WAND_SAND:
            didCast = CastSand(player, play);
            break;
        case WAND_TORNADO:
            didCast = StartWindCharge(player, play);
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
        case WAND_SCEPTER:
            didCast = CastShadow(player, play);
            break;
        default:
            return false;
    }
    if (didCast) {
        SpendMagic(play, sRods[mode].magicCost);
    }
    return didCast;
}

static bool StartSwingClip(PlayState* play, Player* player, u8 clip) {
    PlayerAnimationHeader* anim = ResourceMgr_LoadPlayerAnimAsHeader(clip == WAND_SWING_FORE ? sSwingForeAnim
                                                                                             : sSwingBackAnim);

    if (anim == NULL) {
        return false;
    }
    PlayerAnimation_PlayOnce(play, &player->skelAnimeUpper, anim);
    sSwingClip = clip;
    return true;
}

static bool StartSwing(PlayState* play, Player* player, u8 mode) {
    // The carry animation owns the arms when a rod put something in Link's hands, and the swing would tear
    // the held actor off them.
    if (player->stateFlags1 & PLAYER_STATE1_CARRYING_ACTOR) {
        return false;
    }
    sSwingMode = mode;
    sHasSwingFired = false;
    return StartSwingClip(play, player, WAND_SWING_FORE);
}

/**
 * The swing plays on the upper body, so the legs keep walking or running underneath it and the rod can be cast
 * on the move; returning 1 is what claims Link's arms. The rod goes off on a frame of the second slash: the
 * summon belongs to the animation, not to the button.
 */
static int32_t SwingWandUpper(Player* player, PlayState* play) {
    if (!sIsWandOut || sSwingClip == WAND_SWING_NONE) {
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
    sIsPressPending = false;
    sSwingClip = WAND_SWING_NONE;
}

// The press is only recorded here: resolving it inside the player's own action handler list would let the
// frame's own action func overwrite whatever the rod installed.
static void PressWand(Player* player, PlayState* play) {
    sIsPressPending = true;
}

static void PutWandAway(Player* player, PlayState* play) {
    sIsWandOut = false;
    sSwingClip = WAND_SWING_NONE;
    if (sIsWindCharging) {
        EndWindCharge(player, play);
    }
}

static bool CanUseWand(Player* player, PlayState* play) {
    return !func_800B7118(player) && Z64Wheel_CountUnlocked() > 0;
}

// A rod that owns something outside the staff keeps running with it stowed: the bomb is mid-flight, the
// column still stands and the bolt has not reached anything yet.
static void UpdateWand(Player* player) {
    PlayState* play = gPlayState;
    bool isHeld;
    u8 mode;

    if (play == NULL) {
        return;
    }

    // Everything a rod leaves in the world is an actor the new scene has already thrown away.
    if (sLastScene != play->sceneId) {
        sLastScene = play->sceneId;
        ForgetSandSlabs();
        ForgetGeyser();
        ForgetShadowBolt();
        ForgetStormRay();
        ForgetUpdraft();
        sIsWindCharging = false;
        sGalePhase = GALE_OFF;
        sIsStormColliderReady = false;
        sSwingClip = WAND_SWING_NONE;
    }
    SyncUnlockedRods();

    isHeld = IsButtonHeld(play);
    TickShadowBolt(play);
    TickStormRay(play, player);
    TickWindCharge(play, player, isHeld);
    TickGale(play, player);
    BoostNextJump(player);

    if (!sIsWandOut) {
        sSwingClip = WAND_SWING_NONE; // put away mid-swing: the arms go back to the item change
        sWasWandOut = false;
        sIsPressPending = false;
        return;
    }

    mode = CurrentMode();
    // Holding the button lays a sand road. The Tornado's own hold is its charge, ticked above.
    if (mode == WAND_SAND && sSwingClip == WAND_SWING_NONE && ShouldLaySlab(player, isHeld)) {
        if (HasMagic(play, sRods[WAND_SAND].magicCost)) {
            CastRod(player, play, WAND_SAND);
        }
    }

    // The press that drew the staff is not a cast: sWasWandOut is what tells them apart.
    if (sIsPressPending && sWasWandOut && sSwingClip == WAND_SWING_NONE && sGalePhase == GALE_OFF && !sIsWindCharging) {
        bool didFire;

        // Locked on, the Tornado fires Link himself instead of raising a column: no swing, no charge.
        if (mode == WAND_TORNADO && HasLockOn(player)) {
            didFire = CastGale(player, play);
        } else {
            didFire = HasMagic(play, sRods[mode].magicCost) && StartSwing(play, player, mode);
        }
        if (!didFire) {
            Audio_PlaySfx_AtPos(&player->actor.world.pos, NA_SE_SY_ERROR);
        }
    }
    sIsPressPending = false;
    sWasWandOut = true;
}

// Nothing touches the matrix between the hand limb and the end of the post-limb pass while the staff is out,
// so this is the hand bone's own matrix: the rod follows the arm instead of floating beside Link.
static void CaptureHandMatrix(Player* player, s32 limbIndex) {
    if (limbIndex != PLAYER_LIMB_RIGHT_HAND) {
        return;
    }
    Matrix_Get(&sHandMatrix);
    sHasHandMatrix = true;
}

static void DrawWandInHand(Player* player, PlayState* play) {
    u8 mode = CurrentMode();
    f32 unscale = player->actor.scale.x != 0.0f ? 1.0f / player->actor.scale.x : 1.0f;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    Matrix_Put(&sHandMatrix);
    // That matrix carries Link's own 0.01 body scale: dividing it back out leaves the offsets in world units.
    Matrix_Scale(unscale, unscale, unscale, MTXMODE_APPLY);
    Matrix_RotateYF(WAND_HELD_ROT_Y * WAND_DEGREES_TO_RADIANS, MTXMODE_APPLY);
    Matrix_RotateXF(WAND_HELD_ROT_X * WAND_DEGREES_TO_RADIANS, MTXMODE_APPLY);
    Matrix_RotateZF(WAND_HELD_ROT_Z * WAND_DEGREES_TO_RADIANS, MTXMODE_APPLY);
    // The offsets come after the rotations, so each slides the staff along its OWN shaft.
    Matrix_Translate(0.0f, WAND_HELD_OFFSET_Y, WAND_HELD_OFFSET_Z, MTXMODE_APPLY);
    Matrix_Scale(WAND_HELD_SCALE, WAND_HELD_SCALE, WAND_HELD_SCALE, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sRods[mode].opaModel);

    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sRods[mode].xluModel);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawWand(Actor* actor, PlayState* play) {
    // Both of these outlive the staff being out: the column burns and the bolt flies whatever Link is doing.
    DrawUpdraft(play);
    DrawShadowBolt(play);

    if (!sIsWandOut || !sHasHandMatrix) {
        return;
    }
    DrawWandInHand((Player*)actor, play);
}

static void ForgetSceneState(s16 sceneId, s8 spawnNum) {
    sLastScene = sceneId;
    ForgetSandSlabs();
    ForgetGeyser();
    ForgetShadowBolt();
    ForgetStormRay();
    ForgetUpdraft();
    sIsStormColliderReady = false;
    sIsWandOut = false;
    sIsWindCharging = false;
    sGalePhase = GALE_OFF;
}

static void ReloadRods(s16 fileNum) {
    SyncUnlockedRods();
}

// Shuffled once by the randomizer; no vanilla logic needs it.
static const S2HCustomItemRandomizer sRandomizerItem = {
    sizeof(S2HCustomItemRandomizer), 0, S2H_CUSTOM_ITEM_TYPE_ITEM, 0, 1, WAND_KEY, WAND_KEY, "",
};

S2H_MOD_EXPORT void ModSetApi(const S2HModApi* api) {
    sApi = api;
}

S2H_MOD_EXPORT const S2HModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

S2H_MOD_EXPORT void ModInit(void) {
    S2HModRandoOption shuffle = { sizeof(S2HModRandoOption),
                                  WAND_SHUFFLE_OPTION,
                                  "Elemental Wand Rods",
                                  "Which of the six rods the staff can be tuned to: one per medallion, all "
                                  "six from the staff itself, or one fixed function per pickup of the same staff.",
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
    Z64Items_SetTextbox(&wand, "You got the %rElemental Wand%w!&A staff that answers to six&elements, one at a "
                               "time.^Press its %yC button%w to swing it and cast&the element it is tuned to.&Hold "
                               "%yL%w to turn the wheel.^Every %gmedallion%w you carry adds&a rod the staff "
                               "can become.");
    Z64Items_SetPauseText(&wand, "%rElemental Wand&%wSix rods in one staff.&Its %yC button%w casts, %yL%w picks the "
                                 "rod.");
    Z64Items_SetAction(&wand, TakeOutWand, SwingWandUpper);
    Z64Items_SetHeldCallbacks(&wand, PressWand, PutWandAway, NULL);
    Z64Items_SetCanUse(&wand, CanUseWand);
    Z64Items_SetGetItemModel(&wand, sSandOpa);
    wand.modelGroup = PLAYER_MODELGROUP_DEFAULT;
    wand.onReceive = LightNextRod;

    S2HCustomItemRandomizer wandLogic = sRandomizerItem;
    wandLogic.flags |= S2H_CUSTOM_ITEM_RANDO_ADVANCEMENT;
    wandLogic.poolModeOption = WAND_SHUFFLE_OPTION;
    wandLogic.poolModeValue = WAND_SHUFFLE_RODS;
    wandLogic.poolModeInvert = true;
    Z64Items_SetLogic(&wand, &wandLogic);
    if (!Z64Items_Register(sApi, &wand)) {
        return;
    }
    static const char* const texts[WAND_MODE_MAX] = {
        "You got the %ySand Rod%w!&Its sand raises a road beneath you.",
        "You got the %gTornado Rod%w!&Raise an updraft or launch toward&a locked-on target.",
        "You got the %bWater Rod%w!&Call up a column of water.",
        "You got the %rMeteor Rod%w!&Call down a blazing meteor.",
        "You got the %yStorm Rod%w!&Channel the power of a storm.",
        "You got the %pShadow Scepter%w!&Command the shadows."
    };
    static const char* const icons[WAND_MODE_MAX] = { sIconSand, sIconTornado, sIconWater, sIconMeteor, sIconStorm, sIconScepter };
    for (u8 mode = 0; mode < WAND_MODE_MAX; mode++) {
        S2HCustomPickupVariant pickup = Z64Items_DefinePickup(WAND_KEY, sVariantNames[mode], sSeparateKeys[mode]);
        pickup.iconPath = icons[mode];
        pickup.getItemModelPath = sRods[mode].opaModel;
        pickup.getItemText = texts[mode];
        pickup.onReceive = ReceiveWandFunction;
        S2HCustomItemRandomizer randomizer = sRandomizerItem;
        randomizer.flags |= S2H_CUSTOM_ITEM_RANDO_ADVANCEMENT;
        randomizer.logicKey = randomizer.hintKey = pickup.key;
        randomizer.poolModeOption = WAND_SHUFFLE_OPTION;
        randomizer.poolModeValue = WAND_SHUFFLE_RODS;
        pickup.randomizer = &randomizer;
        if (!Z64Items_RegisterPickup(sApi, &pickup)) return;
    }
    sApi->RegisterRandoOption(&shuffle);
    Z64Wheel_Register(sApi, WAND_KEY, WAND_KEY, sWheelEntries, ARRAY_COUNT(sWheelEntries), false);
    S2H_REGISTER_HOOK(sApi, OnPlayerUpdate, UpdateWand);
    S2H_REGISTER_HOOK(sApi, OnSceneInit, ForgetSceneState);
    S2H_REGISTER_HOOK(sApi, OnSaveLoad, ReloadRods);
    S2H_REGISTER_HOOK(sApi, OnPlayerPostLimbDraw, CaptureHandMatrix);
    S2H_REGISTER_HOOK_FOR_ID(sApi, OnActorDrawEnd, ACTOR_PLAYER, DrawWand);
}
