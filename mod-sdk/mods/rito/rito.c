#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "z64items.h"
#include "z64formmasks.h"
#include "z64logic.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "overlays/actors/ovl_En_Arrow/z_en_arrow.h"
#include "overlays/actors/ovl_En_Light/z_en_light.h"
#include "overlays/actors/ovl_Obj_Switch/z_obj_switch.h"

#define RITO_MASK_KEY "nei.rito_form_mask"
#define ROCS_FEATHER_KEY "nei.rocs_feather"
#define ROCS_CAPE_KEY "nei.rocs_cape"
#define RITO_FORM_KEY "nei.rito"
#define RITO_MODEL_PATH "objects/forms/rito"
#define RITO_MASK_PAGE 1
#define RITO_MASK_SLOT 7
#define RITO_MASK_WHEEL_PRIORITY (-1)

#define ANIM_VALUES_PER_FRAME 67
#define JUMP_GRAVITY (-1.2f)
#define OCARINA_NOTE_NONE OCARINA_PITCH_NONE
#define CLIP_FRAMES_PER_UPDATE 1.5f

// The rig is MM's human skeleton, rooted at 2376, so an adult clip drives it a third too high; a child one fits.
#define RITO_ROOT_SCALE_ADULT 0.7036f
#define RITO_ROOT_SCALE_CHILD 1.0f
#define RITO_HEIGHT 40.0f

// MM leaves these bits unnamed: dead, talking, loading, getting an item, carrying, swimming, first person, ledge
// climb, ladder, horse, damage knockback, the textbox freeze and the shield.
#define BUSY_STATE1                                                                                  \
    (PLAYER_STATE1_DEAD | PLAYER_STATE1_TALKING | PLAYER_STATE1_200 | PLAYER_STATE1_400 |            \
     PLAYER_STATE1_CARRYING_ACTOR | PLAYER_STATE1_8000000 | PLAYER_STATE1_100000 | PLAYER_STATE1_4 | \
     PLAYER_STATE1_200000 | PLAYER_STATE1_800000 | PLAYER_STATE1_4000000 | PLAYER_STATE1_20000000 |  \
     PLAYER_STATE1_400000)
#define STATE1_SHIELDING PLAYER_STATE1_400000
#define STATE1_SWIMMING PLAYER_STATE1_8000000
#define STATE1_FIRST_PERSON PLAYER_STATE1_100000
#define STATE1_DEAD_OR_CUTSCENE (PLAYER_STATE1_DEAD | PLAYER_STATE1_20000000)

#define A_HOLD_FRAMES 6
#define CHARGE_MIN 6
#define CHARGE_FULL 24
#define LAUNCH_MIN 8.0f
#define LAUNCH_ROCS_MULTIPLE 4.0f
#define LAUNCH_DECAY 0.8f
#define LAUNCH_MAX_FRAMES 20
#define LAUNCH_MAGIC_COST 12
#define GLIDE_SPEED 4.5f
#define GLIDE_SINK (-0.35f)
#define GLIDE_TURN_RATE 5.0f
#define GLIDE_TURN_DEADZONE 10.0f
#define HOP_LAND_GUARD 4
#define HOP_ROLL_SCALE 1.5f
#define HOP_ROLL_SPEED 6.0f
#define HOP_SIDE_SPEED 8.5f
#define FLAP_SFX_PERIOD 8
#define CHARGE_FLAP_PERIOD 4
#define ROCS_JUMP_VELOCITY 11.0f
#define ROCS_AIR_MAGIC_COST 12
#define ROCS_BOOST_FRAMES 16
#define ROCS_HUMAN_VELOCITY 7.0f

#define CLIP_SPEED 2.0f
#define BOW_SPEED 1.25f
#define BOW_RELEASE_SPEED 1.5f
#define BOW_SEED_SPEED 2.0f
#define THROW_SPEED 3.0f
#define LAND_LIFT 1500.0f
#define BOW_SEED_LIFT 1500.0f
#define BOW_RELEASE_LIFT (-300.0f)

#define BOW_VOLLEY 3
#define BOW_TWIN_SHOTS 2
#define BOW_LOOSE_FRAME 3
#define BOW_LOOSE_LEAD 5.0f
#define BOW_RANGE 1400.0f
#define BOW_CONE 0x1555
#define BOW_TURN 0x1000
#define BOW_ARROW_SPEED 150.0f
#define BOW_HIT_DIST 170.0f
#define BOW_SNAP_DIST 400.0f
#define BOW_MOVE_SPEED 5.0f
#define BOW_MOVE_DEADZONE 10.0f
#define BOW_TURN_RATE 0x0C00
#define BOW_ICE_RADIUS 90.0f
// EnArrow kills a parentless arrow unless player->unk_D57 is still counting down.
#define BOW_HANDSHAKE 4
#define BOW_FAN_STEP 0x0720
#define BOW_TAP_FRAMES 5
#define BOW_CHARGE_TWIN 20
#define BOW_HOP_SPEED 7.0f
#define BOW_HOP_DECAY 0.5f
#define BOW_DODGE_IFRAMES (-10)

#define SINK_DEPTH 30.0f
#define SINK_START (-2.0f)
#define SINK_GRAVITY (-4.0f)
#define SINK_FRAMES 9

static const Vec3f sShieldInHand = { -20.0f, 115.5f, -145.5f };
static const Vec3f sShieldOnBack = { 608.0f, 10.0f, -142.5f };

// Tornado_GetAxis: axis.y = -sin(pitch), so straight up is -0x4000.
#define WIND_PITCH_UP (-0x4000)
#define WIND_HEIGHT 82.0f
#define WIND_RADIUS 30.0f
#define WIND_SPIN 0x0900
#define WIND_SCROLL 18
#define WIND_RIBBONS 5
#define WIND_RIBBON_MAX 6
#define WIND_RIBBON_SPREAD 2.0f
#define WIND_RIBBON_TURNS 0x18000
#define WIND_RIBBON_STEP (1.0f / 14.0f)
#define WIND_ALPHA 70
#define WIND_GROW_MIN 0.35f
#define WIND_FADE_FRAMES 14.0f
#define WIND_MOTES 8
#define WIND_MOTES_KEPT 4
#define WIND_BURST_FRAMES 7
#define WIND_MOTE_RADIUS 17.0f
#define WIND_MOTE_BURST_RADIUS 40.0f
#define WIND_MOTE_HEIGHT 12.0f
#define WIND_MOTE_DRIFT 0x0700
#define WIND_LIGHT_PARAMS 6
#define WIND_LIGHT_SCALE 0.0010f
#define WIND_QUAKE_SPEED 32000
#define WIND_QUAKE_AMPLITUDE 6
#define WIND_QUAKE_FRAMES 11
#define WIND_LOOP_SFX (NA_SE_EV_WIND_TRAP - SFX_FLAG)
#define TORNADO_MODEL_LENGTH 100.0f
#define TORNADO_MODEL_RADIUS 47.0f
#define TORNADO_TEX_WIDTH 32
#define TORNADO_TEX_HEIGHT 64

#define ANIM_PATH(name) "__OTR__misc/link_animetion/" name
#define GLAIVE(name) ANIM_PATH("gMonsterHunterRise_InsectGlaive_" name)
#define BOW(name) ANIM_PATH("gPlayerAnim_mhr_bow_" name)

static const u16 sItemButtons[S2H_ITEM_BUTTON_COUNT] = { BTN_B,      BTN_CLEFT, BTN_CDOWN, BTN_CRIGHT, 0,
                                                         BTN_DRIGHT, BTN_DLEFT, BTN_DDOWN, BTN_DUP };

static const ALIGN_ASSET(2) char sIconTex[] = "__OTR__textures/icon_item_custom/gItemIconRitoMaskTex";
static const ALIGN_ASSET(2) char sNameTex[] = "__OTR__textures/item_name_custom/gRitoMaskNameTex";
static const ALIGN_ASSET(2) char sShieldDL[] = "__OTR__objects/object_nei_rito_shield/gRitoShieldDL";
static const ALIGN_ASSET(2) char sBowDL[] = "__OTR__objects/object_nei_rito_bow/gRitoBowDL";
static const ALIGN_ASSET(2) char sTornadoMatDL[] = "__OTR__objects/object_nei_tornado/mat_tornado_f3dlite_tornado";
static const ALIGN_ASSET(2) char sTornadoTriDL[] = "__OTR__objects/object_nei_tornado/tornado_mesh_tri_0";
static const ALIGN_ASSET(2) char sOpenHandDL[] = "__OTR__" RITO_MODEL_PATH "/object_link_boy/gLinkAdultRightHandNearDL";
static const ALIGN_ASSET(2) char sClosedHandDL[] = "__OTR__" RITO_MODEL_PATH
                                                   "/object_link_boy/gLinkAdultRightHandClosedNearDL";
static const ALIGN_ASSET(2) char sOpenLeftHandDL[] = "__OTR__" RITO_MODEL_PATH
                                                     "/object_link_boy/gLinkAdultLeftHandNearDL";
static const ALIGN_ASSET(2) char sClosedLeftHandDL[] = "__OTR__" RITO_MODEL_PATH
                                                       "/object_link_boy/gLinkAdultLeftHandClosedNearDL";
static const ALIGN_ASSET(2) char sHeadDL[] = "__OTR__" RITO_MODEL_PATH "/object_link_boy/gLinkAdultSkelLimb_010_DL";
static const ALIGN_ASSET(2) char sCrestDL[] = "__OTR__" RITO_MODEL_PATH "/object_link_boy/gLinkAdultSkelLimb_011_DL";

// The head's lists index the skeleton's skin matrices on segment 0x0D: the head, its crest and the neck seam.
#define MASK_MATRIX_COUNT 18
#define MASK_HEAD_MATRIX 7
#define MASK_CREST_MATRIX 8
#define MASK_SCALE 0.03f
#define MASK_SEAM_SCALE 0.01f

static const Vec3f sMaskCentre = { 300.0f, -430.0f, 0.0f };
static const Vec3f sCrestJoint = { -365.0f, -670.0f, 0.0f };

typedef enum {
    CLIP_LAUNCH,
    CLIP_FLY,
    CLIP_LAND,
    CLIP_HOP_FRONT,
    CLIP_HOP_LEFT,
    CLIP_HOP_BACK,
    CLIP_HOP_RIGHT,
    CLIP_HOP_SETTLE,
    CLIP_THROW,
    CLIP_BOW_AIR,
    CLIP_BOW_ENTER_STILL,
    CLIP_BOW_HOLD_STILL,
    CLIP_BOW_ENTER_MOVE,
    CLIP_BOW_HOLD_MOVE,
    CLIP_BOW_RELEASE,
    CLIP_BOW_SEED,
    CLIP_BOW_AIR_TAP,
    CLIP_HARP_RAISE,
    CLIP_HARP_PLAY,
    CLIP_COUNT,
} RitoClip;

typedef struct {
    const char* path;
    f32 speed;
    bool isRootPinned;
} ClipSource;

static const ClipSource sClipSources[CLIP_COUNT] = {
    { GLAIVE("BackwardRisingDoubleChargedStaffCombo"), CLIP_SPEED, true },
    { ANIM_PATH("gPlayerAnim_mhr_npc_takkuri_fly"), CLIP_SPEED, true },
    { GLAIVE("ForwardSingleAdvancingStaffSweep"), CLIP_SPEED, true },
    { GLAIVE("ForwardRisingMultiHitAerialStaffStrike_Variant08"), CLIP_SPEED, true },
    { GLAIVE("RightHighAerialDoubleSilkbindStaffStrike"), CLIP_SPEED, true },
    { GLAIVE("BackwardHighAerialMultiHitSilkbindStaffStrike_Variant06"), CLIP_SPEED, true },
    { GLAIVE("LeftHighAerialSingleSilkbindStaffStrike"), CLIP_SPEED, true },
    { GLAIVE("StationaryStaffReadyIdle_Variant10"), CLIP_SPEED, true },
    { GLAIVE("ForwardDoubleStaffStrike"), THROW_SPEED, true },
    { BOW("charge_attack09"), BOW_SPEED, true },
    { BOW("motion12"), BOW_SPEED, true },
    { BOW("idle04_loop"), BOW_SPEED, true },
    { BOW("dash_attack07"), BOW_SPEED, true },
    { BOW("run07_loop"), BOW_SPEED, true },
    { BOW("dash_attack13"), BOW_RELEASE_SPEED, true },
    { BOW("charge_attack11"), BOW_SEED_SPEED, true },
    { BOW("attack04"), BOW_SPEED, true },
    { ANIM_PATH("gPlayerAnim_mhr_npc_sheik_pulling_out_harp"), 1.0f, false },
    { ANIM_PATH("gPlayerAnim_mhr_npc_sheik_playing_harp"), 1.0f, false },
};

typedef enum {
    RITO_MOVE_NONE,
    RITO_MOVE_CHARGE,
    RITO_MOVE_LAUNCH,
    RITO_MOVE_GLIDE,
    RITO_MOVE_HOP,
    RITO_MOVE_THROW,
    RITO_MOVE_BOW_ENTER,
    RITO_MOVE_BOW_HOLD,
    RITO_MOVE_BOW_RELEASE,
    RITO_MOVE_BOW_SEED,
    RITO_MOVE_BOW_AIR,
    RITO_MOVE_BOW_AIR_TAP,
    RITO_MOVE_SINK,
} RitoMove;

typedef enum {
    HARP_IDLE,
    HARP_RAISING,
    HARP_PLAYING,
    HARP_LOWERING,
} HarpPhase;

typedef struct {
    Actor* arrow;
    Actor* target;
    u8 targetCategory;
    Vec3f goal;
} HomingArrow;

typedef struct {
    Vec3f origin;
    f32 length;
    f32 radius;
    Color_RGBA8 color;
    s16 spin;
    s16 scrollT;
} WindCone;

typedef struct {
    WindCone cone;
    s32 ribbons[WIND_RIBBON_MAX];
    f32 ribbonPhase[WIND_RIBBON_MAX];
    bool areRibbonsOn;
    Actor* motes[WIND_MOTES];
    f32 motePhase[WIND_MOTES];
    f32 fade;
    f32 grow;
    s32 frames;
    bool isOn;
} Updraft;

static const char* const sRequiredHooks[] = {
    "OnPlayerActionHandler", "OnPlayerResolveAnim", "OnPlayerResolveLimbDraw",
    "OnPlayerPostLimbDraw",  "OnOcarinaInput",      "OnSceneInit",
};
static const S2HModRequirements sRequirements = { sizeof(S2HModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

s32 Player_SetAction(PlayState* play, Player* this, PlayerActionFunc actionFunc, s32 arg3);
void func_8083B1A0(Player* player, PlayState* play);
bool Player_UpdateUpperBody(Player* player, PlayState* play);
bool Player_IsZTargeting(Player* player);
void Player_StopHorizontalMovement(Player* player);
void Player_AnimSfx_PlayFloorLand(Player* player);
extern PlayerAnimationHeader* ResourceMgr_LoadPlayerAnimAsHeader(const char* animPath);
extern HOST_DATA u8 sCurOcarinaPitch;

static const S2HModApi* sApi;
static PlayerAnimationHeader sClips[CLIP_COUNT];
static s16* sClipData[CLIP_COUNT];
static bool sAreClipsLoaded;

static u8 sMove;
static s16 sMoveFrames;
static s16 sCharge;
static s16 sGlideYaw;
static bool sIsHopArmed;
static bool sIsAPending;
static s16 sAFrames;
static s16 sBoostFrames;
static bool sIsThrowPending;
static bool sHasLoosed;
static bool sIsBowMoving;
static Updraft sUpdraft;
static HomingArrow sArrows[BOW_VOLLEY];

static u8 sHarpPhase;
static bool sIsOcarinaOut;
static volatile bool sWasNoteStruck;
static u8 sLastNote = OCARINA_NOTE_NONE;

static void RitoAction(Player* player, PlayState* play);
static bool TryAirBow(PlayState* play, Player* player);

static bool IsRito(void) {
    return sApi != NULL && sApi->IsFormActive(RITO_FORM_KEY);
}

static void ResolveRitoStrength(bool* should, va_list args) {
    s32* strength = va_arg(args, s32*);

    if (IsRito()) {
        *strength = PLAYER_STRENGTH_DEKU;
    }
}

static bool IsGrounded(Player* player) {
    return (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) != 0;
}

static bool IsRitoBusy(Player* player) {
    return player->actionFunc == RitoAction;
}

static bool IsPressed(PlayState* play, u16 button) {
    return CHECK_BTN_ALL(play->state.input[0].press.button, button);
}

static bool IsHeld(PlayState* play, u16 button) {
    return CHECK_BTN_ALL(play->state.input[0].cur.button, button);
}

static bool CanAct(Player* player) {
    return !(player->stateFlags1 & BUSY_STATE1) && player->csAction == PLAYER_CSACTION_NONE;
}

static bool IsFlightMove(void) {
    return sMove == RITO_MOVE_GLIDE || sMove == RITO_MOVE_LAUNCH || sMove == RITO_MOVE_HOP;
}

static bool IsBowOut(void) {
    return sMove >= RITO_MOVE_BOW_ENTER && sMove <= RITO_MOVE_BOW_AIR_TAP;
}

static f32 GetRocsVelocity(void) {
    return ROCS_HUMAN_VELOCITY;
}

static void LoadClip(RitoClip id) {
    const ClipSource* source = &sClipSources[id];
    PlayerAnimationHeader* raw =
        sApi->HasResource(source->path) ? ResourceMgr_LoadPlayerAnimAsHeader(source->path) : NULL;

    if (raw == NULL || raw->common.frameCount <= 0) {
        return;
    }
    s32 count = raw->common.frameCount;
    s32 frames = MAX((s32)((f32)count / source->speed + 0.5f), 2);
    s16* data = (s16*)malloc((size_t)frames * ANIM_VALUES_PER_FRAME * sizeof(s16));
    if (data == NULL) {
        return;
    }
    const s16* from = (const s16*)raw->segmentVoid;
    for (s32 f = 0; f < frames; f++) {
        s32 sourceFrame = MIN((f * count) / frames, count - 1);
        memcpy(&data[f * ANIM_VALUES_PER_FRAME], &from[sourceFrame * ANIM_VALUES_PER_FRAME],
               ANIM_VALUES_PER_FRAME * sizeof(s16));
    }
    for (s32 f = 1; f < frames; f++) {
        data[f * ANIM_VALUES_PER_FRAME + 0] = data[0];
        data[f * ANIM_VALUES_PER_FRAME + 2] = data[2];
        if (source->isRootPinned) {
            data[f * ANIM_VALUES_PER_FRAME + 1] = data[1];
        }
    }
    sClipData[id] = data;
    sClips[id].common.frameCount = (s16)frames;
    sClips[id].segmentVoid = data;
}

static void LoadClips(void) {
    if (sAreClipsLoaded) {
        return;
    }
    sAreClipsLoaded = true;
    for (s32 id = 0; id < CLIP_COUNT; id++) {
        LoadClip((RitoClip)id);
    }
}

static PlayerAnimationHeader* GetClip(RitoClip id) {
    return sClipData[id] != NULL ? &sClips[id] : NULL;
}

static bool IsPlaying(Player* player, RitoClip id) {
    return sClipData[id] != NULL && player->skelAnime.animation == &sClips[id];
}

static void PlayClip(PlayState* play, Player* player, RitoClip id, u8 mode, f32 morph) {
    PlayerAnimationHeader* clip = GetClip(id);

    if (clip == NULL) {
        return;
    }
    PlayerAnimation_Change(play, &player->skelAnime, clip, 1.0f, 0.0f, Animation_GetLastFrame(clip), mode, morph);
}

static void PlayChargePose(PlayState* play, Player* player) {
    PlayerAnimationHeader* pose = (PlayerAnimationHeader*)gPlayerAnim_link_bow_defense_wait;

    PlayerAnimation_Change(play, &player->skelAnime, pose, 1.0f, 0.0f, Animation_GetLastFrame(pose), ANIMMODE_LOOP,
                           -4.0f);
}

static void HoldMote(Actor* thisx, PlayState* play) {
}

static Actor* SpawnMote(PlayState* play, Vec3f* origin) {
    Actor* mote =
        Actor_Spawn(&play->actorCtx, play, ACTOR_EN_LIGHT, origin->x, origin->y, origin->z, 0, 0, 0, WIND_LIGHT_PARAMS);

    if (mote == NULL) {
        return NULL;
    }
    Actor_SetScale(mote, WIND_LIGHT_SCALE);
    Lights_PointNoGlowSetInfo(&((EnLight*)mote)->lightInfo, origin->x, origin->y, origin->z, 0, 0, 0, 0);
    mote->update = HoldMote;
    return mote;
}

static void StartRibbons(PlayState* play) {
    const Color_RGBA8* color = &sUpdraft.cone.color;
    EffectBlureInit2 init = {
        2,
        0,
        0,
        { color->r, color->g, color->b, 255 },
        { color->r / 2, color->g / 2, color->b / 2, 0 },
        { color->r, color->g, color->b, 0 },
        { color->r / 2, color->g / 2, color->b / 2, 0 },
        8,
        0,
        EFF_BLURE_DRAW_MODE_SIMPLE,
        0,
        { 0, 0, 0, 0 },
        { 0, 0, 0, 0 },
    };

    for (s32 i = 0; i < WIND_RIBBONS; i++) {
        sUpdraft.ribbons[i] = -1;
        Effect_Add(play, &sUpdraft.ribbons[i], EFFECT_BLURE2, 0, 0, &init);
        sUpdraft.ribbonPhase[i] = (f32)i / WIND_RIBBONS;
    }
    sUpdraft.areRibbonsOn = true;
}

static void StopRibbons(PlayState* play) {
    if (!sUpdraft.areRibbonsOn) {
        return;
    }
    for (s32 i = 0; i < WIND_RIBBONS; i++) {
        if (sUpdraft.ribbons[i] >= 0) {
            Effect_Destroy(play, sUpdraft.ribbons[i]);
            sUpdraft.ribbons[i] = -1;
        }
    }
    sUpdraft.areRibbonsOn = false;
}

static void FeedRibbons(PlayState* play) {
    f32 radius = sUpdraft.cone.radius * WIND_RIBBON_SPREAD;
    f32 width = radius * 0.16f + 3.0f;

    if (!sUpdraft.areRibbonsOn) {
        StartRibbons(play);
    }
    for (s32 i = 0; i < WIND_RIBBONS; i++) {
        f32 t = sUpdraft.ribbonPhase[i];
        s16 angle = (s16)((s32)sUpdraft.cone.spin + i * (0x10000 / WIND_RIBBONS) + (s32)(t * WIND_RIBBON_TURNS));
        f32 flare = radius * t * t;
        Vec3f base = sUpdraft.cone.origin;
        base.x += Math_CosS(angle) * flare;
        base.y += sUpdraft.cone.length * t;
        base.z += Math_SinS(angle) * flare;
        Vec3f tip = base;
        tip.y += width;
        EffectBlure* blure = sUpdraft.ribbons[i] >= 0 ? (EffectBlure*)Effect_GetByIndex(sUpdraft.ribbons[i]) : NULL;
        if (blure != NULL) {
            EffectBlure_AddVertex(blure, &tip, &base);
        }
        sUpdraft.ribbonPhase[i] = t + WIND_RIBBON_STEP >= 1.0f ? t + WIND_RIBBON_STEP - 1.0f : t + WIND_RIBBON_STEP;
    }
}

static void StopUpdraft(PlayState* play) {
    sUpdraft.isOn = false;
    StopRibbons(play);
    for (s32 i = 0; i < WIND_MOTES; i++) {
        if (sUpdraft.motes[i] != NULL) {
            Actor_Kill(sUpdraft.motes[i]);
            sUpdraft.motes[i] = NULL;
        }
    }
}

static void ForgetUpdraft(void) {
    memset(&sUpdraft, 0, sizeof(sUpdraft));
}

static void StartUpdraft(PlayState* play, Vec3f* origin) {
    if (sUpdraft.isOn) {
        return;
    }
    sUpdraft.isOn = true;
    sUpdraft.frames = 0;
    sUpdraft.fade = 1.0f;
    sUpdraft.grow = WIND_GROW_MIN;
    sUpdraft.cone.origin = *origin;
    for (s32 i = 0; i < WIND_MOTES; i++) {
        sUpdraft.motePhase[i] = (f32)i * (2.0f * M_PIf / WIND_MOTES) + Rand_ZeroFloat(0.6f);
        sUpdraft.motes[i] = SpawnMote(play, origin);
    }
    Actor_RequestQuakeWithSpeed(play, WIND_QUAKE_AMPLITUDE, WIND_QUAKE_FRAMES, WIND_QUAKE_SPEED);
}

static void OrbitMotes(bool isBursting) {
    for (s32 i = 0; i < WIND_MOTES; i++) {
        Actor* mote = sUpdraft.motes[i];
        if (mote == NULL) {
            continue;
        }
        if (!isBursting && i >= WIND_MOTES_KEPT) {
            Actor_Kill(mote);
            sUpdraft.motes[i] = NULL;
            continue;
        }
        f32 angle = sUpdraft.motePhase[i] + BINANG_TO_RAD(WIND_MOTE_DRIFT) * sUpdraft.frames;
        f32 radius = isBursting ? WIND_MOTE_BURST_RADIUS : WIND_MOTE_RADIUS;
        mote->world.pos = sUpdraft.cone.origin;
        mote->world.pos.x += Math_SinF(angle) * radius;
        mote->world.pos.z += Math_CosF(angle) * radius;
        mote->world.pos.y += WIND_MOTE_HEIGHT;
        Actor_SetScale(mote, WIND_LIGHT_SCALE * sUpdraft.fade);
    }
}

static void TickUpdraft(PlayState* play, bool isHolding, f32 fill) {
    if (!sUpdraft.isOn) {
        return;
    }
    bool isBursting = sUpdraft.frames < WIND_BURST_FRAMES;
    sUpdraft.frames++;
    if (isHolding) {
        sUpdraft.fade = 1.0f;
        sUpdraft.grow = WIND_GROW_MIN + (1.0f - WIND_GROW_MIN) * CLAMP(fill, 0.0f, 1.0f);
    } else {
        sUpdraft.fade -= 1.0f / WIND_FADE_FRAMES;
        if (sUpdraft.fade <= 0.0f) {
            StopUpdraft(play);
            return;
        }
    }
    sUpdraft.cone.length = WIND_HEIGHT * sUpdraft.grow;
    sUpdraft.cone.radius = WIND_RADIUS * sUpdraft.grow;
    sUpdraft.cone.color.r = 190;
    sUpdraft.cone.color.g = 255;
    sUpdraft.cone.color.b = 200;
    sUpdraft.cone.color.a = (u8)(WIND_ALPHA * sUpdraft.fade);
    sUpdraft.cone.spin += WIND_SPIN;
    sUpdraft.cone.scrollT = (s16)((sUpdraft.cone.scrollT + WIND_SCROLL) % (TORNADO_TEX_HEIGHT * 4));
    Audio_PlaySfx_AtPos(&sUpdraft.cone.origin, WIND_LOOP_SFX);
    FeedRibbons(play);
    OrbitMotes(isBursting);
}

static void DrawUpdraft(PlayState* play) {
    if (!sUpdraft.isOn || !sApi->HasResource(sTornadoMatDL) || !sApi->HasResource(sTornadoTriDL)) {
        return;
    }
    const WindCone* cone = &sUpdraft.cone;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    Matrix_Push();
    Matrix_Translate(cone->origin.x, cone->origin.y, cone->origin.z, MTXMODE_NEW);
    Matrix_RotateXF(DEG_TO_RAD(90.0f) + BINANG_TO_RAD(WIND_PITCH_UP), MTXMODE_APPLY);
    Matrix_RotateYF(BINANG_TO_RAD(cone->spin), MTXMODE_APPLY);
    Matrix_Scale(cone->radius / TORNADO_MODEL_RADIUS, cone->length / TORNADO_MODEL_LENGTH,
                 cone->radius / TORNADO_MODEL_RADIUS, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sTornadoMatDL);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, cone->color.r, cone->color.g, cone->color.b, cone->color.a);
    gSPDisplayList(POLY_XLU_DISP++,
                   Gfx_TexScroll(play->state.gfxCtx, 0, (u32)cone->scrollT, TORNADO_TEX_WIDTH, TORNADO_TEX_HEIGHT));
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sTornadoTriDL);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

static void StartMove(PlayState* play, Player* player, u8 move) {
    if (player->actionFunc != RitoAction) {
        Player_SetAction(play, player, RitoAction, 0);
    }
    sMove = move;
    sMoveFrames = 0;
    sBoostFrames = 0;
    sIsAPending = false;
}

static void ClearMove(Player* player) {
    sMove = RITO_MOVE_NONE;
    sMoveFrames = 0;
    sBoostFrames = 0;
    player->actor.gravity = JUMP_GRAVITY;
    player->actor.terminalVelocity = -20.0f;
}

static void EndMove(PlayState* play, Player* player) {
    ClearMove(player);
    func_8083B1A0(player, play);
}

static void TakeOff(PlayState* play, Player* player, u8 move) {
    StartMove(play, player, move);
    player->actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND;
    Camera_ChangeMode(GET_ACTIVE_CAM(play), CAM_MODE_JUMP);
    sGlideYaw = player->actor.shape.rot.y;
}

static bool SpendMagic(PlayState* play, s16 cost) {
    return gSaveContext.magicCapacity > 0 && Magic_Consume(play, cost, MAGIC_CONSUME_NOW);
}

static void StartGlide(PlayState* play, Player* player) {
    if (GetClip(CLIP_FLY) == NULL) {
        return;
    }
    TakeOff(play, player, RITO_MOVE_GLIDE);
    PlayClip(play, player, CLIP_FLY, ANIMMODE_LOOP, -4.0f);
}

static void StartCharge(PlayState* play, Player* player) {
    StartMove(play, player, RITO_MOVE_CHARGE);
    sCharge = 0;
    player->speedXZ = 0.0f;
    PlayChargePose(play, player);
    StartUpdraft(play, &player->actor.world.pos);
}

static void Launch(PlayState* play, Player* player) {
    f32 fill = MIN((f32)sCharge / CHARGE_FULL, 1.0f);
    f32 top = GetRocsVelocity() * LAUNCH_ROCS_MULTIPLE;

    TakeOff(play, player, RITO_MOVE_LAUNCH);
    player->actor.velocity.y = LAUNCH_MIN + (top - LAUNCH_MIN) * fill;
    player->actor.gravity = 0.0f;
    PlayClip(play, player, CLIP_LAUNCH, ANIMMODE_ONCE, -4.0f);
    Actor_PlaySfx(&player->actor, NA_SE_PL_ROLL);
}

static void RunCharge(PlayState* play, Player* player) {
    sCharge++;
    player->speedXZ = 0.0f;
    PlayerAnimation_Update(play, &player->skelAnime);
    if (sCharge % CHARGE_FLAP_PERIOD == 0) {
        Actor_PlaySfx(&player->actor, NA_SE_EN_KAICHO_FLUTTER);
    }
    if (IsHeld(play, BTN_A) && sCharge < CHARGE_FULL) {
        return;
    }
    if (sCharge < CHARGE_MIN) {
        EndMove(play, player);
        return;
    }
    if (!SpendMagic(play, LAUNCH_MAGIC_COST)) {
        Audio_PlaySfx(NA_SE_SY_ERROR);
        EndMove(play, player);
        return;
    }
    Launch(play, player);
}

static void RunLaunch(PlayState* play, Player* player) {
    if (TryAirBow(play, player)) {
        return;
    }
    Player_UpdateUpperBody(player, play);
    PlayerAnimation_Update(play, &player->skelAnime);
    player->actor.velocity.y -= LAUNCH_DECAY;
    if (player->actor.velocity.y > 1.0f && ++sMoveFrames <= LAUNCH_MAX_FRAMES) {
        return;
    }
    if (IsHeld(play, BTN_A)) {
        StartGlide(play, player);
    } else {
        EndMove(play, player);
    }
}

static void SteerGlide(PlayState* play, Player* player) {
    f32 stickX = play->state.input[0].rel.stick_x;

    if (fabsf(stickX) > GLIDE_TURN_DEADZONE) {
        sGlideYaw -= (s16)(stickX * GLIDE_TURN_RATE);
    }
    player->actor.world.rot.y = sGlideYaw;
    player->actor.shape.rot.y = sGlideYaw;
    player->yaw = sGlideYaw;
    player->speedXZ = GLIDE_SPEED;
    player->actor.gravity = 0.0f;
}

static void RiseOnBoost(PlayState* play, Player* player) {
    player->actor.velocity.y -= LAUNCH_DECAY;
    if (--sBoostFrames > 0 && player->actor.velocity.y > GLIDE_SINK) {
        return;
    }
    sBoostFrames = 0;
    PlayClip(play, player, CLIP_FLY, ANIMMODE_LOOP, -4.0f);
}

static void RunGlide(PlayState* play, Player* player) {
    if (IsGrounded(player)) {
        Player_AnimSfx_PlayFloorLand(player);
        EndMove(play, player);
        return;
    }
    if (!IsHeld(play, BTN_A)) {
        EndMove(play, player);
        return;
    }
    if (TryAirBow(play, player)) {
        return;
    }
    SteerGlide(play, player);
    if (sBoostFrames > 0) {
        RiseOnBoost(play, player);
    } else {
        player->actor.velocity.y = GLIDE_SINK;
    }
    Player_UpdateUpperBody(player, play);
    PlayerAnimation_Update(play, &player->skelAnime);
    if (++sMoveFrames % FLAP_SFX_PERIOD == 0) {
        Actor_PlaySfx(&player->actor, NA_SE_EN_KAICHO_FLUTTER);
    }
}

static void StartHop(PlayState* play, Player* player, s32 direction) {
    bool isRoll = direction == PLAYER_STICK_DIR_FORWARD;

    TakeOff(play, player, RITO_MOVE_HOP);
    player->actor.velocity.y = GetRocsVelocity() * (isRoll ? HOP_ROLL_SCALE : 1.0f);
    player->actor.gravity = JUMP_GRAVITY;
    player->speedXZ = 0.0f;
    if (isRoll || (direction & 1)) {
        player->yaw = player->actor.shape.rot.y + (isRoll ? 0 : (direction << 0xE));
        player->speedXZ = isRoll ? HOP_ROLL_SPEED : HOP_SIDE_SPEED;
        player->actor.world.rot.y = player->yaw;
    }
    sGlideYaw = player->yaw;
    sIsHopArmed = false;
    PlayClip(play, player, (RitoClip)(CLIP_HOP_FRONT + (direction & 3)), ANIMMODE_ONCE, -4.0f);
    Actor_PlaySfx(&player->actor, NA_SE_PL_SKIP);
}

static void RunHop(PlayState* play, Player* player) {
    bool isAHeld = IsHeld(play, BTN_A);

    sIsHopArmed = sIsHopArmed || !isAHeld;
    if (sIsHopArmed && isAHeld && GetClip(CLIP_FLY) != NULL) {
        StartGlide(play, player);
        return;
    }
    if (IsGrounded(player) && ++sMoveFrames > HOP_LAND_GUARD) {
        Player_AnimSfx_PlayFloorLand(player);
        EndMove(play, player);
        return;
    }
    if (TryAirBow(play, player)) {
        return;
    }
    player->yaw = sGlideYaw;
    Player_UpdateUpperBody(player, play);
    bool isStrikeDone = PlayerAnimation_Update(play, &player->skelAnime);
    if (isStrikeDone && !IsPlaying(player, CLIP_HOP_SETTLE)) {
        PlayClip(play, player, CLIP_HOP_SETTLE, ANIMMODE_LOOP, -4.0f);
    }
}

static void SpawnRocsSparkles(PlayState* play, Player* player) {
    Vec3f zero = { 0.0f, 0.0f, 0.0f };
    Color_RGBA8 prim = { 255, 255, 200, 255 };
    Color_RGBA8 env = { 200, 200, 100, 0 };

    for (s32 i = 0; i < 5; i++) {
        s16 angle = (s16)(Rand_ZeroOne() * 0xFFFF);
        f32 distance = 5.0f + Rand_ZeroOne() * 5.0f;
        Vec3f pos = { player->actor.world.pos.x + Math_SinS(angle) * distance, player->actor.world.pos.y + 5.0f,
                      player->actor.world.pos.z + Math_CosS(angle) * distance };
        EffectSsKirakira_SpawnSmall(play, &pos, &zero, &zero, &prim, &env);
    }
}

static u16 FindRocsButtons(void) {
    u16 buttons = 0;

    if (!S2H_MOD_API_HAS(sApi, GetEquippedCustomItem)) {
        return 0;
    }
    for (u8 button = 0; button < S2H_ITEM_BUTTON_COUNT; button++) {
        const char* equipped = sApi->GetEquippedCustomItem(button);
        if (equipped != NULL && (strcmp(equipped, ROCS_FEATHER_KEY) == 0 || strcmp(equipped, ROCS_CAPE_KEY) == 0)) {
            buttons |= sItemButtons[button];
        }
    }
    return buttons;
}

static void RocsJump(PlayState* play, Player* player) {
    TakeOff(play, player, RITO_MOVE_HOP);
    player->actor.velocity.y = ROCS_JUMP_VELOCITY;
    player->actor.gravity = JUMP_GRAVITY;
    sGlideYaw = player->yaw;
    sIsHopArmed = false;
    PlayClip(play, player, CLIP_HOP_FRONT, ANIMMODE_ONCE, -4.0f);
    Actor_PlaySfx(&player->actor, NA_SE_PL_SKIP);
    SpawnRocsSparkles(play, player);
}

static void TryRocs(PlayState* play, Player* player) {
    u16 buttons = FindRocsButtons();

    if (buttons == 0 || !(play->state.input[0].press.button & buttons) || (IsRitoBusy(player) && !IsFlightMove()) ||
        !CanAct(player)) {
        return;
    }
    if (IsGrounded(player)) {
        RocsJump(play, player);
        return;
    }
    if (!SpendMagic(play, ROCS_AIR_MAGIC_COST)) {
        Audio_PlaySfx(NA_SE_SY_ERROR);
        return;
    }
    if (sMove != RITO_MOVE_GLIDE) {
        RocsJump(play, player);
        return;
    }
    sBoostFrames = ROCS_BOOST_FRAMES;
    player->actor.velocity.y = ROCS_JUMP_VELOCITY;
    StartUpdraft(play, &player->actor.world.pos);
    PlayClip(play, player, CLIP_LAUNCH, ANIMMODE_ONCE, -4.0f);
    Actor_PlaySfx(&player->actor, NA_SE_PL_ROLL);
    SpawnRocsSparkles(play, player);
}

static bool IsThrowable(s32 item) {
    return item == ITEM_BOMB || item == ITEM_BOMBCHU || item == ITEM_DEKU_NUT || item == ITEM_SLINGSHOT;
}

static void NoteAirThrow(bool* should, va_list args) {
    s32 item = va_arg(args, s32);

    if (!*should || !IsRito() || gPlayState == NULL || IsGrounded(GET_PLAYER(gPlayState)) || !IsThrowable(item)) {
        return;
    }
    sIsThrowPending = true;
}

static void StartThrow(PlayState* play, Player* player) {
    sIsThrowPending = false;
    if (IsGrounded(player) || GetClip(CLIP_THROW) == NULL || (IsRitoBusy(player) && !IsFlightMove())) {
        return;
    }
    sBoostFrames = 0;
    StartMove(play, player, RITO_MOVE_THROW);
    player->actor.gravity = JUMP_GRAVITY;
    PlayClip(play, player, CLIP_THROW, ANIMMODE_ONCE, -4.0f);
}

static void RunThrow(PlayState* play, Player* player) {
    if (IsGrounded(player)) {
        EndMove(play, player);
        return;
    }
    Player_UpdateUpperBody(player, play);
    if (!PlayerAnimation_Update(play, &player->skelAnime)) {
        return;
    }
    if (IsHeld(play, BTN_A)) {
        StartGlide(play, player);
    } else {
        EndMove(play, player);
    }
}

static void GetAimPoint(Actor* actor, Vec3f* out) {
    if (actor->id == ACTOR_OBJ_SWITCH) {
        *out = actor->world.pos;
        out->y += 20.0f;
        return;
    }
    *out = actor->focus.pos;
}

static bool IsShootableSwitch(Actor* actor) {
    s32 type = actor->params & 7;
    return actor->id == ACTOR_OBJ_SWITCH &&
           (type == OBJSWITCH_TYPE_EYE || type == OBJSWITCH_TYPE_CRYSTAL || type == OBJSWITCH_TYPE_CRYSTAL_TARGETABLE);
}

static bool IsInSight(Actor* actor, s16 faceYaw) {
    if (actor->update == NULL || actor->xyzDistToPlayerSq > SQ(BOW_RANGE)) {
        return false;
    }
    s16 offset = actor->yawTowardsPlayer + 0x8000 - faceYaw;
    return ABS(offset) < BOW_CONE;
}

static bool IsAlreadyChosen(Actor* actor, Actor** chosen, s32 count) {
    for (s32 i = 0; i < count; i++) {
        if (chosen[i] == actor) {
            return true;
        }
    }
    return false;
}

static s32 FindTargets(PlayState* play, Actor** out, s32 max, s16 faceYaw) {
    static const u8 sPasses[] = { ACTORCAT_SWITCH, ACTORCAT_PROP, ACTORCAT_BG, ACTORCAT_ENEMY };
    s32 found = 0;

    for (u32 pass = 0; pass < ARRAY_COUNT(sPasses) && found < max; pass++) {
        bool wantsSwitch = sPasses[pass] != ACTORCAT_ENEMY;
        while (found < max) {
            Actor* best = NULL;
            f32 bestDist = SQ(BOW_RANGE);
            for (Actor* actor = play->actorCtx.actorLists[sPasses[pass]].first; actor != NULL; actor = actor->next) {
                if ((wantsSwitch && !IsShootableSwitch(actor)) || !IsInSight(actor, faceYaw) ||
                    actor->xyzDistToPlayerSq >= bestDist || IsAlreadyChosen(actor, out, found)) {
                    continue;
                }
                best = actor;
                bestDist = actor->xyzDistToPlayerSq;
            }
            if (best == NULL) {
                break;
            }
            out[found++] = best;
        }
    }
    return found;
}

static void ClearArrows(void) {
    memset(sArrows, 0, sizeof(sArrows));
}

static bool IsInList(PlayState* play, u8 category, Actor* wanted) {
    for (Actor* actor = play->actorCtx.actorLists[category].first; actor != NULL; actor = actor->next) {
        if (actor == wanted) {
            return true;
        }
    }
    return false;
}

static void ShatterIce(PlayState* play, Vec3f* at) {
    static Color_RGBA8 sIceWhite = { 250, 250, 250, 255 };
    static Color_RGBA8 sIceGray = { 180, 200, 230, 255 };

    for (s32 category = 0; category < ACTORCAT_MAX; category++) {
        Actor* actor = play->actorCtx.actorLists[category].first;
        while (actor != NULL) {
            Actor* next = actor->next;
            if (actor->id == ACTOR_OBJ_ICE_POLY && actor->update != NULL &&
                Math_Vec3f_DistXYZ(at, &actor->world.pos) < BOW_ICE_RADIUS) {
                Vec3f accel = { 0.0f, -1.0f, 0.0f };
                for (s32 i = 0; i < 8; i++) {
                    Vec3f pos = actor->world.pos;
                    Vec3f vel = { Rand_CenteredFloat(6.0f), Rand_ZeroOne() * 6.0f, Rand_CenteredFloat(6.0f) };
                    pos.x += Rand_CenteredFloat(40.0f);
                    pos.y += Rand_ZeroOne() * 70.0f;
                    pos.z += Rand_CenteredFloat(40.0f);
                    EffectSsIceSmoke_Spawn(play, &pos, &vel, &accel, 350);
                }
                Audio_PlaySfx_AtPos(&actor->world.pos, NA_SE_EV_ICE_BROKEN);
                if (actor->parent != NULL) {
                    actor->parent->params &= ~0x80;
                }
                Actor_Kill(actor);
            }
            actor = next;
        }
    }
}

static void SteerArrow(PlayState* play, HomingArrow* slot) {
    Actor* arrow = slot->arrow;

    if (!IsInList(play, slot->targetCategory, slot->target)) {
        slot->target = NULL;
        return;
    }
    GetAimPoint(slot->target, &slot->goal);
    f32 dist = Math_Vec3f_DistXYZ(&arrow->world.pos, &slot->goal);
    if (dist < BOW_HIT_DIST) {
        ShatterIce(play, &slot->goal);
        slot->arrow = NULL;
        return;
    }
    s16 turn = dist < BOW_SNAP_DIST ? 0x7FFF : BOW_TURN;
    s16 yaw = Math_Vec3f_Yaw(&arrow->world.pos, &slot->goal) - arrow->world.rot.y;
    s16 pitch = Math_Vec3f_Pitch(&arrow->world.pos, &slot->goal) - arrow->world.rot.x;
    arrow->world.rot.y += CLAMP(yaw, -turn, turn);
    arrow->world.rot.x += CLAMP(pitch, -turn, turn);
    arrow->shape.rot = arrow->world.rot;
    Actor_SetSpeeds(arrow, BOW_ARROW_SPEED);
}

static void TickArrows(PlayState* play) {
    for (s32 i = 0; i < BOW_VOLLEY; i++) {
        HomingArrow* slot = &sArrows[i];
        if (slot->arrow == NULL) {
            continue;
        }
        if (!IsInList(play, ACTORCAT_ITEMACTION, slot->arrow)) {
            slot->arrow = NULL;
            continue;
        }
        if (slot->target != NULL) {
            SteerArrow(play, slot);
        }
    }
}

static Actor* SpawnArrow(PlayState* play, Player* player, s16 yaw, s16 pitch, s32 type) {
    Vec3f* from = &player->bodyPartsPos[PLAYER_BODYPART_LEFT_HAND];

    player->unk_D57 = BOW_HANDSHAKE;
    Actor* arrow = Actor_Spawn(&play->actorCtx, play, ACTOR_EN_ARROW, from->x, from->y, from->z, pitch, yaw, 0, type);
    if (arrow == NULL) {
        return NULL;
    }
    for (s32 i = 0; i < BOW_VOLLEY; i++) {
        if (sArrows[i].arrow == NULL) {
            sArrows[i].arrow = arrow;
            sArrows[i].target = NULL;
            break;
        }
    }
    return arrow;
}

static void AimArrow(Actor* arrow, Actor* target) {
    for (s32 i = 0; i < BOW_VOLLEY; i++) {
        if (sArrows[i].arrow == arrow) {
            sArrows[i].target = target;
            sArrows[i].targetCategory = target->category;
            GetAimPoint(target, &sArrows[i].goal);
            return;
        }
    }
}

static void LooseVolley(PlayState* play, Player* player) {
    Actor* targets[BOW_TWIN_SHOTS];
    Actor* locked = player->focusActor;
    s16 faceYaw = player->actor.shape.rot.y;
    s32 shots = sCharge >= BOW_CHARGE_TWIN ? BOW_TWIN_SHOTS : 1;
    bool isHoming = shots > 1 || locked != NULL;
    s32 count = (locked == NULL && isHoming) ? FindTargets(play, targets, shots, faceYaw) : 0;

    ClearArrows();
    for (s32 i = 0; i < shots; i++) {
        s16 spoke = faceYaw + (s16)(((i * 2 - (shots - 1)) * BOW_FAN_STEP) / 2);
        Actor* arrow = SpawnArrow(play, player, spoke, 0, ARROW_TYPE_NORMAL);
        Actor* target = locked != NULL ? locked : (i < count ? targets[i] : NULL);
        if (arrow != NULL && isHoming && target != NULL) {
            AimArrow(arrow, target);
        }
    }
}

static bool MoveWhileAiming(PlayState* play, Player* player, bool isAdvancing) {
    Input* input = &play->state.input[0];
    f32 stickMagnitude = sqrtf(SQ((f32)input->rel.stick_x) + SQ((f32)input->rel.stick_y));
    s16 stickAngle = Math_Atan2S(input->rel.stick_y, -input->rel.stick_x);

    player->speedXZ = isAdvancing ? BOW_MOVE_SPEED : 0.0f;
    if (stickMagnitude < BOW_MOVE_DEADZONE) {
        return false;
    }
    Math_ScaledStepToS(&player->yaw, Camera_GetInputDirYaw(GET_ACTIVE_CAM(play)) + stickAngle, BOW_TURN_RATE);
    player->actor.world.rot.y = player->yaw;
    player->actor.shape.rot.y = player->yaw;
    return true;
}

static void StartBow(PlayState* play, Player* player) {
    sIsBowMoving = fabsf(player->speedXZ) > 1.0f;
    StartMove(play, player, RITO_MOVE_BOW_ENTER);
    sCharge = 0;
    PlayClip(play, player, sIsBowMoving ? CLIP_BOW_ENTER_MOVE : CLIP_BOW_ENTER_STILL, ANIMMODE_ONCE, -4.0f);
}

static void StartBowAir(PlayState* play, Player* player) {
    StartMove(play, player, RITO_MOVE_BOW_AIR);
    sCharge = 0;
    sHasLoosed = false;
    PlayClip(play, player, CLIP_BOW_AIR, ANIMMODE_ONCE, -4.0f);
}

static void StartLoose(PlayState* play, Player* player) {
    bool isTap = sCharge < BOW_TAP_FRAMES && GetClip(CLIP_BOW_SEED) != NULL;

    sMove = isTap ? RITO_MOVE_BOW_SEED : RITO_MOVE_BOW_RELEASE;
    sMoveFrames = 0;
    sHasLoosed = false;
    PlayClip(play, player, isTap ? CLIP_BOW_SEED : CLIP_BOW_RELEASE, ANIMMODE_ONCE, -4.0f);
}

static void SwapAimLoop(PlayState* play, Player* player, bool isMoving) {
    sIsBowMoving = isMoving;
    sMove = RITO_MOVE_BOW_HOLD;
    PlayClip(play, player, isMoving ? CLIP_BOW_HOLD_MOVE : CLIP_BOW_HOLD_STILL, ANIMMODE_LOOP, -4.0f);
}

static void RunBowAim(PlayState* play, Player* player) {
    bool wantsMove = MoveWhileAiming(play, player, sIsBowMoving);

    if (!IsHeld(play, BTN_B)) {
        StartLoose(play, player);
        return;
    }
    sCharge++;
    if (wantsMove != sIsBowMoving) {
        SwapAimLoop(play, player, wantsMove);
        return;
    }
    if (PlayerAnimation_Update(play, &player->skelAnime) && sMove == RITO_MOVE_BOW_ENTER) {
        SwapAimLoop(play, player, sIsBowMoving);
    }
}

static void RunBowRelease(PlayState* play, Player* player) {
    f32 lead =
        GetClip(CLIP_BOW_RELEASE) != NULL ? Animation_GetLastFrame(GetClip(CLIP_BOW_RELEASE)) - BOW_LOOSE_LEAD : 0.0f;

    if (sMoveFrames++ == 0) {
        player->actor.world.rot.y = player->focusActor != NULL
                                        ? Math_Vec3f_Yaw(&player->focusActor->world.pos, &player->actor.world.pos)
                                        : (s16)(player->actor.shape.rot.y + 0x8000);
        player->yaw = player->actor.world.rot.y;
        player->speedXZ = BOW_HOP_SPEED;
        if (player->invincibilityTimer > BOW_DODGE_IFRAMES) {
            player->invincibilityTimer = BOW_DODGE_IFRAMES;
        }
    }
    Math_StepToF(&player->speedXZ, 0.0f, BOW_HOP_DECAY);
    if (!sHasLoosed && player->skelAnime.curFrame >= lead) {
        sHasLoosed = true;
        LooseVolley(play, player);
    }
    if (PlayerAnimation_Update(play, &player->skelAnime)) {
        if (!sHasLoosed) {
            LooseVolley(play, player);
        }
        EndMove(play, player);
    }
}

static void RunBowSeed(PlayState* play, Player* player) {
    player->speedXZ = 0.0f;
    if (!sHasLoosed && ++sMoveFrames >= BOW_LOOSE_FRAME) {
        sHasLoosed = true;
        ClearArrows();
        SpawnArrow(play, player, player->actor.shape.rot.y, 0, ARROW_TYPE_SLINGSHOT);
        Player_PlaySfx(player, NA_SE_VO_LI_SWORD_N);
    }
    if (PlayerAnimation_Update(play, &player->skelAnime)) {
        EndMove(play, player);
    }
}

static void LooseAirTap(PlayState* play, Player* player) {
    Actor* target = player->focusActor;
    s16 yaw = player->actor.shape.rot.y;
    s16 pitch = 0;

    if (target == NULL) {
        FindTargets(play, &target, 1, yaw);
    }
    if (target != NULL) {
        Vec3f goal;
        GetAimPoint(target, &goal);
        yaw = Math_Vec3f_Yaw(&player->bodyPartsPos[PLAYER_BODYPART_LEFT_HAND], &goal);
        pitch = Math_Vec3f_Pitch(&player->bodyPartsPos[PLAYER_BODYPART_LEFT_HAND], &goal);
    }
    Actor* arrow = SpawnArrow(play, player, yaw, pitch, ARROW_TYPE_NORMAL);
    if (arrow != NULL && target != NULL) {
        AimArrow(arrow, target);
    }
}

static void LooseAirVolley(PlayState* play, Player* player) {
    for (s32 i = 0; i < BOW_VOLLEY; i++) {
        SpawnArrow(play, player, player->actor.shape.rot.y, (s16)(0x4000 + (i - 1) * BOW_FAN_STEP), ARROW_TYPE_NORMAL);
    }
}

static void RunBowAir(PlayState* play, Player* player) {
    bool isBHeld = IsHeld(play, BTN_B);

    player->actor.velocity.y = 0.0f;
    player->actor.gravity = 0.0f;
    player->speedXZ = 0.0f;
    if (isBHeld) {
        sCharge++;
    }
    if (sMove == RITO_MOVE_BOW_AIR && !isBHeld && sCharge < BOW_TAP_FRAMES && GetClip(CLIP_BOW_AIR_TAP) != NULL) {
        sMove = RITO_MOVE_BOW_AIR_TAP;
        sMoveFrames = 0;
        PlayClip(play, player, CLIP_BOW_AIR_TAP, ANIMMODE_ONCE, -4.0f);
        return;
    }
    bool isShotKnown = sMove == RITO_MOVE_BOW_AIR_TAP || sCharge >= BOW_TAP_FRAMES;
    if (!sHasLoosed && ++sMoveFrames >= BOW_LOOSE_FRAME && isShotKnown) {
        sHasLoosed = true;
        ClearArrows();
        if (sMove == RITO_MOVE_BOW_AIR_TAP) {
            LooseAirTap(play, player);
        } else {
            LooseAirVolley(play, player);
        }
    }
    if (!PlayerAnimation_Update(play, &player->skelAnime)) {
        return;
    }
    if (!sHasLoosed) {
        LooseAirVolley(play, player);
    }
    if (IsHeld(play, BTN_A)) {
        StartGlide(play, player);
    } else {
        EndMove(play, player);
    }
}

static bool TryAirBow(PlayState* play, Player* player) {
    if (!IsPressed(play, BTN_B) || GetClip(CLIP_BOW_AIR) == NULL) {
        return false;
    }
    StartBowAir(play, player);
    return true;
}

static void RunSink(PlayState* play, Player* player) {
    Player_StopHorizontalMovement(player);
    PlayerAnimation_Update(play, &player->skelAnime);
    if (++sMoveFrames == SINK_FRAMES) {
        Audio_PlaySfx(NA_SE_OC_ABYSS);
        func_80169EFC(play);
    }
}

static void TrySink(PlayState* play, Player* player) {
    if (sMove == RITO_MOVE_SINK || !(player->stateFlags1 & STATE1_SWIMMING) ||
        player->actor.depthInWater <= SINK_DEPTH ||
        (player->stateFlags1 & (STATE1_DEAD_OR_CUTSCENE | PLAYER_STATE1_400))) {
        return;
    }
    sMove = RITO_MOVE_SINK;
    sMoveFrames = 0;
    player->actor.velocity.y = SINK_START;
    player->actor.gravity = SINK_GRAVITY;
    PlayClip(gPlayState, player, CLIP_FLY, ANIMMODE_LOOP, -4.0f);
    Actor_PlaySfx(&player->actor, NA_SE_EV_DIVE_INTO_WATER);
}

static void DriveSink(bool* should, va_list args) {
    Player* player = va_arg(args, Player*);

    if (!IsRito() || sMove != RITO_MOVE_SINK || gPlayState == NULL || (player->stateFlags1 & STATE1_DEAD_OR_CUTSCENE)) {
        return;
    }
    *should = false;
    RunSink(gPlayState, player);
}

static void RitoAction(Player* player, PlayState* play) {
    switch (sMove) {
        case RITO_MOVE_CHARGE:
            RunCharge(play, player);
            break;
        case RITO_MOVE_LAUNCH:
            RunLaunch(play, player);
            break;
        case RITO_MOVE_GLIDE:
            RunGlide(play, player);
            break;
        case RITO_MOVE_HOP:
            RunHop(play, player);
            break;
        case RITO_MOVE_THROW:
            RunThrow(play, player);
            break;
        case RITO_MOVE_BOW_ENTER:
        case RITO_MOVE_BOW_HOLD:
            RunBowAim(play, player);
            break;
        case RITO_MOVE_BOW_RELEASE:
            RunBowRelease(play, player);
            break;
        case RITO_MOVE_BOW_SEED:
            RunBowSeed(play, player);
            break;
        case RITO_MOVE_BOW_AIR:
        case RITO_MOVE_BOW_AIR_TAP:
            RunBowAir(play, player);
            break;
        default:
            EndMove(play, player);
            break;
    }
}

static bool CanHoldA(Player* player) {
    return !IsRitoBusy(player) && CanAct(player) && IsGrounded(player) && !Player_IsZTargeting(player);
}

static void HoldA(void) {
    sIsAPending = true;
    sAFrames = 0;
}

static int32_t HandleRoll(PlayState* play, Player* player) {
    if (!IsPressed(play, BTN_A) || !CanHoldA(player)) {
        return S2H_FORM_ACTION_VANILLA;
    }
    HoldA();
    return S2H_FORM_ACTION_BLOCKED;
}

static void TrackA(PlayState* play, Player* player) {
    if (!sIsAPending) {
        return;
    }
    if (!CanHoldA(player)) {
        sIsAPending = false;
        return;
    }
    if (IsHeld(play, BTN_A)) {
        if (++sAFrames >= A_HOLD_FRAMES) {
            sIsAPending = false;
            StartCharge(play, player);
        }
        return;
    }
    sIsAPending = false;
    StartHop(play, player, PLAYER_STICK_DIR_FORWARD);
}

static int32_t HandleZTargetA(PlayState* play, Player* player) {
    s8 direction = player->controlStickDirections[player->controlStickDataIndex];

    if (!IsPressed(play, BTN_A) || !CanAct(player) || !IsGrounded(player) || direction <= PLAYER_STICK_DIR_FORWARD) {
        return S2H_FORM_ACTION_VANILLA;
    }
    StartHop(play, player, direction);
    return S2H_FORM_ACTION_STARTED;
}

static int32_t HandleB(PlayState* play, Player* player) {
    if (!IsPressed(play, BTN_B) || !CanAct(player) || !IsGrounded(player) || GetClip(CLIP_BOW_HOLD_STILL) == NULL) {
        return S2H_FORM_ACTION_VANILLA;
    }
    StartBow(play, player);
    return S2H_FORM_ACTION_STARTED;
}

static void TryFreeMoves(PlayState* play, Player* player) {
    if (IsRitoBusy(player) || !CanAct(player)) {
        return;
    }
    if (IsGrounded(player)) {
        if (IsPressed(play, BTN_A) && !sIsAPending && CanHoldA(player)) {
            HoldA();
        }
        return;
    }
    if (TryAirBow(play, player)) {
        return;
    }
    if (IsPressed(play, BTN_A)) {
        StartGlide(play, player);
    }
}

static void DropInterruptedMove(Player* player) {
    if (sMove == RITO_MOVE_NONE || sMove == RITO_MOVE_SINK || IsRitoBusy(player)) {
        return;
    }
    ClearMove(player);
}

static void ResolveLanding(s32 group, s32 animType, PlayerAnimationHeader** anim) {
    if (!IsRito() || (group != PLAYER_ANIMGROUP_landing && group != PLAYER_ANIMGROUP_short_landing)) {
        return;
    }
    PlayerAnimationHeader* land = GetClip(CLIP_LAND);
    if (land != NULL) {
        *anim = land;
    }
}

static bool IsHoldingOcarina(Player* player) {
    return player->rightHandType == PLAYER_MODELTYPE_RH_INSTRUMENT;
}

static bool IsPlayingOcarinaClip(Player* player, const char* name) {
    const char* anim = (const char*)player->skelAnime.animation;

    return anim != NULL && strstr(anim, name) != NULL;
}

// Runs on the audio thread, so it only records the edge; the pose consumes it on the game frame.
static void NoteStruck(void) {
    u8 note = sCurOcarinaPitch;

    if (note != OCARINA_NOTE_NONE && note != sLastNote) {
        sWasNoteStruck = true;
    }
    sLastNote = note;
}

static bool SwapOcarinaClip(PlayState* play, Player* player) {
    PlayerAnimationHeader* raise = GetClip(CLIP_HARP_RAISE);

    if (IsPlayingOcarinaClip(player, "okarina_start")) {
        PlayClip(play, player, CLIP_HARP_RAISE, ANIMMODE_ONCE, -6.0f);
        sHarpPhase = HARP_RAISING;
        return true;
    }
    if (IsPlayingOcarinaClip(player, "okarina_end")) {
        PlayerAnimation_Change(play, &player->skelAnime, raise, -1.0f, Animation_GetLastFrame(raise), 0.0f,
                               ANIMMODE_ONCE, -6.0f);
        sHarpPhase = HARP_LOWERING;
        return true;
    }
    return false;
}

static void PoseHarp(PlayState* play, Player* player) {
    f32 pluckEnd = Animation_GetLastFrame(GetClip(CLIP_HARP_PLAY)) - CLIP_FRAMES_PER_UPDATE;

    if (!IsPlaying(player, CLIP_HARP_PLAY)) {
        PlayClip(play, player, CLIP_HARP_PLAY, ANIMMODE_LOOP, -4.0f);
        player->skelAnime.playSpeed = 0.0f;
        sHarpPhase = HARP_PLAYING;
    }
    bool isPluckDone = player->skelAnime.curFrame >= pluckEnd;
    if (sWasNoteStruck) {
        if (isPluckDone) {
            player->skelAnime.curFrame = 0.0f;
        }
        player->skelAnime.playSpeed = 1.0f;
    } else if (isPluckDone) {
        player->skelAnime.playSpeed = 0.0f;
    }
    sWasNoteStruck = false;
}

static void UpdateHarp(PlayState* play, Player* player) {
    bool isSessionOpen = play->msgCtx.msgMode != MSGMODE_NONE;

    if (IsHoldingOcarina(player)) {
        sIsOcarinaOut = true;
    } else if (!isSessionOpen) {
        if (sIsOcarinaOut) {
            AudioOcarina_SetInstrument(OCARINA_INSTRUMENT_OFF);
        }
        sIsOcarinaOut = false;
        sHarpPhase = HARP_IDLE;
    }
    if (!sIsOcarinaOut || GetClip(CLIP_HARP_RAISE) == NULL || GetClip(CLIP_HARP_PLAY) == NULL ||
        SwapOcarinaClip(play, player)) {
        return;
    }
    bool isRaising = sHarpPhase == HARP_RAISING && IsPlaying(player, CLIP_HARP_RAISE);
    if (isRaising || sHarpPhase == HARP_LOWERING) {
        return;
    }
    if (isSessionOpen) {
        AudioOcarina_SetInstrument(OCARINA_INSTRUMENT_HARP);
    }
    if (isSessionOpen || sHarpPhase == HARP_RAISING) {
        PoseHarp(play, player);
    }
}

static bool IsPathNamed(Gfx* dList, const char* part) {
    return dList != NULL && ((uintptr_t)dList & 1) == 0 && strstr((const char*)dList, part) != NULL;
}

static bool IsHarpOut(void) {
    return sIsOcarinaOut && sHarpPhase != HARP_IDLE;
}

static void ResolveRitoBody(Player* player, s32 limbIndex, Gfx** dList, Gfx* limbDList, Vec3f* pos) {
    if (!IsRito()) {
        return;
    }
    if (limbIndex == PLAYER_LIMB_ROOT && pos != NULL) {
        if (IsPlaying(player, CLIP_LAND)) {
            pos->y += LAND_LIFT;
        } else if (IsPlaying(player, CLIP_BOW_SEED)) {
            pos->y += BOW_SEED_LIFT;
        } else if (IsPlaying(player, CLIP_BOW_RELEASE)) {
            pos->y += BOW_RELEASE_LIFT;
        }
        return;
    }
    if (limbIndex == PLAYER_LIMB_SHEATH) {
        *dList = NULL;
        return;
    }
    bool isLeft = limbIndex == PLAYER_LIMB_LEFT_HAND;
    if (!isLeft && limbIndex != PLAYER_LIMB_RIGHT_HAND) {
        if (IsPathNamed(*dList, "objects/object_link_")) {
            *dList = limbDList;
        }
        return;
    }
    bool isChildHand = IsPathNamed(*dList, "object_link_child") && !IsPathNamed(*dList, "Holding");
    if (!IsPathNamed(*dList, "Shield") && !isChildHand) {
        return;
    }
    bool isClosed = IsPathNamed(*dList, "Closed") || IsPathNamed(*dList, "Shield");
    const char* hand =
        isLeft ? (isClosed ? sClosedLeftHandDL : sOpenLeftHandDL) : (isClosed ? sClosedHandDL : sOpenHandDL);
    *dList = (Gfx*)hand;
}

static void DrawShield(PlayState* play, Player* player, const Vec3f* offset) {
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    Matrix_Translate(offset->x, offset->y, offset->z, MTXMODE_APPLY);
    Matrix_Get(&player->shieldMf);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sShieldDL);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawBow(PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sBowDL);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawRitoParts(Player* player, s32 limbIndex) {
    PlayState* play = gPlayState;

    if (!IsRito()) {
        return;
    }
    bool isShieldInHand =
        player->rightHandType == PLAYER_MODELTYPE_RH_SHIELD && (player->stateFlags1 & STATE1_SHIELDING);
    if (!IsBowOut() && sApi->HasResource(sShieldDL)) {
        if (limbIndex == PLAYER_LIMB_RIGHT_HAND && isShieldInHand) {
            DrawShield(play, player, &sShieldInHand);
        } else if (limbIndex == PLAYER_LIMB_SHEATH && !isShieldInHand) {
            DrawShield(play, player, &sShieldOnBack);
        }
    }
    if (limbIndex == PLAYER_LIMB_LEFT_HAND && IsBowOut() && sApi->HasResource(sBowDL)) {
        DrawBow(play);
    }
}

static void RaiseShieldOnBack(Player* player) {
    bool isHandEmpty =
        player->rightHandType == PLAYER_MODELTYPE_RH_OPEN || player->rightHandType == PLAYER_MODELTYPE_RH_CLOSED;

    if (!isHandEmpty || player->heldActor != NULL || IsBowOut() || IsHarpOut()) {
        return;
    }
    player->rightHandType = PLAYER_MODELTYPE_RH_SHIELD;
}

static void DrawRito(PlayState* play, Player* player) {
    DrawUpdraft(play);
}

static void ResetState(PlayState* play) {
    sMove = RITO_MOVE_NONE;
    sMoveFrames = 0;
    sCharge = 0;
    sIsAPending = false;
    sHarpPhase = HARP_IDLE;
    sWasNoteStruck = false;
    ClearArrows();
    if (play != NULL) {
        StopUpdraft(play);
    }
}

static void EnterRito(PlayState* play, Player* player) {
    LoadClips();
    ResetState(play);
}

static void ExitRito(PlayState* play, Player* player) {
    bool wasBusy = IsRitoBusy(player);

    ResetState(play);
    if (sIsOcarinaOut) {
        AudioOcarina_SetInstrument(OCARINA_INSTRUMENT_OFF);
        sIsOcarinaOut = false;
    }
    player->actor.gravity = JUMP_GRAVITY;
    if (wasBusy) {
        func_8083B1A0(player, play);
    }
}

static void UpdateRito(PlayState* play, Player* player) {
    DropInterruptedMove(player);
    if (sIsThrowPending) {
        StartThrow(play, player);
    }
    TryRocs(play, player);
    TrackA(play, player);
    TryFreeMoves(play, player);
    RaiseShieldOnBack(player);
    TrySink(play, player);
    TickUpdraft(play, sMove == RITO_MOVE_CHARGE, (f32)sCharge / CHARGE_FULL);
    TickArrows(play);
    UpdateHarp(play, player);
}

static void ForgetScene(s16 sceneId, s8 spawnNum) {
    ForgetUpdraft();
    ClearArrows();
    sMove = RITO_MOVE_NONE;
    sIsAPending = false;
    sIsOcarinaOut = false;
    sHarpPhase = HARP_IDLE;
}

static uint16_t ResolveRitoEquipment(int32_t equipType, uint16_t value) {
    if (equipType == EQUIP_TYPE_SWORD) {
        return EQUIP_VALUE_SWORD_NONE;
    }
    if (equipType == EQUIP_TYPE_SHIELD) {
        return EQUIP_VALUE_SHIELD_MIRROR;
    }
    return value;
}

static bool IsAllowedCustomItem(const char* customKey) {
    static const char* const sAllowed[] = {
        "nei.keaton_form_mask", "nei.mask_kafei", "nei.garo_form_mask",   "nei.gerudo_form_mask",
        "nei.rocs_feather",     "nei.rocs_cape",  "nei.progressive_rocs",
    };

    for (u32 i = 0; i < ARRAY_COUNT(sAllowed); i++) {
        if (strcmp(customKey, sAllowed[i]) == 0) {
            return true;
        }
    }
    return false;
}

static bool AllowsRitoItem(uint16_t item, const char* customKey) {
    if (customKey != NULL) {
        return IsAllowedCustomItem(customKey);
    }
    return item == ITEM_OCARINA_FAIRY || item == ITEM_OCARINA_OF_TIME || item == ITEM_BOMB || item == ITEM_BOMBCHU ||
           item == ITEM_DEKU_NUT || item == ITEM_SLINGSHOT || item == ITEM_LENS_OF_TRUTH ||
           (item >= ITEM_MASK_DEKU && item <= ITEM_MASK_FIERCE_DEITY) ||
           (item >= ITEM_BOTTLE && item <= ITEM_OBABA_DRINK);
}

static const S2HFormActionOverride sActions[] = {
    { SOH_PLAYER_ACTION_ROLL, HandleRoll },
    { SOH_PLAYER_ACTION_ZTARGET_A, HandleZTargetA },
    { SOH_PLAYER_ACTION_MELEE, HandleB },
};

static void RegisterForm(void) {
    S2HFormDefinition rito = { 0 };

    rito.structSize = sizeof(rito);
    rito.key = RITO_FORM_KEY;
    rito.label = "Rito";
    rito.kind = S2H_FORM_KIND_LINK;
    rito.item = RITO_MASK_KEY;
    rito.modelPath = RITO_MODEL_PATH;
    rito.rootScaleAdult = RITO_ROOT_SCALE_ADULT;
    rito.rootScaleChild = RITO_ROOT_SCALE_CHILD;
    rito.height = RITO_HEIGHT;
    rito.motionScale = 1.0f;
    rito.resolveEquipment = ResolveRitoEquipment;
    rito.allowsButtonItem = AllowsRitoItem;
    rito.transformVoiceSfx = NA_SE_VO_LI_FALL_L;
    rito.actions = sActions;
    rito.actionCount = ARRAY_COUNT(sActions);
    rito.onEnter = EnterRito;
    rito.onExit = ExitRito;
    rito.update = UpdateRito;
    rito.draw = DrawRito;
    sApi->RegisterForm(&rito);
}

static bool CanWearMask(Player* player, PlayState* play) {
    return IsGrounded(player) &&
           !(player->stateFlags1 & (STATE1_SWIMMING | STATE1_FIRST_PERSON | PLAYER_STATE1_20000000));
}

static void WearMask(PlayState* play, Player* player) {
    sApi->ToggleForm(RITO_FORM_KEY);
}

static const S2HCustomItemRandomizer sRandomizerItem = {
    sizeof(S2HCustomItemRandomizer), 0, S2H_CUSTOM_ITEM_TYPE_ITEM, 0, 1, RITO_MASK_KEY, RITO_MASK_KEY, "",
};

static void AssignMaskFromPause(const char* key) {
    Z64FormMasks_AssignToPressedButton(sApi, key);
}

static void DrawMaskPickup(PlayState* play, S2HGetItemEntry* entry) {
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    Mtx* matrices = GRAPH_ALLOC(gfxCtx, MASK_MATRIX_COUNT * sizeof(Mtx));

    (void)entry;
    if (matrices == NULL) {
        return;
    }
    Matrix_Push();
    Matrix_Scale(MASK_SCALE, MASK_SCALE, MASK_SCALE, MTXMODE_APPLY);
    Matrix_RotateYS(-0x4000, MTXMODE_APPLY);
    Matrix_Translate(-sMaskCentre.x, -sMaskCentre.y, -sMaskCentre.z, MTXMODE_APPLY);
    Matrix_Push();
    Matrix_Scale(MASK_SEAM_SCALE, MASK_SEAM_SCALE, MASK_SEAM_SCALE, MTXMODE_APPLY);
    for (s32 i = 0; i < MASK_MATRIX_COUNT; i++) {
        Matrix_ToMtx(&matrices[i]);
    }
    Matrix_Pop();
    Matrix_ToMtx(&matrices[MASK_HEAD_MATRIX]);
    Matrix_Translate(sCrestJoint.x, sCrestJoint.y, sCrestJoint.z, MTXMODE_APPLY);
    Matrix_ToMtx(&matrices[MASK_CREST_MATRIX]);
    Matrix_Pop();
    OPEN_DISPS(gfxCtx);
    Gfx_SetupDL25_Opa(gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x0D, (uintptr_t)matrices);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sHeadDL);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sCrestDL);
    CLOSE_DISPS(gfxCtx);
}

static void RegisterMask(void) {
    S2HCustomItemDefinition mask = Z64Items_Define(RITO_MASK_KEY, sIconTex, sNameTex);

    Z64Items_SetButtons(&mask, S2H_CUSTOM_ITEM_C_BUTTON | S2H_CUSTOM_ITEM_DPAD);
    mask.flags |= S2H_CUSTOM_ITEM_INSTANT | S2H_CUSTOM_ITEM_WEARABLE;
    Z64Items_SetPlacement(&mask, RITO_MASK_PAGE, RITO_MASK_SLOT, RITO_MASK_WHEEL_PRIORITY);
    mask.getItemEntry.drawFunc = DrawMaskPickup;
    Z64Items_SetTextbox(&mask, "You got the %rRito Mask%w!&The face of a proud bird of the north wind.^Wear it "
                               "with %y\xB2%w to become a %rRito%w: an updraft to ride, wings to glide and a bow "
                               "that never runs dry.");
    Z64Items_SetPauseText(&mask, "%rRito Mask&%wPress %y\xB2%w to become a Rito.&Hold %y\xB0%w: updraft (magic)  "
                                 "%y\xB0%w in the air: glide&%y\xB1%w: bow, tap for a seed  %y\xB4%w: shield");
    Z64Items_SetCanUse(&mask, CanWearMask);
    Z64Items_SetAction(&mask, WearMask, NULL);
    Z64Items_SetLogic(&mask, &sRandomizerItem);
    mask.onPauseToggle = AssignMaskFromPause;
    Z64Items_Register(sApi, &mask);
    Z64FormMasks_PlaceMask(sApi, RITO_MASK_KEY, RITO_MASK_SLOT);
}

S2H_MOD_EXPORT void ModSetApi(const S2HModApi* api) {
    sApi = api;
}

S2H_MOD_EXPORT const S2HModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

static void GrantRitoLogic(void) {
    static const Z64LogicSource sMask = { RITO_MASK_KEY };
    static const char* const sShooting[] = { "CanUseProjectile", "CanHitSwitch", "CanAttack", "CanDamage", NULL };

    if (Z64Logic_Init(sApi)) {
        Z64Logic_GrantCapabilities(&sMask, sShooting);
    }
}

S2H_MOD_EXPORT void ModInit(void) {
    GrantRitoLogic();
    RegisterMask();
    RegisterForm();
    S2H_REGISTER_HOOK(sApi, OnPlayerResolveAnim, ResolveLanding);
    S2H_REGISTER_HOOK(sApi, OnPlayerResolveLimbDraw, ResolveRitoBody);
    S2H_REGISTER_HOOK(sApi, OnPlayerPostLimbDraw, DrawRitoParts);
    S2H_REGISTER_HOOK(sApi, OnOcarinaInput, NoteStruck);
    S2H_REGISTER_HOOK(sApi, OnSceneInit, ForgetScene);
    sApi->RegisterVB(VB_EXECUTE_PLAYER_ACTION_FUNC, DriveSink);
    sApi->RegisterVB(VB_CHANGE_HELD_ITEM_AND_USE_ITEM, NoteAirThrow);
    sApi->RegisterVB(VB_USE_STRENGTH_UPGRADE, ResolveRitoStrength);
}
