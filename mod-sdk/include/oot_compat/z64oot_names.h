#ifndef OOT_COMPAT_Z64OOT_NAMES_H
#define OOT_COMPAT_Z64OOT_NAMES_H

// Shipwright (OoT) names on Majora's Mask, for mods written against Ship. translate_mod.py includes this after a
// mod's own includes, so a mod's prototypes of OoT functions are renamed with its calls.
// Only same-contract names live here; the rest are rewritten by the script or listed in its report.
#include <string.h>

#include "2s2h/ModApi/ModApi.h"
#include "2s2h/ModApi/CustomEquipRegistry/CustomEquipRegistry.h"
#include "z64items.h"
#include "soh_api_names.h"
#include "oot_ids.h"
#include "oot_rando.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "overlays/actors/ovl_En_Arrow/z_en_arrow.h"

#ifdef __cplusplus
extern "C" {
#endif
void Player_SetAction_PreserveItemAction(PlayState* play, Player* player, PlayerActionFunc actionFunc, s32 arg3);
void Player_StopHorizontalMovement(Player* player);
void Player_Anim_PlayOnce(PlayState* play, Player* player, PlayerAnimationHeader* anim);
void Player_Anim_PlayLoop(PlayState* play, Player* player, PlayerAnimationHeader* anim);
void Player_AnimSfx_PlayVoice(Player* player, u16 sfxId);
void func_8083B1A0(Player* player, PlayState* play);
#ifdef __cplusplus
}
#endif

// Signatures that differ go through a wrapper, so OoT-style prototypes in a mod still compile.
static inline void OotCompat_Player_PlaySfx(Actor* actor, u16 sfxId) {
    Player_PlaySfx((Player*)actor, sfxId);
}

static inline void* OotCompat_Graph_Alloc(GraphicsContext* gfxCtx, size_t size) {
    return GRAPH_ALLOC(gfxCtx, size);
}

static inline Mtx* OotCompat_Matrix_NewMtx(GraphicsContext* gfxCtx, const char* file, s32 line) {
    return Matrix_Finalize(gfxCtx);
}

// OoT's soft-sprite dust helpers are EffectSsDust_Spawn with draw flags 0 and 1.
static inline void OotCompat_SpawnDust(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel,
                                       Color_RGBA8* primColor, Color_RGBA8* envColor, s16 scale, s16 scaleStep,
                                       s16 life) {
    EffectSsDust_Spawn(play, 0, pos, velocity, accel, primColor, envColor, scale, scaleStep, life, 0);
}

static inline void OotCompat_SpawnDustLit(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel,
                                          Color_RGBA8* primColor, Color_RGBA8* envColor, s16 scale, s16 scaleStep,
                                          s16 life) {
    EffectSsDust_Spawn(play, 1, pos, velocity, accel, primColor, envColor, scale, scaleStep, life, 0);
}

// OoT passes colors as loose components and a sound flag; MM takes colors and a life.
static inline void OotCompat_EffectSsDeadDb_Spawn(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel,
                                                  s16 scale, s16 scaleStep, s16 primR, s16 primG, s16 primB,
                                                  s16 primA, s16 envR, s16 envG, s16 envB, s16 unused, s32 life,
                                                  s16 playSound) {
    Color_RGBA8 prim = { (u8)primR, (u8)primG, (u8)primB, (u8)primA };
    Color_RGBA8 env = { (u8)envR, (u8)envG, (u8)envB, 255 };
    EffectSsDeadDb_Spawn(play, pos, velocity, accel, &prim, &env, scale, scaleStep, life);
    if (playSound) {
        Audio_PlaySfx_AtPos(pos, NA_SE_EN_EXTINCT);
    }
}

static inline void OotCompat_EffectSsDeadDd_Spawn(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel,
                                                  s16 scale, s16 scaleStep, s16 primR, s16 primG, s16 primB,
                                                  s16 alpha, s16 envR, s16 envG, s16 envB, s16 alphaStep, s32 life) {
    Color_RGBA8 prim = { (u8)primR, (u8)primG, (u8)primB, (u8)alpha };
    Color_RGBA8 env = { (u8)envR, (u8)envG, (u8)envB, 255 };
    EffectSsDeadDd_Spawn(play, pos, velocity, accel, &prim, &env, scale, scaleStep, alphaStep, life);
}

// OoT's request type 5 adds magic; in MM 5 is the Goron/Zora drain and adding is Magic_Add.
#define MAGIC_ADD 5

static inline s32 OotCompat_Magic_RequestChange(PlayState* play, s16 amount, s16 type) {
    if (type == MAGIC_ADD) {
        Magic_Add(play, amount);
        return true;
    }
    return Magic_Consume(play, amount, type);
}

// Ship's helper: a flex skeleton is the one with display lists counted.
static inline void SkelAnime_DrawSkeletonOpa(PlayState* play, SkelAnime* skelAnime, OverrideLimbDrawOpa overrideLimbDraw,
                                             PostLimbDrawOpa postLimbDraw, void* arg) {
    if (skelAnime->dListCount != 0) {
        SkelAnime_DrawFlexOpa(play, skelAnime->skeleton, skelAnime->jointTable, skelAnime->dListCount,
                              overrideLimbDraw, postLimbDraw, (Actor*)arg);
    } else {
        SkelAnime_DrawOpa(play, skelAnime->skeleton, skelAnime->jointTable, overrideLimbDraw, postLimbDraw,
                          (Actor*)arg);
    }
}

static inline Mtx* OotCompat_Matrix_ToMtx(Mtx* dest, const char* file, s32 line) {
    return Matrix_ToMtx(dest);
}

// OoT's freeze flag only picked which drop table bit to set; MM keeps one.
static inline void OotCompat_Actor_SetDropFlag(Actor* actor, ColliderElement* elem, s32 freezeFlag) {
    Actor_SetDropFlag(actor, elem);
}

static inline void OotCompat_Actor_SetDropFlagJntSph(Actor* actor, ColliderJntSph* jntSph, s32 freezeFlag) {
    Actor_SetDropFlagJntSph(actor, jntSph);
}

static inline void OotCompat_Camera_SetCameraData(Camera* camera, s16 setDataFlags, void* data0, void* data1,
                                                  s16 data2, s16 data3, s32 unused) {
    Camera_SetCameraData(camera, setDataFlags, data0, data1, data2, data3);
}

// OoT mods pass GI ids as plain ints; MM takes the enum, which C++ will not convert implicitly.
static inline s32 OotCompat_Actor_OfferGetItem(Actor* actor, PlayState* play, s32 getItemId, f32 xzRange,
                                               f32 yRange) {
    return Actor_OfferGetItem(actor, play, (GetItemId)getItemId, xzRange, yRange);
}

static inline void OotCompat_Matrix_RotateX(f32 x, u8 mode) {
    if (mode == MTXMODE_APPLY) {
        Matrix_RotateXFApply(x);
    } else {
        Matrix_RotateXFNew(x);
    }
}

#define EffectSsDeadDb_Spawn OotCompat_EffectSsDeadDb_Spawn
#define EffectSsDeadDd_Spawn OotCompat_EffectSsDeadDd_Spawn
#define LinkAnimationHeader PlayerAnimationHeader
// Same layouts; translate_mod.py renames their members.
#define SkelAnimeCurve SkelCurve
#define SkelCurveLimbList CurveSkeletonHeader
#define TransformUpdateIndex CurveAnimationHeader
#define TransformData CurveInterpKnot
// Same header; the fields mods read (sampleAddr, size) keep their names.
#define SoundFontSample Sample

#define ColliderInfo ColliderElement
#define ColliderInfoInit ColliderElementInit
#define ColliderTouch ColliderElementDamageInfoAT
#define ColliderBump ColliderElementDamageInfoAC
#define ColliderBumpInit ColliderElementDamageInfoACInit

#define Player_SetupAction Player_SetAction
#define Player_SetupActionPreserveItemAction Player_SetAction_PreserveItemAction
#define Player_ZeroSpeedXZ Player_StopHorizontalMovement
#define Player_AnimPlayOnce Player_Anim_PlayOnce
#define Player_AnimPlayLoop Player_Anim_PlayLoop
#define Player_PlayVoiceSfx Player_AnimSfx_PlayVoice
#define Player_PlaySfx OotCompat_Player_PlaySfx
// Return to idle or Z-target stance.
#define func_80839FFC func_8083B1A0

#define LinkAnimation_Update PlayerAnimation_Update
#define LinkAnimation_Change PlayerAnimation_Change
#define LinkAnimation_PlayOnce PlayerAnimation_PlayOnce
#define LinkAnimation_PlayLoop PlayerAnimation_PlayLoop
#define AnimationContext_SetLoadFrame AnimTaskQueue_AddLoadPlayerFrame
#define AnimationContext_SetCopyAll AnimTaskQueue_AddCopy
#define AnimationContext_SetCopyTrue AnimTaskQueue_AddCopyUsingMap
#define AnimationContext_SetCopyFalse AnimTaskQueue_AddCopyUsingMapInverted
#define AnimationContext_SetInterp AnimTaskQueue_AddInterp
#define AnimationContext_SetMoveActor AnimTaskQueue_AddActorMovement
#define AnimationContext_SetNextQueue AnimTaskQueue_SetNextGroup
#define AnimationContext_DisableQueue AnimTaskQueue_DisableTransformTasksForGroup
#define AnimationContext_Reset AnimTaskQueue_Reset
#define AnimationContext_Update AnimTaskQueue_Update
#define Play_CameraChangeSetting Play_ChangeCameraSetting
#define Play_CameraSetAtEye Play_SetCameraAtEye
#define Play_CameraSetAtEyeUp Play_SetCameraAtEyeUp
#define Play_CameraSetFov Play_SetCameraFov

#define MATRIX_NEWMTX(gfxCtx) Matrix_Finalize(gfxCtx)
#define Matrix_NewMtx OotCompat_Matrix_NewMtx
#define Matrix_RotateX OotCompat_Matrix_RotateX
#define Matrix_RotateY Matrix_RotateYF
#define Matrix_RotateZ Matrix_RotateZF
// OoT's Math_Atan2S takes (x, y); MM's takes (y, x) and keeps the OoT order as Math_Atan2S_XY.
#define Math_Atan2S Math_Atan2S_XY

#define Gfx_SetupDL_25Opa Gfx_SetupDL25_Opa
#define Gfx_SetupDL_25Xlu Gfx_SetupDL25_Xlu
#define Gfx_SetupDL_39Overlay Gfx_SetupDL39_Overlay
#define Graph_Alloc OotCompat_Graph_Alloc

#define Audio_PlayActorSound2 Actor_PlaySfx
#define Audio_PlaySfxGeneral AudioSfx_PlaySfx
#define Audio_StopSfxById AudioSfx_StopById
#define Sfx_PlaySfxCentered Audio_PlaySfx

#define PLAYER_STATE1_FIRST_PERSON PLAYER_STATE1_100000
#define PLAYER_STATE1_IN_WATER PLAYER_STATE1_8000000

#define PLAYER_BODYPART_R_THIGH PLAYER_BODYPART_RIGHT_THIGH
#define PLAYER_BODYPART_R_SHIN PLAYER_BODYPART_RIGHT_SHIN
#define PLAYER_BODYPART_R_FOOT PLAYER_BODYPART_RIGHT_FOOT
#define PLAYER_BODYPART_L_THIGH PLAYER_BODYPART_LEFT_THIGH
#define PLAYER_BODYPART_L_SHIN PLAYER_BODYPART_LEFT_SHIN
#define PLAYER_BODYPART_L_FOOT PLAYER_BODYPART_LEFT_FOOT
#define PLAYER_BODYPART_L_SHOULDER PLAYER_BODYPART_LEFT_SHOULDER
#define PLAYER_BODYPART_L_FOREARM PLAYER_BODYPART_LEFT_FOREARM
#define PLAYER_BODYPART_L_HAND PLAYER_BODYPART_LEFT_HAND
#define PLAYER_BODYPART_R_SHOULDER PLAYER_BODYPART_RIGHT_SHOULDER
#define PLAYER_BODYPART_R_FOREARM PLAYER_BODYPART_RIGHT_FOREARM
#define PLAYER_BODYPART_R_HAND PLAYER_BODYPART_RIGHT_HAND

#define ITEM00_HEART ITEM00_RECOVERY_HEART
#define ITEM00_MAGIC_SMALL ITEM00_MAGIC_JAR_SMALL
#define ITEM00_MAGIC_LARGE ITEM00_MAGIC_JAR_BIG
#define ITEM00_NUTS ITEM00_DEKU_NUTS_1
#define ITEM00_STICK ITEM00_DEKU_STICK


#define Actor_MoveXZGravity Actor_MoveWithGravity
#define Effect_Delete Effect_Destroy
#define Magic_RequestChange OotCompat_Magic_RequestChange

// Ship keeps B, the three C buttons and the D-pad (up, down, left, right) in one array; MM gives B one item per
// form, shares the C buttons (form 0) and stores the D-pad apart in EQUIP_SLOT_D_* order. translate_mod.py turns
// equips.buttonItems[i] / cButtonSlots[i] into these, which work as lvalues.
static inline u8* OotCompat_ButtonItem(s32 button) {
    static const u8 sDpadSlots[4] = { EQUIP_SLOT_D_UP, EQUIP_SLOT_D_DOWN, EQUIP_SLOT_D_LEFT, EQUIP_SLOT_D_RIGHT };
    if (button >= 4) {
        return &DPAD_BUTTON_ITEM_EQUIP(0, sDpadSlots[(button - 4) & 3]);
    }
    return &BUTTON_ITEM_EQUIP((button == EQUIP_SLOT_B) ? CUR_FORM : 0, button);
}

static inline u8* OotCompat_ButtonSlot(s32 cButton) {
    static const u8 sDpadSlots[4] = { EQUIP_SLOT_D_UP, EQUIP_SLOT_D_DOWN, EQUIP_SLOT_D_LEFT, EQUIP_SLOT_D_RIGHT };
    if (cButton >= 3) {
        return &DPAD_SLOT_EQUIP(0, sDpadSlots[(cButton - 3) & 3]);
    }
    return &C_SLOT_EQUIP(0, cButton + 1);
}

// MM has no child/adult: an age-restricted OoT item is a human-form item.
typedef enum {
    SOH_CUSTOM_ITEM_AGE_ANY = 0,
    SOH_CUSTOM_ITEM_AGE_CHILD = 1,
    SOH_CUSTOM_ITEM_AGE_ADULT = 2,
} SOHCustomItemAge;

// A constant expression, so translated static initializers (.ageRequirement = X → .formMask = ...) still compile.
#define OOT_COMPAT_AGE_FORM_MASK(age) (((age) == SOH_CUSTOM_ITEM_AGE_ANY) ? 0 : S2H_CUSTOM_ITEM_FORM_BIT(PLAYER_FORM_HUMAN))

static inline void Z64Items_SetAge(S2HCustomItemDefinition* definition, SOHCustomItemAge age) {
    definition->formMask = OOT_COMPAT_AGE_FORM_MASK(age);
}

#define func_8002836C OotCompat_SpawnDust
#define func_800283D4 OotCompat_SpawnDustLit
#define EffectSsKiraKira_SpawnSmallYellow EffectSsKirakira_SpawnSmallYellow
#define EffectSsKiraKira_SpawnSmall EffectSsKirakira_SpawnSmall
#define EffectSsKiraKira_SpawnDispersed EffectSsKirakira_SpawnDispersed
#define EffectSsKiraKira_SpawnFocused EffectSsKirakira_SpawnFocused
// Player knockback: same arguments, and OoT's hit responses keep their numbers in MM (1 large, 2 small, 3 frozen,
// 4 electrified).
#define func_80837C0C func_80833B18
#define PLAYER_HIT_RESPONSE_NONE 0
#define PLAYER_HIT_RESPONSE_KNOCKBACK_LARGE 1
#define PLAYER_HIT_RESPONSE_KNOCKBACK_SMALL 2
#define PLAYER_HIT_RESPONSE_FROZEN 3
#define PLAYER_HIT_RESPONSE_ELECTRIFIED 4
// Miniboss music: play over the area's BGM, then bring it back.
#define func_800F5ACC Audio_PlayBgm_StorePrevBgm
#define func_800F5B58 Audio_RestorePrevBgm
#define func_8002F8F0 Actor_PlaySfx_Flagged
#define func_8002F974 Actor_PlaySfx_Flagged2
#define Actor_DisableLens Actor_DeactivateLens
#define Quake_Add Quake_Request
#define Quake_SetQuakeValues Quake_SetPerturbations
#define Quake_SetCountdown Quake_SetDuration
#define Gfx_SetupDL_27Xlu Gfx_SetupDL27_Xlu
#define SEGMENTED_TO_VIRTUAL(x) Lib_SegmentedToVirtual((void*)(x))
#define Matrix_ToMtx OotCompat_Matrix_ToMtx
#define MATRIX_TOMTX(mtx) OotCompat_Matrix_ToMtx(mtx, __FILE__, __LINE__)
#define BgCheck_EntityRaycastFloor5 BgCheck_EntityRaycastFloor5_2
#define Actor_OfferGetItem OotCompat_Actor_OfferGetItem
#define Actor_SetDropFlag OotCompat_Actor_SetDropFlag
#define Actor_SetDropFlagJntSph OotCompat_Actor_SetDropFlagJntSph
#define Camera_SetCameraData OotCompat_Camera_SetCameraData
// OoT room behaviours are MM's room types and environments, in the same order.
#define ROOM_BEHAVIOR_TYPE1_0 ROOM_TYPE_NORMAL
#define ROOM_BEHAVIOR_TYPE1_1 ROOM_TYPE_DUNGEON
#define ROOM_BEHAVIOR_TYPE1_2 ROOM_TYPE_INDOORS
#define ROOM_BEHAVIOR_TYPE1_3 ROOM_TYPE_3
#define ROOM_BEHAVIOR_TYPE1_4 ROOM_TYPE_4
#define ROOM_BEHAVIOR_TYPE1_5 ROOM_TYPE_BOSS
#define ROOM_BEHAVIOR_TYPE2_0 ROOM_ENV_DEFAULT
#define ROOM_BEHAVIOR_TYPE2_1 ROOM_ENV_COLD
#define ROOM_BEHAVIOR_TYPE2_2 ROOM_ENV_WARM
#define ROOM_BEHAVIOR_TYPE2_3 ROOM_ENV_HOT
#define ROOM_BEHAVIOR_TYPE2_4 ROOM_ENV_UNK_STRETCH_1
#define ROOM_BEHAVIOR_TYPE2_5 ROOM_ENV_UNK_STRETCH_2
#define ROOM_BEHAVIOR_TYPE2_6 ROOM_ENV_UNK_STRETCH_3
// Ship's trail colour types: MM's blure has no such field, translate_mod.py drops the initializers; other uses
// still need a value.
#define TRAIL_TYPE_REST 0
#define TRAIL_TYPE_SWORDS 1
#define TRAIL_TYPE_BOOMERANG 2
#define TRAIL_TYPE_BOMBCHU 3
#define TRAIL_TYPE_KOKIRI_SWORD 4
#define TRAIL_TYPE_MASTER_SWORD 5
#define TRAIL_TYPE_BIGGORON_SWORD 6
#define TRAIL_TYPE_STICK 7
#define TRAIL_TYPE_HAMMER 8
// Player.unk_B75 is OoT's knockbackType; MM inserts a kind at 1, so OoT's small, large and electrified are 2-4
// (what MM's own small and large helpers pass).
#define PLAYER_KNOCKBACK_NONE 0
#define PLAYER_KNOCKBACK_SMALL 2
#define PLAYER_KNOCKBACK_LARGE 3
#define PLAYER_KNOCKBACK_LARGE_ELECTRIFIED 4

static inline void OotCompat_Actor_SetPlayerKnockback(PlayState* play, Actor* actor, f32 speed, s16 yaw,
                                                      f32 yVelocity, u32 type, u32 damage) {
    func_800B8D10(play, actor, speed, yaw, yVelocity, (type == 0) ? 0 : (s32)type + 1, damage);
}

#define Actor_SetPlayerKnockback OotCompat_Actor_SetPlayerKnockback
#define Actor_SetPlayerKnockbackLarge func_800B8D50
#define Actor_SetPlayerKnockbackLargeNoDamage func_800B8D98
#define Actor_SetPlayerKnockbackSmall func_800B8DD4
#define Actor_SetPlayerKnockbackSmallNoDamage func_800B8E1C
#define EffectSsHitMark_SpawnFixedScale EffectSsHitmark_SpawnFixedScale
#define EffectSsHitMark_SpawnCustomScale EffectSsHitmark_SpawnCustomScale
#define EffectSsHitMark_Spawn EffectSsHitmark_Spawn
// z_effect.c's private "no effect" index: sparks, blures, shield particles and MM's tire marks.
#define TOTAL_EFFECT_COUNT (3 + 25 + 3 + 15)
// Hand and sheath models by role. They index MM's model tables, so every name lands on a real MM entry.
#define PLAYER_MODELTYPE_LH_SWORD PLAYER_MODELTYPE_LH_ONE_HAND_SWORD
#define PLAYER_MODELTYPE_LH_SWORD_2 PLAYER_MODELTYPE_LH_4
#define PLAYER_MODELTYPE_LH_BGS PLAYER_MODELTYPE_LH_TWO_HAND_SWORD
#define PLAYER_MODELTYPE_LH_HAMMER PLAYER_MODELTYPE_LH_CLOSED
#define PLAYER_MODELTYPE_LH_BOOMERANG PLAYER_MODELTYPE_LH_CLOSED
#define PLAYER_MODELTYPE_RH_BOW_SLINGSHOT PLAYER_MODELTYPE_RH_BOW
#define PLAYER_MODELTYPE_RH_BOW_SLINGSHOT_2 PLAYER_MODELTYPE_RH_BOW
#define PLAYER_MODELTYPE_RH_OCARINA PLAYER_MODELTYPE_RH_INSTRUMENT
#define PLAYER_MODELTYPE_RH_OOT PLAYER_MODELTYPE_RH_INSTRUMENT
#define PLAYER_MODELTYPE_SHEATH_16 PLAYER_MODELTYPE_SHEATH_12
#define PLAYER_MODELTYPE_SHEATH_17 PLAYER_MODELTYPE_SHEATH_13
#define PLAYER_MODELTYPE_SHEATH_18 PLAYER_MODELTYPE_SHEATH_14
#define PLAYER_MODELTYPE_SHEATH_19 PLAYER_MODELTYPE_SHEATH_15
#define BOMB_BODY BOMB_TYPE_BODY
#define BOMB_EXPLOSION BOMB_TYPE_EXPLOSION
#define HIT_SPECIAL_EFFECT_NONE 0
#define HIT_SPECIAL_EFFECT_FIRE 1
#define HIT_SPECIAL_EFFECT_ICE 2
#define HIT_SPECIAL_EFFECT_ELECTRIC 3
#define HIT_SPECIAL_EFFECT_KNOCKBACK 4
#define CAM_STAT_CUT CAM_STATUS_CUT
#define CAM_STAT_WAIT CAM_STATUS_WAIT
#define CAM_STAT_UNK3 CAM_STATUS_UNK3
#define CAM_STAT_ACTIVE CAM_STATUS_ACTIVE
#define CAM_STAT_UNK100 CAM_STATUS_INACTIVE
#define PLAYER_IA_BOOMERANG PLAYER_IA_ZORA_BOOMERANG
// Model groups by role; the hammer and OoT's two ocarina groups have no MM counterpart.
#define PLAYER_MODELGROUP_CHILD_HYLIAN_SHIELD PLAYER_MODELGROUP_1
#define PLAYER_MODELGROUP_SWORD_AND_SHIELD PLAYER_MODELGROUP_ONE_HAND_SWORD
#define PLAYER_MODELGROUP_BGS PLAYER_MODELGROUP_TWO_HAND_SWORD
#define PLAYER_MODELGROUP_BOW_SLINGSHOT PLAYER_MODELGROUP_BOW
#define PLAYER_MODELGROUP_BOOMERANG PLAYER_MODELGROUP_ZORA_BOOMERANG
#define PLAYER_MODELGROUP_10 PLAYER_MODELGROUP_DEKU_STICK
#define ARROW_NORMAL ARROW_TYPE_NORMAL
#define ARROW_NORMAL_LIT ARROW_TYPE_NORMAL_LIT
#define ARROW_NORMAL_HORSE ARROW_TYPE_NORMAL_HORSE
#define ARROW_FIRE ARROW_TYPE_FIRE
#define ARROW_ICE ARROW_TYPE_ICE
#define ARROW_LIGHT ARROW_TYPE_LIGHT
// EnArrow's actions, unnamed in MM: shot from the bow, then in flight (unk_260 is the flight timer in both).
#define EnArrow_Shoot func_8088A594
#define EnArrow_Fly func_8088ACE0
#define ARROW_NORMAL_SILENT ARROW_TYPE_NORMAL
#define ARROW_0C ARROW_TYPE_NORMAL
#define ARROW_0D ARROW_TYPE_NORMAL
#define ARROW_0E ARROW_TYPE_NORMAL
#define ARROW_CS_NUT ARROW_TYPE_DEKU_NUT
#define LANGUAGE_FRA LANGUAGE_FRE
#define Actor_SetProjectileSpeed Actor_SetSpeeds
#define NA_BGM_ITEM_GET NA_BGM_GET_ITEM
// OoT's DynaPoly move flags are MM's transform bits: follow the position, then the Y rotation.
#define DPM_UNK 0
#define DPM_PLAYER DYNA_TRANSFORM_POS
#define DPM_ENEMY DYNA_TRANSFORM_ROT_Y
#define DPM_UNK3 (DYNA_TRANSFORM_POS | DYNA_TRANSFORM_ROT_Y)
#define LIGHTNING_MODE_OFF LIGHTNING_OFF
#define LIGHTNING_MODE_ON LIGHTNING_ON
#define LIGHTNING_MODE_LAST LIGHTNING_LAST
// MM keeps the sfx timer in the actor's audio flags, not in its actor flags.
#define ACTOR_FLAG_SFX_TIMER 0
#define ZELDA_ARENA_MALLOC_DEBUG(size) ZeldaArena_Malloc(size)
#define ZELDA_ARENA_FREE_DEBUG(ptr) ZeldaArena_Free(ptr)

// Members MM renamed; the names are unique to the structs they belong to.
#define upperSkelAnime skelAnimeUpper
#define minVelocityY terminalVelocity
#define yDistToWater depthInWater
#define transiActorCtx transitionActors
#define ActorCategory ActorType
#define ACTORCAT_EXPLOSIVE ACTORCAT_EXPLOSIVES
#define OBJECT_GAMEPLAY_KEEP GAMEPLAY_KEEP
#define OBJECT_GAMEPLAY_FIELD_KEEP GAMEPLAY_FIELD_KEEP
#define OBJECT_GAMEPLAY_DANGEON_KEEP GAMEPLAY_DANGEON_KEEP
#define ITEM_BEAN ITEM_MAGIC_BEANS
#define ITEM_NUT ITEM_DEKU_NUT
#define ITEM_STICK ITEM_DEKU_STICK
#define ITEM_LENS ITEM_LENS_OF_TRUTH
#define ITEM_OCARINA_TIME ITEM_OCARINA_OF_TIME
// Blades and shields by role, as Z64ITEMS_MELEE_* below: the upgrade, the two-handers, the starting shield.
#define ITEM_SWORD_MASTER ITEM_SWORD_RAZOR
#define ITEM_SWORD_BGS ITEM_SWORD_GREAT_FAIRY
#define ITEM_SWORD_KNIFE ITEM_SWORD_GREAT_FAIRY
#define ITEM_SHIELD_DEKU ITEM_SHIELD_HERO
#define ITEM_SHIELD_HYLIAN ITEM_SHIELD_HERO

// MM keeps OoT's player state bits (same writes in both z_player.c), so each name maps to its own bit. MM moved
// hostile lock-on and held-item changes to stateFlags3; those and the OoT-only states are 0 (always false).
#define PLAYER_STATE1_LOADING PLAYER_STATE1_1
#define PLAYER_STATE1_SWINGING_BOTTLE PLAYER_STATE1_2
#define PLAYER_STATE1_ITEM_IN_HAND PLAYER_STATE1_8
#define PLAYER_STATE1_INPUT_DISABLED PLAYER_STATE1_20
#define PLAYER_STATE1_GETTING_ITEM PLAYER_STATE1_400
#define PLAYER_STATE1_HANGING_OFF_LEDGE PLAYER_STATE1_2000
#define PLAYER_STATE1_CLIMBING_LEDGE PLAYER_STATE1_4000
#define PLAYER_STATE1_JUMPING PLAYER_STATE1_40000
#define PLAYER_STATE1_FREEFALL PLAYER_STATE1_80000
#define PLAYER_STATE1_CLIMBING_LADDER PLAYER_STATE1_200000
#define PLAYER_STATE1_SHIELDING PLAYER_STATE1_400000
#define PLAYER_STATE1_ON_HORSE PLAYER_STATE1_800000
#define PLAYER_STATE1_USING_BOOMERANG PLAYER_STATE1_USING_ZORA_BOOMERANG
#define PLAYER_STATE1_BOOMERANG_THROWN PLAYER_STATE1_ZORA_BOOMERANG_THROWN
#define PLAYER_STATE1_DAMAGED PLAYER_STATE1_4000000
#define PLAYER_STATE1_IN_ITEM_CS PLAYER_STATE1_10000000
#define PLAYER_STATE1_IN_CUTSCENE PLAYER_STATE1_20000000
#define PLAYER_STATE1_FLOOR_DISABLED PLAYER_STATE1_80000000
#define PLAYER_STATE1_HOSTILE_LOCK_ON 0
#define PLAYER_STATE1_START_CHANGING_HELD_ITEM 0
#define PLAYER_STATE1_READY_TO_FIRE 0
#define PLAYER_STATE1_HOOKSHOT_FALLING 0
#define PLAYER_STATE2_DO_ACTION_GRAB PLAYER_STATE2_1
#define PLAYER_STATE2_DO_ACTION_CLIMB PLAYER_STATE2_4
#define PLAYER_STATE2_FOOTSTEP PLAYER_STATE2_8
#define PLAYER_STATE2_MOVING_DYNAPOLY PLAYER_STATE2_10
#define PLAYER_STATE2_DISABLE_ROTATION_Z_TARGET PLAYER_STATE2_20
#define PLAYER_STATE2_DISABLE_ROTATION_ALWAYS PLAYER_STATE2_40
#define PLAYER_STATE2_GRABBED_BY_ENEMY PLAYER_STATE2_80
#define PLAYER_STATE2_GRABBING_DYNAPOLY PLAYER_STATE2_100
#define PLAYER_STATE2_UNDERWATER PLAYER_STATE2_400
#define PLAYER_STATE2_DIVING PLAYER_STATE2_800
#define PLAYER_STATE2_STATIONARY_LADDER PLAYER_STATE2_1000
#define PLAYER_STATE2_FROZEN PLAYER_STATE2_4000
#define PLAYER_STATE2_PAUSE_MOST_UPDATING PLAYER_STATE2_8000
#define PLAYER_STATE2_DO_ACTION_ENTER PLAYER_STATE2_10000
#define PLAYER_STATE2_SPIN_ATTACKING PLAYER_STATE2_20000
#define PLAYER_STATE2_CRAWLING PLAYER_STATE2_40000
#define PLAYER_STATE2_HOPPING PLAYER_STATE2_80000
#define PLAYER_STATE2_NAVI_ACTIVE PLAYER_STATE2_100000
#define PLAYER_STATE2_NAVI_ALERT PLAYER_STATE2_200000
#define PLAYER_STATE2_DO_ACTION_DOWN PLAYER_STATE2_400000
#define PLAYER_STATE2_NEAR_OCARINA_ACTOR PLAYER_STATE2_800000
#define PLAYER_STATE2_ATTEMPT_PLAY_FOR_ACTOR PLAYER_STATE2_1000000
#define PLAYER_STATE2_PLAY_FOR_ACTOR PLAYER_STATE2_2000000
#define PLAYER_STATE2_OCARINA_PLAYING PLAYER_STATE2_USING_OCARINA
#define PLAYER_STATE2_DISABLE_DRAW PLAYER_STATE2_20000000
#define PLAYER_STATE2_SWORD_LUNGE PLAYER_STATE2_40000000
#define PLAYER_STATE2_FORCED_VOID_OUT PLAYER_STATE2_80000000
#define PLAYER_STATE2_REFLECTION 0
#define PLAYER_STATE3_IGNORE_CEILING_FLOOR_WATER PLAYER_STATE3_1
#define PLAYER_STATE3_MIDAIR PLAYER_STATE3_2
#define PLAYER_STATE3_PAUSE_ACTION_FUNC PLAYER_STATE3_4
#define PLAYER_STATE3_FINISHED_ATTACKING PLAYER_STATE3_8
#define PLAYER_STATE3_CHECK_FLOOR_WATER_COLLISION PLAYER_STATE3_10
#define PLAYER_STATE3_FORCE_PULL_OCARINA PLAYER_STATE3_20
#define PLAYER_STATE3_RESTORE_NAYRUS_LOVE 0

#define PLAYER_IA_SWORD_MASTER PLAYER_IA_SWORD_GILDED
#define PLAYER_IA_SWORD_BIGGORON PLAYER_IA_SWORD_TWO_HANDED
#define PLAYER_IA_FISHING_POLE PLAYER_IA_FISHING_ROD
#define PLAYER_IA_BOTTLE PLAYER_IA_BOTTLE_EMPTY
#define PLAYER_IA_LONGSHOT PLAYER_IA_HOOKSHOT
#define PLAYER_IA_SWORD_CS PLAYER_IA_NONE
// OoT's unused bow actions close its bow range (BOW..BOW_0E); MM's range ends at the Light Arrow.
#define PLAYER_IA_BOW_0C PLAYER_IA_BOW_LIGHT
#define PLAYER_IA_BOW_0D PLAYER_IA_BOW_LIGHT
#define PLAYER_IA_BOW_0E PLAYER_IA_BOW_LIGHT
#define ITEM_FISHING_POLE ITEM_FISHING_ROD

#define PLAYER_LIMB_R_THIGH PLAYER_LIMB_RIGHT_THIGH
#define PLAYER_LIMB_R_SHIN PLAYER_LIMB_RIGHT_SHIN
#define PLAYER_LIMB_R_FOOT PLAYER_LIMB_RIGHT_FOOT
#define PLAYER_LIMB_L_THIGH PLAYER_LIMB_LEFT_THIGH
#define PLAYER_LIMB_L_SHIN PLAYER_LIMB_LEFT_SHIN
#define PLAYER_LIMB_L_FOOT PLAYER_LIMB_LEFT_FOOT
#define PLAYER_LIMB_L_SHOULDER PLAYER_LIMB_LEFT_SHOULDER
#define PLAYER_LIMB_L_FOREARM PLAYER_LIMB_LEFT_FOREARM
#define PLAYER_LIMB_L_HAND PLAYER_LIMB_LEFT_HAND
#define PLAYER_LIMB_R_SHOULDER PLAYER_LIMB_RIGHT_SHOULDER
#define PLAYER_LIMB_R_FOREARM PLAYER_LIMB_RIGHT_FOREARM
#define PLAYER_LIMB_R_HAND PLAYER_LIMB_RIGHT_HAND
#define PLAYER_LIMB_UPPER PLAYER_LIMB_UPPER_ROOT
#define PLAYER_LIMB_BUF_COUNT PLAYER_LIMB_MAX

#define SUBCAM_FREE CAM_ID_NONE
#define MAIN_CAM CAM_ID_MAIN
#define CAM_SET_TURN_AROUND CAM_SET_NONE
#define ARROW_NUT ARROW_TYPE_DEKU_NUT
#define ARROW_SEED ARROW_TYPE_SLINGSHOT
#define NA_SE_VO_LI_AUTO_JUMP_KID NA_SE_VO_LI_AUTO_JUMP
#define NA_SE_VO_LI_SWORD_N_KID NA_SE_VO_LI_SWORD_N
// MM keeps linkAge at 0, which reads as adult; human Link in MM has the child's body, so OoT code picks its
// child animations and sockets.
#undef LINK_IS_ADULT
#undef LINK_IS_CHILD
#define LINK_IS_ADULT 0
#define LINK_IS_CHILD 1

// IS_RANDO guards OoT randomizer data (checks, RAND_INF flags) MM never has; MM rando reaches mods via the ModApi.
#define IS_RANDO 0
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// MM's get-item table filled into Ship's GetItemEntry layout; OoT GI ids MM lacks give an empty entry.
typedef S2HGetItemEntry GetItemEntry;
typedef S2HGetItemDrawFunc CustomDrawFunc;
typedef s16 GetItemID;

static inline GetItemEntry OotCompat_ItemTable_Retrieve(s16 getItemId) {
    GetItemEntry entry;
    VanillaItems_GetItemEntry(getItemId, &entry);
    return entry;
}

static inline GetItemEntry OotCompat_ItemTable_RetrieveEntry(s16 modIndex, s16 getItemId) {
    return OotCompat_ItemTable_Retrieve(modIndex == MOD_NONE ? getItemId : GI_NONE);
}

// MM's get-item draw hooks pass the draw id alone; Ship's callbacks read it from an entry.
static inline GetItemEntry OotCompat_GetItemEntryForDrawId(s16 drawId) {
    GetItemEntry entry = { 0 };
    entry.gid = drawId;
    entry.gi = drawId + 1;
    return entry;
}

#define ItemTable_Retrieve OotCompat_ItemTable_Retrieve
#define ItemTable_RetrieveEntry OotCompat_ItemTable_RetrieveEntry

// OoT randomizer queries answer as an unrandomized OoT save would.
static inline bool OotCompat_Flags_GetRandomizerInf(RandomizerInf flag) {
    return false;
}

static inline void OotCompat_Flags_SetRandomizerInf(RandomizerInf flag) {
}

static inline u8 OotCompat_Randomizer_GetSettingValue(RandomizerSettingKey key) {
    return 0;
}

static inline GetItemEntry OotCompat_Randomizer_GetItemFromKnownCheck(RandomizerCheck check, GetItemID vanilla) {
    return OotCompat_ItemTable_Retrieve(vanilla);
}

static inline RandomizerInf OotCompat_Randomizer_GetRandomizerInfFromCheck(RandomizerCheck check) {
    return 0;
}

static inline s16 OotCompat_Entrance_PeekNextIndexOverride(s16 entrance) {
    return entrance;
}

// OoT's medallions and spiritual stones are custom items in MM (useful_medallions, spiritual_stones mods).
// Their quest numbers sit past MM's 32 quest bits, so the quest macros can tell them apart.
#define QUEST_MEDALLION_FOREST 0x40
#define QUEST_MEDALLION_FIRE 0x41
#define QUEST_MEDALLION_WATER 0x42
#define QUEST_MEDALLION_SPIRIT 0x43
#define QUEST_MEDALLION_SHADOW 0x44
#define QUEST_MEDALLION_LIGHT 0x45
#define QUEST_KOKIRI_EMERALD 0x46
#define QUEST_GORON_RUBY 0x47
#define QUEST_ZORA_SAPPHIRE 0x48

static inline const char* OotCompat_QuestItemKey(s32 item) {
    static const char* const sKeys[] = {
        "nei.medallion.forest",       "nei.medallion.fire",         "nei.medallion.water",
        "nei.medallion.spirit",       "nei.medallion.shadow",       "nei.medallion.light",
        "nei.spiritual_stone.kokiri", "nei.spiritual_stone.goron", "nei.spiritual_stone.zora",
    };
    s32 index = item - QUEST_MEDALLION_FOREST;
    return (index >= 0 && index < (s32)ARRAY_COUNT(sKeys)) ? sKeys[index] : NULL;
}

static inline bool OotCompat_CheckQuestItem(s32 item) {
    const char* key = OotCompat_QuestItemKey(item);
    if (key != NULL) {
        return CustomItemRegistry_IsOwned(key);
    }
    return (GET_SAVE_INVENTORY_QUEST_ITEMS & gBitFlags[item]) != 0;
}

static inline void OotCompat_SetQuestItem(s32 item, bool owned) {
    const char* key = OotCompat_QuestItemKey(item);
    if (key != NULL) {
        CustomItemRegistry_SetOwned(key, owned);
    } else if (owned) {
        gSaveContext.save.saveInfo.inventory.questItems = GET_SAVE_INVENTORY_QUEST_ITEMS | gBitFlags[item];
    } else {
        gSaveContext.save.saveInfo.inventory.questItems = GET_SAVE_INVENTORY_QUEST_ITEMS & ~gBitFlags[item];
    }
}

#undef CHECK_QUEST_ITEM
#undef SET_QUEST_ITEM
#undef REMOVE_QUEST_ITEM
#define CHECK_QUEST_ITEM(item) OotCompat_CheckQuestItem(item)
#define SET_QUEST_ITEM(item) OotCompat_SetQuestItem(item, true)
#define REMOVE_QUEST_ITEM(item) OotCompat_SetQuestItem(item, false)

// Equipment keeps OoT's numbers. MM's custom equipment slots are numbered like OoT's equipment types; a piece worn
// there is either one of oot_equipment's OoT pieces or a mod piece whose vanillaBase names the OoT value it plays.
// With nothing worn, MM's own sword and shield answer by role and the tunic and boots are Kokiri.
enum { OOT_COMPAT_MM_SHIELD_MIRROR = EQUIP_VALUE_SHIELD_MIRROR };
#define EQUIP_TYPE_BOOTS S2H_EQUIP_SLOT_BOOTS
#define EQUIP_VALUE_SWORD_MASTER 2
#define EQUIP_VALUE_SWORD_BIGGORON 3
#define EQUIP_VALUE_SWORD_MAX 4
#define EQUIP_VALUE_SHIELD_DEKU 1
#define EQUIP_VALUE_SHIELD_HYLIAN 2
#define EQUIP_VALUE_SHIELD_MIRROR 3
#define EQUIP_VALUE_SHIELD_MAX 4
#define EQUIP_VALUE_TUNIC_NONE 0
#define EQUIP_VALUE_TUNIC_KOKIRI 1
#define EQUIP_VALUE_TUNIC_GORON 2
#define EQUIP_VALUE_TUNIC_ZORA 3
#define EQUIP_VALUE_TUNIC_MAX 4
#define EQUIP_VALUE_BOOTS_NONE 0
#define EQUIP_VALUE_BOOTS_KOKIRI 1
#define EQUIP_VALUE_BOOTS_IRON 2
#define EQUIP_VALUE_BOOTS_HOVER 3
#define EQUIP_VALUE_BOOTS_MAX 4
#define OOT_COMPAT_EQUIP_VALUES 4

static inline const char* OotCompat_EquipKey(s32 type, s32 value) {
    static const char* const sKeys[S2H_EQUIP_SLOT_UPGRADE][OOT_COMPAT_EQUIP_VALUES] = {
        { NULL, NULL, "oot.equip.master_sword", "oot.equip.biggoron_sword" },
        { NULL, "oot.equip.deku_shield", NULL, "oot.equip.mirror_shield" },
        { NULL, NULL, "oot.equip.goron_tunic", "oot.equip.zora_tunic" },
        { NULL, NULL, "oot.equip.iron_boots", "oot.equip.hover_boots" },
    };
    if (type < 0 || type >= S2H_EQUIP_SLOT_UPGRADE || value < 0 || value >= OOT_COMPAT_EQUIP_VALUES) {
        return NULL;
    }
    return sKeys[type][value];
}

// MM's sword and shield by role: Razor and Gilded are the one-handed upgrade, the Fierce Deity's blade the
// two-hander; the Hero's Shield is the Hylian one.
static inline s32 OotCompat_EquipFromMm(s32 type, s32 mmValue) {
    static const u8 sSwords[EQUIP_VALUE_SWORD_DIETY + 1] = { 0, 1, 2, 2, 3 };
    static const u8 sShields[OOT_COMPAT_MM_SHIELD_MIRROR + 1] = { 0, 2, 3 };
    if (type == EQUIP_TYPE_SWORD) {
        return (mmValue >= 0 && mmValue <= EQUIP_VALUE_SWORD_DIETY) ? sSwords[mmValue] : 0;
    }
    return (mmValue >= 0 && mmValue <= OOT_COMPAT_MM_SHIELD_MIRROR) ? sShields[mmValue] : 0;
}

static inline s32 OotCompat_CurEquipValue(s32 type) {
    const char* worn = CustomEquipRegistry_GetWorn(type);
    if (worn != NULL && worn[0] != '\0') {
        for (s32 value = 1; value < OOT_COMPAT_EQUIP_VALUES; value++) {
            const char* key = OotCompat_EquipKey(type, value);
            if (key != NULL && strcmp(key, worn) == 0) {
                return value;
            }
        }
        const S2HCustomEquipDefinition* piece = CustomEquipRegistry_Find(worn);
        if (piece != NULL && piece->vanillaBase != 0) {
            return (type < EQUIP_TYPE_TUNIC) ? OotCompat_EquipFromMm(type, piece->vanillaBase) : piece->vanillaBase;
        }
    }
    if (type < EQUIP_TYPE_TUNIC) {
        return OotCompat_EquipFromMm(type, GET_CUR_EQUIP_VALUE(type));
    }
    return EQUIP_VALUE_TUNIC_KOKIRI;
}

// The OoT piece if a mod provides it and it is owned; else MM's own sword or shield of that role; the Kokiri
// tunic and boots are "nothing worn".
static inline void OotCompat_Inventory_ChangeEquipment(s16 type, u16 value) {
    const char* key = OotCompat_EquipKey(type, value);
    if (key != NULL && CustomEquipRegistry_IsOwned(key)) {
        CustomEquipRegistry_Wear(key);
        return;
    }
    if (type >= EQUIP_TYPE_TUNIC) {
        if (value == EQUIP_VALUE_TUNIC_KOKIRI) {
            CustomEquipRegistry_TakeOff(type);
        }
        return;
    }
    for (s32 mmValue = 0; mmValue <= EQUIP_VALUE_SWORD_DIETY; mmValue++) {
        if (OotCompat_EquipFromMm(type, mmValue) == value) {
            CustomEquipRegistry_TakeOff(type);
            SET_EQUIP_VALUE(type, mmValue);
            return;
        }
    }
}

static inline bool OotCompat_IsEquipOwned(s32 type, s32 value) {
    const char* key = OotCompat_EquipKey(type, value);
    if (key != NULL && CustomEquipRegistry_IsOwned(key)) {
        return true;
    }
    if (type >= EQUIP_TYPE_TUNIC) {
        return value == EQUIP_VALUE_TUNIC_KOKIRI;
    }
    return value != 0 && OotCompat_EquipFromMm(type, GET_CUR_EQUIP_VALUE(type)) == value;
}

// OoT's ownership bits number each piece from 0: its equipment value minus one.
#define EQUIP_INV_SWORD_KOKIRI 0
#define EQUIP_INV_SWORD_MASTER 1
#define EQUIP_INV_SWORD_BIGGORON 2
#define EQUIP_INV_SWORD_BROKENGIANTKNIFE 3
#define EQUIP_INV_SHIELD_DEKU 0
#define EQUIP_INV_SHIELD_HYLIAN 1
#define EQUIP_INV_SHIELD_MIRROR 2
#define EQUIP_INV_TUNIC_KOKIRI 0
#define EQUIP_INV_TUNIC_GORON 1
#define EQUIP_INV_TUNIC_ZORA 2
#define EQUIP_INV_BOOTS_KOKIRI 0
#define EQUIP_INV_BOOTS_IRON 1
#define EQUIP_INV_BOOTS_HOVER 2

#define CUR_EQUIP_VALUE(type) OotCompat_CurEquipValue(type)
#define CHECK_OWNED_EQUIP(type, inventoryValue) OotCompat_IsEquipOwned(type, (inventoryValue) + 1)
#define Inventory_ChangeEquipment OotCompat_Inventory_ChangeEquipment

// The Player fields keep MM's numbers; OoT's names go by role. Iron boots are a Goron's weight, on the lake bed a
// Zora's; OoT-only states sit past MM's values and never match.
#define PLAYER_BOOTS_KOKIRI PLAYER_BOOTS_HYLIAN
#define PLAYER_BOOTS_IRON PLAYER_BOOTS_GORON
#define PLAYER_BOOTS_IRON_UNDERWATER PLAYER_BOOTS_ZORA_UNDERWATER
#define PLAYER_BOOTS_HOVER 0x70
#define PLAYER_BOOTS_INDOOR 0x71
#define PLAYER_BOOTS_KOKIRI_CHILD 0x72
#define PLAYER_SHIELD_HYLIAN PLAYER_SHIELD_HEROS_SHIELD
#define PLAYER_SHIELD_MIRROR PLAYER_SHIELD_MIRROR_SHIELD
#define PLAYER_SHIELD_DEKU 0x70
#define PLAYER_SWORD_NONE EQUIP_VALUE_SWORD_NONE
#define PLAYER_SWORD_KOKIRI EQUIP_VALUE_SWORD_KOKIRI
#define PLAYER_SWORD_MASTER EQUIP_VALUE_SWORD_MASTER
#define PLAYER_SWORD_BIGGORON EQUIP_VALUE_SWORD_BIGGORON
#define PLAYER_TUNIC_KOKIRI 0
#define PLAYER_TUNIC_GORON 1
#define PLAYER_TUNIC_ZORA 2

// Lifting strength by role: none is the Deku's, the bracelet a human's, the gauntlets a Goron's.
#define PLAYER_STR_NONE PLAYER_STRENGTH_DEKU
#define PLAYER_STR_BRACELET PLAYER_STRENGTH_HUMAN
#define PLAYER_STR_SILVER_G PLAYER_STRENGTH_GORON
#define PLAYER_STR_GOLD_G PLAYER_STRENGTH_GORON
#define PLAYER_STR_MAX PLAYER_STRENGTH_MAX

// OoT's sTunicColors (Kokiri, Goron, Zora); MM only tints through VB_APPLY_TUNIC_COLOR.
static inline Color_RGB8 OotCompat_Player_GetTunicColor(s32 tunic) {
    static const Color_RGB8 sColors[] = { { 30, 105, 27 }, { 100, 20, 0 }, { 0, 60, 100 } };
    return sColors[(tunic >= 0 && tunic < (s32)ARRAY_COUNT(sColors)) ? tunic : 0];
}

#define Player_GetTunicColor OotCompat_Player_GetTunicColor

// MM's Player has no tunic: the worn OoT tunic, as OoT's Player.currentTunic numbers it.
static inline s8 OotCompat_CurrentTunic(void) {
    return OotCompat_CurEquipValue(EQUIP_TYPE_TUNIC) - EQUIP_VALUE_TUNIC_KOKIRI;
}

// OoT items MM only has as oot_items' custom items: their numbers sit in MM's free range (0xE0+), and
// INV_CONTENT answers from the custom item registry. Writing through it does not grant the item.
#define ITEM_HAMMER 0xE0
#define ITEM_BOOMERANG 0xE1
#define ITEM_NAYRUS_LOVE 0xE2
#define ITEM_KOKIRI_EMERALD 0xE3
#define ITEM_GORON_RUBY 0xE4
#define ITEM_ZORA_SAPPHIRE 0xE5
#define ITEM_MEDALLION_FOREST 0xE6
#define ITEM_MEDALLION_FIRE 0xE7
#define ITEM_MEDALLION_WATER 0xE8
#define ITEM_MEDALLION_SPIRIT 0xE9
#define ITEM_MEDALLION_SHADOW 0xEA
#define ITEM_MEDALLION_LIGHT 0xEB

static inline const char* OotCompat_CustomItemKey(s32 item) {
    switch (item) {
        case ITEM_HAMMER:
            return "oot.hammer";
        case ITEM_BOOMERANG:
            return "oot.boomerang";
        case ITEM_NAYRUS_LOVE:
            return "oot.nayrus_love";
        case ITEM_KOKIRI_EMERALD:
            return "nei.spiritual_stone.kokiri";
        case ITEM_GORON_RUBY:
            return "nei.spiritual_stone.goron";
        case ITEM_ZORA_SAPPHIRE:
            return "nei.spiritual_stone.zora";
        case ITEM_MEDALLION_FOREST:
            return "nei.medallion.forest";
        case ITEM_MEDALLION_FIRE:
            return "nei.medallion.fire";
        case ITEM_MEDALLION_WATER:
            return "nei.medallion.water";
        case ITEM_MEDALLION_SPIRIT:
            return "nei.medallion.spirit";
        case ITEM_MEDALLION_SHADOW:
            return "nei.medallion.shadow";
        case ITEM_MEDALLION_LIGHT:
            return "nei.medallion.light";
        default:
            return NULL;
    }
}

static inline u8* OotCompat_InventorySlot(s32 item) {
    static u8 sCustomContent;
    const char* key = OotCompat_CustomItemKey(item);
    if (key == NULL) {
        return &gSaveContext.save.saveInfo.inventory.items[SLOT(item)];
    }
    sCustomContent = CustomItemRegistry_IsOwned(key) ? item : ITEM_NONE;
    return &sCustomContent;
}

#undef INV_CONTENT
#define INV_CONTENT(item) (*OotCompat_InventorySlot(item))

// OoT drains magic toward magicTarget; MM drains magicToConsume and fills magicToAdd.
static inline s16 OotCompat_Magic_GetTarget(void) {
    return gSaveContext.save.saveInfo.playerData.magic - gSaveContext.magicToConsume + gSaveContext.magicToAdd;
}

static inline void OotCompat_Magic_ApplyTarget(void) {
    gSaveContext.save.saveInfo.playerData.magic = OotCompat_Magic_GetTarget();
    gSaveContext.magicToConsume = 0;
    gSaveContext.magicToAdd = 0;
}

static inline void OotCompat_Magic_ClearTarget(void) {
    gSaveContext.magicToConsume = 0;
    gSaveContext.magicToAdd = 0;
}

// OoT's eventChkInf/itemGetInf/infTable have no MM storage: never set, writes dropped.
static inline s32 OotCompat_Flags_GetOotSaveFlag(s32 flag) {
    return false;
}

static inline void OotCompat_Flags_SetOotSaveFlag(s32 flag) {
}

#define Flags_GetEventChkInf OotCompat_Flags_GetOotSaveFlag
#define Flags_SetEventChkInf OotCompat_Flags_SetOotSaveFlag
#define Flags_UnsetEventChkInf OotCompat_Flags_SetOotSaveFlag
#define Flags_GetItemGetInf OotCompat_Flags_GetOotSaveFlag
#define Flags_SetItemGetInf OotCompat_Flags_SetOotSaveFlag
#define Flags_UnsetItemGetInf OotCompat_Flags_SetOotSaveFlag
#define Flags_GetInfTable OotCompat_Flags_GetOotSaveFlag
#define Flags_SetInfTable OotCompat_Flags_SetOotSaveFlag
#define Flags_UnsetInfTable OotCompat_Flags_SetOotSaveFlag
#define Flags_GetRandomizerInf OotCompat_Flags_GetRandomizerInf
#define Flags_SetRandomizerInf OotCompat_Flags_SetRandomizerInf
#define Randomizer_GetSettingValue OotCompat_Randomizer_GetSettingValue
#define Randomizer_GetItemFromKnownCheck OotCompat_Randomizer_GetItemFromKnownCheck
#define Randomizer_GetRandomizerInfFromCheck OotCompat_Randomizer_GetRandomizerInfFromCheck
#define Entrance_PeekNextIndexOverride OotCompat_Entrance_PeekNextIndexOverride

// Damage by role: OoT's blades and spells meet MM's sword and elemental arrows, the hammer meets the Goron's blows.
#define DMG_SLINGSHOT DMG_NORMAL_ARROW
#define DMG_EXPLOSIVE DMG_EXPLOSIVES
#define DMG_BOOMERANG DMG_ZORA_BOOMERANG
#define DMG_ARROW_NORMAL DMG_NORMAL_ARROW
#define DMG_HAMMER_SWING DMG_GORON_PUNCH
#define DMG_HAMMER_JUMP DMG_GORON_POUND
#define DMG_SLASH_KOKIRI DMG_SWORD
#define DMG_SLASH_MASTER DMG_SWORD
#define DMG_SLASH_GIANT DMG_SWORD
#define DMG_ARROW_FIRE DMG_FIRE_ARROW
#define DMG_ARROW_ICE DMG_ICE_ARROW
#define DMG_ARROW_LIGHT DMG_LIGHT_ARROW
#define DMG_ARROW_UNK1 DMG_NORMAL_ARROW
#define DMG_ARROW_UNK2 DMG_NORMAL_ARROW
#define DMG_ARROW_UNK3 DMG_NORMAL_ARROW
#define DMG_MAGIC_FIRE DMG_FIRE_ARROW
#define DMG_MAGIC_ICE DMG_ICE_ARROW
#define DMG_MAGIC_LIGHT DMG_LIGHT_ARROW
#define DMG_SHIELD DMG_NORMAL_SHIELD
#define DMG_MIR_RAY DMG_LIGHT_RAY
#define DMG_SPIN_KOKIRI DMG_SPIN_ATTACK
#define DMG_SPIN_GIANT DMG_SPIN_ATTACK
#define DMG_SPIN_MASTER DMG_SPIN_ATTACK
#define DMG_JUMP_KOKIRI DMG_SWORD
#define DMG_JUMP_GIANT DMG_SWORD
#define DMG_JUMP_MASTER DMG_SWORD
#define DMG_UNKNOWN_1 DMG_UNK_0x1C
#define DMG_UNKNOWN_2 DMG_UNK_0x1E
#define DMG_SLASH DMG_SWORD
#define DMG_JUMP_SLASH DMG_SWORD
#define DMG_HAMMER (DMG_GORON_PUNCH | DMG_GORON_POUND)
#define DMG_FIRE DMG_FIRE_ARROW
#define DMG_ARROW (DMG_NORMAL_ARROW | DMG_FIRE_ARROW | DMG_ICE_ARROW | DMG_LIGHT_ARROW)
#define DMG_RANGED (DMG_ARROW | DMG_HOOKSHOT)
#define DMG_DEFAULT ~(DMG_NORMAL_SHIELD | DMG_LIGHT_RAY)

// OoT blades by role: the one-handed upgrade and the two-handed weapons.
#define Z64ITEMS_MELEE_MASTER_SWORD Z64ITEMS_MELEE_RAZOR_SWORD
#define Z64ITEMS_MELEE_BIGGORON_SWORD Z64ITEMS_MELEE_GREAT_FAIRY_SWORD
#define Z64ITEMS_MELEE_HAMMER Z64ITEMS_MELEE_GREAT_FAIRY_SWORD

#endif
