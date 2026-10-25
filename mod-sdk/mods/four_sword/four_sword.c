/**
 * Four Sword: holding the shield up with B held splits Link in four. The three clones keep a formation, swing with
 * him and fire what he fires; holding L picks the formation. The grid pushes any block, the cross spins as one
 * wheel, and the totem is a stack he can carry and jump off.
 */

#include <math.h>
#include <string.h>

#include "z64items.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "objects/object_link_child/object_link_child.h"
#include "overlays/actors/ovl_En_Arrow/z_en_arrow.h"
#include "overlays/actors/ovl_En_Bom/z_en_bom.h"
#include "overlays/actors/ovl_En_M_Thunder/z_en_m_thunder.h"
#include "z64wheel.h"

#define FOUR_SWORD_KEY "nei.equip.four_sword"
#define CLONE_KEY "nei.equip.four_sword_clone"
#define MOD_NAME "four_sword"
#define CLONE_COUNT 3
#define ALL_CLONES ((1 << CLONE_COUNT) - 1)
#define CHARGE_HOLD 15
#define ITEM_COOLDOWN 10
// Long enough that a tap of L still reaches the vanilla Z-target.
#define WHEEL_HOLD_FRAMES 8

// Shoulder to shoulder: two Link radii is 34, so this is barely wider than the pushing huddle.
#define SLOT_RADIUS 45.0f
#define STACK_HEIGHT 40.0f
#define BLADE_RADIUS 25
#define PUSH_SPREAD 34.0f
#define CLONE_SCALE 0.01f
#define SUMMON_FRAMES 12
#define SLOT_PULL 0.45f
#define SWING_FRAMES 8
#define WALL_HEIGHT 26.0f
#define WALL_RADIUS 10.0f
#define BGCHECK_FLAGS (UPDBGCHECKINFO_FLAG_1 | UPDBGCHECKINFO_FLAG_4)
#define SPIN_RADIUS_SCALE 2.5f
#define SPIN_STEP 0x0700
#define RING_SCALE 2.0f
#define CROSS_CHARGE_YAW_DEG 27
#define STRAFE_MOVING 0.5f
#define STRAFE_RATE 1.0f
#define TOTEM_BASE 1
#define TOTEM_TOP (CLONE_COUNT - 1)
// Actor_OfferCarry reaches 50 units in XZ and only 10 in Y: the base parks well inside both.
#define GRAB_REACH 30.0f
#define TOTEM_CAMERA_SCALE 1.6f
#define TOTEM_THROW_UP 16.0f
#define TOTEM_THROW_FORWARD 14.0f
#define CLONE_TINT_LERP 190
// z_effect.c keeps its slot count private: 3 sparks, 25 blurs, 3 shield particles and 15 tire marks.
#define NO_EFFECT_SLOT 46

static const ALIGN_ASSET(2) char sIcon[] = "__OTR__textures/icon_item_custom/gItemIconFourSwordTex";
static const ALIGN_ASSET(2) char sName[] = "__OTR__textures/item_name_custom/gFourSwordNameTex";
static const ALIGN_ASSET(2) char sBladeDL[] = "__OTR__objects/object_nei_four_sword/gNeiFourSwordBladeDL";
static const ALIGN_ASSET(2) char sHiltDL[] = "__OTR__objects/object_nei_four_sword/gNeiFourSwordHiltDL";

// Imported by address: compared against the player's actionFunc, which holds the real function.
extern HOST_DATA void Player_Action_37(Player* player, PlayState* play);
void func_808379C0(PlayState* play, Player* player);
void func_80834DB8(Player* player, PlayerAnimationHeader* anim, f32 lift, PlayState* play);

typedef enum {
    FORM_TRIANGLE,
    FORM_LINE_LEFT,
    FORM_LINE_RIGHT,
    FORM_GRID,
    FORM_CROSS_BACK,
    FORM_CROSS_FRONT,
    FORM_CROSS_LEFT,
    FORM_CROSS_RIGHT,
    FORM_TOTEM,
    FORM_DISPERSE,
    FORM_MAX,
} Formation;

typedef struct {
    f32 leftOfLink;
    f32 aheadOfLink;
    f32 aboveLink;
} FormationSlot;

static const FormationSlot sFormationSlots[FORM_MAX][CLONE_COUNT] = {
    { { 0.0f, SLOT_RADIUS, 0.0f }, { 69.3f, -40.0f, 0.0f }, { -69.3f, -40.0f, 0.0f } },
    { { -SLOT_RADIUS, 0.0f, 0.0f }, { -2 * SLOT_RADIUS, 0.0f, 0.0f }, { -3 * SLOT_RADIUS, 0.0f, 0.0f } },
    { { SLOT_RADIUS, 0.0f, 0.0f }, { 2 * SLOT_RADIUS, 0.0f, 0.0f }, { 3 * SLOT_RADIUS, 0.0f, 0.0f } },
    { { -SLOT_RADIUS, 0.0f, 0.0f }, { 0.0f, -SLOT_RADIUS, 0.0f }, { -SLOT_RADIUS, -SLOT_RADIUS, 0.0f } },
    { { SLOT_RADIUS, -SLOT_RADIUS, 0.0f }, { -SLOT_RADIUS, -SLOT_RADIUS, 0.0f }, { 0.0f, -2 * SLOT_RADIUS, 0.0f } },
    { { SLOT_RADIUS, SLOT_RADIUS, 0.0f }, { -SLOT_RADIUS, SLOT_RADIUS, 0.0f }, { 0.0f, 2 * SLOT_RADIUS, 0.0f } },
    { { -SLOT_RADIUS, SLOT_RADIUS, 0.0f }, { -SLOT_RADIUS, -SLOT_RADIUS, 0.0f }, { -2 * SLOT_RADIUS, 0.0f, 0.0f } },
    { { SLOT_RADIUS, SLOT_RADIUS, 0.0f }, { SLOT_RADIUS, -SLOT_RADIUS, 0.0f }, { 2 * SLOT_RADIUS, 0.0f, 0.0f } },
    { { 0.0f, 0.0f, STACK_HEIGHT }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 2 * STACK_HEIGHT } },
    { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } },
};

// The wheel has one box per formation; the stick's vertical axis walks the variants of a line or a cross.
typedef enum {
    ENTRY_TRIANGLE,
    ENTRY_LINE,
    ENTRY_GRID,
    ENTRY_CROSS,
    ENTRY_TOTEM,
    ENTRY_DISPERSE,
    ENTRY_MAX,
} WheelEntry;

#define VARIANT_MAX 4
#define VARIANT_STICK_DEADZONE 30

static const Formation sEntryFirstFormation[ENTRY_MAX] = {
    FORM_TRIANGLE, FORM_LINE_LEFT, FORM_GRID, FORM_CROSS_BACK, FORM_TOTEM, FORM_DISPERSE,
};
static const u8 sEntryVariantCount[ENTRY_MAX] = { 1, 2, 1, 4, 1, 1 };
static const char* const sEntryIcons[ENTRY_MAX][VARIANT_MAX] = {
    { "__OTR__textures/four_sword/gFourSwordFormTriangleTex" },
    { "__OTR__textures/four_sword/gFourSwordFormLine0Tex", "__OTR__textures/four_sword/gFourSwordFormLine1Tex" },
    { "__OTR__textures/four_sword/gFourSwordFormGridTex" },
    { "__OTR__textures/four_sword/gFourSwordFormCross0Tex", "__OTR__textures/four_sword/gFourSwordFormCross1Tex",
      "__OTR__textures/four_sword/gFourSwordFormCross2Tex", "__OTR__textures/four_sword/gFourSwordFormCross3Tex" },
    { "__OTR__textures/four_sword/gFourSwordFormTotemTex" },
    { "__OTR__textures/four_sword/gFourSwordFormDisperseTex" },
};

// Every entry wears the sword's own icon on its item, so picking a formation never repaints the Four Sword.
static Z64WheelEntry sWheelEntries[ENTRY_MAX] = {
    { "__OTR__textures/four_sword/gFourSwordFormTriangleTex", "Triangle", sIcon },
    { "__OTR__textures/four_sword/gFourSwordFormLine0Tex", "Line", sIcon },
    { "__OTR__textures/four_sword/gFourSwordFormGridTex", "Grid", sIcon },
    { "__OTR__textures/four_sword/gFourSwordFormCross0Tex", "Cross", sIcon },
    { "__OTR__textures/four_sword/gFourSwordFormTotemTex", "Totem", sIcon },
    { "__OTR__textures/four_sword/gFourSwordFormDisperseTex", "Disperse", sIcon },
};

static struct {
    u8 variant;
    u8 browsed[ENTRY_MAX];
    bool wasOpen;
    bool isStickHeld;
} sWheel;

static const Color_RGB8 sCloneTints[CLONE_COUNT] = { { 190, 40, 40 }, { 40, 70, 200 }, { 140, 40, 190 } };

typedef struct {
    Actor actor;
    ColliderCylinder hurtbox;
    ColliderCylinder blade;
    ColliderQuad shield;
    Vec3s pose[PLAYER_LIMB_MAX + 1];
    f32 strafeFrame;
    bool ownsPose;
    s32 trailEffectIndex;
    WeaponInfo trailInfo[3];
    MtxF swordHandMtx;
    bool isSwordHandMtxValid;
    u8 index;
    bool isColliderReady;
    s16 summonTimer;
    s16 swingTimer;
} FourSwordClone;

// OC leaves TYPE_PLAYER out so a formation can never wall Link into a corridor.
static ColliderCylinderInit sHurtboxInit = {
    { COL_MATERIAL_NONE, AT_NONE, AC_ON | AC_TYPE_ENEMY, OC1_ON | OC1_TYPE_1 | OC1_TYPE_2, OC2_TYPE_1,
      COLSHAPE_CYLINDER },
    { ELEM_MATERIAL_UNK0, { 0x00000000, 0x00, 0x00 }, { 0xFFFFFFFF, 0x00, 0x00 }, ATELEM_NONE, ACELEM_ON, OCELEM_ON },
    { 18, 46, 0, { 0, 0, 0 } },
};

// The player's own shield quad, so a clone's shield blocks like the real one.
static ColliderQuadInit sShieldInit = {
    { COL_MATERIAL_METAL, AT_ON | AT_TYPE_PLAYER, AC_ON | AC_HARD | AC_TYPE_ENEMY, OC1_NONE, OC2_TYPE_PLAYER,
      COLSHAPE_QUAD },
    { ELEM_MATERIAL_UNK2,
      { 0x00100000, 0x00, 0x00 },
      { 0xD7CFFFFF, 0x00, 0x00 },
      ATELEM_ON | ATELEM_SFX_NORMAL,
      ACELEM_ON,
      OCELEM_NONE },
    { { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } } },
};

static ColliderCylinderInit sBladeInit = {
    { COL_MATERIAL_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_CYLINDER },
    { ELEM_MATERIAL_UNK2,
      { DMG_SWORD, 0x00, 0x02 },
      { 0xFFCFFFFF, 0x00, 0x00 },
      ATELEM_ON | ATELEM_SFX_NORMAL,
      ACELEM_NONE,
      OCELEM_NONE },
    { BLADE_RADIUS, 60, 30, { 0, 0, 0 } },
};

static const S2HModApi* sApi;
static s16 sCloneActorId = -1;
static FourSwordClone* sLiveClones[CLONE_COUNT];
// The actors die with every scene; the mask is what carries a summon across a room change.
static u8 sCloneMask;
static Player sCloneDrawTemplate;
static u8 sUpperBodyMap[PLAYER_LIMB_MAX + 1];
static Vec3s sChargePose[PLAYER_LIMB_MAX + 1];
static Gfx sHeldSwordDL[8];

static struct {
    s16 chargeHold;
    bool isCharging;
    s16 wheelHold;
    s16 itemCooldown;
    u8 prevShotTimer;
    bool wasCarrying;
    bool wasTotemReleased;
    s16 previousHealth;
    s16 spinOrbit;
    Vec3f crossHub;
    Vec3f crossLinkPlaced;
    bool isCrossHubHeld;
    s16 crossFormationYaw;
    s16 crossLinkYawOffset;
} sSword;

static bool IsWorn(void) {
    return CustomEquipRegistry_IsWorn(FOUR_SWORD_KEY);
}

static Formation GetFormation(void) {
    u32 entry = Z64Wheel_GetSelection();

    if (entry >= ENTRY_MAX) {
        return FORM_TRIANGLE;
    }
    return sEntryFirstFormation[entry] + MIN(sWheel.variant, sEntryVariantCount[entry] - 1);
}

static bool IsCross(Formation formation) {
    return formation >= FORM_CROSS_BACK && formation <= FORM_CROSS_RIGHT;
}

static u8 CountClones(void) {
    u8 count = 0;

    for (s32 i = 0; i < CLONE_COUNT; i++) {
        count += sLiveClones[i] != NULL;
    }
    return count;
}

static s16 GetCrossChargeYaw(void) {
    return (s16)(CROSS_CHARGE_YAW_DEG * 0x10000 / 360);
}

static FourSwordClone* GetTotemBase(void) {
    return GetFormation() == FORM_TOTEM ? sLiveClones[TOTEM_BASE] : NULL;
}

static bool IsTotemCarried(void) {
    FourSwordClone* base = GetTotemBase();

    return base != NULL && base->actor.parent != NULL && sCloneMask == ALL_CLONES;
}

// Reads the mask, not the live actors: the grant must not blink off while a room change rebuilds them.
static bool DoesGridPushAnyBlock(void) {
    return IsWorn() && GetFormation() == FORM_GRID && sCloneMask == ALL_CLONES;
}

static bool IsPushing(Player* player) {
    return GetFormation() == FORM_GRID && (player->stateFlags2 & PLAYER_STATE2_10);
}

// The charge is read off the live En_M_Thunder, so the blades turn inward on exactly the frames the glow is up.
static EnMThunder* FindSpinCharge(PlayState* play) {
    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_ITEMACTION].first; actor != NULL; actor = actor->next) {
        EnMThunder* thunder = (EnMThunder*)actor;

        if (actor->id == ACTOR_EN_M_THUNDER && thunder->isCharging) {
            return thunder;
        }
    }
    return NULL;
}

static bool IsCrossCharging(PlayState* play) {
    return IsCross(GetFormation()) && FindSpinCharge(play) != NULL;
}

static bool IsCrossSpinning(Player* player) {
    return IsCross(GetFormation()) && player->meleeWeaponState > 0 &&
           player->meleeWeaponAnimation >= PLAYER_MWA_SPIN_ATTACK_1H &&
           player->meleeWeaponAnimation <= PLAYER_MWA_BIG_SPIN_2H;
}

// The cross puts Link on one arm, so its hub is the average of the four bodies, not his feet.
static void GetCrossHubOffset(f32* left, f32* ahead) {
    Formation formation = GetFormation();

    *left = 0.0f;
    *ahead = 0.0f;
    for (s32 i = 0; i < CLONE_COUNT; i++) {
        *left += sFormationSlots[formation][i].leftOfLink;
        *ahead += sFormationSlots[formation][i].aheadOfLink;
    }
    *left /= CLONE_COUNT + 1;
    *ahead /= CLONE_COUNT + 1;
}

// Link's body carries the inward turn too, so the geometry is measured from the yaw without it.
static s16 GetFormationYaw(Player* player) {
    return sSword.isCrossHubHeld ? sSword.crossFormationYaw : player->actor.shape.rot.y;
}

static void GetCrossHub(Player* player, Vec3f* out) {
    f32 hubLeft;
    f32 hubAhead;
    f32 sin = Math_SinS(GetFormationYaw(player));
    f32 cos = Math_CosS(GetFormationYaw(player));

    if (sSword.isCrossHubHeld) {
        *out = sSword.crossHub;
        return;
    }
    GetCrossHubOffset(&hubLeft, &hubAhead);
    *out = player->actor.world.pos;
    out->x += hubAhead * sin + hubLeft * cos;
    out->z += hubAhead * cos - hubLeft * sin;
}

static void RotateAroundHub(f32 armLeft, f32 armAhead, f32* spunLeft, f32* spunAhead) {
    f32 orbitSin = Math_SinS(sSword.spinOrbit);
    f32 orbitCos = Math_CosS(sSword.spinOrbit);

    *spunLeft = armLeft * orbitCos - armAhead * orbitSin;
    *spunAhead = armLeft * orbitSin + armAhead * orbitCos;
}

// The totem hangs off the base clone, which parks within grab reach; the other two stack on it.
static Vec3f GetTotemSlot(Player* player, u8 index) {
    FourSwordClone* base = sLiveClones[TOTEM_BASE];
    Vec3f out = player->actor.world.pos;

    if (index == TOTEM_BASE) {
        out.x += Math_SinS(player->actor.shape.rot.y) * GRAB_REACH;
        out.z += Math_CosS(player->actor.shape.rot.y) * GRAB_REACH;
        return out;
    }
    if (base != NULL) {
        out = base->actor.world.pos;
    }
    out.y += sFormationSlots[FORM_TOTEM][index].aboveLink;
    return out;
}

// Spinning, the four orbit the hub as one wheel and the ring opens up.
static Vec3f GetCrossSlot(Player* player, const FormationSlot* slot) {
    f32 hubLeft;
    f32 hubAhead;
    f32 reach = IsCrossSpinning(player) ? SPIN_RADIUS_SCALE : 1.0f;
    f32 spunLeft;
    f32 spunAhead;
    f32 sin = Math_SinS(GetFormationYaw(player));
    f32 cos = Math_CosS(GetFormationYaw(player));
    Vec3f out;

    GetCrossHubOffset(&hubLeft, &hubAhead);
    GetCrossHub(player, &out);
    RotateAroundHub((slot->leftOfLink - hubLeft) * reach, (slot->aheadOfLink - hubAhead) * reach, &spunLeft,
                    &spunAhead);
    out.x += spunAhead * sin + spunLeft * cos;
    out.z += spunAhead * cos - spunLeft * sin;
    return out;
}

static Vec3f GetSlotPosition(Player* player, u8 index) {
    Formation formation = GetFormation();
    const FormationSlot* slot = &sFormationSlots[formation][index];
    f32 spread = IsPushing(player) ? PUSH_SPREAD / SLOT_RADIUS : 1.0f;
    f32 sin = Math_SinS(GetFormationYaw(player));
    f32 cos = Math_CosS(GetFormationYaw(player));
    Vec3f out = player->actor.world.pos;

    if (formation == FORM_TOTEM) {
        return GetTotemSlot(player, index);
    }
    if (IsCross(formation)) {
        return GetCrossSlot(player, slot);
    }
    out.x += slot->aheadOfLink * spread * sin + slot->leftOfLink * spread * cos;
    out.z += slot->aheadOfLink * spread * cos - slot->leftOfLink * spread * sin;
    return out;
}

// Link is one of the four: while the cross charges or spins he rides the hub like the rest, and his blade turns
// inward with theirs. Last frame's turn is taken back first, or it would compound every frame.
static void UpdateCrossSpin(PlayState* play, Player* player) {
    bool isCharging = IsCrossCharging(play);
    bool isSpinning = IsCrossSpinning(player);
    f32 hubLeft;
    f32 hubAhead;
    f32 spunLeft;
    f32 spunAhead;
    f32 reach = isSpinning ? SPIN_RADIUS_SCALE : 1.0f;
    f32 sin;
    f32 cos;

    player->actor.shape.rot.y -= sSword.crossLinkYawOffset;
    player->actor.world.rot.y = player->actor.shape.rot.y;
    sSword.crossLinkYawOffset = 0;
    if (!isCharging && !isSpinning) {
        sSword.spinOrbit = 0;
        sSword.isCrossHubHeld = false;
        return;
    }
    if (!sSword.isCrossHubHeld) {
        GetCrossHub(player, &sSword.crossHub);
        sSword.crossLinkPlaced = player->actor.world.pos;
        sSword.isCrossHubHeld = true;
    }
    sSword.crossFormationYaw = player->actor.shape.rot.y;
    if (isSpinning) {
        sSword.spinOrbit += SPIN_STEP;
    }
    // Whatever Link walked this frame carries the anchor; only his rotation turns into an orbit.
    sSword.crossHub.x += player->actor.world.pos.x - sSword.crossLinkPlaced.x;
    sSword.crossHub.z += player->actor.world.pos.z - sSword.crossLinkPlaced.z;
    GetCrossHubOffset(&hubLeft, &hubAhead);
    RotateAroundHub(-hubLeft * reach, -hubAhead * reach, &spunLeft, &spunAhead);
    sin = Math_SinS(sSword.crossFormationYaw);
    cos = Math_CosS(sSword.crossFormationYaw);
    player->actor.world.pos.x = sSword.crossHub.x + spunAhead * sin + spunLeft * cos;
    player->actor.world.pos.z = sSword.crossHub.z + spunAhead * cos - spunLeft * sin;
    sSword.crossLinkPlaced = player->actor.world.pos;
    if (isCharging) {
        sSword.crossLinkYawOffset = GetCrossChargeYaw();
        player->actor.shape.rot.y += sSword.crossLinkYawOffset;
        player->actor.world.rot.y = player->actor.shape.rot.y;
    }
}

// Aimed from the slot, not the lagging body: backs to the hub, turned inward while the spin charges.
static s16 GetCrossFacing(PlayState* play, Player* player, Vec3f* slotPos) {
    Vec3f hub;
    s16 outward;

    GetCrossHub(player, &hub);
    outward = Math_Atan2S(slotPos->z - hub.z, slotPos->x - hub.x);
    return IsCrossCharging(play) ? outward + GetCrossChargeYaw() : outward;
}

static void Sparkle(PlayState* play, Vec3f* at, s32 count) {
    Vec3f accel = { 0.0f, 0.0f, 0.0f };
    Color_RGBA8 prim = { 150, 210, 255, 255 };
    Color_RGBA8 env = { 60, 110, 255, 0 };

    for (s32 i = 0; i < count; i++) {
        Vec3f pos = { at->x + Rand_CenteredFloat(20.0f), at->y + 30.0f + Rand_ZeroFloat(20.0f),
                      at->z + Rand_CenteredFloat(20.0f) };
        Vec3f vel = { Rand_CenteredFloat(2.0f), 1.5f, Rand_CenteredFloat(2.0f) };

        EffectSsKirakira_SpawnSmall(play, &pos, &vel, &accel, &prim, &env);
    }
}

// Link's own sword trail with the clone's livery; the alphas are the fade.
static void AllocTrail(PlayState* play, FourSwordClone* clone) {
    const Color_RGB8* tint = &sCloneTints[clone->index];
    EffectBlureInit2 init = {
        0,
        EFFECT_BLURE_ELEMENT_FLAG_8,
        0,
        { tint->r, tint->g, tint->b, 255 },
        { tint->r, tint->g, tint->b, 64 },
        { tint->r, tint->g, tint->b, 0 },
        { tint->r, tint->g, tint->b, 0 },
        4,
        0,
        EFF_BLURE_DRAW_MODE_SMOOTH,
        0,
        { 0, 0, 0, 0 },
        { 0, 0, 0, 0 },
    };

    Effect_Add(play, &clone->trailEffectIndex, EFFECT_BLURE2, 0, 0, &init);
    memset(clone->trailInfo, 0, sizeof(clone->trailInfo));
}

// The trail repaints itself from the cosmetic settings every frame; the livery goes back on after it.
static void StampTrailColor(FourSwordClone* clone) {
    EffectBlure* trail = Effect_GetByIndex(clone->trailEffectIndex);
    const Color_RGB8* tint = &sCloneTints[clone->index];

    if (trail == NULL) {
        return;
    }
    trail->p1StartColor[0] = trail->p2StartColor[0] = trail->p1EndColor[0] = trail->p2EndColor[0] = tint->r;
    trail->p1StartColor[1] = trail->p2StartColor[1] = trail->p1EndColor[1] = trail->p2EndColor[1] = tint->g;
    trail->p1StartColor[2] = trail->p2StartColor[2] = trail->p1EndColor[2] = trail->p2EndColor[2] = tint->b;
}

static void CloneInit(Actor* actor, PlayState* play) {
    FourSwordClone* clone = (FourSwordClone*)actor;

    // Before any early return: index 0 is EffectSpark 0, and a Destroy right after must not free it.
    clone->trailEffectIndex = NO_EFFECT_SLOT;
    clone->index = (u8)(actor->params & 3);
    if (clone->index >= CLONE_COUNT) {
        Actor_Kill(actor);
        return;
    }
    Actor_SetScale(actor, CLONE_SCALE);
    ActorShape_Init(&actor->shape, 0.0f, ActorShadow_DrawCircle, 30.0f);
    Collider_InitCylinder(play, &clone->hurtbox);
    Collider_SetCylinder(play, &clone->hurtbox, actor, &sHurtboxInit);
    Collider_InitCylinder(play, &clone->blade);
    Collider_SetCylinder(play, &clone->blade, actor, &sBladeInit);
    Collider_InitQuad(play, &clone->shield);
    Collider_SetQuad(play, &clone->shield, actor, &sShieldInit);
    clone->isColliderReady = true;
    actor->gravity = -2.0f;
    clone->summonTimer = SUMMON_FRAMES;
    AllocTrail(play, clone);
    sLiveClones[clone->index] = clone;
    Sparkle(play, &actor->world.pos, 6);
    Actor_PlaySfx(actor, NA_SE_SY_LOCK_ON);
}

// The trail slot must be freed: the blur effects never retire one, and re-summoning would exhaust them.
static void CloneDestroy(Actor* actor, PlayState* play) {
    FourSwordClone* clone = (FourSwordClone*)actor;

    Effect_Destroy(play, clone->trailEffectIndex);
    clone->trailEffectIndex = NO_EFFECT_SLOT;
    if (clone->isColliderReady) {
        Collider_DestroyCylinder(play, &clone->hurtbox);
        Collider_DestroyCylinder(play, &clone->blade);
        Collider_DestroyQuad(play, &clone->shield);
    }
    if (clone->index < CLONE_COUNT && sLiveClones[clone->index] == clone) {
        sLiveClones[clone->index] = NULL;
    }
}

// The four of them are one Link: a clone never dies, it passes the hit to the body that owns the hearts.
static void PassHitToLink(FourSwordClone* clone, PlayState* play) {
    ColliderElement* hitBy = clone->hurtbox.elem.acHitElem;
    s32 damage = hitBy != NULL ? hitBy->atDmgInfo.damage : 0;

    if (!(clone->hurtbox.base.acFlags & AC_HIT)) {
        return;
    }
    clone->hurtbox.base.acFlags &= ~AC_HIT;
    if (damage > 0 && play->damagePlayer != NULL) {
        play->damagePlayer(play, -damage);
    }
    Sparkle(play, &clone->actor.world.pos, 4);
}

// The offer is renewed every frame: the player clears it each update, and restarting the lift action more than
// once keeps it from ever reaching the frame that attaches the actor.
static void OfferTotemCarry(FourSwordClone* clone, PlayState* play, Player* player) {
    Actor_OfferCarry(&clone->actor, play);
    if (player->interactRangeActor == &clone->actor && player->actionFunc != Player_Action_37) {
        player->stateFlags1 |= PLAYER_STATE1_CARRYING_ACTOR;
        func_808379C0(play, player);
    }
}

static void FollowSlot(FourSwordClone* clone, PlayState* play, Player* player, bool isStacked) {
    Vec3f slot = GetSlotPosition(player, clone->index);
    // The stack is rigid: easing the two on top shows as the tower coming apart while he walks.
    f32 settled = isStacked ? 1.0f : SLOT_PULL;
    f32 pull = clone->summonTimer > 0 ? 1.0f - (f32)clone->summonTimer / SUMMON_FRAMES : settled;

    clone->actor.world.pos.x += (slot.x - clone->actor.world.pos.x) * pull;
    clone->actor.world.pos.z += (slot.z - clone->actor.world.pos.z) * pull;
    clone->actor.shape.rot.y =
        IsCross(GetFormation()) ? GetCrossFacing(play, player, &slot) : player->actor.shape.rot.y;
    clone->actor.world.rot.y = clone->actor.shape.rot.y;
    if (isStacked) {
        clone->actor.world.pos.y += (slot.y - clone->actor.world.pos.y) * pull;
        clone->actor.velocity.y = 0.0f;
    }
}

// A clone never turns to follow the group, so in the cross it walks like Link does under Z-target.
static void StrafePose(FourSwordClone* clone, Player* player, PlayState* play) {
    const char* anim = gPlayerAnim_link_normal_waitR_free;
    s16 heading;

    if (!IsCross(GetFormation())) {
        clone->ownsPose = false;
        clone->strafeFrame = 0.0f;
        return;
    }
    if (player->speedXZ > STRAFE_MOVING) {
        heading = player->actor.world.rot.y - clone->actor.shape.rot.y;
        if (heading > 0x6000 || heading < -0x6000) {
            anim = gPlayerAnim_link_normal_back_walk;
        } else if (heading > 0x2000) {
            anim = gPlayerAnim_link_normal_side_walkL_free;
        } else if (heading < -0x2000) {
            anim = gPlayerAnim_link_normal_side_walkR_free;
        } else {
            anim = gPlayerAnim_link_normal_run_free;
        }
        clone->strafeFrame += STRAFE_RATE;
    } else {
        clone->strafeFrame = 0.0f;
    }
    if (clone->strafeFrame >= Animation_GetLastFrame((void*)anim)) {
        clone->strafeFrame = 0.0f;
    }
    AnimTaskQueue_AddLoadPlayerFrame(play, (PlayerAnimationHeader*)anim, (s32)clone->strafeFrame,
                                     player->skelAnime.limbCount, clone->pose);
    clone->ownsPose = true;
}

// Four blades meeting at the hub are one strike: one clone carries a single wide cylinder there.
static void SwingBlade(FourSwordClone* clone, PlayState* play, Player* player) {
    Vec3f hub;

    if (player->meleeWeaponState > 0) {
        clone->swingTimer = SWING_FRAMES;
    } else if (clone->swingTimer > 0) {
        clone->swingTimer--;
    }
    if (IsCrossCharging(play) || IsCrossSpinning(player)) {
        if (clone->index != 0) {
            return;
        }
        GetCrossHub(player, &hub);
        clone->blade.dim.radius = (s16)(BLADE_RADIUS * SPIN_RADIUS_SCALE);
        clone->blade.dim.pos.x = (s16)hub.x;
        clone->blade.dim.pos.y = (s16)hub.y;
        clone->blade.dim.pos.z = (s16)hub.z;
        CollisionCheck_SetAT(play, &play->colChkCtx, &clone->blade.base);
    } else if (clone->swingTimer > 0) {
        clone->blade.dim.radius = BLADE_RADIUS;
        Collider_UpdateCylinder(&clone->actor, &clone->blade);
        CollisionCheck_SetAT(play, &play->colChkCtx, &clone->blade.base);
    }
}

static void CloneUpdate(Actor* actor, PlayState* play) {
    FourSwordClone* clone = (FourSwordClone*)actor;
    Player* player = GET_PLAYER(play);
    Formation formation = GetFormation();
    bool isTotemBase = formation == FORM_TOTEM && clone->index == TOTEM_BASE;
    bool isCarried = isTotemBase && actor->parent != NULL;
    bool isStacked = formation == FORM_TOTEM && !isTotemBase;

    if (!IsWorn() || player->transformation != PLAYER_FORM_HUMAN) {
        Actor_Kill(actor);
        return;
    }
    PassHitToLink(clone, play);
    if (clone->summonTimer > 0) {
        clone->summonTimer--;
    }
    if (isTotemBase && !isCarried && player->heldActor == NULL) {
        OfferTotemCarry(clone, play, player);
    }
    // Lifted, the base rides in Link's hands and he owns its position.
    if (formation != FORM_DISPERSE && !isCarried) {
        FollowSlot(clone, play, player, isStacked);
    }
    actor->speed = 0.0f;
    if (!isStacked && !isCarried) {
        Actor_MoveWithGravity(actor);
        Actor_UpdateBgCheckInfo(play, actor, WALL_HEIGHT, WALL_RADIUS, 0.0f, BGCHECK_FLAGS);
        if (actor->bgCheckFlags & BGCHECKFLAG_GROUND) {
            actor->velocity.y = 0.0f;
        }
    }
    StrafePose(clone, player, play);
    Collider_UpdateCylinder(actor, &clone->hurtbox);
    CollisionCheck_SetAC(play, &play->colChkCtx, &clone->hurtbox.base);
    // Closed up on a block they overlap by design; OC would shove them back out of formation.
    if (!IsPushing(player)) {
        CollisionCheck_SetOC(play, &play->colChkCtx, &clone->hurtbox.base);
    }
    SwingBlade(clone, play, player);
}

// In a totem the real Link is the base and wears the bottom clone's purple, while the top clone stands in green.
static bool GetCloneTint(u8 index, Color_RGBA8* out) {
    if (IsTotemCarried() && index == TOTEM_TOP) {
        return false;
    }
    *out = (Color_RGBA8){ sCloneTints[index].r, sCloneTints[index].g, sCloneTints[index].b, CLONE_TINT_LERP };
    return true;
}

// The limb override writes back into the pose (leg IK), so a clone poses from its own copy. The damage quads and
// the held actor in the shared template still name Link's actor; damage comes from the clone's own cylinder.
static void PrepareTemplate(FourSwordClone* clone, Player* player) {
    Player* template = &sCloneDrawTemplate;
    s32 limbCount = player->skelAnime.limbCount;

    if (!clone->ownsPose) {
        memcpy(clone->pose, player->skelAnime.jointTable, sizeof(Vec3s) * (limbCount + 1));
    }
    // A carried body pedalling Link's walk cycle in mid air reads wrong: the stack stands to attention.
    if (GetFormation() == FORM_TOTEM) {
        for (s32 limb = PLAYER_LIMB_RIGHT_THIGH; limb <= PLAYER_LIMB_LEFT_FOOT; limb++) {
            clone->pose[limb] = (Vec3s){ 0, 0, 0 };
        }
    }
    *template = *player;
    template->actor.world.pos = clone->actor.world.pos;
    template->actor.shape.rot = clone->actor.shape.rot;
    template->skelAnime.jointTable = clone->pose;
    template->meleeWeaponEffectIndex[0] = clone->trailEffectIndex;
    memcpy(template->meleeWeaponInfo, clone->trailInfo, sizeof(clone->trailInfo));
    template->meleeWeaponAnimation = PLAYER_MWA_SPIN_ATTACK_1H;
    template->stateFlags2 &= ~PLAYER_STATE2_20000;
    template->heldActor = NULL;
    template->shieldQuad = clone->shield;
    if (Effect_GetByIndex(clone->trailEffectIndex) == NULL) {
        template->meleeWeaponState = 0;
    }
}

// Copying the live Player and overriding only transform and pose is what makes the sword, shield, sheath and
// tunic render at all; a bare skeleton draw is a naked body.
static void CloneDraw(Actor* actor, PlayState* play) {
    FourSwordClone* clone = (FourSwordClone*)actor;
    Player* player = GET_PLAYER(play);
    Player* template = &sCloneDrawTemplate;
    Color_RGBA8 tint;
    void* playerObject;
    f32 yOffset;

    if (player->skelAnime.skeleton == NULL || player->transformation != PLAYER_FORM_HUMAN ||
        player->actor.objectSlot < 0 || !Object_IsLoaded(&play->objectCtx, player->actor.objectSlot)) {
        return;
    }
    // The player places the base in its post-limb draw, after every update; drawing after it, this is final.
    if (GetFormation() == FORM_TOTEM && clone->index != TOTEM_BASE) {
        actor->world.pos = GetSlotPosition(player, clone->index);
    }
    PrepareTemplate(clone, player);
    yOffset = template->actor.shape.yOffset * template->actor.scale.y;
    playerObject = play->objectCtx.slots[player->actor.objectSlot].segment;

    OPEN_DISPS(play->state.gfxCtx);
    // Actors drawn since Link have rebound these two segments.
    gSPSegment(POLY_OPA_DISP++, 0x06, (uintptr_t)playerObject);
    gSPSegment(POLY_XLU_DISP++, 0x06, (uintptr_t)playerObject);
    gSPSegment(POLY_OPA_DISP++, 0x0C, (uintptr_t)gCullBackDList);
    gSPSegment(POLY_XLU_DISP++, 0x0C, (uintptr_t)gCullBackDList);
    if (GetCloneTint(clone->index, &tint)) {
        gSPGrayscale(POLY_OPA_DISP++, true);
        gDPSetGrayscaleColor(POLY_OPA_DISP++, tint.r, tint.g, tint.b, tint.a);
    }
    Matrix_SetTranslateRotateYXZ(template->actor.world.pos.x, template->actor.world.pos.y + yOffset,
                                 template->actor.world.pos.z, &template->actor.shape.rot);
    Matrix_Scale(template->actor.scale.x, template->actor.scale.y, template->actor.scale.z, MTXMODE_APPLY);
    Player_DrawImpl(play, template->skelAnime.skeleton, template->skelAnime.jointTable, template->skelAnime.dListCount,
                    0, PLAYER_FORM_HUMAN, template->currentBoots, template->actor.shape.face,
                    Player_OverrideLimbDrawGameplayDefault, Player_PostLimbDrawGameplay, &template->actor);
    gSPGrayscale(POLY_OPA_DISP++, false);
    CLOSE_DISPS(play->state.gfxCtx);

    // Without the write-back the trail takes its first-frame branch forever.
    memcpy(clone->trailInfo, template->meleeWeaponInfo, sizeof(clone->trailInfo));
    clone->shield = template->shieldQuad;
    StampTrailColor(clone);
    // The post-limb draw wrote the sword hand into the Player it was handed: this clone's own hand.
    clone->swordHandMtx = template->leftHandMf;
    clone->isSwordHandMtxValid = true;
}

static void KillClones(void) {
    for (s32 i = 0; i < CLONE_COUNT; i++) {
        if (sLiveClones[i] != NULL) {
            Actor_Kill(&sLiveClones[i]->actor);
            sLiveClones[i] = NULL;
        }
    }
    sCloneMask = 0;
}

static void SpawnClone(PlayState* play, Player* player, u8 index) {
    if (sCloneActorId < 0 || sLiveClones[index] != NULL) {
        return;
    }
    sCloneMask |= 1 << index;
    Actor_Spawn(&play->actorCtx, play, sCloneActorId, player->actor.world.pos.x, player->actor.world.pos.y,
                player->actor.world.pos.z, 0, player->actor.shape.rot.y, 0, index);
}

static void RebuildClones(PlayState* play, Player* player) {
    for (u8 i = 0; i < CLONE_COUNT; i++) {
        if ((sCloneMask & (1 << i)) && sLiveClones[i] == NULL) {
            SpawnClone(play, player, i);
        }
    }
}

static void OpenFormationWheel(PlayState* play) {
    if (CountClones() == 0 || !CHECK_BTN_ALL(play->state.input[0].cur.button, BTN_L)) {
        sSword.wheelHold = 0;
        return;
    }
    if (sSword.wheelHold <= WHEEL_HOLD_FRAMES && ++sSword.wheelHold == WHEEL_HOLD_FRAMES && Z64Wheel_Open()) {
        sWheel.browsed[Z64Wheel_GetSelection() % ENTRY_MAX] = sWheel.variant;
        for (s32 entry = 0; entry < ENTRY_MAX; entry++) {
            sWheelEntries[entry].iconPath = sEntryIcons[entry][sWheel.browsed[entry]];
        }
    }
}

// The library only reads the stick sideways; one flick up or down steps the highlighted box's variant.
static void BrowseVariants(PlayState* play) {
    s8 stickY = play->state.input[0].rel.stick_y;
    u32 entry = sZ64Wheel.highlighted;
    s32 direction = stickY > VARIANT_STICK_DEADZONE ? 1 : (stickY < -VARIANT_STICK_DEADZONE ? -1 : 0);
    u8 count;

    if (direction == 0) {
        sWheel.isStickHeld = false;
        return;
    }
    if (sWheel.isStickHeld || entry >= ENTRY_MAX || sEntryVariantCount[entry] < 2) {
        return;
    }
    sWheel.isStickHeld = true;
    count = sEntryVariantCount[entry];
    sWheel.browsed[entry] = (sWheel.browsed[entry] + (direction > 0 ? 1 : count - 1)) % count;
    sWheelEntries[entry].iconPath = sEntryIcons[entry][sWheel.browsed[entry]];
    Z64Wheel_PlaySfx(NA_SE_SY_CURSOR);
}

// A closes the wheel on the picked box and keeps its variant; B closes it without touching the formation.
static void TrackWheel(PlayState* play) {
    bool isOpen = Z64Wheel_IsOpen();

    if (isOpen) {
        BrowseVariants(play);
    } else if (sWheel.wasOpen && CHECK_BTN_ALL(play->state.input[0].press.button, BTN_A)) {
        sWheel.variant = sWheel.browsed[Z64Wheel_GetSelection() % ENTRY_MAX];
    }
    sWheel.wasOpen = isOpen;
}

// The charge pose goes in behind whatever the actions loaded this frame, torso only.
static void HoldChargePose(Player* player, PlayState* play) {
    AnimTaskQueue_AddLoadPlayerFrame(play, (PlayerAnimationHeader*)gPlayerAnim_link_fighter_power_kiru_wait, 0,
                                     player->skelAnime.limbCount, sChargePose);
    AnimTaskQueue_AddCopyUsingMap(play, player->skelAnime.limbCount, player->skelAnime.jointTable, sChargePose,
                                  sUpperBodyMap);
}

static void ChargeSummon(Player* player, PlayState* play) {
    bool isShielding = (player->stateFlags1 & PLAYER_STATE1_400000) != 0;

    if (!isShielding || !CHECK_BTN_ALL(play->state.input[0].cur.button, BTN_B)) {
        sSword.chargeHold = 0;
        sSword.isCharging = false;
        return;
    }
    if (!sSword.isCharging && ++sSword.chargeHold >= CHARGE_HOLD) {
        sSword.isCharging = true;
        Audio_PlaySfx(NA_SE_SY_LOCK_ON);
        for (u8 i = 0; i < CLONE_COUNT; i++) {
            SpawnClone(play, player, i);
        }
    }
    if (sSword.isCharging) {
        HoldChargePose(player, play);
    }
}

static void ReleaseTotem(Player* player, Actor* base) {
    base->parent = NULL;
    if (player->heldActor == base) {
        player->heldActor = NULL;
        player->interactRangeActor = NULL;
        player->stateFlags1 &= ~PLAYER_STATE1_CARRYING_ACTOR;
    }
}

static void EndTotem(Player* player, Actor* base) {
    ReleaseTotem(player, base);
    sSword.wasTotemReleased = true;
    KillClones();
    Z64Wheel_SetSelection(ENTRY_TRIANGLE);
    sWheel.variant = 0;
}

// Carried, A launches Link off the top; this runs after the player's update, so the throw lands on the next move.
static void RideTotem(Player* player, PlayState* play) {
    FourSwordClone* base = GetTotemBase();
    s16 health = gSaveContext.save.saveInfo.playerData.health;
    bool wasHurt = health < sSword.previousHealth;
    Vec3f top;

    sSword.previousHealth = health;
    sSword.wasTotemReleased = false;
    if (base == NULL || base->actor.parent == NULL) {
        return;
    }
    if (wasHurt) {
        EndTotem(player, &base->actor);
        return;
    }
    if (!CHECK_BTN_ALL(play->state.input[0].press.button, BTN_A) || sLiveClones[TOTEM_TOP] == NULL) {
        return;
    }
    top = sLiveClones[TOTEM_TOP]->actor.world.pos;
    EndTotem(player, &base->actor);
    player->actor.world.pos = top;
    func_80834DB8(player, (PlayerAnimationHeader*)gPlayerAnim_link_normal_jump, TOTEM_THROW_UP, play);
    player->speedXZ = TOTEM_THROW_FORWARD;
    player->actor.speed = TOTEM_THROW_FORWARD;
    player->actor.velocity.y = TOTEM_THROW_UP;
    player->actor.world.rot.y = player->actor.shape.rot.y;
    player->actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND;
}

// The explicit table: the plain arrow type is not the first enum value.
static s16 GetShotArrowType(Player* player) {
    switch (player->heldItemAction) {
        case PLAYER_IA_BOW:
            return ARROW_TYPE_NORMAL;
        case PLAYER_IA_BOW_FIRE:
            return ARROW_TYPE_FIRE;
        case PLAYER_IA_BOW_ICE:
            return ARROW_TYPE_ICE;
        case PLAYER_IA_BOW_LIGHT:
            return ARROW_TYPE_LIGHT;
        default:
            return ARROW_TYPE_DEKU_NUT;
    }
}

static void SpawnFromClones(PlayState* play, s16 actorId, s16 rotX, s16 rotY, s16 params) {
    for (u8 i = 0; i < CLONE_COUNT; i++) {
        Vec3f* pos;

        if (sLiveClones[i] == NULL) {
            continue;
        }
        pos = &sLiveClones[i]->actor.world.pos;
        Actor_Spawn(&play->actorCtx, play, actorId, pos->x, pos->y + 7.0f, pos->z, rotX, rotY, 0, params);
    }
    sSword.itemCooldown = ITEM_COOLDOWN;
}

// The frame a shot leaves Link's hands is the frame the clones fire theirs: unk_D57 goes to 4 exactly then.
static void MirrorShots(Player* player, PlayState* play) {
    bool isCarrying = (player->stateFlags1 & PLAYER_STATE1_CARRYING_ACTOR) != 0;

    if (sSword.itemCooldown > 0) {
        sSword.itemCooldown--;
    } else if (CountClones() != 0) {
        if (player->unk_D57 == 4 && sSword.prevShotTimer != 4) {
            s16 arrowType = GetShotArrowType(player);

            SpawnFromClones(play, ACTOR_EN_ARROW, arrowType == ARROW_TYPE_DEKU_NUT ? 0x1000 : 0,
                            player->actor.shape.rot.y, arrowType);
        }
        if (sSword.wasCarrying && !isCarrying && !sSword.wasTotemReleased) {
            SpawnFromClones(play, ACTOR_EN_BOM, BOMB_EXPLOSIVE_TYPE_BOMB, 0, BOMB_TYPE_BODY);
        }
    }
    sSword.prevShotTimer = player->unk_D57;
    sSword.wasCarrying = isCarrying;
}

static void TickSword(Player* player) {
    PlayState* play = gPlayState;

    if (play == NULL || player != GET_PLAYER(play) || !IsWorn() || player->transformation != PLAYER_FORM_HUMAN) {
        return;
    }
    if ((player->stateFlags1 & PLAYER_STATE1_DEAD) || Player_InBlockingCsMode(play, player)) {
        return;
    }
    // The wheel holds the world still while it is up.
    TrackWheel(play);
    if (Z64Wheel_IsOpen()) {
        return;
    }
    OpenFormationWheel(play);
    ChargeSummon(player, play);
    RebuildClones(play, player);
    UpdateCrossSpin(play, player);
    RideTotem(player, play);
    MirrorShots(player, play);
}

// Blade, hilt, then the vanilla open hand last: its material restores the player state for the torso after it.
static void HoldFourSword(Player* player, int32_t limbIndex, Gfx** dList, Gfx* limbDList, Vec3f* pos) {
    Gfx* dl = sHeldSwordDL;

    if (!IsWorn() || limbIndex != PLAYER_LIMB_LEFT_HAND || player->transformation != PLAYER_FORM_HUMAN ||
        player->heldItemAction != PLAYER_IA_SWORD_KOKIRI ||
        Player_GetMeleeWeaponHeld(player) == PLAYER_MELEEWEAPON_NONE) {
        return;
    }
    gSPDisplayList(dl++, (Gfx*)sBladeDL);
    gSPDisplayList(dl++, (Gfx*)sHiltDL);
    gSPDisplayList(dl++, (Gfx*)gLinkHumanLeftHandOpenDL);
    gDPPipeSync(dl++);
    gSPLoadGeometryMode(dl++, G_ZBUFFER | G_SHADE | G_CULL_BACK | G_LIGHTING | G_SHADING_SMOOTH);
    gSPEndDisplayList(dl++);
    *dList = sHeldSwordDL;
}

// Four Links on one block clear its size gate; only while grabbing one, so no boulder gets lifted by it.
static void PushAnyBlock(bool* should, va_list args) {
    int32_t* strength = va_arg(args, int32_t*);
    PlayState* play = gPlayState;

    if (play == NULL || !DoesGridPushAnyBlock() || !(GET_PLAYER(play)->stateFlags2 & PLAYER_STATE2_10)) {
        return;
    }
    *strength = PLAYER_STRENGTH_ZORA;
    *should = false;
}

// A stack of four Links needs more room than one.
static void WidenTotemCamera(Camera* camera, Vec3f* eye, Vec3f* at, Vec3f* up) {
    PlayState* play = gPlayState;

    if (play == NULL || camera != Play_GetCamera(play, CAM_ID_MAIN) || !IsTotemCarried()) {
        return;
    }
    eye->x = at->x + (eye->x - at->x) * TOTEM_CAMERA_SCALE;
    eye->y = at->y + (eye->y - at->y) * TOTEM_CAMERA_SCALE;
    eye->z = at->z + (eye->z - at->z) * TOTEM_CAMERA_SCALE;
}

// Link wears the livery of the clone he stands in for at the bottom of the stack.
static void TintLinkInTotem(Actor* actor, PlayState* play, Color_RGBA8* grayscale) {
    if (IsWorn() && IsTotemCarried()) {
        *grayscale = (Color_RGBA8){ sCloneTints[TOTEM_TOP].r, sCloneTints[TOTEM_TOP].g, sCloneTints[TOTEM_TOP].b,
                                    CLONE_TINT_LERP };
    }
}

// In the cross the release bursts from where the four blades met, twice as wide.
static void CenterSpinRing(Actor* actor, bool* should) {
    PlayState* play = gPlayState;
    Player* player;
    Vec3f hub;

    if (play == NULL || !IsWorn() || !IsCross(GetFormation())) {
        return;
    }
    player = GET_PLAYER(play);
    GetCrossHub(player, &hub);
    Matrix_SetTranslateRotateYXZ(hub.x, hub.y, hub.z, &player->actor.shape.rot);
    Matrix_Scale(RING_SCALE, RING_SCALE, RING_SCALE, MTXMODE_APPLY);
}

// One En_M_Thunder serves the whole charge; its glow is drawn again off each clone's sword hand. Only while the
// charge is up, since the release draws the ring too and a second draw would repeat it.
static void GlowOnClones(Actor* actor, PlayState* play) {
    EnMThunder* thunder = (EnMThunder*)actor;
    Player* player = GET_PLAYER(play);
    MtxF linkHand;

    if (!IsWorn() || !thunder->isCharging) {
        return;
    }
    linkHand = player->leftHandMf;
    for (s32 i = 0; i < CLONE_COUNT; i++) {
        FourSwordClone* clone = sLiveClones[i];

        if (clone == NULL || !clone->isSwordHandMtxValid || clone->summonTimer != 0) {
            continue;
        }
        player->leftHandMf = clone->swordHandMtx;
        actor->draw(actor, play);
    }
    player->leftHandMf = linkHand;
}

static void UnlockAllFormations(int32_t fileNum) {
    for (u32 entry = 0; entry < ENTRY_MAX; entry++) {
        Z64Wheel_SetEntryUnlocked(entry, true);
    }
    sWheel.variant = 0;
}

static void ForgetSceneState(s16 sceneId, s8 spawnNum) {
    for (s32 i = 0; i < CLONE_COUNT; i++) {
        sLiveClones[i] = NULL;
    }
    sSword.isCrossHubHeld = false;
    sSword.crossLinkYawOffset = 0;
}

static void DismissClones(const char* key) {
    KillClones();
    memset(&sSword, 0, sizeof(sSword));
}

static bool RegisterCloneActor(void) {
    S2HActorDefinition definition = { 0 };

    definition.structSize = sizeof(definition);
    definition.key = CLONE_KEY;
    definition.description = "Four Sword clone";
    definition.category = ACTORCAT_MISC;
    definition.actorFlags = ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
    definition.objectId = GAMEPLAY_KEEP;
    definition.instanceSize = sizeof(FourSwordClone);
    definition.init = CloneInit;
    definition.destroy = CloneDestroy;
    definition.update = CloneUpdate;
    definition.draw = CloneDraw;
    sCloneActorId = sApi->RegisterActor(&definition);
    return sCloneActorId >= 0;
}

static const char* const sRequiredHooks[] = {
    "OnPlayerUpdate", "OnPlayerResolveLimbDraw", "OnCameraResolveView", "ShouldActorDraw",         "OnActorDrawEnd",
    "OnSceneInit",    "OnLoadFile",              "OnInterfaceDrawEnd",  "OnActorResolveGrayscale",
};

static const S2HModRequirements sRequirements = { sizeof(S2HModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

S2H_MOD_EXPORT void ModSetApi(const S2HModApi* api) {
    sApi = api;
}

S2H_MOD_EXPORT const S2HModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

S2H_MOD_EXPORT void ModInit(void) {
    S2HCustomEquipDefinition definition = { 0 };

    if (!S2H_MOD_API_HAS(sApi, RegisterActor) || !RegisterCloneActor()) {
        return;
    }
    for (s32 limb = PLAYER_LIMB_UPPER_ROOT; limb < PLAYER_LIMB_MAX; limb++) {
        sUpperBodyMap[limb] = true;
    }

    definition.structSize = sizeof(definition);
    definition.key = FOUR_SWORD_KEY;
    definition.iconPath = sIcon;
    definition.namePath = sName;
    definition.pauseText = "%rFour Sword&%wHold %y\xA3%w and %y\xA0%w to split in four. Hold %y\xA2%w to choose "
                           "the formation.";
    definition.getItemText = "You got the %rFour Sword%w!&The blade that splits its bearer in four. Raise your shield "
                             "and hold %y\xA0%w: three more of you fight at your side, and each formation has its "
                             "own trick.";
    definition.slot = S2H_EQUIP_SLOT_SWORD;
    definition.page = 0;
    definition.row = S2H_EQUIP_SLOT_SWORD;
    definition.column = 2;
    definition.formMask = S2H_CUSTOM_ITEM_FORM_BIT(PLAYER_FORM_HUMAN);
    definition.vanillaBase = EQUIP_VALUE_SWORD_KOKIRI;
    definition.toggles = 1;
    definition.onUnequip = DismissClones;
    definition.getItemModelPath = sBladeDL;
    if (!CustomEquipRegistry_Register(&definition) ||
        !Z64Wheel_Register(sApi, FOUR_SWORD_KEY, MOD_NAME, sWheelEntries, ENTRY_MAX, true)) {
        return;
    }

    S2H_REGISTER_HOOK(sApi, OnPlayerUpdate, TickSword);
    S2H_REGISTER_HOOK(sApi, OnPlayerResolveLimbDraw, HoldFourSword);
    S2H_REGISTER_HOOK(sApi, OnCameraResolveView, WidenTotemCamera);
    S2H_REGISTER_HOOK(sApi, OnSceneInit, ForgetSceneState);
    S2H_REGISTER_HOOK(sApi, OnLoadFile, UnlockAllFormations);
    S2H_REGISTER_HOOK_FOR_ID(sApi, ShouldActorDraw, ACTOR_EN_M_THUNDER, CenterSpinRing);
    S2H_REGISTER_HOOK_FOR_ID(sApi, OnActorDrawEnd, ACTOR_EN_M_THUNDER, GlowOnClones);
    S2H_REGISTER_HOOK_FOR_ID(sApi, OnActorResolveGrayscale, ACTOR_PLAYER, TintLinkInTotem);
    sApi->RegisterVB(VB_USE_STRENGTH_UPGRADE, PushAnyBlock);
}
