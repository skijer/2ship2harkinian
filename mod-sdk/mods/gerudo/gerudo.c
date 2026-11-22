#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "z64items.h"
#include "z64logic.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "interface/parameter_static/parameter_static.h"
#include "overlays/actors/ovl_En_M_Thunder/z_en_m_thunder.h"

#define GERUDO_MASK_KEY "nei.gerudo_form_mask"
#define GERUDO_FORM_KEY "nei.gerudo"
#define GERUDO_MODEL_PATH "objects/forms/gerudo"
#define GERUDO_MASK_PAGE S2H_ITEM_PAGE_MASK_VANILLA
#define GERUDO_MASK_SLOT 21
#define GERUDO_MASK_WHEEL_PRIORITY (-1)

#define STICK_MOVING 10.0f
#define STICK_FULL 60.0f
#define OCARINA_NOTE_NONE OCARINA_PITCH_NONE
// PlayerAnimation advances R_UPDATE_RATE * 0.5 clip frames per update, and the rate is always 3.
#define CLIP_FRAMES_PER_UPDATE 1.5f

#define A_TAP_FRAMES 8
#define SPRINT_SCALE 1.5f
#define ROLL_SCALE 2.0f
#define COMBO_STEPS 4
#define COMBO_RESET_FRAMES 40
#define LUNGE_SPEED 15.0f
#define CHARGE_RATE 0.06f
#define CHARGE_SPARK_COST (2 << 8)
#define CHARGE_FAST_BEGIN 33.0f
#define CHARGE_FAST_END 53.0f
#define CHARGE_FAST_SCALE 3.0f
#define SPIN_INVULNERABILITY (-8)
#define SPIN_RADIUS 45
#define SPIN_HEIGHT 60
#define SPIN_DAMAGE 2
#define FRONT_SLASH_DISTANCE 100.0f
#define FRONT_SLASH_TRAVEL_FRAMES 30.0f
#define WALL_MARGIN 14.0f
#define JUMP_SLASH_LIFT 2.1f
#define JUMP_SLASH_SPEED_MIN 6.0f
#define JUMP_SLASH_SPEED_MAX 20.0f
#define JUMP_SLASH_LEAD 6.0f
#define JUMP_GRAVITY (-1.2f)
#define AERIAL_MIN_AIR_FRAMES 2
#define CREEP_SPEED 18.0f
#define SHEATHE_HIDE_FRAME 17.0f
#define DRAW_SPEED 1.5f
#define DRAW_MAX_SPEED 4.0f
#define GUARD_AIM_PITCH_MAX 3500
#define GUARD_TWIST_X 6954
#define GUARD_TWIST_Y 3969
#define GUARD_TWIST_Z 9412
#define QUAD_TIP_REACH 1200.0f

#define RAGE_BASE 400
#define RAGE_HITS_TO_FILL 10
#define FURY_DAMAGE_MIN 10
#define FURY_DAMAGE_MAX 255
#define FURY_INVULNERABILITY 60
#define FURY_BLAST_RADIUS 320
#define FURY_BLAST_HEIGHT 400
#define FURY_BLAST_FRAMES 90.0f
#define FURY_STRIKE_FRAME 80.0f
#define FURY_DIM 140
#define FURY_DIM_FADE 10
#define FURY_QUAKE_STRENGTH 7
#define FURY_QUAKE_FRAMES 22
#define FURY_QUAKE_SPEED 27767
#define FURY_DMG_FLAGS (DMG_SWORD | DMG_GORON_POUND | DMG_FIRE_ARROW | DMG_LIGHT_ARROW)

#define ANIM_VALUES_PER_FRAME 67
#define CLIP_CACHE_MAX 40
#define VOICE_SLOTS 4
#define VOICE_ADVANCE 0.5f

// Magic_DrawMeter's layout: the rage meter takes the magic meter's place, or the row below it.
#define RAGE_BAR_LEFT 18
#define RAGE_BAR_MID_LEFT 26
#define RAGE_BAR_Y_ONE_HEART_ROW 34
#define RAGE_BAR_Y_TWO_HEART_ROWS 42
#define RAGE_BAR_BELOW_MAGIC 12
#define RAGE_BAR_FILL_TOP 3
#define RAGE_BAR_FILL_BOTTOM 10
#define TWO_HEART_ROWS_HEALTH 0xA0

#define BUSY_STATE1                                                                                  \
    (PLAYER_STATE1_DEAD | PLAYER_STATE1_TALKING | PLAYER_STATE1_200 | PLAYER_STATE1_400 |            \
     PLAYER_STATE1_CARRYING_ACTOR | PLAYER_STATE1_8000000 | PLAYER_STATE1_100000 | PLAYER_STATE1_4 | \
     PLAYER_STATE1_200000 | PLAYER_STATE1_800000 | PLAYER_STATE1_4000000 | PLAYER_STATE1_20000000)
#define STATE1_SHIELDING PLAYER_STATE1_400000
#define STATE1_SWIMMING PLAYER_STATE1_8000000
#define STATE1_FIRST_PERSON PLAYER_STATE1_100000
#define STATE1_DAMAGED PLAYER_STATE1_4000000
#define STATE2_SPIN_ATTACKING PLAYER_STATE2_20000
#define STATE2_NO_Z_TARGET_ROTATION PLAYER_STATE2_20
#define STATE2_NO_ROTATION PLAYER_STATE2_40
#define STATE2_HOPPING PLAYER_STATE2_80000
#define STATE3_MIDAIR PLAYER_STATE3_2
#define WALK_FRAMES 29
#define RUN_FRAMES 20
#define FIGHTER_COLUMNS ((1 << PLAYER_ANIMTYPE_1) | (1 << PLAYER_ANIMTYPE_3))

#define MHR(name) "__OTR__misc/link_animetion/gMonsterHunterRise_DualBlade_" name
#define CLIP_IDLE MHR("StationaryReadyIdle_Variant03")
#define CLIP_RUN MHR("ForwardCombatRun")
#define CLIP_SPRINT MHR("ForwardCombatRun_Variant02")
#define CLIP_DAMAGE_RUN MHR("ForwardDoubleRushSlash_Variant15")
#define CLIP_GUARD MHR("DemonModeActivationFlourish")
#define CLIP_LANDING MHR("ForwardSingleTwinSlash")
#define CLIP_ROLL MHR("ForwardAcrobaticEvasion")
#define CLIP_CHARGE MHR("LowExtendedChargeStance")
#define CLIP_JUMP_SLASH MHR("ForwardRisingDoubleAerialSlash_Variant18")
#define CLIP_HOP_LEFT MHR("RightRisingTripleLateralSlash")
#define CLIP_HOP_RIGHT MHR("LeftDoubleLateralSlash")
#define CLIP_BACKFLIP MHR("BackwardHighAerialLeftLeadDoubleSilkbindSlash")
#define CLIP_FRONT_SLASH MHR("BackwardRisingDoubleAerialSlash")
#define CLIP_AERIAL MHR("StationaryRisingTripleAerialSlash")
#define CLIP_SHEATHE MHR("ForwardDoubleTwinSlash")
#define CLIP_SOFT_HIT MHR("ForwardEvasiveStep")
#define CLIP_FALL MHR("StationaryReadyIdle_Variant05")
#define CLIP_CRIT_IDLE MHR("StationaryReadyIdle_Variant08")
#define CLIP_SING "__OTR__misc/link_animetion/gPlayerAnim_mhr_npc_malon_sing"
#define CLIP_FURY "__OTR__misc/link_animetion/gPlayerAnim_mhr_gs_wirebug_attack04"
static const ALIGN_ASSET(2) char sMaskDL[] = "__OTR__objects/oot_object_link_child/gLinkChildGerudoMaskDL";
static const ALIGN_ASSET(2) char sEmptyHandDL[] = "__OTR__" GERUDO_MODEL_PATH
                                                  "/object_link_child/gLinkChildRightHandNearDL";
// The adult rig's sword hand is the scimitar mesh; the child rig only carries a Master Sword.
static const ALIGN_ASSET(2) char sScimitarDL[] = "__OTR__" GERUDO_MODEL_PATH
                                                 "/object_link_boy/gLinkAdultLeftHandHoldingMasterSwordNearDL";
// The mask keeps the Captain's Hat slot, but it looks and reads as OoT's Gerudo Mask.
static const ALIGN_ASSET(2) char sIconTex[] = "__OTR__textures/oot_icon_item_static/gItemIconMaskGerudoTex";
static const ALIGN_ASSET(2) char sNameTex[] = "__OTR__textures/oot_item_name_static/gGerudoMaskItemNameENGTex";
static const ALIGN_ASSET(2) char sGetItemDL[] = "__OTR__objects/oot_object_gi_gerudomask/gGiGerudoMaskDL";

typedef struct {
    s16 begin;
    s16 end;
} Window;

typedef struct {
    const char* clip;
    f32 speed;
    s16 swingEnd;
    Window left;
    Window right;
    bool isSpin;
    u8 mwa;
} SwingRow;

typedef enum {
    ROW_COMBO_1,
    ROW_COMBO_2,
    ROW_COMBO_3,
    ROW_COMBO_4,
    ROW_STAB,
    ROW_JUMP_LAND,
    ROW_CHARGE_RELEASE,
    ROW_QUICK_SPIN,
    ROW_COUNT,
} SwingRowId;

static const SwingRow sRows[ROW_COUNT] = {
    { MHR("AlternatingCrossSlashLeftRight"), 1.7f, 25, { 13, 25 }, { 13, 25 }, false, PLAYER_MWA_FORWARD_SLASH_1H },
    { MHR("StationaryRisingSingleAerialSlash"), 1.7f, 28, { 1, 28 }, { 1, 28 }, false, PLAYER_MWA_FORWARD_COMBO_1H },
    { MHR("StationaryRightLeadDoubleTwinSlash"), 1.7f, 28, { 1, 28 }, { 1, 28 }, false, PLAYER_MWA_RIGHT_SLASH_1H },
    { MHR("RightRisingTripleAerialSlash"), 1.7f, 41, { 12, 41 }, { 12, 41 }, true, PLAYER_MWA_RIGHT_COMBO_1H },
    { MHR("ForwardRisingLeftLeadDoubleAerialSlash"), 1.5f, -1, { 4, 30 }, { 4, 30 }, false, PLAYER_MWA_STAB_1H },
    { CLIP_LANDING, 1.0f, -1, { 2, 40 }, { 2, 40 }, true, PLAYER_MWA_JUMPSLASH_FINISH },
    { MHR("ForwardTumbleDelayedCrossFinish"), 2.1f, -1, { 28, 40 }, { 28, 40 }, false, PLAYER_MWA_SPIN_ATTACK_1H },
    { MHR("ForwardRisingDoubleRushSlash_Variant21"), 2.1f, -1, { 6, 60 }, { 6, 60 }, true, PLAYER_MWA_BIG_SPIN_1H },
};

typedef struct {
    u16 sfxId;
    u8 variants;
} GerudoVoice;

static const GerudoVoice sVoices[] = {
    { 0x6800, 4 }, { 0x6801, 2 }, { 0x6802, 2 }, { 0x6803, 2 }, { 0x6804, 1 }, { 0x6805, 6 }, { 0x6806, 1 },
    { 0x6808, 2 }, { 0x6809, 6 }, { 0x680A, 1 }, { 0x680B, 1 }, { 0x680E, 1 }, { 0x680F, 1 }, { 0x6810, 1 },
    { 0x6811, 2 }, { 0x6813, 1 }, { 0x6814, 1 }, { 0x6816, 5 }, { 0x6818, 1 }, { 0x6819, 1 }, { 0x681A, 1 },
    { 0x681C, 1 }, { 0x6820, 4 }, { 0x6822, 2 }, { 0x6823, 2 }, { 0x6824, 1 }, { 0x6825, 6 }, { 0x6826, 1 },
    { 0x6828, 2 }, { 0x6829, 6 }, { 0x682A, 1 }, { 0x682B, 1 }, { 0x682E, 1 }, { 0x682F, 1 }, { 0x6830, 1 },
    { 0x6831, 2 }, { 0x6833, 1 }, { 0x6834, 1 }, { 0x6836, 5 }, { 0x6838, 1 }, { 0x6839, 1 }, { 0x683A, 1 },
    { 0x683C, 1 },
};
#define VOICE_SAMPLES_MAX 96

static const char* const sRequiredHooks[] = {
    "OnPlayerActionHandler",    "OnPlayerResolveAnim", "OnPlayerResolveMotionScale", "OnPlayerResolveLimbDraw",
    "OnPlayerPostLimbDraw",     "OnActorPlaySfx",      "OnInterfaceDrawEnd",         "OnSceneInit",
    "OnCollisionResolveDamage", "OnOcarinaInput",      "OnPlayerResolveAnimSite",    "OnActorUpdate",
    "ShouldActorDraw",          "OnActorDestroy",
};
static const S2HModRequirements sRequirements = { sizeof(S2HModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static ColliderCylinderInit sSpinCylinderInit = {
    { COL_MATERIAL_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_TYPE_PLAYER, COLSHAPE_CYLINDER },
    { ELEM_MATERIAL_UNK2,
      { DMG_SWORD, 0x00, SPIN_DAMAGE },
      { 0x00000000, 0x00, 0x00 },
      ATELEM_ON | ATELEM_NEAREST | ATELEM_SFX_NORMAL,
      ACELEM_NONE,
      OCELEM_NONE },
    { SPIN_RADIUS, SPIN_HEIGHT, 0, { 0, 0, 0 } },
};

static EffectBlureInit2 sTrailInit = {
    0,
    EFFECT_BLURE_ELEMENT_FLAG_8,
    0,
    { 255, 255, 255, 255 },
    { 255, 255, 255, 64 },
    { 255, 255, 255, 0 },
    { 255, 255, 255, 0 },
    4,
    0,
    EFF_BLURE_DRAW_MODE_SMOOTH,
    0,
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
};

typedef enum {
    GERUDO_MOVE_NONE,
    GERUDO_MOVE_SWING,
    GERUDO_MOVE_CHARGE,
    GERUDO_MOVE_FRONT_SLASH,
    GERUDO_MOVE_JUMP_SLASH,
    GERUDO_MOVE_AERIAL,
    GERUDO_MOVE_FURY,
    GERUDO_MOVE_SHEATHE,
    GERUDO_MOVE_DRAW,
    GERUDO_MOVE_GUARD,
} GerudoMove;

typedef struct {
    s32 dmgFlags;
    u8 dmgTransformedNormal;
    u8 dmgTransformedStrong;
    u8 dmgHumanNormal;
    u8 dmgHumanStrong;
} MeleeDamage;

typedef struct {
    const char* path;
    s16 first;
    s16 last;
    s16 frames;
    PlayerAnimationHeader header;
    s16* data;
} CachedClip;

typedef struct {
    const s16* samples;
    s32 count;
} VoiceSample;

typedef struct {
    s32 sample;
    f32 position;
} VoiceSlot;

s32 Player_SetAction(PlayState* play, Player* this, PlayerActionFunc actionFunc, s32 arg3);
void func_8083B1A0(Player* player, PlayState* play);
void func_80836B3C(PlayState* play, Player* player, f32 arg2);
void func_80833728(Player* player, s32 index, u32 dmgFlags, s32 damage);
void func_8082FA5C(PlayState* play, Player* player, s32 meleeWeaponState);
s32 func_80126440(PlayState* play, ColliderQuad* collider, WeaponInfo* weaponInfo, Vec3f* newTip, Vec3f* newBase);
s32 Player_CanSpinAttack(Player* player);
PlayerItemAction Player_ItemToItemAction(Player* player, ItemId item);
void func_80123C58(Player* player);
bool Player_IsZTargeting(Player* player);
void Player_StopHorizontalMovement(Player* player);
void Player_RequestRumble(PlayState* play, Player* player, s32 sourceIntensity, s32 decayTimer, s32 decayStep,
                          s32 distSq);
extern PlayerAnimationHeader* ResourceMgr_LoadPlayerAnimAsHeader(const char* animPath);
extern void* ResourceGetDataByName(const char* name);
extern HOST_DATA u8 sCurOcarinaPitch;
extern HOST_DATA MeleeDamage D_8085D09C[];

static const S2HModApi* sApi;
static CachedClip sClipCache[CLIP_CACHE_MAX];
static s32 sClipCacheCount;
static PlayerAnimationHeader sBackflipClip;
static s16* sBackflipData;

static u8 sMove;
static u8 sRow;
static f32 sPrevFrame;
static bool sIsNextHitQueued;
static bool sIsLungePending;
static s32 sComboStep;
static s32 sComboIdleFrames;
static u8 sBladeMask;
static u32 sBladeDmgFlags;
static u8 sBladeDamage;
static bool sOwnsBladeFlags;
static bool sIsCylinderOn;
static u32 sCylinderDmgFlags;
static u8 sCylinderDamage;
static s16 sCylinderRadius;
static s16 sCylinderHeight;
static ColliderCylinder sSpinCylinder;
static bool sIsCylinderReady;
static s32 sTrailRight = -1;
static s16 sLockedYaw;
static Vec3s sGuardAim;
static bool sIsGuardLowering;
static s8 sBladeVisibility = -1;
static ItemId sPendingItem;
static s16 sAirFrames;
static s16 sJumpSlashFrames;
static bool sIsJumpLooping;
static bool sIsStanceLooping;

static bool sIsAPending;
static s16 sAFrames;
static bool sIsSprinting;

static s16 sRageMeter;
static u8 sFuryDamage;
static bool sHasFuryStruck;
static f32 sFuryPrevFrame = -1.0f;

static bool sIsOcarinaOut;
static bool sIsInstrumentLent;
static volatile bool sWasNoteStruck;
static u8 sLastNote = OCARINA_NOTE_NONE;

static VoiceSample sVoiceSamples[VOICE_SAMPLES_MAX];
static s32 sVoiceSampleCount;
static s32 sVoiceFirstSample[ARRAY_COUNT(sVoices)];
static VoiceSlot sVoiceSlots[VOICE_SLOTS];
static volatile s32 sRequestedVoice = -1;

static void GerudoAction(Player* player, PlayState* play);

static bool IsGerudo(void) {
    return sApi != NULL && sApi->IsFormActive(GERUDO_FORM_KEY);
}

static bool IsGrounded(Player* player) {
    return (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) != 0;
}

static bool IsGerudoBusy(Player* player) {
    return player->actionFunc == GerudoAction;
}

static bool IsPressed(PlayState* play, u16 button) {
    return CHECK_BTN_ALL(play->state.input[0].press.button, button);
}

static bool IsHeld(PlayState* play, u16 button) {
    return CHECK_BTN_ALL(play->state.input[0].cur.button, button);
}

static f32 GetStickMagnitude(PlayState* play) {
    f32 x = play->state.input[0].rel.stick_x;
    f32 y = play->state.input[0].rel.stick_y;

    return MIN(sqrtf(SQ(x) + SQ(y)), STICK_FULL);
}

static s8 GetStickDirection(Player* player) {
    return player->controlStickDirections[player->controlStickDataIndex];
}

static bool CanAct(Player* player) {
    return !(player->stateFlags1 & BUSY_STATE1) && player->csAction == PLAYER_CSACTION_NONE;
}

static bool HoldsSword(Player* player) {
    s32 weapon = Player_GetMeleeWeaponHeld(player);

    return weapon >= PLAYER_MELEEWEAPON_SWORD_KOKIRI && weapon <= PLAYER_MELEEWEAPON_SWORD_GILDED;
}

// Sheathing and drawing override what the held item says, so the blades leave the hands mid-clip.
static bool AreBladesOut(Player* player) {
    if (sBladeVisibility >= 0) {
        return sBladeVisibility != 0;
    }
    return HoldsSword(player);
}

static bool IsFighter(Player* player) {
    return HoldsSword(player) || (player->stateFlags1 & STATE1_SHIELDING);
}

static s32 GetSwordTier(Player* player) {
    return CLAMP(Player_GetMeleeWeaponHeld(player) - PLAYER_MELEEWEAPON_SWORD_KOKIRI, 0, 2);
}

static u8 GetTierDamage(Player* player, bool isStrong) {
    MeleeDamage* info = &D_8085D09C[PLAYER_MELEEWEAPON_SWORD_KOKIRI + GetSwordTier(player)];

    return isStrong ? info->dmgHumanStrong : info->dmgHumanNormal;
}

// z_player_lib's sMeleeWeaponLengths for the three swords, so the right blade reaches as far as the left one.
static f32 GetBladeLength(Player* player) {
    static const f32 sBladeLengths[] = { 3000.0f, 3000.0f, 4000.0f };

    return sBladeLengths[GetSwordTier(player)];
}

// In-place range loader: the MHR clips carry their root travel, so the root is pinned to the first
// frame's; nearest-frame resampling on purpose, since interpolating packed angles smears any that wraps.
static PlayerAnimationHeader* LoadClip(const char* path, s16 first, s16 last, s16 frames) {
    for (s32 i = 0; i < sClipCacheCount; i++) {
        CachedClip* clip = &sClipCache[i];
        if (strcmp(clip->path, path) == 0 && clip->first == first && clip->last == last && clip->frames == frames) {
            return &clip->header;
        }
    }
    PlayerAnimationHeader* source = ResourceMgr_LoadPlayerAnimAsHeader(path);
    if (source == NULL || source->common.frameCount <= 0 || sClipCacheCount >= CLIP_CACHE_MAX) {
        return NULL;
    }
    s32 count = source->common.frameCount;
    s32 begin = CLAMP(first < 0 ? 0 : first, 0, count - 1);
    s32 end = (last < 0 || last >= count) ? count - 1 : MAX(last, begin);
    s32 span = end - begin + 1;
    s32 outFrames = frames > 0 ? frames : span;
    s16* data = (s16*)malloc((size_t)outFrames * ANIM_VALUES_PER_FRAME * sizeof(s16));
    if (data == NULL) {
        return NULL;
    }
    const s16* frameData = (const s16*)source->segmentVoid + (size_t)begin * ANIM_VALUES_PER_FRAME;
    for (s32 f = 0; f < outFrames; f++) {
        s32 sourceFrame = MIN((f * span) / outFrames, span - 1);
        memcpy(&data[f * ANIM_VALUES_PER_FRAME], &frameData[sourceFrame * ANIM_VALUES_PER_FRAME],
               ANIM_VALUES_PER_FRAME * sizeof(s16));
    }
    for (s32 f = 1; f < outFrames; f++) {
        data[f * ANIM_VALUES_PER_FRAME + 0] = data[0];
        data[f * ANIM_VALUES_PER_FRAME + 2] = data[2];
    }
    CachedClip* clip = &sClipCache[sClipCacheCount++];
    clip->path = path;
    clip->first = first;
    clip->last = last;
    clip->frames = frames;
    clip->data = data;
    clip->header.common.frameCount = (s16)outFrames;
    clip->header.segmentVoid = data;
    return &clip->header;
}

static PlayerAnimationHeader* LoadWholeClip(const char* path) {
    return LoadClip(path, -1, -1, 0);
}

// The flip's two halves are resampled at their own pace: a quick takeoff, then a slower fall.
static PlayerAnimationHeader* LoadBackflipClip(void) {
    const f32 compensation = 1.5f;
    const s16 split = 31;

    if (sBackflipClip.segmentVoid != NULL) {
        return &sBackflipClip;
    }
    PlayerAnimationHeader* whole = LoadWholeClip(CLIP_BACKFLIP);
    if (whole == NULL) {
        return NULL;
    }
    s16 last = whole->common.frameCount - 1;
    s16 framesA = (s16)MAX((f32)split / (1.5f * compensation) + 0.5f, 1.0f);
    s16 framesB = (s16)MAX((f32)(last - split + 1) / (2.0f * compensation) + 0.5f, 1.0f);
    PlayerAnimationHeader* partA = LoadClip(CLIP_BACKFLIP, 0, split - 1, framesA);
    PlayerAnimationHeader* partB = LoadClip(CLIP_BACKFLIP, split, last, framesB);
    if (partA == NULL || partB == NULL) {
        return NULL;
    }
    s32 total = partA->common.frameCount + partB->common.frameCount;
    sBackflipData = (s16*)malloc((size_t)total * ANIM_VALUES_PER_FRAME * sizeof(s16));
    if (sBackflipData == NULL) {
        return NULL;
    }
    size_t sizeA = (size_t)partA->common.frameCount * ANIM_VALUES_PER_FRAME * sizeof(s16);
    memcpy(sBackflipData, partA->segmentVoid, sizeA);
    memcpy((u8*)sBackflipData + sizeA, partB->segmentVoid,
           (size_t)partB->common.frameCount * ANIM_VALUES_PER_FRAME * sizeof(s16));
    sBackflipClip.common.frameCount = (s16)total;
    sBackflipClip.segmentVoid = sBackflipData;
    return &sBackflipClip;
}

static void PlayClip(PlayState* play, Player* player, PlayerAnimationHeader* clip, f32 speed, u8 mode, f32 morph) {
    if (clip == NULL) {
        return;
    }
    f32 last = Animation_GetLastFrame(clip);
    f32 start = speed >= 0.0f ? 0.0f : last;
    f32 end = speed >= 0.0f ? last : 0.0f;
    PlayerAnimation_Change(play, &player->skelAnime, clip, speed, start, end, mode, morph);
    sPrevFrame = start;
}

static PlayerAnimationHeader* GetFighterGroupClip(s32 group) {
    switch (group) {
        case PLAYER_ANIMGROUP_wait:
            return LoadWholeClip(CLIP_IDLE);
        case PLAYER_ANIMGROUP_walk:
            return LoadClip(CLIP_RUN, -1, -1, WALK_FRAMES);
        case PLAYER_ANIMGROUP_run:
            return LoadClip(sIsSprinting ? CLIP_SPRINT : CLIP_RUN, -1, -1, RUN_FRAMES);
        case PLAYER_ANIMGROUP_damage_run:
            return LoadClip(CLIP_DAMAGE_RUN, -1, -1, RUN_FRAMES);
        case PLAYER_ANIMGROUP_landing:
        case PLAYER_ANIMGROUP_short_landing:
            return LoadWholeClip(CLIP_LANDING);
        case PLAYER_ANIMGROUP_landing_roll:
            return LoadClip(CLIP_ROLL, 0, 30, 0);
        default:
            return NULL;
    }
}

static void ResolveFighterAnim(s32 group, s32 animType, PlayerAnimationHeader** anim) {
    if (!IsGerudo() || gPlayState == NULL || !(FIGHTER_COLUMNS & (1 << animType)) ||
        !IsFighter(GET_PLAYER(gPlayState))) {
        return;
    }
    PlayerAnimationHeader* clip = GetFighterGroupClip(group);
    if (clip != NULL) {
        *anim = clip;
    }
}

static bool IsRolling(Player* player) {
    return player->skelAnime.animation == LoadClip(CLIP_ROLL, 0, 30, 0);
}

static void ScaleSprintAndRoll(Player* player, s32 kind, f32* scale) {
    if (!IsGerudo()) {
        return;
    }
    if (IsRolling(player)) {
        *scale *= ROLL_SCALE;
    } else if (sIsSprinting) {
        *scale *= SPRINT_SCALE;
    }
}

// Site indices are z_player.c's: a hop is direction * 3 + phase (start, end, end while locked on).
static PlayerAnimationHeader* GetHopClip(s32 index) {
    s32 direction = index / 3;
    s32 phase = index % 3;

    if (phase != 0) {
        return LoadClip(CLIP_LANDING, -1, -1, 8);
    }
    if (direction == PLAYER_STICK_DIR_BACKWARD) {
        return LoadBackflipClip();
    }
    return LoadClip(direction == PLAYER_STICK_DIR_LEFT ? CLIP_HOP_LEFT : CLIP_HOP_RIGHT, -1, -1, 12);
}

// sFidgetAnimations flattened: rows 7 and 8 are the critical-health fidgets, in their second column.
#define FIDGET_CRIT_START_ARMED (7 * 2 + 1)
#define FIDGET_CRIT_LOOP_ARMED (8 * 2 + 1)
// D_8085D0D4 lists the soft hits first.
#define SOFT_HIT_COUNT 4

static PlayerAnimationHeader* GetSiteClip(s32 site, s32 index) {
    switch (site) {
        case SOH_PLAYER_ANIM_SITE_HOP:
            return GetHopClip(index);
        case SOH_PLAYER_ANIM_SITE_DAMAGE:
            return index < SOFT_HIT_COUNT ? LoadWholeClip(CLIP_SOFT_HIT) : NULL;
        case SOH_PLAYER_ANIM_SITE_FIDGET:
            if (index == FIDGET_CRIT_START_ARMED || index == FIDGET_CRIT_LOOP_ARMED) {
                return LoadWholeClip(CLIP_CRIT_IDLE);
            }
            return NULL;
        case SOH_PLAYER_ANIM_SITE_FALL:
            return IsFighter(GET_PLAYER(gPlayState)) ? LoadWholeClip(CLIP_FALL) : NULL;
        default:
            return NULL;
    }
}

static void ResolveSiteAnim(s32 site, s32 index, PlayerAnimationHeader** anim) {
    if (!IsGerudo() || gPlayState == NULL) {
        return;
    }
    PlayerAnimationHeader* clip = GetSiteClip(site, index);
    if (clip != NULL) {
        *anim = clip;
    }
}

static s16 GetRageCapacity(void) {
    return (s16)(RAGE_BASE << CLAMP(gSaveContext.save.saveInfo.playerData.magicLevel, 0, 2));
}

static bool IsRageWorthy(Actor* victim) {
    return victim != NULL && (victim->category == ACTORCAT_ENEMY || victim->category == ACTORCAT_BOSS);
}

static void GainRage(s32 hits) {
    s16 capacity = GetRageCapacity();
    sRageMeter = MIN(sRageMeter + MAX(capacity / RAGE_HITS_TO_FILL, 1) * hits, capacity);
}

static void BankBladeHits(Player* player) {
    bool isHit = false;

    if (sMove == GERUDO_MOVE_FURY) {
        return;
    }
    for (s32 quad = 0; quad < 2; quad++) {
        ColliderQuad* collider = &player->meleeWeaponQuads[quad];
        isHit = isHit || ((collider->base.atFlags & AT_HIT) && IsRageWorthy(collider->base.at));
    }
    if (sIsCylinderReady && (sSpinCylinder.base.atFlags & AT_HIT)) {
        isHit = isHit || IsRageWorthy(sSpinCylinder.base.at);
        sSpinCylinder.base.atFlags &= ~AT_HIT;
    }
    if (isHit) {
        GainRage(1);
    }
}

static void DealFuryDamage(Actor* victim, ColliderElement* attack, f32* damage) {
    if (!IsGerudo() || sMove != GERUDO_MOVE_FURY || victim == NULL || gPlayState == NULL) {
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    bool isFuryCollider = attack == &sSpinCylinder.elem || attack == &player->meleeWeaponQuads[0].elem ||
                          attack == &player->meleeWeaponQuads[1].elem;
    if (isFuryCollider) {
        *damage = (f32)MIN(sFuryDamage, 255 - victim->colChkInfo.damage);
    }
}

static bool IsInWindow(const Window* window, f32 prev, f32 cur) {
    return window->end >= window->begin && cur >= (f32)window->begin && prev <= (f32)window->end;
}

static void SetBladeFlags(Player* player, u32 dmgFlags, u8 damage, bool ownsFlags) {
    sBladeDmgFlags = dmgFlags;
    sBladeDamage = damage;
    sOwnsBladeFlags = ownsFlags;
    func_80833728(player, 0, dmgFlags, damage);
    func_80833728(player, 1, dmgFlags, damage);
}

static void GateBlades(PlayState* play, Player* player, u8 mask) {
    sBladeMask = mask;
    if (mask != 0 && player->meleeWeaponState == 0) {
        func_8082FA5C(play, player, PLAYER_MELEE_WEAPON_STATE_1);
    } else if (mask == 0) {
        player->meleeWeaponState = PLAYER_MELEE_WEAPON_STATE_0;
    }
}

static void TurnCylinderOn(u32 dmgFlags, u8 damage) {
    sIsCylinderOn = true;
    sCylinderDmgFlags = dmgFlags;
    sCylinderDamage = damage;
    sCylinderRadius = SPIN_RADIUS;
    sCylinderHeight = SPIN_HEIGHT;
}

static void SubmitCylinder(PlayState* play, Player* player) {
    if (!sIsCylinderReady) {
        Collider_InitCylinder(play, &sSpinCylinder);
        Collider_SetCylinder(play, &sSpinCylinder, &player->actor, &sSpinCylinderInit);
        sIsCylinderReady = true;
    }
    if (!sIsCylinderOn) {
        return;
    }
    sSpinCylinder.elem.atDmgInfo.dmgFlags = sCylinderDmgFlags;
    sSpinCylinder.elem.atDmgInfo.damage = sCylinderDamage;
    sSpinCylinder.dim.radius = sCylinderRadius;
    sSpinCylinder.dim.height = sCylinderHeight;
    sSpinCylinder.dim.yShift = (s16)(-sCylinderHeight / 2);
    sSpinCylinder.base.atFlags |= AT_ON;
    sSpinCylinder.dim.pos.x = (s16)player->actor.world.pos.x;
    sSpinCylinder.dim.pos.y = (s16)player->actor.world.pos.y;
    sSpinCylinder.dim.pos.z = (s16)player->actor.world.pos.z;
    CollisionCheck_SetAT(play, &play->colChkCtx, &sSpinCylinder.base);
}

static void StopAllHits(PlayState* play, Player* player) {
    GateBlades(play, player, 0);
    sIsCylinderOn = false;
    player->meleeWeaponQuads[0].base.atFlags &= ~AT_ON;
    player->meleeWeaponQuads[1].base.atFlags &= ~AT_ON;
}

// Vanilla lays both quads along the left blade; the left hook keeps quad 0 there and the right hand takes quad 1
// onto its own blade, with a trail of its own.
static void PlaceRightBlade(PlayState* play, Player* player) {
    f32 length = GetBladeLength(player);
    Vec3f trailTip = { length, 400.0f, 0.0f };
    Vec3f trailBase = { 0.0f, 400.0f, 0.0f };
    Vec3f quadTip = { length + QUAD_TIP_REACH, 1400.0f, -1000.0f };
    Vec3f quadBase = { 0.0f, 1400.0f, -1000.0f };
    Vec3f tip;
    Vec3f base;

    Matrix_MultVec3f(&trailTip, &tip);
    Matrix_MultVec3f(&trailBase, &base);
    EffectBlure* trail = sTrailRight >= 0 ? (EffectBlure*)Effect_GetByIndex(sTrailRight) : NULL;
    if (trail != NULL && !(player->stateFlags1 & STATE1_SHIELDING)) {
        EffectBlure_AddVertex(trail, &tip, &base);
    }
    Matrix_MultVec3f(&quadTip, &tip);
    Matrix_MultVec3f(&quadBase, &base);
    ColliderQuad* quad = &player->meleeWeaponQuads[1];
    quad->base.atFlags |= AT_ON;
    quad->elem.atDmgInfo.dmgFlags = sBladeDmgFlags;
    if (sOwnsBladeFlags) {
        quad->elem.atDmgInfo.damage = sBladeDamage;
    }
    func_80126440(play, quad, &player->meleeWeaponInfo[2], &tip, &base);
}

static void StopRightTrail(void) {
    EffectBlure* trail = sTrailRight >= 0 ? (EffectBlure*)Effect_GetByIndex(sTrailRight) : NULL;

    if (trail != NULL) {
        EffectBlure_AddSpace(trail);
    }
}

static void GateBladesAtHands(PlayState* play, Player* player, s32 limbIndex) {
    if (limbIndex == PLAYER_LIMB_LEFT_HAND) {
        ColliderQuad* quad = &player->meleeWeaponQuads[0];

        if (!(sBladeMask & 1)) {
            quad->base.atFlags &= ~AT_ON;
            return;
        }
        quad->base.atFlags |= AT_ON;
        if (sOwnsBladeFlags) {
            quad->elem.atDmgInfo.damage = sBladeDamage;
        }
        return;
    }
    if (limbIndex != PLAYER_LIMB_RIGHT_HAND) {
        return;
    }
    if (sBladeMask & 2) {
        PlaceRightBlade(play, player);
    } else {
        player->meleeWeaponQuads[1].base.atFlags &= ~AT_ON;
        StopRightTrail();
    }
}

static const Vec3f sHandShieldCorners[4] = {
    { -4500.0f, -3000.0f, -600.0f },
    { 1500.0f, -3000.0f, -600.0f },
    { -4500.0f, 3000.0f, -600.0f },
    { 1500.0f, 3000.0f, -600.0f },
};

static void GuardWithBlades(PlayState* play, Player* player, s32 limbIndex) {
    Vec3f corners[4];

    if (limbIndex != PLAYER_LIMB_RIGHT_HAND || !(player->stateFlags1 & STATE1_SHIELDING) ||
        player->currentShield != PLAYER_SHIELD_NONE) {
        return;
    }
    for (s32 i = 0; i < 4; i++) {
        Vec3f local = sHandShieldCorners[i];

        Matrix_MultVec3f(&local, &corners[i]);
    }
    player->shieldQuad.base.colMaterial = COL_MATERIAL_METAL;
    Collider_SetQuadVertices(&player->shieldQuad, &corners[0], &corners[1], &corners[2], &corners[3]);
    CollisionCheck_SetAC(play, &play->colChkCtx, &player->shieldQuad.base);
    CollisionCheck_SetAT(play, &play->colChkCtx, &player->shieldQuad.base);
}

static void DrawAtHands(Player* player, s32 limbIndex) {
    PlayState* play = gPlayState;

    if (!IsGerudo()) {
        return;
    }
    GateBladesAtHands(play, player, limbIndex);
    GuardWithBlades(play, player, limbIndex);
}

static void StartMove(PlayState* play, Player* player, u8 move) {
    Player_SetAction(play, player, GerudoAction, 0);
    Player_StopHorizontalMovement(player);
    sMove = move;
    sLockedYaw = player->actor.shape.rot.y;
    sIsCylinderOn = false;
    sBladeMask = 0;
}

static void EndMove(PlayState* play, Player* player) {
    StopAllHits(play, player);
    StopRightTrail();
    sMove = GERUDO_MOVE_NONE;
    sBladeVisibility = -1;
    player->stateFlags1 &= ~PLAYER_STATE1_CHARGING_SPIN_ATTACK;
    player->stateFlags2 &= ~(STATE2_SPIN_ATTACKING | STATE2_NO_ROTATION);
    player->actor.gravity = JUMP_GRAVITY;
    func_8083B1A0(player, play);
}

static void Plant(Player* player) {
    player->actor.shape.rot.y = sLockedYaw;
    player->yaw = sLockedYaw;
    player->speedXZ = 0.0f;
}

static void StepForward(PlayState* play, Player* player, f32 distance) {
    f32 sinYaw = Math_SinS(player->actor.shape.rot.y);
    f32 cosYaw = Math_CosS(player->actor.shape.rot.y);
    Vec3f from = { player->actor.world.pos.x, player->actor.world.pos.y + 20.0f, player->actor.world.pos.z };
    Vec3f to = { from.x + sinYaw * (distance + WALL_MARGIN), from.y, from.z + cosYaw * (distance + WALL_MARGIN) };
    Vec3f hit;
    CollisionPoly* poly = NULL;
    s32 bgId;

    if (distance <= 0.0f ||
        BgCheck_EntityLineTest1(&play->colCtx, &from, &to, &hit, &poly, true, false, false, true, &bgId)) {
        return;
    }
    player->actor.world.pos.x += sinYaw * distance;
    player->actor.world.pos.z += cosYaw * distance;
}

static void EnsureRightTrail(PlayState* play) {
    if (sTrailRight < 0 || Effect_GetByIndex(sTrailRight) == NULL) {
        Effect_Add(play, &sTrailRight, EFFECT_BLURE2, 0, 0, &sTrailInit);
    }
}

static s32 ChooseRow(PlayState* play, Player* player) {
    if (Player_CanSpinAttack(player)) {
        return ROW_QUICK_SPIN;
    }
    if (Player_IsZTargeting(player) && GetStickDirection(player) == PLAYER_STICK_DIR_FORWARD) {
        sIsLungePending = true;
        return ROW_STAB;
    }
    s32 row = ROW_COMBO_1 + sComboStep;
    sComboStep = (sComboStep + 1) % COMBO_STEPS;
    sComboIdleFrames = 0;
    return row;
}

static void StartSwing(PlayState* play, Player* player, s32 row) {
    const SwingRow* swing = &sRows[row];
    bool isJump = row == ROW_JUMP_LAND;

    StartMove(play, player, GERUDO_MOVE_SWING);
    sRow = (u8)row;
    sIsNextHitQueued = false;
    player->meleeWeaponAnimation = swing->mwa;
    player->yaw = player->actor.shape.rot.y;
    SetBladeFlags(player, DMG_SWORD, GetTierDamage(player, isJump), false);
    EnsureRightTrail(play);
    PlayClip(play, player, LoadWholeClip(swing->clip), swing->speed, ANIMMODE_ONCE, -3.0f);
}

static f32 GetSwingSpeed(const SwingRow* swing, f32 frame) {
    if (sRow == ROW_CHARGE_RELEASE && frame >= CHARGE_FAST_BEGIN && frame <= CHARGE_FAST_END) {
        return swing->speed * CHARGE_FAST_SCALE;
    }
    return swing->speed;
}

static void GateSwing(PlayState* play, Player* player, const SwingRow* swing, f32 prev, f32 cur) {
    u8 mask = 0;

    if (IsInWindow(&swing->left, prev, cur)) {
        mask |= 1;
    }
    if (IsInWindow(&swing->right, prev, cur)) {
        mask |= 2;
    }
    // En_M_Thunder drops SPIN_ATTACKING on the release; the release swing keeps it while its blades are live.
    if (sRow == ROW_CHARGE_RELEASE && mask != 0) {
        player->stateFlags2 |= STATE2_SPIN_ATTACKING;
    }
    if (swing->isSpin) {
        GateBlades(play, player, 0);
        sIsCylinderOn = mask != 0;
        if (sIsCylinderOn) {
            TurnCylinderOn(DMG_SWORD, SPIN_DAMAGE);
        }
        return;
    }
    GateBlades(play, player, mask);
}

static void StartCharge(PlayState* play, Player* player);

static void RunSwing(PlayState* play, Player* player) {
    const SwingRow* swing = &sRows[sRow];
    f32 swingEnd = swing->swingEnd >= 0 ? (f32)swing->swingEnd : Animation_GetLastFrame(player->skelAnime.animation);

    player->stateFlags2 |= STATE2_NO_Z_TARGET_ROTATION;
    if (sIsLungePending && sPrevFrame <= 0.0f) {
        player->speedXZ = LUNGE_SPEED;
        sIsLungePending = false;
    }
    Math_StepToF(&player->speedXZ, 0.0f, 5.0f);
    player->skelAnime.playSpeed = GetSwingSpeed(swing, player->skelAnime.curFrame);
    bool isDone = PlayerAnimation_Update(play, &player->skelAnime);
    f32 cur = player->skelAnime.curFrame;
    GateSwing(play, player, swing, sPrevFrame, cur);
    sPrevFrame = cur;
    if (IsPressed(play, BTN_B)) {
        sIsNextHitQueued = true;
    }
    if (cur >= swingEnd && sRow <= ROW_COMBO_4) {
        if (sIsNextHitQueued) {
            StartSwing(play, player, ChooseRow(play, player));
            return;
        }
        if (IsPressed(play, BTN_A)) {
            StopAllHits(play, player);
            sMove = GERUDO_MOVE_NONE;
            func_80836B3C(play, player, 0.0f);
            return;
        }
    }
    if (!isDone) {
        return;
    }
    if (sRow <= ROW_COMBO_4 && IsHeld(play, BTN_B)) {
        StartCharge(play, player);
        return;
    }
    EndMove(play, player);
}

// The same spawn as z_player.c's spin charge: the actor reads unk_B08 for its sparks and turns into the release.
static void SpawnChargeSparks(PlayState* play, Player* player) {
    Vec3f* waist = &player->bodyPartsPos[PLAYER_BODYPART_WAIST];

    Actor_Spawn(&play->actorCtx, play, ACTOR_EN_M_THUNDER, waist->x, waist->y, waist->z, 0, 0, 0,
                GetSwordTier(player) | CHARGE_SPARK_COST);
}

static void StartCharge(PlayState* play, Player* player) {
    StartMove(play, player, GERUDO_MOVE_CHARGE);
    player->unk_B08 = 0.0f;
    player->stateFlags1 |= PLAYER_STATE1_CHARGING_SPIN_ATTACK;
    PlayClip(play, player, LoadClip(CLIP_CHARGE, 0, 24, 14), 1.0f, ANIMMODE_ONCE, -6.0f);
    sIsStanceLooping = false;
    SpawnChargeSparks(play, player);
}

static void CreepWhileCharging(PlayState* play, Player* player) {
    f32 stick = GetStickMagnitude(play) / STICK_FULL;
    s16 yawTarget = Math_Atan2S(play->state.input[0].rel.stick_y, -play->state.input[0].rel.stick_x) +
                    Camera_GetInputDirYaw(GET_ACTIVE_CAM(play));

    Math_AsymStepToF(&player->speedXZ, stick * CREEP_SPEED * 0.2f, 1.0f, 0.5f);
    if (stick > 0.0f) {
        Math_ScaledStepToS(&player->yaw, yawTarget, 0x800);
    }
}

static void ReleaseCharge(PlayState* play, Player* player) {
    sIsLungePending = true;
    StartSwing(play, player, ROW_CHARGE_RELEASE);
    player->stateFlags2 |= STATE2_SPIN_ATTACKING;
    player->invincibilityTimer = MIN(player->invincibilityTimer, SPIN_INVULNERABILITY);
}

static void RunCharge(PlayState* play, Player* player) {
    player->stateFlags1 |= PLAYER_STATE1_CHARGING_SPIN_ATTACK;
    if (PlayerAnimation_Update(play, &player->skelAnime) && !sIsStanceLooping) {
        sIsStanceLooping = true;
        PlayClip(play, player, LoadClip(CLIP_CHARGE, 25, 172, 60), 1.0f, ANIMMODE_LOOP, -4.0f);
    }
    if (!sIsStanceLooping) {
        return;
    }
    Math_StepToF(&player->unk_B08, 1.0f, CHARGE_RATE);
    CreepWhileCharging(play, player);
    if (!IsHeld(play, BTN_B)) {
        ReleaseCharge(play, player);
    }
}

static void StartFrontSlash(PlayState* play, Player* player) {
    StartMove(play, player, GERUDO_MOVE_FRONT_SLASH);
    PlayClip(play, player, LoadWholeClip(CLIP_FRONT_SLASH), 1.2f, ANIMMODE_ONCE, -3.0f);
    Player_PlaySfx(player, NA_SE_IT_SWORD_SWING);
    Player_PlaySfx(player, NA_SE_VO_LI_SWORD_N);
}

static void RunFrontSlash(PlayState* play, Player* player) {
    Plant(player);
    bool isDone = PlayerAnimation_Update(play, &player->skelAnime);
    f32 frame = player->skelAnime.curFrame;
    if (frame < FRONT_SLASH_TRAVEL_FRAMES) {
        StepForward(play, player, FRONT_SLASH_DISTANCE / FRONT_SLASH_TRAVEL_FRAMES);
    }
    sIsCylinderOn = frame >= 4.0f && frame <= 60.0f;
    if (sIsCylinderOn) {
        TurnCylinderOn(DMG_SWORD, SPIN_DAMAGE);
    }
    if (IsPressed(play, BTN_B)) {
        sIsCylinderOn = false;
        StartSwing(play, player, ChooseRow(play, player));
        return;
    }
    if (isDone || IsPressed(play, BTN_A)) {
        EndMove(play, player);
    }
}

static void StartJumpSlash(PlayState* play, Player* player) {
    StartMove(play, player, GERUDO_MOVE_JUMP_SLASH);
    player->stateFlags3 |= STATE3_MIDAIR;
    player->meleeWeaponAnimation = PLAYER_MWA_JUMPSLASH_START;
    SetBladeFlags(player, DMG_SWORD, GetTierDamage(player, true), false);
    EnsureRightTrail(play);
    PlayClip(play, player, LoadWholeClip(CLIP_JUMP_SLASH), 1.0f, ANIMMODE_ONCE, -3.0f);
    sIsJumpLooping = false;
    sJumpSlashFrames = 0;
    player->yaw = player->actor.shape.rot.y;
    player->speedXZ = 5.0f;
    player->actor.velocity.y = 5.0f * JUMP_SLASH_LIFT;
    player->actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND;
    Player_PlaySfx(player, NA_SE_IT_SWORD_SWING);
    Player_PlaySfx(player, NA_SE_VO_LI_SWORD_L);
}

static void HomeOnTarget(Player* player, Actor* target) {
    f32 dx = target->world.pos.x - player->actor.world.pos.x;
    f32 dz = target->world.pos.z - player->actor.world.pos.z;
    f32 distance = MAX(sqrtf(SQ(dx) + SQ(dz)) - target->colChkInfo.cylRadius, 0.0f);

    player->yaw = Math_Vec3f_Yaw(&player->actor.world.pos, &target->world.pos);
    player->actor.shape.rot.y = player->yaw;
    player->speedXZ = MIN(CLAMP(distance / JUMP_SLASH_LEAD, JUMP_SLASH_SPEED_MIN, JUMP_SLASH_SPEED_MAX), distance);
}

static void RunJumpSlash(PlayState* play, Player* player) {
    player->stateFlags3 |= STATE3_MIDAIR;
    player->actor.gravity = JUMP_GRAVITY;
    if (PlayerAnimation_Update(play, &player->skelAnime) && !sIsJumpLooping) {
        sIsJumpLooping = true;
        PlayClip(play, player, LoadClip(CLIP_JUMP_SLASH, 6, 9, 0), 1.0f, ANIMMODE_LOOP, -3.0f);
    }
    GateBlades(play, player, 3);
    Actor* target = player->focusActor;
    if (target != NULL && target->update != NULL) {
        HomeOnTarget(player, target);
    } else {
        f32 stick = GetStickMagnitude(play) / STICK_FULL;

        Math_StepToF(&player->speedXZ, stick * CREEP_SPEED * 0.5f, 1.0f);
    }
    if (++sJumpSlashFrames < 2 || !IsGrounded(player)) {
        return;
    }
    Player_PlaySfx(player, NA_SE_PL_WALK_GROUND);
    StartSwing(play, player, ROW_JUMP_LAND);
}

static void StartAerial(PlayState* play, Player* player) {
    StartMove(play, player, GERUDO_MOVE_AERIAL);
    player->stateFlags3 |= STATE3_MIDAIR;
    SetBladeFlags(player, DMG_SWORD, GetTierDamage(player, false), false);
    EnsureRightTrail(play);
    PlayClip(play, player, LoadWholeClip(CLIP_AERIAL), 1.2f, ANIMMODE_ONCE, -3.0f);
    Player_PlaySfx(player, NA_SE_IT_SWORD_SWING);
    Player_PlaySfx(player, NA_SE_VO_LI_SWORD_N);
}

static void RunAerial(PlayState* play, Player* player) {
    static const Window sAerialWindow = { 2, 40 };

    player->stateFlags3 |= STATE3_MIDAIR;
    player->actor.gravity = JUMP_GRAVITY;
    bool isDone = PlayerAnimation_Update(play, &player->skelAnime);
    f32 cur = player->skelAnime.curFrame;
    GateBlades(play, player, IsInWindow(&sAerialWindow, sPrevFrame, cur) ? 3 : 0);
    sPrevFrame = cur;
    if (isDone || IsGrounded(player)) {
        EndMove(play, player);
    }
}

typedef struct {
    Color_RGBA8 gold;
    Color_RGBA8 orange;
    Color_RGBA8 blue;
    Color_RGBA8 deepBlue;
    Color_RGBA8 white;
    Color_RGBA8 paleBlue;
} FuryColors;

static FuryColors sFury = {
    { 255, 210, 45, 255 }, { 255, 90, 0, 180 },    { 85, 150, 255, 255 },
    { 25, 45, 190, 190 },  { 255, 255, 220, 255 }, { 180, 220, 255, 210 },
};

static bool HasCrossed(f32 prev, f32 cur, f32 mark) {
    return prev < mark && cur >= mark;
}

static Vec3f GetGroundPos(Player* player) {
    Vec3f pos = player->actor.world.pos;

    if (player->actor.floorHeight > BGCHECK_Y_MIN + 1.0f) {
        pos.y = player->actor.floorHeight;
    }
    return pos;
}

static void SpawnChargeBurst(PlayState* play, Player* player, s32 burst) {
    f32 progress = burst / 5.0f;
    f32 radius = 28.0f + progress * 48.0f;

    for (s32 i = 0; i < 3; i++) {
        s16 yaw = (s16)(burst * 0x1D00 + i * 0x5555 + play->gameplayFrames * 0x300);
        Vec3f pos = { player->actor.world.pos.x + Math_SinS(yaw) * radius,
                      player->actor.world.pos.y + 12.0f + i * 22.0f,
                      player->actor.world.pos.z + Math_CosS(yaw) * radius };
        bool isGold = (burst + i) % 3 == 0;
        Color_RGBA8* prim = isGold ? &sFury.gold : &sFury.blue;
        Color_RGBA8* env = isGold ? &sFury.orange : &sFury.deepBlue;
        Vec3f velocity = { -Math_SinS(yaw) * 1.4f, 1.2f + progress, -Math_CosS(yaw) * 1.4f };
        Vec3f accel = { 0.0f, -0.08f, 0.0f };
        EffectSsLightning_Spawn(play, &pos, prim, env, (s16)(75 + burst * 10), yaw, 10, 2);
        EffectSsKirakira_SpawnFocused(play, &pos, &velocity, &accel, prim, &sFury.white, (s16)(260 + burst * 24), 14);
    }
}

static void SpawnCloudConvergence(PlayState* play, Player* player, s32 stage) {
    f32 radius = 82.0f - stage * 18.0f;

    for (s32 i = 0; i < 4; i++) {
        s16 yaw = (s16)(i * 0x4000 + stage * 0x1100);
        Vec3f pos = { player->actor.world.pos.x + Math_SinS(yaw) * radius,
                      player->actor.world.pos.y + 85.0f + stage * 45.0f,
                      player->actor.world.pos.z + Math_CosS(yaw) * radius };
        EffectSsLightning_Spawn(play, &pos, (i & 1) ? &sFury.gold : &sFury.blue, &sFury.paleBlue,
                                (s16)(125 + stage * 28), yaw, 12, 3);
    }
}

// The sky strike borrows the storm layer: LIGHTNING_LAST switches a dry scene's lightning back off by itself.
static void StrikeFromSky(PlayState* play) {
    if (gLightningStrike.state != LIGHTNING_STRIKE_WAIT) {
        return;
    }
    Environment_AddLightningBolts(play, 3);
    gLightningStrike.flashRed = 210;
    gLightningStrike.flashGreen = 220;
    gLightningStrike.flashBlue = 255;
    gLightningStrike.flashAlphaTarget = 200;
    gLightningStrike.state = LIGHTNING_STRIKE_START;
    if (play->envCtx.lightningState == LIGHTNING_OFF) {
        play->envCtx.lightningState = LIGHTNING_LAST;
    }
}

static void SpawnFuryImpact(PlayState* play, Player* player) {
    Vec3f zero = { 0.0f, 0.0f, 0.0f };
    Vec3f ground = GetGroundPos(player);

    StrikeFromSky(play);
    for (s32 i = 0; i < 7; i++) {
        Vec3f column = { ground.x, ground.y + 18.0f + i * 48.0f, ground.z };
        EffectSsLightning_Spawn(play, &column, (i & 1) ? &sFury.white : &sFury.blue, &sFury.gold, (s16)(250 - i * 12),
                                (s16)(i * 0x1555), 13, 4);
    }
    EffectSsBlast_Spawn(play, &ground, &zero, &zero, &sFury.white, &sFury.orange, 80, 660, 42, 15);
    EffectSsBlast_Spawn(play, &ground, &zero, &zero, &sFury.paleBlue, &sFury.deepBlue, 110, 500, 24, 21);
    for (s32 i = 0; i < 12; i++) {
        s16 yaw = (s16)(i * 0x1555 + (i % 3) * 0x500);
        f32 reach = 24.0f + (i % 4) * 12.0f;
        Vec3f arc = { ground.x + Math_SinS(yaw) * reach, ground.y + 4.0f, ground.z + Math_CosS(yaw) * reach };
        Color_RGBA8* prim = (i % 4 == 0) ? &sFury.blue : &sFury.gold;
        f32 push = 2.0f + (i % 3) * 0.35f;
        Vec3f velocity = { Math_SinS(yaw) * push, 2.2f, Math_CosS(yaw) * push };
        Vec3f accel = { 0.0f, -0.16f, 0.0f };
        EffectSsLightning_Spawn(play, &arc, prim, &sFury.orange, 105, yaw, 11, 2);
        EffectSsKirakira_SpawnFocused(play, &arc, &velocity, &accel, prim, &sFury.white, 420, 18);
    }
    Actor_RequestQuakeWithSpeed(play, FURY_QUAKE_STRENGTH, FURY_QUAKE_FRAMES, FURY_QUAKE_SPEED);
    Rumble_Request(0.0f, 255, 20, 150);
    Player_PlaySfx(player, NA_SE_EV_LIGHTNING);
    Player_PlaySfx(player, NA_SE_IT_HAMMER_HIT);
}

static void SpawnResidualRing(PlayState* play, Player* player, s32 stage) {
    Vec3f ground = GetGroundPos(player);
    f32 radius = 58.0f + stage * 43.0f;

    for (s32 i = 0; i < 6; i++) {
        s16 yaw = (s16)(i * 0x2AAA + stage * 0x900 + play->gameplayFrames * 0x100);
        Vec3f pos = { ground.x + Math_SinS(yaw) * radius, ground.y + 5.0f, ground.z + Math_CosS(yaw) * radius };
        EffectSsLightning_Spawn(play, &pos, ((i + stage) & 1) ? &sFury.gold : &sFury.blue, &sFury.deepBlue,
                                (s16)(100 - stage * 10), yaw, 9, 2);
    }
}

static f32 GetFuryDarkness(f32 frame) {
    if (frame >= 28.0f && frame < 68.0f) {
        return ((frame - 28.0f) / 40.0f) * 0.84f;
    }
    if (frame >= 68.0f && frame < FURY_STRIKE_FRAME) {
        return 0.84f;
    }
    if (frame >= 84.0f && frame < 116.0f) {
        return 0.52f * (1.0f - ((frame - 84.0f) / 32.0f));
    }
    return 0.0f;
}

static void TickFuryVfx(PlayState* play, Player* player, f32 frame) {
    static const f32 sConvergeMarks[] = { 57.0f, 67.0f, 75.0f };
    static const f32 sResidualMarks[] = { 88.0f, 98.0f, 108.0f };
    f32 prev = sFuryPrevFrame < 0.0f || frame < sFuryPrevFrame ? frame : sFuryPrevFrame;

    Environment_AdjustLights(play, GetFuryDarkness(frame), 850.0f, 0.2f, 0.9f);
    if (frame >= 34.0f && frame < FURY_STRIKE_FRAME) {
        Actor_PlaySfx_Flagged(&player->actor, NA_SE_EN_BIRI_SPARK);
    }
    for (s32 burst = 0; burst < 6; burst++) {
        if (HasCrossed(prev, frame, 36.0f + burst * 7.0f)) {
            SpawnChargeBurst(play, player, burst);
        }
    }
    for (s32 stage = 0; stage < 3; stage++) {
        if (HasCrossed(prev, frame, sConvergeMarks[stage])) {
            SpawnCloudConvergence(play, player, stage);
        }
        if (HasCrossed(prev, frame, sResidualMarks[stage])) {
            SpawnResidualRing(play, player, stage);
        }
    }
    if (HasCrossed(prev, frame, FURY_STRIKE_FRAME)) {
        SpawnFuryImpact(play, player);
    }
    sFuryPrevFrame = frame;
}

static void RestoreFuryLights(PlayState* play) {
    if (sFuryPrevFrame < 0.0f) {
        return;
    }
    Environment_AdjustLights(play, 0.0f, 850.0f, 0.2f, 0.9f);
    sFuryPrevFrame = -1.0f;
}

static void DimForImpact(PlayState* play, u8 dim) {
    play->envCtx.fillScreen = true;
    play->envCtx.screenFillColor[0] = 0;
    play->envCtx.screenFillColor[1] = 0;
    play->envCtx.screenFillColor[2] = 40;
    play->envCtx.screenFillColor[3] = dim;
}

static void FadeImpactDim(PlayState* play) {
    if (!play->envCtx.fillScreen) {
        return;
    }
    if (play->envCtx.screenFillColor[3] <= FURY_DIM_FADE) {
        play->envCtx.screenFillColor[3] = 0;
        play->envCtx.fillScreen = false;
        return;
    }
    play->envCtx.screenFillColor[3] -= FURY_DIM_FADE;
}

static void StartFury(PlayState* play, Player* player) {
    f32 charge = (f32)sRageMeter / (f32)GetRageCapacity();

    sFuryDamage = (u8)(FURY_DAMAGE_MIN + (FURY_DAMAGE_MAX - FURY_DAMAGE_MIN) * charge);
    sRageMeter = 0;
    sComboStep = 0;
    sHasFuryStruck = false;
    player->stateFlags1 &= ~STATE1_SHIELDING;
    StartMove(play, player, GERUDO_MOVE_FURY);
    SetBladeFlags(player, FURY_DMG_FLAGS, sFuryDamage, true);
    EnsureRightTrail(play);
    PlayClip(play, player, LoadWholeClip(CLIP_FURY), 2.4f, ANIMMODE_ONCE, -6.0f);
    Player_PlaySfx(player, NA_SE_VO_LI_AUTO_JUMP);
}

static void RunFury(PlayState* play, Player* player) {
    static const Window sFuryWindows[2] = { { 20, 28 }, { 80, 110 } };

    Plant(player);
    player->invincibilityTimer = MIN(player->invincibilityTimer, -FURY_INVULNERABILITY);
    bool isDone = PlayerAnimation_Update(play, &player->skelAnime);
    f32 frame = player->skelAnime.curFrame;
    bool isBladeLive =
        IsInWindow(&sFuryWindows[0], sPrevFrame, frame) || IsInWindow(&sFuryWindows[1], sPrevFrame, frame);
    GateBlades(play, player, isBladeLive ? 1 : 0);
    sPrevFrame = frame;
    f32 grow = MIN(frame / FURY_BLAST_FRAMES, 1.0f);
    TurnCylinderOn(FURY_DMG_FLAGS, sFuryDamage);
    sCylinderRadius = (s16)(SPIN_RADIUS + (FURY_BLAST_RADIUS - SPIN_RADIUS) * grow);
    sCylinderHeight = (s16)(SPIN_HEIGHT + (FURY_BLAST_HEIGHT - SPIN_HEIGHT) * grow);
    TickFuryVfx(play, player, frame);
    if (!sHasFuryStruck && frame >= FURY_STRIKE_FRAME) {
        sHasFuryStruck = true;
        Player_RequestRumble(play, player, 255, 20, 150, 0);
        DimForImpact(play, FURY_DIM);
    }
    if (isDone) {
        RestoreFuryLights(play);
        EndMove(play, player);
    }
}

// The item changes hands at the end of the clip, so vanilla's own put-away and draw never play.
static void HandOver(Player* player, ItemId item) {
    player->heldItemId = item;
    player->heldItemAction = item == ITEM_NONE ? PLAYER_IA_NONE : Player_ItemToItemAction(player, item);
    func_80123C58(player);
}

static void StartSheathe(PlayState* play, Player* player) {
    StartMove(play, player, GERUDO_MOVE_SHEATHE);
    sBladeVisibility = 1;
    sPendingItem = ITEM_NONE;
    PlayClip(play, player, LoadWholeClip(CLIP_SHEATHE), DRAW_SPEED, ANIMMODE_ONCE, -3.0f);
    Player_PlaySfx(player, NA_SE_IT_SWORD_PUTAWAY);
}

static void StartDraw(PlayState* play, Player* player, ItemId item) {
    StartMove(play, player, GERUDO_MOVE_DRAW);
    sBladeVisibility = 0;
    sPendingItem = item;
    PlayClip(play, player, LoadWholeClip(CLIP_SHEATHE), -DRAW_SPEED, ANIMMODE_ONCE, -3.0f);
}

static void RunSheathe(PlayState* play, Player* player) {
    Plant(player);
    bool isDone = PlayerAnimation_Update(play, &player->skelAnime);
    if (player->skelAnime.curFrame >= SHEATHE_HIDE_FRAME) {
        sBladeVisibility = 0;
    }
    if (isDone) {
        HandOver(player, sPendingItem);
        EndMove(play, player);
    }
}

static void RunDraw(PlayState* play, Player* player) {
    Plant(player);
    bool isDone = PlayerAnimation_Update(play, &player->skelAnime);
    if (sBladeVisibility == 0 && player->skelAnime.curFrame <= SHEATHE_HIDE_FRAME) {
        sBladeVisibility = 1;
        Player_PlaySfx(player, NA_SE_IT_SWORD_PICKOUT);
    }
    if (isDone) {
        HandOver(player, sPendingItem);
        EndMove(play, player);
    }
}

static void StartGuard(PlayState* play, Player* player) {
    StartMove(play, player, GERUDO_MOVE_GUARD);
    sBladeVisibility = 1;
    sIsGuardLowering = false;
    sGuardAim.x = 0;
    sGuardAim.y = 0;
    PlayClip(play, player, LoadClip(CLIP_GUARD, 1, 20, 10), 1.0f, ANIMMODE_ONCE, -3.0f);
    Player_PlaySfx(player, NA_SE_IT_SHIELD_SWING);
}

// The stick aims the crossed blades the way it aims a shield, on top of the guard's fixed twist.
static void AimGuard(PlayState* play, Player* player) {
    Input* input = &play->state.input[0];
    f32 pitchInput = input->rel.stick_y * 100.0f;
    f32 turnInput = input->rel.stick_x * -120.0f;
    s16 yawToCamera = player->actor.shape.rot.y - Camera_GetInputDirYaw(GET_ACTIVE_CAM(play));
    s16 pitch = (s16)(Math_SinS(yawToCamera) * turnInput + Math_CosS(yawToCamera) * pitchInput);
    s16 turn = (s16)(Math_CosS(yawToCamera) * turnInput - Math_SinS(yawToCamera) * pitchInput);

    pitch = MIN(pitch, GUARD_AIM_PITCH_MAX);
    Math_ScaledStepToS(&sGuardAim.x, pitch, MAX((s16)(ABS(pitch - sGuardAim.x) * 0.25f), 100));
    Math_ScaledStepToS(&sGuardAim.y, turn, MAX((s16)(ABS(turn - sGuardAim.y) * 0.25f), 50));
    player->actor.focus.rot.x = sGuardAim.x;
    player->upperLimbRot.x = sGuardAim.x + GUARD_TWIST_X;
    player->upperLimbRot.y = sGuardAim.y + GUARD_TWIST_Y;
    player->upperLimbRot.z = GUARD_TWIST_Z;
    player->unk_AA6_rotFlags |= UNKAA6_ROT_FOCUS_X | UNKAA6_ROT_UPPER_X | UNKAA6_ROT_UPPER_Y | UNKAA6_ROT_UPPER_Z;
}

static void LowerGuard(PlayState* play, Player* player) {
    sIsGuardLowering = true;
    PlayClip(play, player, LoadClip(CLIP_GUARD, 31, 45, 8), 1.0f, ANIMMODE_ONCE, -2.0f);
    Player_PlaySfx(player, NA_SE_IT_SHIELD_REMOVE);
}

static void RunGuard(PlayState* play, Player* player) {
    Plant(player);
    bool isDone = PlayerAnimation_Update(play, &player->skelAnime);

    if (sIsGuardLowering) {
        if (isDone) {
            EndMove(play, player);
        }
        return;
    }
    player->stateFlags1 |= STATE1_SHIELDING;
    if (isDone) {
        PlayClip(play, player, LoadClip(CLIP_GUARD, 30, 30, 2), 1.0f, ANIMMODE_LOOP, 0.0f);
    }
    AimGuard(play, player);
    if (IsPressed(play, BTN_B) && sRageMeter > 0) {
        StartFury(play, player);
        return;
    }
    if (!IsHeld(play, BTN_R) || !CanAct(player)) {
        LowerGuard(play, player);
    }
}

static void GerudoAction(Player* player, PlayState* play) {
    BankBladeHits(player);
    switch (sMove) {
        case GERUDO_MOVE_SWING:
            RunSwing(play, player);
            break;
        case GERUDO_MOVE_CHARGE:
            RunCharge(play, player);
            break;
        case GERUDO_MOVE_FRONT_SLASH:
            RunFrontSlash(play, player);
            break;
        case GERUDO_MOVE_JUMP_SLASH:
            RunJumpSlash(play, player);
            break;
        case GERUDO_MOVE_AERIAL:
            RunAerial(play, player);
            break;
        case GERUDO_MOVE_FURY:
            RunFury(play, player);
            break;
        case GERUDO_MOVE_SHEATHE:
            RunSheathe(play, player);
            break;
        case GERUDO_MOVE_DRAW:
            RunDraw(play, player);
            break;
        case GERUDO_MOVE_GUARD:
            RunGuard(play, player);
            break;
        default:
            EndMove(play, player);
            break;
    }
}

static bool CanJumpFromFloor(PlayState* play, Player* player) {
    return play->roomCtx.curRoom.type != ROOM_TYPE_INDOORS && player->actor.floorPoly != NULL &&
           SurfaceType_GetFloorType(&play->colCtx, player->actor.floorPoly, player->actor.floorBgId) != FLOOR_TYPE_7 &&
           SurfaceType_GetFloorEffect(&play->colCtx, player->actor.floorPoly, player->actor.floorBgId) !=
               FLOOR_EFFECT_1;
}

static bool WantsFrontSlash(PlayState* play, Player* player) {
    if (sIsSprinting) {
        return true;
    }
    return player->focusActor == NULL && GetStickMagnitude(play) >= STICK_MOVING &&
           GetStickDirection(player) == PLAYER_STICK_DIR_FORWARD;
}

static int32_t HandleSlash(PlayState* play, Player* player) {
    if (!IsPressed(play, BTN_B) || !HoldsSword(player) || !CanAct(player) || !IsGrounded(player)) {
        return S2H_FORM_ACTION_VANILLA;
    }
    if (IsHeld(play, BTN_R)) {
        return S2H_FORM_ACTION_BLOCKED;
    }
    if (WantsFrontSlash(play, player)) {
        StartFrontSlash(play, player);
    } else {
        StartSwing(play, player, ChooseRow(play, player));
    }
    return S2H_FORM_ACTION_STARTED;
}

// The vanilla guard needs a shield, which the Gerudo never carries: the blades guard in its place.
static int32_t HandleShield(PlayState* play, Player* player) {
    if (!IsHeld(play, BTN_R)) {
        return S2H_FORM_ACTION_VANILLA;
    }
    if (IsGerudoBusy(player) || !CanAct(player) || !IsGrounded(player)) {
        return S2H_FORM_ACTION_BLOCKED;
    }
    StartGuard(play, player);
    return S2H_FORM_ACTION_STARTED;
}

static int32_t HandleA(PlayState* play, Player* player) {
    if (!IsPressed(play, BTN_A) || !HoldsSword(player) || Player_IsZTargeting(player) || !IsGrounded(player)) {
        return S2H_FORM_ACTION_VANILLA;
    }
    sIsAPending = true;
    sAFrames = 0;
    return S2H_FORM_ACTION_BLOCKED;
}

static int32_t HandleZTargetA(PlayState* play, Player* player) {
    if (!Player_IsZTargeting(player)) {
        return sIsAPending ? S2H_FORM_ACTION_BLOCKED : S2H_FORM_ACTION_VANILLA;
    }
    if (!IsPressed(play, BTN_A) || !CanAct(player) || !IsGrounded(player) || !CanJumpFromFloor(play, player)) {
        return S2H_FORM_ACTION_VANILLA;
    }
    if (GetStickDirection(player) > PLAYER_STICK_DIR_FORWARD || !HoldsSword(player)) {
        return S2H_FORM_ACTION_VANILLA;
    }
    StartJumpSlash(play, player);
    return S2H_FORM_ACTION_STARTED;
}

static bool IsSwordItem(s32 item) {
    return item >= ITEM_SWORD_KOKIRI && item <= ITEM_SWORD_GILDED;
}

// B with the blades sheathed would draw and swing at once; standing, the Gerudo draws them first.
static void InterceptDraw(bool* should, va_list args) {
    s32 item = va_arg(args, s32);
    Player* player = gPlayState == NULL ? NULL : GET_PLAYER(gPlayState);

    if (player == NULL || !IsGerudo() || !*should || HoldsSword(player) || IsGerudoBusy(player)) {
        return;
    }
    if (!IsSwordItem(item) || !IsGrounded(player) || !CanAct(player) || fabsf(player->speedXZ) > DRAW_MAX_SPEED) {
        return;
    }
    *should = false;
    StartDraw(gPlayState, player, (ItemId)item);
}

static void TrackA(PlayState* play, Player* player) {
    bool isEnabled = HoldsSword(player) && !IsGerudoBusy(player) && IsGrounded(player) && !IsRolling(player) &&
                     !(player->stateFlags1 & STATE1_SHIELDING);

    if (!isEnabled) {
        sIsAPending = false;
        sIsSprinting = false;
        return;
    }
    if (IsHeld(play, BTN_A)) {
        if (sIsAPending && ++sAFrames >= A_TAP_FRAMES) {
            sIsAPending = false;
            sIsSprinting = true;
        }
        return;
    }
    sIsSprinting = false;
    if (!sIsAPending) {
        return;
    }
    sIsAPending = false;
    if (!CanAct(player)) {
        return;
    }
    if (GetStickMagnitude(play) >= STICK_MOVING) {
        func_80836B3C(play, player, 0.0f);
    } else {
        StartSheathe(play, player);
    }
}

static void TryFury(PlayState* play, Player* player) {
    if (!HoldsSword(player) || IsGerudoBusy(player) || !IsGrounded(player) || !CanAct(player) || sRageMeter <= 0) {
        return;
    }
    if (IsHeld(play, BTN_R) && IsPressed(play, BTN_B)) {
        StartFury(play, player);
    }
}

static void TryAerial(PlayState* play, Player* player) {
    sAirFrames = IsGrounded(player) ? 0 : MIN(sAirFrames + 1, 1000);
    if (!HoldsSword(player) || IsGerudoBusy(player) || IsGrounded(player) || !CanAct(player) ||
        sAirFrames < AERIAL_MIN_AIR_FRAMES || !IsPressed(play, BTN_A) || player->meleeWeaponState != 0 ||
        (player->stateFlags2 & STATE2_HOPPING)) {
        return;
    }
    StartAerial(play, player);
}

static void CountComboGap(Player* player) {
    if (sComboStep == 0 || (IsGerudoBusy(player) && sMove == GERUDO_MOVE_SWING)) {
        return;
    }
    if (++sComboIdleFrames >= COMBO_RESET_FRAMES) {
        sComboStep = 0;
        sComboIdleFrames = 0;
    }
}

static void DrawScimitars(Player* player, s32 limbIndex, Gfx** dList, Gfx* limbDList, Vec3f* pos) {
    if (!IsGerudo() || !AreBladesOut(player)) {
        return;
    }
    if (limbIndex == PLAYER_LIMB_LEFT_HAND || limbIndex == PLAYER_LIMB_RIGHT_HAND) {
        *dList = (Gfx*)sScimitarDL;
    } else if (limbIndex == PLAYER_LIMB_SHEATH) {
        *dList = NULL;
    }
}

static void LoadVoices(void) {
    char path[64];

    for (u32 voice = 0; voice < ARRAY_COUNT(sVoices); voice++) {
        sVoiceFirstSample[voice] = sVoiceSampleCount;
        for (u8 variant = 0; variant < sVoices[voice].variants && sVoiceSampleCount < VOICE_SAMPLES_MAX; variant++) {
            snprintf(path, sizeof(path), "custom/samples/gerudo_voice_%04x_%d", sVoices[voice].sfxId, variant);
            Sample* sample = (Sample*)ResourceGetDataByName(path);
            VoiceSample* slot = &sVoiceSamples[sVoiceSampleCount++];
            slot->samples = sample != NULL ? (const s16*)sample->sampleAddr : NULL;
            slot->count = sample != NULL ? (s32)(sample->size / sizeof(s16)) : 0;
        }
    }
}

static s32 FindVoice(u16 sfxId) {
    for (u32 voice = 0; voice < ARRAY_COUNT(sVoices); voice++) {
        if (sVoices[voice].sfxId == sfxId) {
            return (s32)voice;
        }
    }
    return -1;
}

static void SpeakAsGerudo(Actor* actor, s32 kind, u16* sfxId, bool* handled) {
    if (!IsGerudo() || kind != SOH_ACTOR_SFX_VOICE) {
        return;
    }
    *handled = true;
    s32 voice = FindVoice(*sfxId);
    if (voice < 0) {
        return;
    }
    sRequestedVoice = sVoiceFirstSample[voice] + (s32)(Rand_ZeroOne() * sVoices[voice].variants * 0.999f);
}

// Audio thread: only the request and the slots are touched here; the host applies the SFX volume.
static void MixVoices(s16* samples, u32 frameCount) {
    s32 request = sRequestedVoice;

    if (request >= 0) {
        sRequestedVoice = -1;
        for (s32 slot = 0; slot < VOICE_SLOTS; slot++) {
            if (sVoiceSlots[slot].sample < 0) {
                sVoiceSlots[slot].sample = request;
                sVoiceSlots[slot].position = 0.0f;
                break;
            }
        }
    }
    for (s32 slot = 0; slot < VOICE_SLOTS; slot++) {
        VoiceSlot* voice = &sVoiceSlots[slot];
        const VoiceSample* sample = voice->sample >= 0 ? &sVoiceSamples[voice->sample] : NULL;
        for (u32 frame = 0; sample != NULL && frame < frameCount; frame++) {
            s32 index = (s32)voice->position;
            if (sample->samples == NULL || index >= sample->count) {
                voice->sample = -1;
                break;
            }
            s32 value = sample->samples[index];
            samples[frame * 2 + 0] = (s16)CLAMP(samples[frame * 2 + 0] + value, -32768, 32767);
            samples[frame * 2 + 1] = (s16)CLAMP(samples[frame * 2 + 1] + value, -32768, 32767);
            voice->position += VOICE_ADVANCE;
        }
    }
}

static bool IsHoldingOcarina(Player* player) {
    return player->rightHandType == PLAYER_MODELTYPE_RH_INSTRUMENT;
}

// Runs on the audio thread, so it only records the edge; the pose consumes it on the game frame.
static void NoteStruck(void) {
    u8 note = sCurOcarinaPitch;

    if (note != OCARINA_NOTE_NONE && note != sLastNote) {
        sWasNoteStruck = true;
    }
    sLastNote = note;
}

static bool IsPlayingOcarinaClip(Player* player, const char* name) {
    const char* anim = (const char*)player->skelAnime.animation;

    return anim != NULL && strstr(anim, name) != NULL;
}

static bool SkipOcarinaIntro(PlayState* play, Player* player) {
    PlayerAnimationHeader* sing = LoadWholeClip(CLIP_SING);

    if (sing == NULL || !IsPlayingOcarinaClip(player, "okarina_start")) {
        return false;
    }
    PlayerAnimation_Change(play, &player->skelAnime, sing, 1.0f, 0.0f, 0.0f, ANIMMODE_ONCE, -6.0f);
    return true;
}

// Malon's song, parked between notes: each note sings on from where the last one stopped. It loops: the
// ocarina action reopens the ocarina whenever a clip reports its end, which would wipe the song's notes.
static void PoseSinging(PlayState* play, Player* player) {
    PlayerAnimationHeader* sing = LoadWholeClip(CLIP_SING);

    if (sing == NULL) {
        return;
    }
    f32 last = Animation_GetLastFrame(sing);
    f32 singEnd = last - CLIP_FRAMES_PER_UPDATE;
    if (player->skelAnime.animation != sing || player->skelAnime.endFrame < last) {
        PlayerAnimation_Change(play, &player->skelAnime, sing, 0.0f, 0.0f, last, ANIMMODE_LOOP, -6.0f);
    }
    if (sWasNoteStruck) {
        if (player->skelAnime.curFrame >= singEnd) {
            player->skelAnime.curFrame = 0.0f;
        }
        player->skelAnime.playSpeed = 1.0f;
    } else if (player->skelAnime.curFrame >= singEnd) {
        player->skelAnime.playSpeed = 0.0f;
    }
    sWasNoteStruck = false;
}

static void ReleaseVoiceInstrument(void) {
    if (sIsInstrumentLent) {
        AudioOcarina_SetInstrument(OCARINA_INSTRUMENT_OFF);
        sIsInstrumentLent = false;
    }
}

static void UpdateOcarinaVoice(PlayState* play, Player* player) {
    bool isSessionOpen = play->msgCtx.msgMode != MSGMODE_NONE;

    if (IsHoldingOcarina(player)) {
        sIsOcarinaOut = true;
    } else if (!isSessionOpen) {
        ReleaseVoiceInstrument();
        sIsOcarinaOut = false;
    }
    if (!sIsOcarinaOut || SkipOcarinaIntro(play, player) || !isSessionOpen) {
        return;
    }
    AudioOcarina_SetInstrument(OCARINA_INSTRUMENT_FEMALE_VOICE);
    sIsInstrumentLent = true;
    PoseSinging(play, player);
}

static void HideOcarina(Player* player, s32 limbIndex, Gfx** dList, Gfx* limbDList, Vec3f* pos) {
    if (!IsGerudo() || limbIndex != PLAYER_LIMB_RIGHT_HAND || !sIsOcarinaOut || !IsHoldingOcarina(player)) {
        return;
    }
    *dList = (Gfx*)sEmptyHandDL;
}

static void ResolveGerudoLimb(Player* player, s32 limbIndex, Gfx** dList, Gfx* limbDList, Vec3f* pos) {
    DrawScimitars(player, limbIndex, dList, limbDList, pos);
    HideOcarina(player, limbIndex, dList, limbDList, pos);
}

static s16 GetRageBarY(void) {
    SavePlayerData* playerData = &gSaveContext.save.saveInfo.playerData;
    s16 y = playerData->healthCapacity > TWO_HEART_ROWS_HEALTH ? RAGE_BAR_Y_TWO_HEART_ROWS : RAGE_BAR_Y_ONE_HEART_ROW;

    return playerData->magicLevel != 0 ? y + RAGE_BAR_BELOW_MAGIC : y;
}

// The magic meter's own frame and fill textures, so the rage reads as a second meter.
static void DrawRageMeter(PlayState* play) {
    if (!IsGerudo()) {
        return;
    }
    InterfaceContext* interfaceCtx = &play->interfaceCtx;
    s16 width = gSaveContext.save.saveInfo.playerData.magicLevel >= 2 ? MAGIC_DOUBLE_METER : MAGIC_NORMAL_METER;
    s16 barY = GetRageBarY();
    s16 rightEnd = RAGE_BAR_MID_LEFT + width;
    s16 fillWidth = (s16)CLAMP((f32)sRageMeter / GetRageCapacity() * width, 0.0f, (f32)width);
    bool isReady = sRageMeter >= GetRageCapacity() / 4;
    u8 pulse = (u8)(200 + 55 * Math_SinS(play->gameplayFrames * 3000));

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL39_Overlay(play->state.gfxCtx);
    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 255, 255, interfaceCtx->magicAlpha);
    gDPSetEnvColor(OVERLAY_DISP++, 100, 50, 50, 255);
    OVERLAY_DISP =
        Gfx_DrawTexRectIA8(OVERLAY_DISP, (TexturePtr)gMagicMeterEndTex, 8, 16, RAGE_BAR_LEFT, barY, 8, 16, 1 << 10, 1 << 10);
    OVERLAY_DISP = Gfx_DrawTexRectIA8(OVERLAY_DISP, (TexturePtr)gMagicMeterMidTex, 24, 16, RAGE_BAR_MID_LEFT, barY, width,
                                      16, 1 << 10, 1 << 10);
    gDPLoadTextureBlock(OVERLAY_DISP++, gMagicMeterEndTex, G_IM_FMT_IA, G_IM_SIZ_8b, 8, 16, 0, G_TX_MIRROR | G_TX_WRAP,
                        G_TX_NOMIRROR | G_TX_WRAP, 3, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gSPWideTextureRectangle(OVERLAY_DISP++, rightEnd << 2, barY << 2, (rightEnd + 8) << 2, (barY + 16) << 2,
                            G_TX_RENDERTILE, 256, 0, 1 << 10, 1 << 10);
    gDPPipeSync(OVERLAY_DISP++);
    gDPSetCombineLERP(OVERLAY_DISP++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, 0, 0, 0, PRIMITIVE, PRIMITIVE,
                      ENVIRONMENT, TEXEL0, ENVIRONMENT, 0, 0, 0, PRIMITIVE);
    gDPSetEnvColor(OVERLAY_DISP++, 0, 0, 0, 255);
    if (isReady) {
        gDPSetPrimColor(OVERLAY_DISP++, 0, 0, pulse, pulse, 255, interfaceCtx->magicAlpha);
    } else {
        gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 240, 140, 20, interfaceCtx->magicAlpha);
    }
    gDPLoadTextureBlock_4b(OVERLAY_DISP++, gMagicMeterFillTex, G_IM_FMT_I, 16, 16, 0, G_TX_NOMIRROR | G_TX_WRAP,
                           G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    if (fillWidth > 0) {
        gSPWideTextureRectangle(OVERLAY_DISP++, RAGE_BAR_MID_LEFT << 2, (barY + RAGE_BAR_FILL_TOP) << 2,
                                (RAGE_BAR_MID_LEFT + fillWidth) << 2, (barY + RAGE_BAR_FILL_BOTTOM) << 2,
                                G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

#define CONE_RADIUS 1875.0f
#define CONE_HEIGHT 900.0f
#define CONE_ARC 0x5555
#define CONE_SEGMENTS 10
#define CONE_VERTEX_COUNT ((CONE_SEGMENTS + 1) * 2)
#define CONE_ARM_CAP 24
#define CONE_RAGE_HITS 2
#define CONE_FOLLOW_FRAMES 8
// Every En_M_Thunder list is drawn under this scale; subtype 0 is its private ENMTHUNDER_SUBTYPE_SPIN_GREAT.
#define THUNDER_UNIT 0.02f
#define THUNDER_SUBTYPE_SPIN_GREAT 0

static EnMThunder* sCone;
static u8 sConeWaitFrames;
static bool sIsConeArmed;

// En_M_Thunder sets its target scale only on the release, so a non-zero one tells a release from a charge.
static bool IsThunderReleased(EnMThunder* thunder) {
    return thunder->scaleTarget != 0;
}

static bool IsReleaseSwinging(void) {
    return sMove == GERUDO_MOVE_SWING && sRow == ROW_CHARGE_RELEASE;
}

static bool ShouldArmCone(Player* player) {
    return !IsReleaseSwinging() || player->skelAnime.curFrame >= CHARGE_FAST_BEGIN || sConeWaitFrames >= CONE_ARM_CAP;
}

static void HoldCone(EnMThunder* thunder) {
    thunder->lightColorFrac = 1.0f;
    thunder->alphaFrac = 0.0f;
    thunder->timer = CONE_FOLLOW_FRAMES;
    Actor_SetScale(&thunder->actor, 0.0f);
    thunder->collider.base.atFlags &= ~AT_ON;
    sConeWaitFrames++;
}

// Runs after the actor submitted its ring collider; the AT pass reads it later in the frame, so it hits as the cone.
static void ShapeConeCollider(EnMThunder* thunder) {
    f32 radius = CONE_RADIUS * THUNDER_UNIT * thunder->actor.scale.x;
    s16 facing = thunder->actor.shape.rot.y + 0x8000;

    thunder->collider.base.atFlags |= AT_ON;
    thunder->collider.dim.radius = (s16)(radius * 0.62f);
    thunder->collider.dim.height = 70;
    thunder->collider.dim.yShift = -35;
    Collider_UpdateCylinder(&thunder->actor, &thunder->collider);
    thunder->collider.dim.pos.x += (s16)(Math_SinS(facing) * radius * 0.55f);
    thunder->collider.dim.pos.z += (s16)(Math_CosS(facing) * radius * 0.55f);
}

// The charge release becomes a forward cone, held back until the swing reaches its fast cross.
static void ShapeChargeRelease(Actor* actor) {
    EnMThunder* thunder = (EnMThunder*)actor;

    if (!IsGerudo() || gPlayState == NULL || !IsThunderReleased(thunder)) {
        return;
    }
    if (thunder != sCone) {
        if (!IsReleaseSwinging()) {
            return;
        }
        sCone = thunder;
        sConeWaitFrames = 0;
        sIsConeArmed = false;
    }
    if (!sIsConeArmed && !ShouldArmCone(GET_PLAYER(gPlayState))) {
        HoldCone(thunder);
        return;
    }
    sIsConeArmed = true;
    ShapeConeCollider(thunder);
}

static void ForgetCone(Actor* actor) {
    if (sCone != NULL && actor == &sCone->actor) {
        sCone = NULL;
    }
}

static void PayConeRage(Actor* victim, ColliderElement* attack, f32* damage) {
    if (sCone != NULL && attack == &sCone->collider.elem && IsRageWorthy(victim)) {
        GainRage(CONE_RAGE_HITS);
    }
}

static void SetConeVertex(Vtx* vertex, s16 x, s16 y, s16 z, s16 u, s16 v) {
    vertex->v.ob[0] = x;
    vertex->v.ob[1] = y;
    vertex->v.ob[2] = z;
    vertex->v.flag = 0;
    vertex->v.tc[0] = u;
    vertex->v.tc[1] = v;
    vertex->v.cn[0] = 255;
    vertex->v.cn[1] = 255;
    vertex->v.cn[2] = 255;
    vertex->v.cn[3] = 255;
}

// Two tiles across the arc on the ring's own scroll; tc is s10.5, so the endless counter is wrapped into one tile.
static Vtx* BuildConeWall(PlayState* play, f32 scroll) {
    Vtx* vertices = GRAPH_ALLOC(play->state.gfxCtx, sizeof(Vtx) * CONE_VERTEX_COUNT);
    f32 wrapped = scroll - 64.0f * floorf(scroll / 64.0f);

    if (vertices == NULL) {
        return NULL;
    }
    for (s32 i = 0; i <= CONE_SEGMENTS; i++) {
        f32 t = (f32)i / CONE_SEGMENTS;
        s16 angle = (s16)(-(CONE_ARC / 2) + (s32)(CONE_ARC * t));
        s16 x = (s16)(Math_SinS(angle) * CONE_RADIUS);
        s16 z = (s16)(Math_CosS(angle) * CONE_RADIUS);
        s16 u = (s16)((t * 2.0f * 64.0f + wrapped) * 32.0f);

        SetConeVertex(&vertices[i * 2], x, (s16)(-CONE_HEIGHT / 2), z, u, 32 * 32);
        SetConeVertex(&vertices[i * 2 + 1], x, (s16)(CONE_HEIGHT / 2), z, u, 0);
    }
    return vertices;
}

// gameplay_keep exports no symbol for the ring's texture, so its lists run squashed to a point first: nothing is
// rasterised, but they leave the ring's texture, tiles and combiner loaded for the arc drawn after them.
static void DrawCone(EnMThunder* thunder, PlayState* play) {
    Vtx* vertices = BuildConeWall(play, thunder->scroll);
    bool isGreat = thunder->subtype == THUNDER_SUBTYPE_SPIN_GREAT;
    u8 alpha = (u8)(thunder->alphaFrac * 255.0f);

    if (vertices == NULL) {
        return;
    }
    Gfx* scrollDL = Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 0xFF - ((u16)(s32)(thunder->scroll * 30.0f) & 0xFF), 0,
                                       64, 32, 1, 0xFF - ((u16)(s32)(thunder->scroll * 20.0f) & 0xFF), 0, 8, 8, -30, 0,
                                       -20, 0);
    Gfx* ringWall = (Gfx*)(isGreat ? gGreatSpinAttackDiskDL : gSpinAttackDiskDL);
    Gfx* ringRim = (Gfx*)(isGreat ? gGreatSpinAttackCylinderDL : gSpinAttackCylinderDL);

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    gSPSegment(POLY_XLU_DISP++, 0x08, (uintptr_t)scrollDL);
    if (isGreat) {
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 255, 255, 170, alpha);
    } else {
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 170, 255, 255, alpha);
    }
    Matrix_Push();
    Matrix_Scale(0.0f, 0.0f, 0.0f, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, ringWall);
    gSPDisplayList(POLY_XLU_DISP++, ringRim);
    Matrix_Pop();

    Matrix_Scale(THUNDER_UNIT, THUNDER_UNIT, THUNDER_UNIT, MTXMODE_APPLY);
    Matrix_RotateYF(M_PIf, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPClearGeometryMode(POLY_XLU_DISP++, G_LIGHTING | G_TEXTURE_GEN | G_TEXTURE_GEN_LINEAR | G_CULL_BOTH);
    gSPVertex(POLY_XLU_DISP++, (uintptr_t)vertices, CONE_VERTEX_COUNT, 0);
    for (s32 i = 0; i < CONE_SEGMENTS; i++) {
        s32 bottom = i * 2;
        gSP2Triangles(POLY_XLU_DISP++, bottom, bottom + 1, bottom + 2, 0, bottom + 1, bottom + 3, bottom + 2, 0);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawChargeRelease(Actor* actor, bool* shouldDraw) {
    if (sCone == NULL || actor != &sCone->actor) {
        return;
    }
    *shouldDraw = false;
    if (sIsConeArmed) {
        DrawCone(sCone, gPlayState);
    }
}

static void ResetMoves(PlayState* play, Player* player) {
    if (player != NULL && play != NULL && IsGerudoBusy(player)) {
        EndMove(play, player);
    }
    sMove = GERUDO_MOVE_NONE;
    sBladeMask = 0;
    sIsCylinderOn = false;
    sBladeVisibility = -1;
    sIsAPending = false;
    sIsSprinting = false;
    sComboStep = 0;
    sComboIdleFrames = 0;
}

static void EnterGerudo(PlayState* play, Player* player) {
    ResetMoves(play, player);
}

static void ExitGerudo(PlayState* play, Player* player) {
    ResetMoves(play, player);
    RestoreFuryLights(play);
    sRageMeter = 0;
    if (sIsOcarinaOut) {
        ReleaseVoiceInstrument();
        sIsOcarinaOut = false;
    }
    if (sTrailRight >= 0) {
        Effect_Destroy(play, sTrailRight);
        sTrailRight = -1;
    }
}

static void DropInterruptedMove(PlayState* play, Player* player) {
    if (sMove == GERUDO_MOVE_NONE || IsGerudoBusy(player)) {
        return;
    }
    if (sMove == GERUDO_MOVE_FURY) {
        RestoreFuryLights(play);
    }
    sMove = GERUDO_MOVE_NONE;
    sBladeVisibility = -1;
    StopAllHits(play, player);
    StopRightTrail();
}

static void UpdateGerudo(PlayState* play, Player* player) {
    DropInterruptedMove(play, player);
    FadeImpactDim(play);
    CountComboGap(player);
    TrackA(play, player);
    TryFury(play, player);
    TryAerial(play, player);
    SubmitCylinder(play, player);
    UpdateOcarinaVoice(play, player);
}

static void ForgetScene(s16 sceneId, s8 spawnNum) {
    sIsCylinderReady = false;
    sTrailRight = -1;
    sFuryPrevFrame = -1.0f;
    sCone = NULL;
    ResetMoves(NULL, NULL);
}

static bool CanWearMask(Player* player, PlayState* play) {
    return IsGrounded(player) &&
           !(player->stateFlags1 & (STATE1_SWIMMING | STATE1_FIRST_PERSON | PLAYER_STATE1_20000000));
}

static void WearMask(PlayState* play, Player* player) {
    sApi->ToggleForm(GERUDO_FORM_KEY);
}

static const S2HCustomItemRandomizer sRandomizerItem = {
    sizeof(S2HCustomItemRandomizer), 0, S2H_CUSTOM_ITEM_TYPE_ITEM, 0, 1, GERUDO_MASK_KEY, GERUDO_MASK_KEY, "",
};

static void DrawMaskPickup(PlayState* play, S2HGetItemEntry* entry) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL26_Opa(play->state.gfxCtx);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sGetItemDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void RegisterMask(void) {
    S2HCustomItemDefinition mask = Z64Items_Define(GERUDO_MASK_KEY, sIconTex, sNameTex);

    Z64Items_SetButtons(&mask, S2H_CUSTOM_ITEM_C_BUTTON | S2H_CUSTOM_ITEM_DPAD);
    mask.flags |= S2H_CUSTOM_ITEM_INSTANT | S2H_CUSTOM_ITEM_WEARABLE;
    Z64Items_SetPlacement(&mask, GERUDO_MASK_PAGE, GERUDO_MASK_SLOT, GERUDO_MASK_WHEEL_PRIORITY);
    mask.getItemEntry.drawFunc = DrawMaskPickup;
    Z64Items_SetTextbox(&mask, "You got the %rGerudo Mask%w!&The face of a desert warrior.^Wear it with %y\xB2%w to "
                               "become a %rGerudo%w: twin scimitars, a blade guard, and the fury of Urbosa.");
    Z64Items_SetPauseText(&mask, "%rGerudo Mask&%wPress %y\xB2%w to become a Gerudo.&%y\xB1%w: dual blades  Hold "
                                 "%y\xB0%w: sprint  %y\xB4%w+%y\xB1%w: Urbosa's Fury");
    Z64Items_SetCanUse(&mask, CanWearMask);
    Z64Items_SetAction(&mask, WearMask, NULL);
    Z64Items_SetLogic(&mask, &sRandomizerItem);
    Z64Items_Register(sApi, &mask);
}

static void ResolveGerudoStrength(bool* should, va_list args) {
    s32* strength = va_arg(args, s32*);

    if (IsGerudo()) {
        *strength = PLAYER_STRENGTH_ZORA;
    }
}

static const S2HFormActionOverride sActions[] = {
    { SOH_PLAYER_ACTION_MELEE, HandleSlash },
    { SOH_PLAYER_ACTION_ROLL, HandleA },
    { SOH_PLAYER_ACTION_ZTARGET_A, HandleZTargetA },
    { SOH_PLAYER_ACTION_SHIELD, HandleShield },
};

// No shield, and the scimitars always: a Link without a sword still fights with the Kokiri tier.
static u16 ResolveGerudoEquipment(s32 equipType, u16 value) {
    if (equipType == EQUIP_TYPE_SHIELD) {
        return EQUIP_VALUE_SHIELD_NONE;
    }
    if (equipType == EQUIP_TYPE_SWORD && value == EQUIP_VALUE_SWORD_NONE) {
        return EQUIP_VALUE_SWORD_KOKIRI;
    }
    return value;
}

static void RegisterForm(void) {
    S2HFormDefinition gerudo = { 0 };

    gerudo.structSize = sizeof(gerudo);
    gerudo.key = GERUDO_FORM_KEY;
    gerudo.label = "Gerudo";
    gerudo.kind = S2H_FORM_KIND_LINK;
    gerudo.item = GERUDO_MASK_KEY;
    gerudo.modelPath = GERUDO_MODEL_PATH;
    gerudo.motionScale = 1.0f;
    gerudo.transformMask = sMaskDL;
    gerudo.transformVoiceSfx = NA_SE_VO_LI_FALL_L;
    gerudo.actions = sActions;
    gerudo.actionCount = ARRAY_COUNT(sActions);
    gerudo.resolveEquipment = ResolveGerudoEquipment;
    gerudo.onEnter = EnterGerudo;
    gerudo.onExit = ExitGerudo;
    gerudo.update = UpdateGerudo;
    sApi->RegisterForm(&gerudo);
}

S2H_MOD_EXPORT void ModSetApi(const S2HModApi* api) {
    sApi = api;
}

S2H_MOD_EXPORT const S2HModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

static void GrantGerudoLogic(void) {
    static const Z64LogicSource sMask = { GERUDO_MASK_KEY };
    static const char* const sSlashing[] = { "CanUseSword", "CanHitSwitch", "CanAttack", "CanDamage", NULL };

    if (Z64Logic_Init(sApi)) {
        Z64Logic_GrantCapabilities(&sMask, sSlashing);
    }
}

S2H_MOD_EXPORT void ModInit(void) {
    for (s32 slot = 0; slot < VOICE_SLOTS; slot++) {
        sVoiceSlots[slot].sample = -1;
    }
    GrantGerudoLogic();
    LoadVoices();
    RegisterMask();
    RegisterForm();
    S2H_REGISTER_HOOK(sApi, OnPlayerResolveAnim, ResolveFighterAnim);
    S2H_REGISTER_HOOK_FOR_ID(sApi, OnPlayerResolveMotionScale, SOH_PLAYER_MOTION_STICK_SPEED, ScaleSprintAndRoll);
    S2H_REGISTER_HOOK(sApi, OnPlayerResolveLimbDraw, ResolveGerudoLimb);
    S2H_REGISTER_HOOK(sApi, OnPlayerPostLimbDraw, DrawAtHands);
    S2H_REGISTER_HOOK_FOR_ID(sApi, OnActorPlaySfx, ACTOR_PLAYER, SpeakAsGerudo);
    S2H_REGISTER_HOOK(sApi, OnInterfaceDrawEnd, DrawRageMeter);
    S2H_REGISTER_HOOK(sApi, OnSceneInit, ForgetScene);
    S2H_REGISTER_HOOK(sApi, OnOcarinaInput, NoteStruck);
    S2H_REGISTER_HOOK(sApi, OnCollisionResolveDamage, DealFuryDamage);
    S2H_REGISTER_HOOK(sApi, OnCollisionResolveDamage, PayConeRage);
    S2H_REGISTER_HOOK(sApi, OnPlayerResolveAnimSite, ResolveSiteAnim);
    S2H_REGISTER_HOOK_FOR_ID(sApi, OnActorUpdate, ACTOR_EN_M_THUNDER, ShapeChargeRelease);
    S2H_REGISTER_HOOK_FOR_ID(sApi, ShouldActorDraw, ACTOR_EN_M_THUNDER, DrawChargeRelease);
    S2H_REGISTER_HOOK_FOR_ID(sApi, OnActorDestroy, ACTOR_EN_M_THUNDER, ForgetCone);
    sApi->RegisterVB(VB_CHANGE_HELD_ITEM_AND_USE_ITEM, InterceptDraw);
    sApi->RegisterVB(VB_USE_STRENGTH_UPGRADE, ResolveGerudoStrength);
    if (S2H_MOD_API_HAS(sApi, RegisterAudioMixInGroup)) {
        sApi->RegisterAudioMixInGroup(MixVoices, S2H_AUDIO_GROUP_SFX, NULL);
    }
}
