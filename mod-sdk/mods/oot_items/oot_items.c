#include "z64items.h"
#include "z64aiming.h"
#include "z64logic.h"
#include <stdio.h>
#include <string.h>
#include "overlays/actors/ovl_En_Arrow/z_en_arrow.h"
#include "overlays/kaleido_scope/ovl_kaleido_scope/z_kaleido_scope.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "interface/parameter_static/parameter_static.h"
#include "z64quake.h"
#include "2s2h/ModApi/CustomItemRegistry/KaleidoItemManager.h"
#include "2s2h/ModApi/RandoInfCustom/RandoInfCustom.h"
#include <libultraship/bridge/consolevariablebridge.h>

#define MOD "oot_items"
#define SLING "oot.slingshot"
#define BOOM "oot.boomerang"
#define HAMMER "oot.hammer"
#define DIN "oot.dins_fire"
#define FARORE "oot.farores_wind"
#define NAYRU "oot.nayrus_love"
#define SLING_CHAIN "oot.slingshot.capacity"
#define SLING_CONDENSED_CHAIN "oot.slingshot.condensed_capacity"
#define SLING_PROGRESSIVE "oot.slingshot.progressive"
#define SLING_CONDENSED "oot.slingshot.condensed"
#define SLING_INFINITY_OPTION "oot.slingshot.infinite_upgrades"
#define SLING_SHOW_KEG "gMods.OotItems.ShowPowderKeg"
#define SLING_FIRST_PERSON_LIFT 4.0f
#define OOT_BAG_MODEL(name) "__OTR__objects/oot_object_gi_dekupouch/" name
// The bow reticle's reach: 341000 hand units at the player's 0.01 scale.
#define SLINGSHOT_RETICLE_RANGE 3410.0f
#define OOT_ICON(name) "__OTR__textures/oot_icon_item_static/" name
#define OOT_NAME(name) "__OTR__textures/oot_item_name_static/" name
#define OOT_SPELL_MODEL(name) "__OTR__objects/oot_object_gi_goddess/" name
// MM's NES textbox control bytes.
#define GLYPH_A "\xB0"
#define GLYPH_B "\xB1"
#define GLYPH_C "\xB2"
#define GLYPH_R "\xB4"
#define GLYPH_C_UP "\xB6"

enum {
    OOT_SLOT_DINS_FIRE = 5,
    OOT_SLOT_FARORES_WIND = 11,
    OOT_SLOT_NAYRUS_LOVE = 17,
};

void func_8083B1A0(Player* player, PlayState* play);
void Player_InitItemActionWithAnim(PlayState* play, Player* player, PlayerItemAction itemAction);
void func_8088A594(EnArrow* arrow, PlayState* play);
s32 Player_SetAction(PlayState* play, Player* player, PlayerActionFunc action, s32 flags);
bool Player_UpdateUpperBody(Player* player, PlayState* play);
void Player_Anim_PlayOnceAdjusted(PlayState* play, Player* player, PlayerAnimationHeader* animation);
void Player_SetAction_PreserveItemAction(PlayState* play, Player* player, PlayerActionFunc action, s32 flags);
void Player_Action_Idle(Player* player, PlayState* play);
// Compared against actionFunc, so it must be the DLL's address rather than this module's import thunk.
#ifdef _WIN32
__declspec(dllimport) void Player_Action_84(Player* player, PlayState* play);
#else
void Player_Action_84(Player* player, PlayState* play);
#endif
void Player_StopHorizontalMovement(Player* player);
void func_80836988(Player* player, PlayState* play);
s32 func_8083FCF0(PlayState* play, Player* player, f32 startFrame, f32 activeFrame, f32 endFrame);
void func_8083A548(Player* player);
void func_8082DC38(Player* player);
void func_80833728(Player* player, s32 index, u32 dmgFlags, s32 damage);
s32 Player_DecelerateToZero(Player* player);
f32 func_80835D2C(PlayState* play, Player* player, Vec3f* offset, Vec3f* pos);
s16 func_80832754(Player* player, s32 arg1);
void Player_RequestQuakeAndRumble(PlayState* play, Player* player, u16 sfxId);
void Player_AnimReplace_Setup(PlayState* play, Player* player, s32 movementFlags);
void Player_Anim_PlayOnceWaterAdjustment(PlayState* play, Player* player, PlayerAnimationHeader* anim);
void Player_SetParallel(Player* player);
void Player_AnimSfx_PlayVoice(Player* player, u16 sfxId);
Vtx* ResourceMgr_LoadVtxByName(char* path);
void ResourceMgr_PatchGfxByName(const char* path, const char* patchName, int index, Gfx instruction);
// The player overlay keeps the animation sound entry layout private.
typedef struct {
    u16 sfxId;
    s16 flags;
} OotAnimSfxEntry;
enum { OOT_ANIMSFX_GENERAL = 1, OOT_ANIMSFX_FLOOR = 2, OOT_ANIMSFX_VOICE = 4 };
void Player_PlayAnimSfx(Player* player, OotAnimSfxEntry* entry);

typedef struct {
    Actor actor;
    ColliderQuad collider;
    WeaponInfo weaponInfo;
    Actor* moveTo;
    Actor* grabbed;
    s32 effectIndex;
    u8 returnTimer;
    u8 activeTimer;
} OotBoom;

typedef enum {
    DINS_FIRE_INITIALIZE,
    DINS_FIRE_EXPAND_SLOWLY,
    DINS_FIRE_STOP_EXPANDING,
    DINS_FIRE_EXPAND_QUICKLY,
} DinsFireAction;

typedef enum {
    DINS_TINT_NONE,
    DINS_TINT_FADE_IN,
    DINS_TINT_MAINTAIN,
    DINS_TINT_FADE_OUT,
    DINS_TINT_FINISHED,
} DinsFireTint;

typedef struct {
    Actor actor;
    ColliderCylinder collider;
    f32 alphaMultiplier;
    f32 scalingSpeed;
    s16 action;
    s16 actionTimer;
    s16 tintBehaviour;
    s16 tintTimer;
    f32 tintIntensity;
    bool casting;
} OotDinsFire;

typedef struct {
    Actor actor;
    s16 timer;
    u8 primAlpha;
    bool diamond;
    Vec3f orbOffset;
    f32 scale;
} OotNayrusLove;

typedef struct {
    u32 version;
    bool valid;
    s32 entrance;
    s16 scene;
    s8 room;
    s16 yaw;
    Vec3f pos;
    s16 seeds;
} OotSave;

static const S2HModApi* sApi;
static OotSave sSave;
static struct {
    u32 switches;
    u32 collectible1;
    u32 collectible2;
} sFaroreRoomFlags;
static u8 sBulletBagLevel;
static s16 sBoomId, sFireId, sWindId, sNayruId;
static Actor* sBoom;
static Actor* sSlingSeed;
static Actor* sSlingPickup;
static const char* sSlingPickupStage;
static bool sSlingPull;
static enum { BOOM_STOWED, BOOM_PENDING, BOOM_AIM, BOOM_THROW, BOOM_FLIGHT, BOOM_CATCH } sBoomPhase;
static s16 sBoomYaw, sBoomPitch;
static bool sBoomLocked;
// Z-targeted aiming keeps Link's own locomotion (OoT's side walk); only the upper body aims.
static bool sBoomTargeted;
static bool sBoomArmed;
static bool sSlingTargeted;
static bool sHammerSwingPending;
static bool sHammerSideSwing;
// OoT av1 order: Farore's Wind, Nayru's Love, Din's Fire.
typedef enum { CAST_NONE, CAST_FARORE, CAST_NAYRU, CAST_DIN } OotCast;
static const u8 sCastCosts[] = { 0, 12, 24, 12 };
typedef enum { CAST_PHASE_CHARGE, CAST_PHASE_RAISE, CAST_PHASE_HOLD, CAST_PHASE_LOWER } OotCastPhase;
static OotCast sPendingCast, sActiveCast;
static OotCastPhase sCastPhase;
static s16 sCastHold;
static u16 sCastAge;
static s16 sCastCamera = CAM_ID_NONE;
static s16 sCastStartYaw, sCastStartPitch, sCastTurnYaw;
static bool sDinShortCamera, sDinMirrorCamera;
static u32 sLastDinCameraFrame;
static bool sPlayedDinCamera;
static bool sFarorePrompt;
static bool sFarorePromptPending;
static bool sFaroreReturnPending;
static s16 sFaroreArrivalTimer;
static ActorFunc sFaroreHiddenDraw;
static bool sWaiveVoid, sSlingOut;
static bool sSlingLoading, sSlingAimPending;
static bool sWheelPrompt;
static u16 sPreviousAAction;
static s16 sSlingCooldown;
// OoT gSaveContext.nayrusLoveTimer; kept apart because useful_medallions reads that field as Shadow stealth.
static s16 sNayruTimer;
static Actor* sNayrusLove;
// OoT respawn[RESPAWN_MODE_TOP].data for the warp-point pointer: <40 grows, 40 rests, >40 fades into the camera.
static s32 sFaroreMarkerData;
static Vec3f sFaroreMarkerPos;
static s32 sFaroreFlyTimer;
static f32 sFaroreFlyProgress;
static LightInfo sFaroreLightInfo;
static LightNode* sFaroreLightNode;
static struct {
    const char* key;
    const char* icon;
    u32 startFrame;
    s16 x, y;
    u8 button;
} sEquipFlight;
static const u32 sButtons[S2H_ITEM_BUTTON_COUNT] = {
    BTN_B, BTN_CLEFT, BTN_CDOWN, BTN_CRIGHT, 0, BTN_DRIGHT, BTN_DLEFT, BTN_DDOWN, BTN_DUP
};
static const char* const sHooks[] = { "OnPlayerUpdate", "OnSceneInit", "OnLoadFile", "OnActorDestroy",
                                      "OnPlayDrawEnd", "OnPlayerResolveLimbDraw", "OnPlayerPostLimbDraw", "OnKaleidoItemDraw",
                                      "OnInterfaceDrawEnd", "OnKaleidoItemEquip", "OnKaleidoInput",
                                      "OnResolveCustomGetItem", "OnPlayerResolveAnim", "OnPlayerActionHandler" };
static const S2HModRequirements sRequirements = { sizeof(S2HModRequirements), sHooks, ARRAY_COUNT(sHooks) };
static const ALIGN_ASSET(2) char sBoomDL[] = "__OTR__objects/oot_object_gi_boomerang/gGiBoomerangDL";
static const ALIGN_ASSET(2) char sBoomFlightDL[] = "__OTR__objects/oot_gameplay_keep/gBoomerangDL";
static const ALIGN_ASSET(2) char sBoomHandDL[] = "__OTR__objects/oot_object_link_child/gLinkChildLeftFistAndBoomerangNearDL";
static const ALIGN_ASSET(2) char sBoomThrowAnim[] = "__OTR__objects/oot_gameplay_keep/gPlayerAnim_link_boom_throwR";
static const ALIGN_ASSET(2) char sBoomCatchAnim[] = "__OTR__objects/oot_gameplay_keep/gPlayerAnim_link_boom_catch";
static const ALIGN_ASSET(2) char sHammerDL[] = "__OTR__objects/oot_object_gi_hammer/gGiHammerDL";
static const ALIGN_ASSET(2) char sHammerHandDL[] = "__OTR__objects/oot_object_link_boy/gLinkAdultLeftHandHoldingHammerNearDL";
static const ALIGN_ASSET(2) char sHumanLeftFistDL[] = "__OTR__objects/object_link_child/gLinkHumanLeftHandClosedDL";
static const ALIGN_ASSET(2) char sHammerHitAnim[] = "__OTR__objects/oot_gameplay_keep/gPlayerAnim_link_hammer_hit";
static const ALIGN_ASSET(2) char sHammerHitEndAnim[] = "__OTR__objects/oot_gameplay_keep/gPlayerAnim_link_hammer_hit_end";
static const ALIGN_ASSET(2) char sHammerHitEndLockedAnim[] = "__OTR__objects/oot_gameplay_keep/gPlayerAnim_link_hammer_hit_endR";
static const ALIGN_ASSET(2) char sHammerSideAnim[] = "__OTR__objects/oot_gameplay_keep/gPlayerAnim_link_hammer_side_hit";
static const ALIGN_ASSET(2) char sHammerSideEndAnim[] = "__OTR__objects/oot_gameplay_keep/gPlayerAnim_link_hammer_side_hit_end";
static const ALIGN_ASSET(2) char sHammerSideEndLockedAnim[] = "__OTR__objects/oot_gameplay_keep/gPlayerAnim_link_hammer_side_hit_endR";
static const ALIGN_ASSET(2) char sSlingDL[] = "__OTR__objects/oot_object_link_child/gLinkChildRightHandHoldingSlingshotNearDL";
static const ALIGN_ASSET(2) char sSlingStringDL[] = "__OTR__objects/oot_object_link_child/gLinkChildSlinghotStringDL";
static const ALIGN_ASSET(2) char sSlingFirstPersonDL[] = "__OTR__objects/oot_object_link_child/gLinkChildRightArmStretchedSlingshotDL";
static const ALIGN_ASSET(2) char sSlingShoulderDL[] = "__OTR__objects/oot_object_link_child/gLinkChildRightShoulderNearDL";
static const ALIGN_ASSET(2) char sSlingGetDL[] = "__OTR__objects/oot_object_gi_pachinko/gGiSlingshotDL";
static const ALIGN_ASSET(2) char sSpellDiamondDL[] = OOT_SPELL_MODEL("gGiMagicSpellDiamondDL");
static const ALIGN_ASSET(2) char sSpellOrbDL[] = OOT_SPELL_MODEL("gGiMagicSpellOrbDL");
static const ALIGN_ASSET(2) char sDinColorDL[] = OOT_SPELL_MODEL("gGiDinsFireColorDL");
static const ALIGN_ASSET(2) char sFaroreColorDL[] = OOT_SPELL_MODEL("gGiFaroresWindColorDL");
static const ALIGN_ASSET(2) char sNayruColorDL[] = OOT_SPELL_MODEL("gGiNayrusLoveColorDL");
static const char* const sSpellColors[] = { sDinColorDL, sFaroreColorDL, sNayruColorDL };
static const ALIGN_ASSET(2) char sBagDL[] = OOT_BAG_MODEL("gGiBulletBagDL");
static const ALIGN_ASSET(2) char sBagColor[] = OOT_BAG_MODEL("gGiBulletBagColorDL");
static const ALIGN_ASSET(2) char sBagLargeColor[] = OOT_BAG_MODEL("gGiBulletBag50ColorDL");
static const ALIGN_ASSET(2) char sBagStringColor[] = OOT_BAG_MODEL("gGiBulletBagStringColorDL");
static const ALIGN_ASSET(2) char sBagLargeStringColor[] = OOT_BAG_MODEL("gGiBulletBag50StringColorDL");
static const ALIGN_ASSET(2) char sBagString[] = OOT_BAG_MODEL("gGiBulletBagStringDL");
static const ALIGN_ASSET(2) char sBagWriting[] = OOT_BAG_MODEL("gGiBulletBagWritingDL");
static const ALIGN_ASSET(2) char sSeedDL[] = "__OTR__objects/oot_object_gi_seed/gGiSeedDL";
static const u8 sSeedCapacities[] = { 30, 40, 50, 50 };
static const char* const sInfinityModes[] = { "Off", "Progressive", "Condensed Progressive" };
static const S2HModRandoOption sInfinityOption = {
    sizeof(S2HModRandoOption), SLING_INFINITY_OPTION, "Slingshot Infinite Upgrades",
    "Progressive adds an infinite Bullet Bag after the finite upgrades. Condensed upgrades directly from 30 to infinite.",
    sInfinityModes, ARRAY_COUNT(sInfinityModes), 0, S2H_MOD_RANDO_WIDGET_COMBOBOX,
};
static const char* const sAmmoDigits[] = {
    gAmmoDigit0Tex, gAmmoDigit1Tex, gAmmoDigit2Tex, gAmmoDigit3Tex, gAmmoDigit4Tex,
    gAmmoDigit5Tex, gAmmoDigit6Tex, gAmmoDigit7Tex, gAmmoDigit8Tex, gAmmoDigit9Tex,
};
static const ALIGN_ASSET(2) char sInfinityTex[] = "__OTR__kanji/gMsgKanji8187InfinityTex";
static const ALIGN_ASSET(2) char sKegIcon[] = "__OTR__icon_item_static_yar/gItemIconPowderKegTex";
static const ALIGN_ASSET(2) char sSlingIcon[] = OOT_ICON("gItemIconSlingshotTex");
static const ALIGN_ASSET(2) char sDinFireModel[] = "__OTR__overlays/oot_ovl_Magic_Fire/sModelDL";
static const ALIGN_ASSET(2) char sDinFireMaterial[] = "__OTR__overlays/oot_ovl_Magic_Fire/sMaterialDL";
static const ALIGN_ASSET(2) char sDinFireTexture[] = "__OTR__overlays/oot_ovl_Magic_Fire/sTex";
static const char sDinFireSphereVtx[] = "__OTR__overlays/oot_ovl_Magic_Fire/sSphereVtx";
static const char sWindCylinderVtx[] = "__OTR__overlays/oot_ovl_Magic_Wind/sCylinderVtx";
static const ALIGN_ASSET(2) char sNayruModel[] = "__OTR__overlays/oot_ovl_Magic_Dark/sDiamondModelDL";
static const ALIGN_ASSET(2) char sNayruMaterial[] = "__OTR__overlays/oot_ovl_Magic_Dark/sDiamondMaterialDL";
static const ALIGN_ASSET(2) char sWindSkeleton[] = "__OTR__overlays/oot_ovl_Magic_Wind/sSkel";
static const ALIGN_ASSET(2) char sWindAnimation[] = "__OTR__overlays/oot_ovl_Magic_Wind/sAnim";

static bool HasSlingshotKey(const char* key) {
    return key != NULL && strcmp(key, SLING) == 0;
}

static u8 SeedCapacity(void) {
    return sSeedCapacities[sBulletBagLevel ? sBulletBagLevel - 1 : 0];
}

static ColliderQuadInit sBoomCollider = {
    { COL_MATERIAL_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_TYPE_PLAYER, COLSHAPE_QUAD },
    { ELEM_MATERIAL_UNK2, { DMG_ZORA_BOOMERANG, 0, 1 }, { 0xF7CFFFFF, 0, 0 },
      ATELEM_ON | ATELEM_NEAREST | ATELEM_SFX_NORMAL, ACELEM_NONE, OCELEM_NONE },
    { { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } } },
};

static ColliderCylinderInit sDinsFireCollider = {
    { COL_MATERIAL_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_TYPE_1, COLSHAPE_CYLINDER },
    { ELEM_MATERIAL_UNK0, { DMG_FIRE_ARROW, 0, 1 }, { 0, 0, 0 }, ATELEM_ON | ATELEM_SFX_NONE, ACELEM_NONE,
      OCELEM_NONE },
    { 9, 9, 0, { 0, 0, 0 } },
};

static void Store(void) { sApi->StorageSet(MOD, "state", &sSave, sizeof(sSave)); }
static void Load(s32 fileNum) {
    sSlingPickup=NULL;
    sSlingPickupStage=NULL;
    sNayruTimer = 0;
    memset(&sFaroreRoomFlags, 0, sizeof(sFaroreRoomFlags));
    sApi->StorageGet(MOD, "farore_room_flags", &sFaroreRoomFlags, sizeof(sFaroreRoomFlags));
    memset(&sSave, 0, sizeof(sSave));
    if (sApi->StorageGet(MOD, "state", &sSave, sizeof(sSave)) != sizeof(sSave) || sSave.version != 1) {
        memset(&sSave, 0, sizeof(sSave));
        sSave.version = 1;
        sSave.seeds = 30;
    }
    sBulletBagLevel = 0;
    sApi->StorageGet(MOD, "bullet_bag_level", &sBulletBagLevel, sizeof(sBulletBagLevel));
    if (sBulletBagLevel > 4) sBulletBagLevel = 0;
    if (sBulletBagLevel == 0 && sApi->IsCustomItemOwned(SLING)) sBulletBagLevel = 3;
    sSave.seeds = CLAMP(sSave.seeds, 0, SeedCapacity());
}
static bool CanUse(Player* player, PlayState* play) {
    return play != NULL && player != NULL && player->transformation == PLAYER_FORM_HUMAN &&
           play->pauseCtx.state == PAUSE_STATE_OFF && play->transitionTrigger != TRANS_TRIGGER_START &&
           play->msgCtx.msgLength == 0 && !(player->stateFlags1 & PLAYER_STATE1_20000000);
}
static bool HasUnlimitedMagic(void) {
    return CHECK_WEEKEVENTREG(WEEKEVENTREG_DRANK_CHATEAU_ROMANI);
}


static bool UseTargetCamera(Player* player, PlayState* play) {
    return player->focusActor != NULL || (play->state.input[0].cur.button & BTN_Z);
}

static void UpdateWeaponAim(Player* player, PlayState* play) {
    if (UseTargetCamera(player, play)) {
        Z64Aiming_Release(player, play);
    } else {
        if (!Z64Aiming_IsAiming()) Z64Aiming_Request(player, play);
        Z64Aiming_Update(player, play);
    }
}

static void GetWeaponAim(Player* player, s16* yaw, s16* pitch) {
    if (Z64Aiming_IsAiming()) {
        Z64Aiming_GetDirection(player, yaw, pitch);
    } else if (player->focusActor != NULL) {
        Vec3f* point = &player->focusActor->focus.pos;
        *yaw = Actor_WorldYawTowardPoint(&player->actor, point);
        *pitch = Math_Vec3f_Pitch(&player->actor.focus.pos, point);
    } else {
        *yaw = player->actor.shape.rot.y;
        *pitch = 0;
    }
}

// First-person aim turns Link's whole body like the native bow, whose aim yaw is clamped around his facing;
// lock-on keeps the body on its side walk and twists only the upper body.
static void FaceWeaponAim(Player* player, s16 yaw, s16 pitch) {
    if (Z64Aiming_IsAiming()) {
        player->actor.shape.rot.y = yaw;
        player->actor.world.rot.y = yaw;
        player->yaw = yaw;
    }
    player->upperLimbRot.x = pitch;
    player->upperLimbRot.y = (s16)(yaw - player->actor.shape.rot.y);
    player->unk_AA6_rotFlags |= UNKAA6_ROT_UPPER_X | UNKAA6_ROT_UPPER_Y;
}
static bool HasModels(Player* player, PlayState* play) {
    if (!CanUse(player, play)) return false;
    if (sApi->HasResource(sBoomDL)) return true;
    sApi->ShowTextbox(play, "Extract your OoT assets with %rOoT Asset Loader%w, then restart.", true);
    return false;
}

static bool CanUseBoom(Player* player, PlayState* play) {
    return HasModels(player, play) && sApi->HasResource(sBoomFlightDL) && sApi->HasResource(sBoomHandDL) &&
           sApi->HasResource(sBoomThrowAnim) && sApi->HasResource(sBoomCatchAnim);
}

static void BoomInit(Actor* actor, PlayState* play) {
    OotBoom* boom = (OotBoom*)actor;
    actor->room = -1;
    actor->attentionRangeType = ATTENTION_RANGE_5;
    actor->shape.rot.x = actor->shape.rot.y = actor->shape.rot.z = 0;
    EffectBlureInit1 trail = { 0 };
    const u8 bright[] = { 255, 255, 100, 255 };
    const u8 soft[] = { 255, 255, 100, 64 };
    const u8 fade[] = { 255, 255, 100, 0 };
    memcpy(trail.p1StartColor, bright, sizeof(bright));
    memcpy(trail.p2StartColor, soft, sizeof(soft));
    memcpy(trail.p1EndColor, fade, sizeof(fade));
    memcpy(trail.p2EndColor, fade, sizeof(fade));
    trail.elemDuration = 8;
    boom->effectIndex = -1;
    Effect_Add(play, &boom->effectIndex, EFFECT_BLURE1, 0, 0, &trail);
    Collider_InitQuad(play, &boom->collider);
    Collider_SetQuad(play, &boom->collider, actor, &sBoomCollider);
    boom->returnTimer = 20;
}

static bool IsCustomPickup(Actor* actor) {
    return actor->id == ACTOR_EN_ITEM00 && sApi->GetActorCustomItem(actor) != NULL;
}

// Console drops only register their AC cylinder with ABLE_TO_ZORA_RANG, so test their
// radius 10, height 30 cylinder against the segment the quad swept this frame.
static bool IsPickupInSweep(Actor* pickup, Vec3f* from, Vec3f* to) {
    const f32 reach = 10.0f + 9.6f;
    Vec3f segment = { to->x - from->x, to->y - from->y, to->z - from->z };
    Vec3f center = { pickup->world.pos.x, pickup->world.pos.y + 15.0f, pickup->world.pos.z };
    f32 lengthSq = SQ(segment.x) + SQ(segment.y) + SQ(segment.z);
    f32 t = 0.0f;
    if (lengthSq > 0.0f) {
        t = ((center.x - from->x) * segment.x + (center.y - from->y) * segment.y + (center.z - from->z) * segment.z) /
            lengthSq;
        t = CLAMP(t, 0.0f, 1.0f);
    }
    Vec3f nearest = { from->x + segment.x * t, from->y + segment.y * t, from->z + segment.z * t };
    return Math_Vec3f_DistXZ(&nearest, &center) <= reach && fabsf(nearest.y - center.y) <= 15.0f + 9.6f;
}

static Actor* FindCustomPickupInSweep(PlayState* play, Vec3f* from, Vec3f* to) {
    for (Actor* item = play->actorCtx.actorLists[ACTORCAT_MISC].first; item != NULL; item = item->next) {
        if (IsCustomPickup(item) && item->parent == NULL && IsPickupInSweep(item, from, to)) {
            return item;
        }
    }
    return NULL;
}

static void BoomDestroy(Actor* actor, PlayState* play) {
    OotBoom* boom = (OotBoom*)actor;
    Effect_Destroy(play, boom->effectIndex);
    Collider_DestroyQuad(play, &boom->collider);
    if (sBoom == actor) {
        sBoom = NULL;
    }
}

static void SteerBoom(OotBoom* boom, Player* player) {
    Actor* target = boom->moveTo;
    if (target == NULL) {
        return;
    }
    s16 yawTarget = Actor_WorldYawTowardPoint(&boom->actor, &target->focus.pos);
    s16 yawDiff = boom->actor.world.rot.y - yawTarget;
    s16 pitchTarget = Actor_WorldPitchTowardPoint(&boom->actor, &target->focus.pos);
    s16 pitchDiff = boom->actor.world.rot.x - pitchTarget;
    f32 turnScale = (200.0f - Math_Vec3f_DistXYZ(&boom->actor.world.pos, &target->focus.pos)) * 0.005f;
    turnScale = MAX(turnScale, 0.12f);
    if (target != &player->actor && (target->update == NULL || ABS(yawDiff) > 0x4000)) {
        boom->moveTo = NULL;
        return;
    }
    Math_ScaledStepToS(&boom->actor.world.rot.y, yawTarget, (s16)(ABS(yawDiff) * turnScale));
    Math_ScaledStepToS(&boom->actor.world.rot.x, pitchTarget, (s16)(ABS(pitchDiff) * turnScale));
}

static void CatchBoom(OotBoom* boom, Player* player) {
    Actor* grabbed = boom->grabbed;
    if (grabbed != NULL) {
        Math_Vec3f_Copy(&grabbed->world.pos, &player->actor.world.pos);
        if (grabbed->id == ACTOR_EN_ITEM00) {
            grabbed->gravity = -0.9f;
            grabbed->bgCheckFlags &= ~(BGCHECKFLAG_GROUND | BGCHECKFLAG_GROUND_TOUCH);
        } else {
            grabbed->flags &= ~ACTOR_FLAG_HOOKSHOT_ATTACHED;
        }
    }
    Actor_Kill(&boom->actor);
}

static bool HasBoomHitWall(OotBoom* boom, PlayState* play) {
    Vec3f hitPoint;
    s32 bgId;
    if (!BgCheck_EntityLineTest1(&play->colCtx, &boom->actor.prevPos, &boom->actor.world.pos, &hitPoint,
                                 &boom->actor.wallPoly, true, true, true, true, &bgId)) {
        return false;
    }
    if (func_800B90AC(play, &boom->actor, boom->actor.wallPoly, bgId, &hitPoint)) {
        return false;
    }
    CollisionCheck_SpawnShieldParticlesMetal(play, &hitPoint);
    return true;
}

static void BoomFly(OotBoom* boom, PlayState* play) {
    Player* player = GET_PLAYER(play);
    SteerBoom(boom, player);
    Actor_SetSpeeds(&boom->actor, 12.0f);
    Actor_MoveWithGravity(&boom->actor);
    Actor_PlaySfx_Flagged(&boom->actor, NA_SE_IT_BOOMERANG_FLY - SFX_FLAG);

    bool collided = (boom->collider.base.atFlags & AT_HIT) != 0;
    Actor* hitActor = collided ? boom->collider.base.at : NULL;
    if (hitActor == NULL || IsCustomPickup(hitActor)) {
        Actor* pickup = FindCustomPickupInSweep(play, &boom->actor.prevPos, &boom->actor.world.pos);
        if (pickup != NULL) {
            hitActor = pickup;
            collided = true;
        }
    }
    if (hitActor != NULL && (hitActor->id == ACTOR_EN_ITEM00 || hitActor->id == ACTOR_EN_SI)) {
        boom->grabbed = hitActor;
        if (hitActor->id == ACTOR_EN_SI) {
            hitActor->flags |= ACTOR_FLAG_HOOKSHOT_ATTACHED;
        }
    }

    if (DECR(boom->returnTimer) == 0) {
        boom->moveTo = &player->actor;
        if (Math_Vec3f_DistXYZ(&boom->actor.world.pos, &player->actor.focus.pos) < 40.0f) {
            CatchBoom(boom, player);
        }
    } else {
        if (collided) {
            Math_Vec3f_Copy(&boom->actor.world.pos, &boom->actor.prevPos);
        } else {
            collided = HasBoomHitWall(boom, play);
        }
        if (collided) {
            boom->actor.world.rot.x = -boom->actor.world.rot.x;
            boom->actor.world.rot.y += 0x8000;
            boom->moveTo = &player->actor;
            boom->returnTimer = 0;
        }
    }

    Actor* grabbed = boom->grabbed;
    if (grabbed != NULL) {
        if (grabbed->update == NULL) {
            boom->grabbed = NULL;
        } else {
            Math_Vec3f_Copy(&grabbed->world.pos, &boom->actor.world.pos);
        }
    }
}

static void BoomUpdate(Actor* actor, PlayState* play) {
    OotBoom* boom = (OotBoom*)actor;
    if (GET_PLAYER(play)->stateFlags1 & PLAYER_STATE1_20000000) {
        return;
    }
    BoomFly(boom, play);
    Actor_SetFocus(actor, 0.0f);
    boom->activeTimer++;
}

static void BoomDraw(Actor* actor, PlayState* play) {
    static Vec3f sBaseOffset = { -960.0f, 0.0f, 0.0f };
    static Vec3f sTipOffset = { 960.0f, 0.0f, 0.0f };
    OotBoom* boom = (OotBoom*)actor;
    Vec3f base;
    Vec3f tip;
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_RotateYS(actor->world.rot.y, MTXMODE_APPLY);
    Matrix_RotateZS(0x1F40, MTXMODE_APPLY);
    Matrix_RotateXS(actor->world.rot.x, MTXMODE_APPLY);
    Matrix_MultVec3f(&sBaseOffset, &base);
    Matrix_MultVec3f(&sTipOffset, &tip);
    if (func_80126440(play, &boom->collider, &boom->weaponInfo, &base, &tip)) {
        EffectBlure_AddVertex(Effect_GetByIndex(boom->effectIndex), &base, &tip);
    }
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    Matrix_RotateYS((s16)(boom->activeTimer * 12000), MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sBoomFlightDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void LaunchBoom(PlayState* play, Player* player) {
    f32 x = Math_SinS(player->actor.shape.rot.y) * 10.0f + player->actor.world.pos.x;
    f32 z = Math_CosS(player->actor.shape.rot.y) * 10.0f + player->actor.world.pos.z;
    // OoT releases at frame 6, at +30 on its child pose. MM's human pose places
    // the throwing hand higher: release at that hand's actual height instead of
    // starting the projectile below the visible weapon. Keep OoT's forward offset.
    f32 y = player->bodyPartsPos[PLAYER_BODYPART_LEFT_HAND].y;
    // Use the direction captured while aiming; the camera is released before frame 6.
    s16 pitch = sBoomPitch;
    s16 yaw = sBoomLocked ? player->actor.shape.rot.y + 0x36B0 : sBoomYaw;
    sBoom = Actor_Spawn(&play->actorCtx, play, sBoomId, x, y, z, pitch, yaw, 0, 0);
    if (sBoom == NULL) {
        return;
    }
    ((OotBoom*)sBoom)->moveTo = sBoomLocked ? player->focusActor : NULL;
    if (!Player_CheckHostileLockOn(player)) {
        Player_SetParallel(player);
    }
    Player_PlaySfx(player, NA_SE_IT_BOOMERANG_THROW);
    Player_AnimSfx_PlayVoice(player, NA_SE_VO_LI_SWORD_N);
}

static void TakeBoom(PlayState* play, Player* player) {
    if (sBoom == NULL && sBoomPhase == BOOM_STOWED) sBoomPhase = BOOM_PENDING;
}

static void PutBoom(Player* player, PlayState* play) {
    if (sBoomPhase == BOOM_AIM && !sBoomLocked) Z64Aiming_Release(player, play);
    sBoomPhase = BOOM_STOWED;
    sBoomTargeted = false;
}

// A lock-on aim keeps A for the side hop, as OoT does; first person cancels with it too.
static u32 AimCancelButtons(bool isTargeted) {
    return isTargeted ? (BTN_B | BTN_CUP) : (BTN_A | BTN_B | BTN_CUP);
}

static void UseBoom(Player* player, PlayState* play) {
    if (sBoom == NULL) TakeBoom(play, player);
}

static void BoomAimAction(Player* player, PlayState* play);

// A boomerang raised by Z-targeting alone waits for a press, so the release that throws it is the player's.
static void RaiseBoom(Player* player, PlayState* play, bool isArmed) {
    sBoomLocked = player->focusActor != NULL;
    sBoomTargeted = UseTargetCamera(player, play);
    sBoomArmed = isArmed;
    if (!sBoomTargeted && !Z64Aiming_Request(player, play)) {
        sBoomPhase = BOOM_STOWED;
        return;
    }
    sBoomPhase = BOOM_AIM;
    if (!sBoomTargeted) {
        Player_SetAction_PreserveItemAction(play, player, BoomAimAction, 0);
    }
    PlayerAnimation_PlayLoop(play, &player->skelAnimeUpper, (PlayerAnimationHeader*)gPlayerAnim_link_boom_throw_waitR);
}

static s32 UpdateBoomHeld(Player* player, PlayState* play) {
    SkelAnime* upper = &player->skelAnimeUpper;
    switch (sBoomPhase) {
        case BOOM_STOWED:
            if (sBoom != NULL || !UseTargetCamera(player, play) ||
                (player->stateFlags3 & PLAYER_STATE3_START_CHANGING_HELD_ITEM)) {
                return 0;
            }
            RaiseBoom(player, play, false);
            return sBoomPhase == BOOM_AIM;
        case BOOM_AIM: {
            Input* input = &play->state.input[0];
            const u32 button = player->heldItemButton < ARRAY_COUNT(sButtons) ? sButtons[player->heldItemButton] : 0;
            if (input->press.button & button) {
                sBoomArmed = true;
            }
            if (sBoomTargeted && !UseTargetCamera(player, play)) {
                if (!sBoomArmed) {
                    PutBoom(player, play);
                    return 0;
                }
                sBoomTargeted = false;
                Player_SetAction_PreserveItemAction(play, player, BoomAimAction, 0);
            }
            UpdateWeaponAim(player, play);
            sBoomLocked = player->focusActor != NULL;
            GetWeaponAim(player, &sBoomYaw, &sBoomPitch);
            FaceWeaponAim(player, sBoomYaw, sBoomPitch);
            if (input->press.button & AimCancelButtons(sBoomTargeted)) {
                PutBoom(player, play);
                return 0;
            }
            if (sBoomArmed && !(input->cur.button & button)) {
                Z64Aiming_Release(player, play);
                sBoomPhase = BOOM_THROW;
                PlayerAnimation_PlayOnce(play, upper, (PlayerAnimationHeader*)sBoomThrowAnim);
            } else {
                PlayerAnimation_Update(play, upper);
            }
            return 1;
        }
        case BOOM_THROW: {
            bool finished = PlayerAnimation_Update(play, upper);
            if (PlayerAnimation_OnFrame(upper, 6.0f)) LaunchBoom(play, player);
            if (finished) {
                sBoomPhase = sBoom != NULL ? BOOM_FLIGHT : BOOM_STOWED;
                if (sBoomPhase == BOOM_FLIGHT && sBoomLocked) {
                    PlayerAnimation_PlayLoop(play, upper, (PlayerAnimationHeader*)gPlayerAnim_link_boom_throw_waitR);
                }
            }
            return 1;
        }
        case BOOM_FLIGHT:
            if (sBoom == NULL) {
                sBoomPhase = BOOM_CATCH;
                PlayerAnimation_PlayOnce(play, upper, (PlayerAnimationHeader*)sBoomCatchAnim);
                Player_PlaySfx(player, NA_SE_PL_CATCH_BOOMERANG);
                Player_AnimSfx_PlayVoice(player, NA_SE_VO_LI_SWORD_N);
                return 1;
            }
            if (sBoomLocked && player->focusActor != NULL) {
                PlayerAnimation_Update(play, upper);
                return 1;
            }
            return 0;
        case BOOM_CATCH:
            if (PlayerAnimation_Update(play, upper)) sBoomPhase = BOOM_STOWED;
            return 1;
        default:
            return 0;
    }
}

static void BoomAimAction(Player* player, PlayState* play) {
    Z64Items_KeepHeld(sApi);
    Player_UpdateUpperBody(player, play);
    if (player->actionFunc == BoomAimAction && sBoomPhase != BOOM_AIM && sBoomPhase != BOOM_THROW) {
        func_8083B1A0(player, play);
    }
}

// OoT Item Upgrades owns the axe model and keeps the Hammer item as its slot.
static bool IsHammerUpgraded(void) {
    return sApi->IsCustomItemOwned("nei.weapon.gerudo_axe");
}

// SoH's child-hammer patch: instruction 92 starts the baked adult hand; draw MM's fist there instead.
static bool PatchHammerHand(void) {
    static bool sIsPatched;
    if (sIsPatched) {
        return true;
    }
    if (!sApi->HasResource(sHammerHandDL)) {
        return false;
    }
    Gfx fist = gsSPDisplayListOTRFilePath(sHumanLeftFistDL);
    Gfx end = gsSPEndDisplayList();
    ResourceMgr_PatchGfxByName(sHammerHandDL, "ootItemsHammerFist", 92, fist);
    ResourceMgr_PatchGfxByName(sHammerHandDL, "ootItemsHammerEnd", 93, end);
    sIsPatched = true;
    return true;
}

static void DrawOotLeftHand(Player* player, s32 limb, Gfx** displayList, Gfx* original, Vec3f* position) {
    if (gPlayState == NULL || player != GET_PLAYER(gPlayState) || limb != PLAYER_LIMB_LEFT_HAND ||
        player->heldItemAction != PLAYER_IA_CUSTOM) return;
    const char* held = sApi->GetEquippedCustomItem(player->heldItemButton);
    if (held == NULL) return;
    if (strcmp(held, BOOM) == 0 && sBoom == NULL && sApi->HasResource(sBoomHandDL)) {
        *displayList = (Gfx*)sBoomHandDL;
    } else if (strcmp(held, HAMMER) == 0 && !IsHammerUpgraded() && PatchHammerHand()) {
        *displayList = (Gfx*)sHammerHandDL;
    }
}

typedef struct {
    const char* swing;
    const char* recover;
    const char* recoverLocked;
    f32 activeFrame;
    f32 endFrame;
} HammerSwing;

// OoT D_80854190 rows PLAYER_MWA_HAMMER_FORWARD and PLAYER_MWA_HAMMER_SIDE.
static const HammerSwing sHammerSwings[] = {
    { sHammerHitAnim, sHammerHitEndAnim, sHammerHitEndLockedAnim, 3.0f, 10.0f },
    { sHammerSideAnim, sHammerSideEndAnim, sHammerSideEndLockedAnim, 2.0f, 11.0f },
};

static void HammerAttackAction(Player* player, PlayState* play);

static bool IsHoldingHammer(Player* player) {
    const char* held = sApi->GetEquippedCustomItem(player->heldItemButton);
    return player->heldItemAction == PLAYER_IA_CUSTOM && held != NULL && strcmp(held, HAMMER) == 0;
}

static void RequestHammerSwing(PlayState* play, Player* player) {
    sHammerSwingPending = true;
}

static void UseHammer(Player* player, PlayState* play) {
    RequestHammerSwing(play, player);
}

static void PutHammer(Player* player, PlayState* play) {
    sHammerSwingPending = false;
}

// MM has no hammer damage flags; NEI hits like the Goron moveset.
static void SetHammerDamage(Player* player, bool isSideSwing) {
    u32 dmgFlags = isSideSwing ? DMG_GORON_PUNCH : DMG_GORON_POUND;
    s32 damage = isSideSwing ? 2 : 4;
    if (IsHammerUpgraded()) {
        dmgFlags |= DMG_POWDER_KEG;
    }
    func_80833728(player, 0, dmgFlags, damage);
    func_80833728(player, 1, dmgFlags, damage);
}

static void StartHammerSwing(PlayState* play, Player* player) {
    s32 direction = player->controlStickDirections[player->controlStickDataIndex];
    sHammerSideSwing = direction == PLAYER_STICK_DIR_LEFT || direction == PLAYER_STICK_DIR_RIGHT;
    sHammerSwingPending = false;
    Player_SetAction(play, player, HammerAttackAction, 0);
    player->av2.actionVar2 = 0;
    func_8082DC38(player);
    SetHammerDamage(player, sHammerSideSwing);
    player->meleeWeaponAnimation = sHammerSideSwing ? PLAYER_MWA_RIGHT_SLASH_2H : PLAYER_MWA_FORWARD_SLASH_2H;
    player->unk_ADD = 0;
    Player_Anim_PlayOnceAdjusted(play, player, (PlayerAnimationHeader*)sHammerSwings[sHammerSideSwing].swing);
    player->unk_ADC = player->skelAnime.animLength + 4.0f;
    Player_AnimReplace_Setup(play, player, ANIM_FLAG_1 | ANIM_FLAG_ENABLE_MOVEMENT | ANIM_FLAG_NOMOVE);
    player->yaw = player->actor.shape.rot.y;
    Player_PlaySfx(player, NA_SE_IT_HAMMER_SWING);
}

static void HammerRecoilAction(Player* player, PlayState* play) {
    PlayerAnimation_Update(play, &player->skelAnime);
    Player_DecelerateToZero(player);
    if (player->skelAnime.curFrame >= 6.0f) {
        func_80836988(player, play);
    }
}

static void RecoilHammer(PlayState* play, Player* player) {
    static const char* const sRecoilAnims[] = {
        gPlayerAnim_link_fighter_rebound_long,
        gPlayerAnim_link_fighter_rebound_longR,
    };
    func_8082DC38(player);
    Player_SetAction(play, player, HammerRecoilAction, 0);
    Player_Anim_PlayOnceAdjusted(play, player,
                                 (PlayerAnimationHeader*)sRecoilAnims[Player_CheckHostileLockOn(player)]);
    player->speedXZ = -18.0f;
}

static void SmashHammer(PlayState* play, Player* player) {
    Player_RequestQuakeAndRumble(play, player, NA_SE_IT_HAMMER_HIT);
    play->actorCtx.unk2 = 4;
}

// OoT func_80842DF4 hammer branch: a bounce or a wall in front of the head stops the swing.
static bool HasHammerHitWall(PlayState* play, Player* player) {
    if (player->meleeWeaponState <= 0) {
        return false;
    }
    if ((player->meleeWeaponQuads[0].base.atFlags & AT_BOUNCED) ||
        (player->meleeWeaponQuads[1].base.atFlags & AT_BOUNCED)) {
        RecoilHammer(play, player);
        return true;
    }
    if (player->skelAnime.curFrame < 2.0f) {
        return false;
    }
    Vec3f* tip = &player->meleeWeaponInfo[0].tip;
    Vec3f reach;
    f32 extension = Math_Vec3f_DistXYZAndStoreDiff(tip, &player->meleeWeaponInfo[0].base, &reach);
    if (extension != 0.0f) {
        extension = (extension + 10.0f) / extension;
    }
    Vec3f start = { tip->x + reach.x * extension, tip->y + reach.y * extension, tip->z + reach.z * extension };
    Vec3f hitPoint;
    CollisionPoly* poly;
    s32 bgId;
    if (!BgCheck_EntityLineTest2(&play->colCtx, &start, tip, &hitPoint, &poly, true, false, false, true, &bgId,
                                 &player->actor) ||
        SurfaceType_IsIgnoredByEntities(&play->colCtx, poly, bgId) ||
        SurfaceType_GetFloorType(&play->colCtx, poly, bgId) == FLOOR_TYPE_6 ||
        func_800B90AC(play, &player->actor, poly, bgId, &hitPoint)) {
        return false;
    }
    SmashHammer(play, player);
    RecoilHammer(play, player);
    return true;
}

static void UpdateHammerSmash(PlayState* play, Player* player, f32 impactFrame) {
    static Vec3f sCheckOffset = { 0.0f, 40.0f, 45.0f };
    Vec3f shockwavePos;
    shockwavePos.y = func_80835D2C(play, player, &sCheckOffset, &shockwavePos);
    f32 floorDistance = player->actor.world.pos.y - shockwavePos.y;
    Math_ScaledStepToS(&player->actor.focus.rot.x, Math_Atan2S_XY(45.0f, floorDistance), 800);
    func_80832754(player, false);
    if (!PlayerAnimation_OnFrame(&player->skelAnime, impactFrame) || floorDistance <= -40.0f ||
        floorDistance >= 40.0f) {
        return;
    }
    SmashHammer(play, player);
    Actor_SetPlayerImpact(play, PLAYER_IMPACT_GORON_GROUND_POUND, 2, 100.0f, &player->actor.world.pos);
    EffectSsBlast_SpawnWhiteShockwave(play, &shockwavePos, &gZeroVec3f, &gZeroVec3f);
}

static void FinishHammerSwing(PlayState* play, Player* player) {
    const HammerSwing* swing = &sHammerSwings[sHammerSideSwing];
    func_8082DC38(player);
    if (sHammerSwingPending && IsHoldingHammer(player)) {
        StartHammerSwing(play, player);
        return;
    }
    u8 movementFlags = player->skelAnime.movementFlags;
    const char* recover = Player_CheckHostileLockOn(player) ? swing->recoverLocked : swing->recover;
    player->skelAnime.movementFlags = 0;
    Player_SetAction(play, player, Player_Action_Idle, 1);
    Player_Anim_PlayOnceWaterAdjustment(play, player, (PlayerAnimationHeader*)recover);
    player->yaw = player->actor.shape.rot.y;
    player->skelAnime.movementFlags = movementFlags;
    player->stateFlags3 |= PLAYER_STATE3_8;
}

static void HammerAttackAction(Player* player, PlayState* play) {
    const HammerSwing* swing = &sHammerSwings[sHammerSideSwing];
    player->stateFlags2 |= PLAYER_STATE2_20;
    if (HasHammerHitWall(play, player)) {
        return;
    }
    func_8083FCF0(play, player, 0.0f, swing->activeFrame, swing->endFrame);
    Math_StepToF(&player->speedXZ, 0.0f, 5.0f);
    func_8083A548(player);
    if (PlayerAnimation_Update(play, &player->skelAnime)) {
        FinishHammerSwing(play, player);
        return;
    }
    if (!sHammerSideSwing) {
        UpdateHammerSmash(play, player, 7.0f);
    }
}

// OoT animation headers sit under oot_gameplay_keep with the same names as MM's.
static const char* GetOotPlayerAnim(const char* mmAnim) {
    static const char sMmPrefix[] = "__OTR__objects/gameplay_keep/";
    static const char* sSources[32];
    static char sPaths[32][96];
    static s32 sCount;
    if (mmAnim == NULL || strncmp(mmAnim, sMmPrefix, sizeof(sMmPrefix) - 1) != 0) {
        return NULL;
    }
    for (s32 i = 0; i < sCount; i++) {
        if (sSources[i] == mmAnim) {
            return sPaths[i][0] != '\0' ? sPaths[i] : NULL;
        }
    }
    if (sCount == ARRAY_COUNT(sPaths)) {
        return NULL;
    }
    char* path = sPaths[sCount];
    snprintf(path, sizeof(sPaths[0]), "__OTR__objects/oot_gameplay_keep/%s", mmAnim + sizeof(sMmPrefix) - 1);
    if (!sApi->HasResource(path)) {
        path[0] = '\0';
    }
    sSources[sCount++] = mmAnim;
    return path[0] != '\0' ? path : NULL;
}

// OoT holds the hammer with PLAYER_ANIMTYPE_3; MM's "_long" poses are redrawn for the Great Fairy's Sword.
static void ResolveHammerStance(s32 group, s32 animType, PlayerAnimationHeader** anim) {
    PlayState* play = gPlayState;
    if (play == NULL || animType != PLAYER_ANIMTYPE_3 || *anim == NULL || !IsHoldingHammer(GET_PLAYER(play))) {
        return;
    }
    const char* name = (const char*)*anim;
    const char* ootAnim = strstr(name, "_long") != NULL ? GetOotPlayerAnim(name) : NULL;
    if (ootAnim != NULL) {
        *anim = (PlayerAnimationHeader*)ootAnim;
    }
}

static bool IsHammerJumpslash(Player* player) {
    return player->meleeWeaponAnimation == PLAYER_MWA_JUMPSLASH_START ||
           player->meleeWeaponAnimation == PLAYER_MWA_JUMPSLASH_FINISH;
}

// MM's jumpslash runs its own action; swap in OoT's two-handed poses and the hammer landing.
static void UpdateHammerJumpslash(PlayState* play, Player* player) {
    if (!IsHammerJumpslash(player)) {
        return;
    }
    SkelAnime* skelAnime = &player->skelAnime;
    const char* name = (const char*)skelAnime->animation;
    const char* ootAnim = (name != NULL && strstr(name, "power_jump_kiru") != NULL) ? GetOotPlayerAnim(name) : NULL;
    if (ootAnim != NULL) {
        PlayerAnimation_Change(play, skelAnime, (PlayerAnimationHeader*)ootAnim, skelAnime->playSpeed,
                               skelAnime->curFrame, Animation_GetLastFrame((void*)ootAnim), skelAnime->mode, 0.0f);
    }
    SetHammerDamage(player, false);
    // NEI smashes from the landing action itself; the hitbox can already be off on frame 2.
    if (player->meleeWeaponAnimation == PLAYER_MWA_JUMPSLASH_FINISH && player->actionFunc == Player_Action_84) {
        UpdateHammerSmash(play, player, 2.0f);
    }
}
static bool IsSongPlaying(PlayState* play) {
    return play->msgCtx.msgMode == MSGMODE_SONG_PLAYED;
}

static void DinsFireInit(Actor* actor, PlayState* play) {
    OotDinsFire* fire = (OotDinsFire*)actor;
    fire->alphaMultiplier = -3.0f;
    Actor_SetScale(actor, 0.0f);
    Collider_InitCylinder(play, &fire->collider);
    Collider_SetCylinder(play, &fire->collider, actor, &sDinsFireCollider);
    Collider_UpdateCylinder(actor, &fire->collider);
    fire->casting = true;
    fire->actionTimer = 20;
    actor->room = -1;
}

static void DinsFireDestroy(Actor* actor, PlayState* play) {
    OotDinsFire* fire = (OotDinsFire*)actor;
    Collider_DestroyCylinder(play, &fire->collider);
    Magic_Reset(play);
}

static void UpdateDinsFireSphere(OotDinsFire* fire) {
    Actor* actor = &fire->actor;
    switch (fire->action) {
        case DINS_FIRE_INITIALIZE:
            fire->actionTimer = 30;
            actor->scale.x = actor->scale.y = actor->scale.z = 0.0f;
            actor->world.rot.x = actor->world.rot.y = actor->world.rot.z = 0;
            actor->shape.rot.x = actor->shape.rot.y = actor->shape.rot.z = 0;
            fire->alphaMultiplier = 0.0f;
            fire->scalingSpeed = 0.08f;
            fire->action++;
            break;
        case DINS_FIRE_EXPAND_SLOWLY:
            Math_StepToF(&fire->alphaMultiplier, 1.0f, 1.0f / 30.0f);
            if (fire->actionTimer > 0) {
                Math_SmoothStepToF(&actor->scale.x, 0.4f, fire->scalingSpeed, 0.1f, 0.001f);
                actor->scale.y = actor->scale.z = actor->scale.x;
            } else {
                fire->actionTimer = 25;
                fire->action++;
            }
            break;
        case DINS_FIRE_STOP_EXPANDING:
            if (fire->actionTimer <= 0) {
                fire->actionTimer = 15;
                fire->action++;
                fire->scalingSpeed = 0.05f;
            }
            break;
        case DINS_FIRE_EXPAND_QUICKLY:
            fire->alphaMultiplier -= 8.0f / 119.00001f;
            actor->scale.x += fire->scalingSpeed;
            actor->scale.y += fire->scalingSpeed;
            actor->scale.z += fire->scalingSpeed;
            if (fire->alphaMultiplier <= 0.0f) {
                fire->action = DINS_FIRE_INITIALIZE;
                Actor_Kill(actor);
            }
            break;
        default:
            break;
    }
}

static void UpdateDinsFireTint(OotDinsFire* fire) {
    switch (fire->tintBehaviour) {
        case DINS_TINT_NONE:
            if (fire->tintTimer <= 0) {
                fire->tintTimer = 20;
                fire->tintBehaviour = DINS_TINT_FADE_IN;
            }
            break;
        case DINS_TINT_FADE_IN:
            fire->tintIntensity = 1.0f - (fire->tintTimer / 20.0f);
            if (fire->tintTimer <= 0) {
                fire->tintTimer = 45;
                fire->tintBehaviour = DINS_TINT_MAINTAIN;
            }
            break;
        case DINS_TINT_MAINTAIN:
            if (fire->tintTimer <= 0) {
                fire->tintTimer = 5;
                fire->tintBehaviour = DINS_TINT_FADE_OUT;
            }
            break;
        case DINS_TINT_FADE_OUT:
            fire->tintIntensity = fire->tintTimer / 5.0f;
            if (fire->tintTimer <= 0) {
                fire->tintBehaviour = DINS_TINT_FINISHED;
            }
            break;
        default:
            break;
    }
}

static void DinsFireUpdate(Actor* actor, PlayState* play) {
    OotDinsFire* fire = (OotDinsFire*)actor;
    Player* player = GET_PLAYER(play);
    actor->world.pos = player->actor.world.pos;
    if (IsSongPlaying(play)) {
        Actor_Kill(actor);
        return;
    }
    if (fire->casting) {
        if (fire->actionTimer > 0) {
            fire->actionTimer--;
        } else {
            fire->casting = false;
            Player_PlaySfx(player, NA_SE_PL_MAGIC_FIRE);
        }
        return;
    }
    if (fire->action == DINS_FIRE_EXPAND_SLOWLY) {
        fire->collider.elem.atDmgInfo.damage = fire->actionTimer + 25;
    } else if (fire->action == DINS_FIRE_STOP_EXPANDING) {
        fire->collider.elem.atDmgInfo.damage = fire->actionTimer;
    }
    Collider_UpdateCylinder(actor, &fire->collider);
    fire->collider.dim.radius = actor->scale.x * 325.0f;
    fire->collider.dim.height = actor->scale.y * 450.0f;
    fire->collider.dim.yShift = actor->scale.y * -225.0f;
    CollisionCheck_SetAT(play, &play->colChkCtx, &fire->collider.base);
    UpdateDinsFireSphere(fire);
    UpdateDinsFireTint(fire);
    if (fire->actionTimer > 0) {
        fire->actionTimer--;
    }
    if (fire->tintTimer > 0) {
        fire->tintTimer--;
    }
}

// OoT's bright core and translucent shell come from per-vertex alpha on the shared sphere.
static void FadeDinsFireSphere(f32 alphaMultiplier) {
    static const u8 sVertexIndices[] = {
        3,  4,  5,  6,  7,  8,  9,  10, 16, 17, 18, 19, 25, 26, 27, 32, 35, 36, 37, 38,
        39, 45, 46, 47, 52, 53, 54, 59, 60, 61, 67, 68, 69, 70, 71, 72, 0,  1,  11, 12,
        14, 20, 21, 23, 28, 30, 33, 34, 40, 41, 43, 48, 50, 55, 57, 62, 64, 65, 73, 74,
    };
    Vtx* vertices = ResourceMgr_LoadVtxByName((char*)sDinFireSphereVtx);
    if (vertices == NULL) {
        return;
    }
    u8 coreAlpha = (s32)(alphaMultiplier * 255);
    u8 shellAlpha = (s32)(alphaMultiplier * 76);
    for (s32 i = 0; i < ARRAY_COUNT(sVertexIndices); i++) {
        vertices[sVertexIndices[i]].n.a = (i < 36) ? coreAlpha : shellAlpha;
    }
}

static void DinsFireDraw(Actor* actor, PlayState* play) {
    OotDinsFire* fire = (OotDinsFire*)actor;
    u32 frames = play->gameplayFrames;
    if (fire->action <= DINS_FIRE_INITIALIZE || !sApi->HasResource(sDinFireModel) ||
        !sApi->HasResource(sDinFireMaterial) || !sApi->HasResource(sDinFireTexture)) {
        return;
    }
    f32 tint = fire->tintIntensity;
    u8 alpha = (u8)(fire->alphaMultiplier * 255);
    OPEN_DISPS(play->state.gfxCtx);
    POLY_XLU_DISP = Gfx_SetupDL57(POLY_XLU_DISP);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, (u8)(s32)(60 * tint), (u8)(s32)(20 * tint), 0, (u8)(s32)(120 * tint));
    gDPSetAlphaDither(POLY_XLU_DISP++, G_AD_DISABLE);
    gDPSetColorDither(POLY_XLU_DISP++, G_CD_DISABLE);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)D_0E000000_TO_SEGMENTED(fillRect));
    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 255, 200, 0, alpha);
    gDPSetEnvColor(POLY_XLU_DISP++, 255, 0, 0, alpha);
    Matrix_Scale(0.15f, 0.15f, 0.15f, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gDPPipeSync(POLY_XLU_DISP++);
    gSPTexture(POLY_XLU_DISP++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
    gDPSetTextureLUT(POLY_XLU_DISP++, G_TT_NONE);
    gDPLoadTextureBlock(POLY_XLU_DISP++, sDinFireTexture, G_IM_FMT_I, G_IM_SIZ_8b, 64, 64, 0,
                        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, 6, 6, 15, G_TX_NOLOD);
    gDPSetTile(POLY_XLU_DISP++, G_IM_FMT_I, G_IM_SIZ_8b, 8, 0, 1, 0, G_TX_NOMIRROR | G_TX_WRAP, 6, 14,
               G_TX_NOMIRROR | G_TX_WRAP, 6, 14);
    gDPSetTileSize(POLY_XLU_DISP++, 1, 0, 0, 252, 252);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sDinFireMaterial);
    gSPDisplayList(POLY_XLU_DISP++,
                   Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, (frames * 2) % 512, 511 - ((frames * 5) % 512), 64, 64,
                                      1, (frames * 2) % 256, 255 - ((frames * 20) % 256), 32, 32, 2, -5, 2, -20));
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sDinFireModel);
    CLOSE_DISPS(play->state.gfxCtx);
    FadeDinsFireSphere(fire->alphaMultiplier);
}

static void DimNayruLighting(PlayState* play, f32 intensity) {
    EnvironmentContext* env = &play->envCtx;
    intensity = CLAMP(intensity, 0.0f, 1.0f);
    f32 fogScale = (intensity < 0.2f) ? 0.0f : intensity - 0.2f;
    env->adjLightSettings.fogNear = (850.0f - env->lightSettings.fogNear) * fogScale;
    f32 colorScale = MIN(intensity * 5.0f, 1.0f);
    for (s32 i = 0; i < ARRAY_COUNT(env->adjLightSettings.fogColor); i++) {
        env->adjLightSettings.fogColor[i] = (intensity == 0.0f) ? 0 : -(s16)(env->lightSettings.fogColor[i] * colorScale);
    }
}

static void NayrusLoveInit(Actor* actor, PlayState* play) {
    OotNayrusLove* nayru = (OotNayrusLove*)actor;
    nayru->scale = 0.4f;
    actor->world.pos = GET_PLAYER(play)->actor.world.pos;
    Actor_SetScale(actor, 0.0f);
    actor->room = -1;
    nayru->timer = 0;
    if (sNayruTimer != 0) {
        nayru->diamond = true;
        actor->scale.x = actor->scale.z = nayru->scale * 1.6f;
        actor->scale.y = nayru->scale * 0.8f;
        nayru->primAlpha = 0;
    }
    sNayrusLove = actor;
}

static void NayrusLoveDestroy(Actor* actor, PlayState* play) {
    if (sNayruTimer == 0) {
        Magic_Reset(play);
    }
    if (sNayrusLove == actor) {
        sNayrusLove = NULL;
    }
}

static void UpdateNayruOrb(OotNayrusLove* nayru, PlayState* play) {
    Actor* actor = &nayru->actor;
    Actor_PlaySfx_Flagged(actor, NA_SE_PL_MAGIC_SOUL_BALL - SFX_FLAG);
    if (nayru->timer < 35) {
        DimNayruLighting(play, nayru->timer * (1 / 45.0f));
        Math_SmoothStepToF(&actor->scale.x, nayru->scale * (1 / 12.000001f), 0.05f, 0.01f, 0.0001f);
        Actor_SetScale(actor, actor->scale.x);
    } else if (nayru->timer < 55) {
        Actor_SetScale(actor, actor->scale.x * 0.9f);
        Math_SmoothStepToF(&nayru->orbOffset.y, GET_PLAYER(play)->bodyPartsPos[0].y, 0.5f, 3.0f, 1.0f);
        if (nayru->timer > 48) {
            DimNayruLighting(play, (54 - nayru->timer) * 0.2f);
        }
    } else {
        nayru->diamond = true;
        actor->scale.x = actor->scale.z = nayru->scale * 1.6f;
        actor->scale.y = nayru->scale * 0.8f;
        nayru->timer = 0;
        nayru->primAlpha = 0;
    }
    nayru->timer++;
}

static void UpdateNayruDiamond(OotNayrusLove* nayru, PlayState* play) {
    Actor* actor = &nayru->actor;
    Player* player = GET_PLAYER(play);
    s16 timer = sNayruTimer;
    if (timer >= 1200) {
        player->invincibilityTimer = 0;
        sNayruTimer = 0;
        Actor_Kill(actor);
        return;
    }
    player->invincibilityTimer = -100;
    if (nayru->timer < 20) {
        actor->scale.x = actor->scale.z = (1.6f - (nayru->timer * 0.03f)) * nayru->scale;
        actor->scale.y = ((nayru->timer * 0.01f) + 0.8f) * nayru->scale;
    } else {
        actor->scale.x = actor->scale.z = nayru->scale;
        actor->scale.y = nayru->scale;
    }
    actor->scale.x *= 1.3f;
    actor->scale.z *= 1.3f;
    u8 fadeIn = (nayru->timer < 20) ? (nayru->timer * 12) : 255;
    if (timer >= 1180) {
        nayru->primAlpha = 15595 - (timer * 13);
        if (timer & 1) {
            nayru->primAlpha >>= 1;
        }
    } else if (timer >= 1100) {
        nayru->primAlpha = (u8)(timer << 7) + 127;
    } else {
        nayru->primAlpha = 255;
    }
    nayru->primAlpha = MIN(nayru->primAlpha, fadeIn);
    actor->world.rot.y += 0x3E8;
    actor->shape.rot.y = actor->world.rot.y + Camera_GetCamDirYaw(GET_ACTIVE_CAM(play));
    nayru->timer++;
    sNayruTimer = timer + 1;
    Actor_PlaySfx_Flagged(actor, (timer < 1100 ? NA_SE_PL_MAGIC_SOUL_NORMAL : NA_SE_PL_MAGIC_SOUL_FLASH) - SFX_FLAG);
}

static void NayrusLoveUpdate(Actor* actor, PlayState* play) {
    OotNayrusLove* nayru = (OotNayrusLove*)actor;
    if (!nayru->diamond) {
        UpdateNayruOrb(nayru, play);
        return;
    }
    if (IsSongPlaying(play)) {
        Actor_Kill(actor);
        return;
    }
    UpdateNayruDiamond(nayru, play);
}

static void DrawNayruOrb(OotNayrusLove* nayru, PlayState* play) {
    Actor* actor = &nayru->actor;
    Player* player = GET_PLAYER(play);
    s16 spin = (play->state.frames & 0x1F) * 0x400;
    Vec3f pos;
    if (nayru->timer < 32) {
        pos.x = (player->bodyPartsPos[12].x + player->bodyPartsPos[15].x) * 0.5f;
        pos.y = (player->bodyPartsPos[12].y + player->bodyPartsPos[15].y) * 0.5f;
        pos.z = (player->bodyPartsPos[12].z + player->bodyPartsPos[15].z) * 0.5f;
        if (nayru->timer > 20) {
            pos.y += (nayru->timer - 20) * 1.4f;
        }
        nayru->orbOffset = pos;
    } else if (nayru->timer < 130) {
        pos = nayru->orbOffset;
    } else {
        return;
    }
    Camera* camera = GET_ACTIVE_CAM(play);
    s16 camYaw = Camera_GetCamDirYaw(camera);
    s16 camPitch = Camera_GetCamDirPitch(camera);
    pos.x -= actor->scale.x * 300.0f * Math_SinS(camYaw) * Math_CosS(camPitch);
    pos.y -= actor->scale.x * 300.0f * Math_SinS(camPitch);
    pos.z -= actor->scale.x * 300.0f * Math_CosS(camYaw) * Math_CosS(camPitch);
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 170, 255, 255, 255);
    gDPSetEnvColor(POLY_XLU_DISP++, 0, 150, 255, 255);
    Matrix_Translate(pos.x, pos.y, pos.z, MTXMODE_NEW);
    Matrix_Scale(actor->scale.x, actor->scale.y, actor->scale.z, MTXMODE_APPLY);
    Matrix_Mult(&play->billboardMtxF, MTXMODE_APPLY);
    Matrix_Push();
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    Matrix_RotateZS(spin, MTXMODE_APPLY);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gEffFlash1DL);
    Matrix_Pop();
    Matrix_RotateZS(-spin, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gEffFlash1DL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawNayruDiamond(OotNayrusLove* nayru, PlayState* play) {
    Actor* actor = &nayru->actor;
    Player* player = GET_PLAYER(play);
    u16 frames = play->gameplayFrames;
    if (!sApi->HasResource(sNayruModel) || !sApi->HasResource(sNayruMaterial)) {
        return;
    }
    actor->world.pos.x = player->bodyPartsPos[0].x;
    actor->world.pos.z = player->bodyPartsPos[0].z;
    f32 heightDiff = player->bodyPartsPos[0].y - actor->world.pos.y;
    if (heightDiff < -2.0f) {
        actor->world.pos.y = player->bodyPartsPos[0].y + 2.0f;
    } else if (heightDiff > 2.0f) {
        actor->world.pos.y = player->bodyPartsPos[0].y - 2.0f;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    Matrix_Translate(actor->world.pos.x, actor->world.pos.y, actor->world.pos.z, MTXMODE_NEW);
    Matrix_Scale(actor->scale.x, actor->scale.y, actor->scale.z, MTXMODE_APPLY);
    Matrix_RotateYS(actor->shape.rot.y, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 170, 255, 255, (s32)(nayru->primAlpha * 0.6f) & 0xFF);
    gDPSetEnvColor(POLY_XLU_DISP++, 0, 100, 255, 128);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sNayruMaterial);
    gSPDisplayList(POLY_XLU_DISP++, Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, frames * 2, frames * -4, 32, 32, 1, 0,
                                                       frames * -16, 64, 32, 2, -4, 0, -16));
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sNayruModel);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void NayrusLoveDraw(Actor* actor, PlayState* play) {
    OotNayrusLove* nayru = (OotNayrusLove*)actor;
    if (nayru->diamond) {
        DrawNayruDiamond(nayru, play);
    } else {
        DrawNayruOrb(nayru, play);
    }
}
static void WaiveVoid(bool* should, va_list args) {
    if (sWaiveVoid) { *should = false; sWaiveVoid = false; }
}
typedef enum { WIND_WAIT, WIND_GROW, WIND_HOLD, WIND_FADE, WIND_SHRINK } OotWindPhase;

typedef struct {
    Actor actor;
    SkelCurve curve;
    s16 timer;
    OotWindPhase phase;
    bool ready;
} OotWindEffect;

// The cylinder vertices are a cached resource, so the alpha a fade leaves behind must be restored.
static void SetWindAlpha(f32 alpha) {
    static const u8 sAlphaVertices[] = {
        0x00, 0x03, 0x04, 0x07, 0x09, 0x0A, 0x0D, 0x0F, 0x11, 0x12, 0x15, 0x16, 0x19, 0x1B, 0x1C, 0x1F, 0x21, 0x23,
    };
    Vtx* vertices = ResourceMgr_LoadVtxByName((char*)sWindCylinderVtx);
    if (vertices == NULL) {
        return;
    }
    for (s32 i = 0; i < ARRAY_COUNT(sAlphaVertices); i++) {
        vertices[sAlphaVertices[i]].n.a = alpha * 255.0f;
    }
}

static void WindInit(Actor* actor, PlayState* play) {
    OotWindEffect* wind = (OotWindEffect*)actor;
    actor->room = -1;
    if (!sApi->HasResource(sWindSkeleton) || !sApi->HasResource(sWindAnimation) ||
        !SkelCurve_Init(play, &wind->curve, (CurveSkeletonHeader*)sWindSkeleton,
                        (CurveAnimationHeader*)sWindAnimation) || wind->curve.jointTable == NULL) {
        Actor_Kill(actor);
        return;
    }
    wind->ready = true;
    SetWindAlpha(1.0f);
    if (actor->params == 0) {
        SkelCurve_SetAnim(&wind->curve, (CurveAnimationHeader*)sWindAnimation, 0.0f, 60.0f, 0.0f, 1.0f);
        wind->timer = 29;
        wind->phase = WIND_WAIT;
    } else {
        SkelCurve_SetAnim(&wind->curve, (CurveAnimationHeader*)sWindAnimation, 60.0f, 0.0f, 60.0f, -1.0f);
        wind->phase = WIND_SHRINK;
        Player_PlaySfx(GET_PLAYER(play), NA_SE_PL_MAGIC_WIND_WARP);
    }
}

static void WindDestroy(Actor* actor, PlayState* play) {
    OotWindEffect* wind = (OotWindEffect*)actor;
    if (wind->ready) {
        SkelCurve_Destroy(play, &wind->curve);
    }
    Magic_Reset(play);
}

static void WindUpdate(Actor* actor, PlayState* play) {
    OotWindEffect* wind = (OotWindEffect*)actor;
    if (IsSongPlaying(play)) {
        Actor_Kill(actor);
        return;
    }
    switch (wind->phase) {
        case WIND_WAIT:
            if (wind->timer > 0) {
                wind->timer--;
                break;
            }
            Player_PlaySfx(GET_PLAYER(play), NA_SE_PL_MAGIC_WIND_NORMAL);
            SetWindAlpha(1.0f);
            wind->phase = WIND_GROW;
            SkelCurve_Update(play, &wind->curve);
            break;
        case WIND_GROW:
            if (SkelCurve_Update(play, &wind->curve)) {
                wind->phase = WIND_HOLD;
                wind->timer = 50;
            }
            break;
        case WIND_HOLD:
            if (wind->timer > 0) {
                wind->timer--;
            } else {
                wind->phase = WIND_FADE;
                wind->timer = 30;
            }
            break;
        case WIND_FADE:
            if (wind->timer > 0) {
                SetWindAlpha(wind->timer * (1.0f / 30.0f));
                wind->timer--;
            } else {
                Actor_Kill(actor);
            }
            break;
        case WIND_SHRINK:
            if (SkelCurve_Update(play, &wind->curve)) {
                Actor_Kill(actor);
            }
            break;
        default:
            Actor_Kill(actor);
            break;
    }
}

static s32 WindLimb(PlayState* play, SkelCurve* curve, s32 limb, Actor* actor) {
    GraphicsContext* gfx = play->state.gfxCtx;
    u32 frames = play->state.frames;
    OPEN_DISPS(gfx);
    if (limb == 1) {
        gSPSegment(POLY_XLU_DISP++, 8,
                   (uintptr_t)Gfx_TwoTexScrollEx(gfx, 0, (frames * 9) & 0xFF, 0xFF - ((frames * 0xF) & 0xFF), 0x40,
                                                  0x40, 1, (frames * 0xF) & 0xFF, 0xFF - ((frames * 0x1E) & 0xFF),
                                                  0x40, 0x40, 9, -0xF, 0xF, -0x1E));
    } else if (limb == 2) {
        gSPSegment(POLY_XLU_DISP++, 9,
                   (uintptr_t)Gfx_TwoTexScrollEx(gfx, 0, (frames * 3) & 0xFF, 0xFF - ((frames * 5) & 0xFF), 0x40,
                                                  0x40, 1, (frames * 6) & 0xFF, 0xFF - ((frames * 0xA) & 0xFF), 0x40,
                                                  0x40, 3, -5, 6, -0xA));
    }
    CLOSE_DISPS(gfx);
    return true;
}

static void WindDraw(Actor* actor, PlayState* play) {
    OotWindEffect* wind = (OotWindEffect*)actor;
    if (!wind->ready || wind->phase == WIND_WAIT) {
        return;
    }
    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    SkelCurve_Draw(actor, play, &wind->curve, WindLimb, NULL, 1, actor);
}

// OoT stores respawn[RESPAWN_MODE_DOWN]; the marker then flies there from the cast spot.
static void SetFaroresPoint(PlayState* play, Player* player) {
    RespawnData* down = &gSaveContext.respawn[RESPAWN_MODE_DOWN];
    sSave.valid = true;
    sSave.entrance = down->entrance;
    sSave.scene = play->sceneId;
    sSave.room = down->roomIndex;
    sSave.yaw = down->yaw;
    sSave.pos = down->pos;
    sFaroreRoomFlags.switches = down->tempSwitchFlags;
    sFaroreRoomFlags.collectible1 = down->unk_18;
    sFaroreRoomFlags.collectible2 = down->tempCollectFlags;
    sApi->StorageSet(MOD, "farore_room_flags", &sFaroreRoomFlags, sizeof(sFaroreRoomFlags));
    Store();
    sFaroreMarkerData = 1;
    sFaroreMarkerPos = player->actor.world.pos;
}

// OoT greys Farore's Wind out on the overworld; these are MM's dungeons and boss lairs.
static bool CanSceneHoldFaroresWind(PlayState* play) {
    switch (Play_GetOriginalSceneId(play->sceneId)) {
        case SCENE_MITURIN:
        case SCENE_MITURIN_BS:
        case SCENE_HAKUGIN:
        case SCENE_HAKUGIN_BS:
        case SCENE_SEA:
        case SCENE_SEA_BS:
        case SCENE_INISIE_N:
        case SCENE_INISIE_R:
        case SCENE_INISIE_BS:
        case SCENE_LAST_DEKU:
        case SCENE_LAST_GORON:
        case SCENE_LAST_ZORA:
        case SCENE_LAST_LINK:
        case SCENE_LAST_BS:
            return true;
        default:
            return false;
    }
}

static void SetCastCutsceneFlags(Player* player, bool active) {
    if (active) {
        player->stateFlags1 |= PLAYER_STATE1_10000000 | PLAYER_STATE1_20000000;
    } else {
        player->stateFlags1 &= ~(PLAYER_STATE1_10000000 | PLAYER_STATE1_20000000);
    }
}

static void ReturnToFaroresPoint(PlayState* play) {
    RespawnData* respawn = &gSaveContext.respawn[RESPAWN_MODE_DOWN];
    respawn->entrance = sSave.entrance;
    respawn->roomIndex = sSave.room;
    respawn->pos = sSave.pos;
    respawn->yaw = sSave.yaw;
    respawn->playerParams = 0xDFF;
    respawn->tempSwitchFlags = sFaroreRoomFlags.switches;
    respawn->unk_18 = sFaroreRoomFlags.collectible1;
    respawn->tempCollectFlags = sFaroreRoomFlags.collectible2;
    play->nextEntrance = sSave.entrance;
    play->transitionTrigger = TRANS_TRIGGER_START;
    play->transitionType = TRANS_TYPE_FADE_WHITE;
    gSaveContext.nextTransitionType = TRANS_TYPE_FADE_WHITE;
    gSaveContext.respawnFlag = 1;
    sWaiveVoid = true;
    sFaroreReturnPending = true;
}

static void DispelFaroresPoint(void) {
    sSave.valid = false;
    Store();
    Audio_PlaySfx(NA_SE_PL_MAGIC_WIND_VANISH);
    if (sFaroreMarkerData > 0) {
        sFaroreMarkerData = -sFaroreMarkerData;
    }
}

static void ResolveFarorePrompt(PlayState* play) {
    if (!sFarorePrompt) {
        return;
    }
    if (play->msgCtx.msgMode == MSGMODE_NONE) {
        sFarorePrompt = false;
        return;
    }
    if (Message_GetState(&play->msgCtx) != TEXT_STATE_CHOICE || !Message_ShouldAdvance(play)) {
        return;
    }
    u8 choice = play->msgCtx.choiceIndex;
    Message_CloseTextbox(play);
    sFarorePrompt = false;
    if (choice == 0 && sSave.valid) {
        ReturnToFaroresPoint(play);
    } else if (choice == 1) {
        DispelFaroresPoint();
    }
}

static void FarorePromptAction(Player* player, PlayState* play) {
    player->stateFlags2 |= PLAYER_STATE2_20;
    PlayerAnimation_Update(play, &player->skelAnime);
    Player_UpdateUpperBody(player, play);
    ResolveFarorePrompt(play);
    if (!sFarorePrompt && !sFaroreReturnPending) {
        SetCastCutsceneFlags(player, false);
        func_80836988(player, play);
    }
}

static void QueueCast(PlayState* play, OotCast spell) {
    if (sActiveCast != CAST_NONE || sPendingCast != CAST_NONE) {
        return;
    }
    if (!gSaveContext.save.saveInfo.playerData.isMagicAcquired || gSaveContext.magicState != MAGIC_STATE_IDLE ||
        (!HasUnlimitedMagic() && gSaveContext.save.saveInfo.playerData.magic < sCastCosts[spell])) {
        Audio_PlaySfx(NA_SE_SY_ERROR);
        return;
    }
    sPendingCast = spell;
}

static void CastDin(PlayState* play, Player* player) {
    QueueCast(play, CAST_DIN);
}

static void CastFarore(PlayState* play, Player* player) {
    if (sActiveCast != CAST_NONE || sPendingCast != CAST_NONE || sFarorePrompt) {
        return;
    }
    if (!CanSceneHoldFaroresWind(play)) {
        Audio_PlaySfx(NA_SE_SY_ERROR);
    } else if (sSave.valid) {
        sFarorePrompt = sApi->ShowTextbox(play, "\xC3\x02" "Return to Warp Point\x11"
                                               "Dispel Warp Point\x11" "Exit\xBF", false);
        sFarorePromptPending = sFarorePrompt;
    } else {
        QueueCast(play, CAST_FARORE);
    }
}

static void CastNayru(PlayState* play, Player* player) {
    QueueCast(play, CAST_NAYRU);
}

static void ReleaseOotCastCamera(PlayState* play) {
    if (sCastCamera != CAM_ID_NONE) {
        Play_ChangeCameraStatus(play, CAM_ID_MAIN, CAM_STATUS_ACTIVE);
        Play_ClearCamera(play, sCastCamera);
        sCastCamera = CAM_ID_NONE;
    }
}

// A cast cut short before its spell spawned leaves the WAIT_PREVIEW reservation flashing forever.
static void AbortOotCast(PlayState* play, Player* player) {
    sActiveCast = CAST_NONE;
    if (gSaveContext.magicState == MAGIC_STATE_METER_FLASH_2) {
        Magic_Reset(play);
    }
    SetCastCutsceneFlags(player, false);
    ReleaseOotCastCamera(play);
}

static void FinishOotCast(PlayState* play, Player* player) {
    sActiveCast = CAST_NONE;
    SetCastCutsceneFlags(player, false);
    ReleaseOotCastCamera(play);
    func_80836988(player, play);
}

typedef struct {
    s16 duration;
    s16 roll;
    f32 fov;
    Vec3f at;
    Vec3f eye;
} DinCameraShot;

// OoT OnePointCutscene 1100 (Din's Fire); at/eye are pitch and yaw in degrees, then radius.
// The short version replays within 3600 frames, mirrored on odd frames.
static const DinCameraShot sDinShots[] = {
    { 2, 8, 70, { -27, -96, 25 }, { 37, -5, 100 } },
    { 38, 4, 60, { 64, -109, 55 }, { 37, 150, 155 } },
    { 2, 8, 70, { 45, 123, 45 }, { 70, 5, 125 } },
    { 58, 4, 60, { 82, 95, 55 }, { 25, -175, 180 } },
};
static const DinCameraShot sDinRepeatShots[] = {
    { 20, 8, 70, { 65, -150, 50 }, { 30, 10, 90 } },
    { 100, 0, 60, { 70, -160, 50 }, { 25, 180, 180 } },
};

static s16 CastCameraAngle(f32 degrees) {
    return (s16)(s32)(degrees * (65536.0f / 360.0f));
}

static Vec3f CastCameraOffset(Vec3f origin, s16 pitch, s16 yaw, f32 radius) {
    f32 horizontal = Math_CosS(pitch) * radius;
    return (Vec3f){ origin.x + Math_SinS(yaw) * horizontal,
                    origin.y + Math_SinS(pitch) * radius,
                    origin.z + Math_CosS(yaw) * horizontal };
}

static Vec3f DinCameraPoint(Player* player, Vec3f offset, bool mirror) {
    return CastCameraOffset(player->actor.world.pos, CastCameraAngle(offset.x) - player->actor.world.rot.x,
                            player->actor.world.rot.y + CastCameraAngle(mirror ? -offset.y : offset.y), offset.z);
}

static void UpdateDinCamera(PlayState* play, Player* player, Camera* camera) {
    const DinCameraShot* shots = sDinShortCamera ? sDinRepeatShots : sDinShots;
    s32 count = sDinShortCamera ? ARRAY_COUNT(sDinRepeatShots) : ARRAY_COUNT(sDinShots);
    s32 frame = sCastAge;
    s32 shotIndex = 0;
    while (shotIndex < count && frame >= shots[shotIndex].duration) {
        frame -= shots[shotIndex++].duration;
    }
    if (shotIndex == count) {
        ReleaseOotCastCamera(play);
        return;
    }
    const DinCameraShot* shot = &shots[shotIndex];
    Vec3f at = DinCameraPoint(player, shot->at, sDinMirrorCamera);
    Vec3f eye = DinCameraPoint(player, shot->eye, sDinMirrorCamera && shotIndex == 0);
    f32 fov = shot->fov;
    s16 roll = CastCameraAngle(sDinMirrorCamera && shotIndex == 0 ? -shot->roll : shot->roll);
    if (shotIndex & 1) {
        f32 step = (shotIndex == 3 ? 0.9f : 1.0f) / (shot->duration - frame);
        s16 yaw = Math_Vec3f_Yaw(&camera->at, &camera->eye);
        s16 pitch = -Math_Vec3f_Pitch(&camera->at, &camera->eye);
        f32 radius = Math_Vec3f_DistXYZ(&camera->at, &camera->eye);
        yaw += (s16)(Math_Vec3f_Yaw(&at, &eye) - yaw) * step;
        pitch += (s16)(-Math_Vec3f_Pitch(&at, &eye) - pitch) * step;
        radius += (Math_Vec3f_DistXYZ(&at, &eye) - radius) * step;
        eye = CastCameraOffset(camera->at, pitch, yaw, radius);
        at.x = camera->at.x + (at.x - camera->at.x) * step;
        at.y = camera->at.y + (at.y - camera->at.y) * step;
        at.z = camera->at.z + (at.z - camera->at.z) * step;
        fov = camera->fov + (fov - camera->fov) * step;
        roll = camera->roll + (s16)(roll - camera->roll) * step;
    }
    Play_SetCameraAtEye(play, sCastCamera, &at, &eye);
    Play_SetCameraFov(play, sCastCamera, fov);
    Play_SetCameraRoll(play, sCastCamera, roll);
}

static void UpdateOotCastCamera(PlayState* play, Player* player) {
    if (sCastCamera == CAM_ID_NONE) return;
    Camera* camera = Play_GetCamera(play, sCastCamera);
    if (sActiveCast == CAST_DIN) {
        UpdateDinCamera(play, player, camera);
        return;
    }

    // OoT KeepOn4, item type 10: turn toward the front over 60 frames.
    f32 height = Player_GetHeight(player);
    f32 normalizedHeight = (height + 68.0f) * 0.5f;
    f32 turn = MIN(sCastAge, 60) / 60.0f;
    s16 yaw = sCastStartYaw + (s16)(sCastTurnYaw - sCastStartYaw) * turn;
    s16 pitch = sCastStartPitch + (s16)(CastCameraAngle(-15.0f) - sCastStartPitch) * turn;
    Vec3f at = player->actor.world.pos;
    at.y += height - normalizedHeight * 0.5f;
    at.x = (at.x + camera->at.x) * 0.5f;
    at.y = (at.y + camera->at.y) * 0.5f;
    at.z = (at.z + camera->at.z) * 0.5f;
    f32 radius = Math_Vec3f_DistXYZ(&camera->at, &camera->eye);
    radius += (normalizedHeight * 1.5f - radius) * 0.25f;
    Vec3f eye = CastCameraOffset(at, pitch, yaw, radius);
    Vec3f hit;
    CollisionPoly* poly;
    s32 bgId;
    if (BgCheck_EntityLineTest1(&play->colCtx, &at, &eye, &hit, &poly, true, true, true, true, &bgId)) {
        eye = hit;
    }
    Play_SetCameraAtEye(play, sCastCamera, &at, &eye);
    Play_SetCameraFov(play, sCastCamera, camera->fov + (70.0f - camera->fov) * 0.25f);
}

// OoT D_80854A58..A70: av1 1 (Nayru) uses the honoo poses and av1 2 (Din) the tamashii ones.
static const char* const sCastRaiseAnims[] = {
    gPlayerAnim_link_magic_kaze1, gPlayerAnim_link_magic_honoo1, gPlayerAnim_link_magic_tamashii1
};
static const char* const sCastHoldAnims[] = {
    gPlayerAnim_link_magic_kaze2, gPlayerAnim_link_magic_honoo2, gPlayerAnim_link_magic_tamashii2
};
static const char* const sCastLowerAnims[] = {
    gPlayerAnim_link_magic_kaze3, gPlayerAnim_link_magic_honoo3, gPlayerAnim_link_magic_tamashii3
};
static const u8 sCastHoldDurations[] = { 70, 10, 10 };
static OotAnimSfxEntry sCastChargeSfx[] = {
    { NA_SE_PL_SKIP, (OOT_ANIMSFX_GENERAL << 11) | 20 },
    { NA_SE_VO_LI_SWORD_N, (OOT_ANIMSFX_VOICE << 11) | 20 },
    { NA_SE_PL_WALK_GROUND, -((OOT_ANIMSFX_FLOOR << 11) | 26) },
};
static OotAnimSfxEntry sCastVoiceSfx[][2] = {
    { { NA_SE_PL_WALK_GROUND, (OOT_ANIMSFX_FLOOR << 11) | 20 },
      { NA_SE_VO_LI_MAGIC_FROL, -((OOT_ANIMSFX_VOICE << 11) | 30) } },
    { { NA_SE_PL_WALK_GROUND, (OOT_ANIMSFX_FLOOR << 11) | 20 },
      { NA_SE_VO_LI_MAGIC_NALE, -((OOT_ANIMSFX_VOICE << 11) | 44) } },
    { { NA_SE_VO_LI_MAGIC_ATTACK, (OOT_ANIMSFX_VOICE << 11) | 20 },
      { NA_SE_IT_SWORD_SWING_HARD, -((OOT_ANIMSFX_GENERAL << 11) | 20) } },
};

static Actor* SpawnSpell(PlayState* play, Player* player, OotCast spell) {
    static s16* const sSpellActorIds[] = { &sWindId, &sNayruId, &sFireId };
    Vec3f* pos = &player->actor.world.pos;
    return Actor_Spawn(&play->actorCtx, play, *sSpellActorIds[spell - CAST_FARORE], pos->x, pos->y, pos->z, 0, 0, 0,
                       0);
}

static void PlayCastAnim(PlayState* play, Player* player, const char* anim, bool loop) {
    if (loop) {
        PlayerAnimation_PlayLoopSetSpeed(play, &player->skelAnime, (PlayerAnimationHeader*)anim, 0.83f);
    } else {
        PlayerAnimation_PlayOnceSetSpeed(play, &player->skelAnime, (PlayerAnimationHeader*)anim, 0.83f);
    }
}

static void OnCastAnimFinished(PlayState* play, Player* player, OotCast spell, s32 index) {
    switch (sCastPhase) {
        case CAST_PHASE_CHARGE:
            PlayCastAnim(play, player, sCastRaiseAnims[index], false);
            if (SpawnSpell(play, player, spell) != NULL) {
                SetCastCutsceneFlags(player, true);
                gSaveContext.magicState = MAGIC_STATE_CONSUME_SETUP;
            } else {
                Magic_Reset(play);
            }
            sCastPhase = CAST_PHASE_RAISE;
            break;
        case CAST_PHASE_RAISE:
            PlayCastAnim(play, player, sCastHoldAnims[index], true);
            sCastHold = (spell == CAST_FARORE) ? -9 : 2;
            sCastPhase = CAST_PHASE_HOLD;
            break;
        case CAST_PHASE_LOWER: {
            bool canEnd = spell == CAST_NAYRU || gSaveContext.magicState == MAGIC_STATE_IDLE;
            if (!canEnd && gSaveContext.magicState == MAGIC_STATE_METER_FLASH_2) {
                Magic_Reset(play);
                canEnd = true;
            }
            if (canEnd) {
                FinishOotCast(play, player);
            }
            break;
        }
        default:
            break;
    }
}

static void OnCastAnimPlaying(Player* player, OotCast spell, s32 index) {
    if (sCastHold < 0) {
        if (++sCastHold == 0) {
            SetFaroresPoint(gPlayState, player);
            sCastHold = 2;
        }
    } else if (sCastPhase == CAST_PHASE_CHARGE) {
        Player_PlayAnimSfx(player, sCastChargeSfx);
    } else if (sCastPhase == CAST_PHASE_RAISE) {
        Player_PlayAnimSfx(player, sCastVoiceSfx[index]);
        // Enemies stay frozen while IN_CUTSCENE is set, so Din's dome would never reach them.
        if (spell == CAST_DIN && PlayerAnimation_OnFrame(&player->skelAnime, 30.0f)) {
            SetCastCutsceneFlags(player, false);
        }
    } else if (sCastPhase == CAST_PHASE_HOLD && sCastHoldDurations[index] < sCastHold++) {
        PlayCastAnim(gPlayState, player, sCastLowerAnims[index], false);
        player->yaw = player->actor.shape.rot.y;
        sCastPhase = CAST_PHASE_LOWER;
    }
}

static void OotCastAction(Player* player, PlayState* play) {
    OotCast spell = sActiveCast;
    if (spell == CAST_NONE) {
        return;
    }
    s32 index = spell - CAST_FARORE;
    sCastAge++;
    UpdateOotCastCamera(play, player);
    if (PlayerAnimation_Update(play, &player->skelAnime)) {
        OnCastAnimFinished(play, player, spell, index);
    } else {
        OnCastAnimPlaying(player, spell, index);
    }
    Player_DecelerateToZero(player);
}

static void StartOotCast(PlayState* play, Player* player) {
    OotCast spell = sPendingCast;
    sPendingCast = CAST_NONE;
    if (!Magic_Consume(play, sCastCosts[spell], MAGIC_CONSUME_WAIT_PREVIEW)) {
        return;
    }
    sActiveCast = spell;
    sCastPhase = CAST_PHASE_CHARGE;
    sCastHold = 0;
    sCastAge = 0;
    Player_SetAction_PreserveItemAction(play, player, OotCastAction, 0);
    SetCastCutsceneFlags(player, true);
    PlayCastAnim(play, player, gPlayerAnim_link_magic_tame, false);
    Player_StopHorizontalMovement(player);

    sCastCamera = Play_CreateSubCamera(play);
    if (sCastCamera == CAM_ID_NONE) {
        return;
    }
    Play_CopyCamera(play, sCastCamera, CAM_ID_MAIN);
    Camera* camera = Play_GetCamera(play, sCastCamera);
    Camera_ChangeSetting(camera, CAM_SET_FREE0);
    sCastStartYaw = Math_Vec3f_Yaw(&camera->at, &camera->eye);
    sCastStartPitch = -Math_Vec3f_Pitch(&camera->at, &camera->eye);
    s16 rearYaw = player->actor.shape.rot.y + 0x8000;
    sCastTurnYaw = rearYaw + ((s16)(rearYaw - sCastStartYaw) > 0 ? 1 : -1) * CastCameraAngle(175.0f);
    if (spell == CAST_DIN) {
        u32 frame = play->state.frames;
        sDinShortCamera = sPlayedDinCamera && frame - sLastDinCameraFrame <= 3600;
        sDinMirrorCamera = sDinShortCamera && (frame & 1);
        sLastDinCameraFrame = frame;
        sPlayedDinCamera = true;
    }
    UpdateOotCastCamera(play, player);
    Play_ChangeCameraStatus(play, CAM_ID_MAIN, CAM_STATUS_WAIT);
    Play_ChangeCameraStatus(play, sCastCamera, CAM_STATUS_ACTIVE);
}

static u32 SlingButton(Player* player) {
    return player->heldItemButton < S2H_ITEM_BUTTON_COUNT ? sButtons[player->heldItemButton] : 0;
}
static void PutSling(Player* player, PlayState* play);

static bool HoldSlingAimForWheel(Player* player, PlayState* play) {
    bool (*wheelCapturesInput)(void)=(bool(*)(void))sApi->FindService("nei.arrow_wheel.captures_input");
    if(!wheelCapturesInput || !wheelCapturesInput())return false;
    // Keep the camera/weapon state alive without using the wheel's stick input
    // as aiming input. Native Player_UpdateCommon clears the first-person bit.
    Z64Items_KeepHeld(sApi);
    player->actor.focus.rot.x=sZ64AimingPitch;
    player->actor.focus.rot.y=sZ64AimingYaw;
    Z64Aiming_HoldState(player);
    Player_StopHorizontalMovement(player);
    Z64AimingImpl_HoldCamera(play);
    return true;
}
static void SyncHeldSlingAim(Player* player) {
    // The elemental cone reads the held seed's basis. Unlike OoT's native
    // slingshot, this custom action must keep that basis in sync while aiming.
    if(sSlingSeed!=NULL && sSlingSeed->parent==&player->actor){
        s16 yaw,pitch;
        GetWeaponAim(player,&yaw,&pitch);
        sSlingSeed->world.rot.x=sSlingSeed->shape.rot.x=pitch;
        sSlingSeed->world.rot.y=sSlingSeed->shape.rot.y=yaw;
        sSlingSeed->world.rot.z=sSlingSeed->shape.rot.z=0;
    }
}
static void StowSling(Player* player, PlayState* play) {
    PutSling(player, play);
    player->heldItemId = ITEM_NONE;
    player->nextModelGroup = Player_ActionToModelGroup(player, PLAYER_IA_NONE);
    Player_InitItemActionWithAnim(play, player, PLAYER_IA_NONE);
}

static void FaceSlingAim(Player* player) {
    s16 yaw;
    s16 pitch;

    GetWeaponAim(player, &yaw, &pitch);
    FaceWeaponAim(player, yaw, pitch);
}

static void UpdateSlingShot(Player* player, PlayState* play);

static void AimSling(Player* player, PlayState* play) {
    if(HoldSlingAimForWheel(player,play))return;
    Input* input = &play->state.input[0];
    if (!sSlingOut || (input->press.button & AimCancelButtons(true))) {
        StowSling(player, play);
        return;
    }
    // OoT leaves first person with A but keeps the slingshot in hand.
    if (input->press.button & BTN_A) {
        PutSling(player, play);
        return;
    }
    Z64Items_KeepHeld(sApi);
    UpdateWeaponAim(player, play);
    Player_UpdateUpperBody(player, play);
    if (player->actionFunc == AimSling) {
        player->unk_AA5 = PLAYER_UNKAA5_3;
        FaceSlingAim(player);
    }
    UpdateSlingShot(player, play);
}

// Runs from the held-item upper body, so Link keeps his lock-on side walk; releasing Z enters first person.
static void UpdateTargetedSling(Player* player, PlayState* play) {
    if (play->state.input[0].press.button & AimCancelButtons(true)) {
        StowSling(player, play);
        return;
    }
    if (!UseTargetCamera(player, play)) {
        if (!(play->state.input[0].cur.button & SlingButton(player))) {
            PutSling(player, play);
            return;
        }
        sSlingTargeted = false;
        Player_SetAction_PreserveItemAction(play, player, AimSling, 0);
        UpdateWeaponAim(player, play);
        return;
    }
    Z64Items_KeepHeld(sApi);
    FaceSlingAim(player);
    UpdateSlingShot(player, play);
}

static void UpdateSlingShot(Player* player, PlayState* play) {
    Input* input = &play->state.input[0];
    if (sSlingCooldown > 0) --sSlingCooldown;
    const u32 button = SlingButton(player);
    if (input->press.button & button) sSlingPull = true;
    if (sSlingCooldown == 0 && sSlingPull && sSlingSeed == NULL) {
        sSlingPull = false;
        if (sSave.seeds <= 0) { Audio_PlaySfx(NA_SE_IT_SLING_FLICK); return; }
        sSlingSeed = Actor_SpawnAsChild(&play->actorCtx, &player->actor, play, ACTOR_EN_ARROW,
                                      player->actor.focus.pos.x, player->actor.focus.pos.y,
                                      player->actor.focus.pos.z, 0, player->actor.shape.rot.y, 0,
                                      ARROW_TYPE_SLINGSHOT);
        if (sSlingSeed != NULL) {
            player->heldActor = sSlingSeed;
            Audio_PlaySfx_AtPos(&player->actor.projectedPos, NA_SE_IT_SLING_DRAW);
            sSlingLoading = true;
            PlayerAnimation_PlayOnce(play, &player->skelAnimeUpper,
                                     (PlayerAnimationHeader*)gPlayerAnim_link_bow_bow_ready);
        }
    }
    SyncHeldSlingAim(player);
    if (sSlingSeed != NULL && !(input->cur.button & button)) {
        s16 yaw, pitch;
        GetWeaponAim(player, &yaw, &pitch);
        Actor* shot = sSlingSeed;
        sSlingSeed = NULL;
        if (player->heldActor == shot) player->heldActor = NULL;
        if (player->actor.child == shot) player->actor.child = NULL;
        shot->parent = NULL;
        shot->world.rot.x = shot->shape.rot.x = pitch;
        shot->world.rot.y = shot->shape.rot.y = yaw;
        u8 previous = player->unk_D57;
        player->unk_D57 = 1;
        func_8088A594((EnArrow*)shot, play);
        player->unk_D57 = previous;
        if (sBulletBagLevel < 4) {
            --sSave.seeds;
            Store();
        }
        sSlingLoading = true;
        PlayerAnimation_PlayOnce(play, &player->skelAnimeUpper,
                                 (PlayerAnimationHeader*)gPlayerAnim_link_bow_bow_shoot_next);
        sSlingCooldown = 12;
    }
}
static void TakeSling(PlayState* play, Player* player) {
    sSlingAimPending = true;
}

static void StartSlingAim(Player* player, PlayState* play, bool isPulling) {
    sSlingAimPending = false;
    sSlingOut = true;
    sSlingLoading = true;
    sSlingPull = isPulling;
    sSlingCooldown = 8;
    sSlingTargeted = UseTargetCamera(player, play);
    if (!sSlingTargeted) {
        Player_SetAction_PreserveItemAction(play, player, AimSling, 0);
        UpdateWeaponAim(player, play);
    }
    PlayerAnimation_PlayOnce(play, &player->skelAnimeUpper,
                             (PlayerAnimationHeader*)gPlayerAnim_link_bow_bow_ready);
}
static void PutSling(Player* player, PlayState* play) {
    bool wasOut = sSlingOut;
    sSlingAimPending = false;
    sSlingOut = false;
    sSlingTargeted = false;
    sSlingLoading = false;
    sSlingPull = false;
    if (sSlingSeed != NULL) {
        if (player->heldActor == sSlingSeed) player->heldActor = NULL;
        if (player->actor.child == sSlingSeed) player->actor.child = NULL;
        Actor_Kill(sSlingSeed);
        sSlingSeed = NULL;
    }
    sSlingCooldown = 0;
    Z64Aiming_Release(player, play);
    if (wasOut && player->unk_AA5 == PLAYER_UNKAA5_3) player->unk_AA5 = PLAYER_UNKAA5_0;
    if (player->actionFunc == AimSling) func_8083B1A0(player, play);
}
static void DrawOotReticle(void) {
    PlayState* play = gPlayState;

    if (play == NULL || !Z64Aiming_IsAiming() || play->pauseCtx.state != PAUSE_STATE_OFF) {
        return;
    }
    if (sBoomPhase == BOOM_AIM) {
        Z64Aiming_DrawReticle(play, GET_PLAYER(play), 432.0f,
                             (Z64AimingReticleStyle){ Z64AIMING_TOGGLE_BOOMERANG, Z64AIMING_TINT_LAUNCH });
    } else if (sSlingOut) {
        Z64Aiming_DrawReticle(play, GET_PLAYER(play), SLINGSHOT_RETICLE_RANGE,
                             (Z64AimingReticleStyle){ Z64AIMING_TOGGLE_BOW, Z64AIMING_TINT_LAUNCH });
    }
}

static void InitFaroreMarker(PlayState* play) {
    sFaroreMarkerData = sSave.valid ? 40 : 0;
    sFaroreMarkerPos = sSave.valid ? sSave.pos : (Vec3f){ 0.0f, 0.0f, 0.0f };
    sFaroreFlyTimer = 0;
    sFaroreFlyProgress = 0.0f;
    Lights_PointNoGlowSetInfo(&sFaroreLightInfo, sFaroreMarkerPos.x, sFaroreMarkerPos.y + 60.0f, sFaroreMarkerPos.z,
                              255, 255, 255, -1);
    sFaroreLightNode = LightContext_InsertLight(play, &play->lightCtx, &sFaroreLightInfo);
}

// OoT Actor_DrawFaroresWindPointer leg that carries the marker from the cast spot to the stored point.
static void FlyFaroreMarker(PlayState* play, f32* yOffset) {
    static Vec3f sSparkleVelocity = { 0.0f, -0.05f, 0.0f };
    static Vec3f sSparkleAccel = { 0.0f, -0.025f, 0.0f };
    static Color_RGBA8 sSparklePrim = { 255, 255, 255, 0 };
    static Color_RGBA8 sSparkleEnv = { 100, 200, 0, 0 };
    Vec3f target = sSave.pos;
    Vec3f diff;
    f32 previousProgress = sFaroreFlyProgress;
    f32 distance = Math_Vec3f_DistXYZAndStoreDiff(&target, &sFaroreMarkerPos, &diff);
    if (distance < 20.0f) {
        sFaroreFlyProgress = 0.0f;
        sFaroreMarkerPos = target;
    } else {
        f32 length = distance * (1.0f / sFaroreFlyProgress);
        f32 speed = MAX(20.0f / length, 0.05f);
        Math_StepToF(&sFaroreFlyProgress, 0.0f, speed);
        f32 factor = (distance * (sFaroreFlyProgress / previousProgress)) / distance;
        sFaroreMarkerPos.x = target.x + diff.x * factor;
        sFaroreMarkerPos.y = target.y + diff.y * factor;
        sFaroreMarkerPos.z = target.z + diff.z * factor;
        length *= 0.5f;
        f32 dx = distance - length;
        *yOffset += sqrtf(SQ(length) - SQ(dx)) * 0.2f;
    }
    Vec3f sparklePos = { sFaroreMarkerPos.x + Rand_CenteredFloat(6.0f),
                         sFaroreMarkerPos.y + 80.0f + (6.0f * Rand_ZeroOne()),
                         sFaroreMarkerPos.z + Rand_CenteredFloat(6.0f) };
    EffectSsKirakira_SpawnDispersed(play, &sparklePos, &sSparkleVelocity, &sSparkleAccel, &sSparklePrim, &sSparkleEnv,
                                    1000, 16);
    if (sFaroreFlyProgress == 0.0f) {
        sFaroreMarkerData = 40;
    }
}

// OoT arrival leg: the marker is pulled into the camera and the point is consumed when it fades out.
static f32 FadeFaroreMarker(PlayState* play, s32 fadeStep, f32 yOffset, s32* alpha) {
    f32 nextRatio = 1.0f - fadeStep * 0.1f;
    f32 currentRatio = 1.0f - (f32)(fadeStep - 1) * 0.1f;
    if (nextRatio > 0.0f) {
        Vec3f eye = { play->view.eye.x, play->view.eye.y - yOffset, play->view.eye.z };
        Vec3f diff;
        f32 distance = Math_Vec3f_DistXYZAndStoreDiff(&eye, &sFaroreMarkerPos, &diff);
        f32 factor = (distance * (nextRatio / currentRatio)) / distance;
        sFaroreMarkerPos.x = eye.x + diff.x * factor;
        sFaroreMarkerPos.y = eye.y + diff.y * factor;
        sFaroreMarkerPos.z = eye.z + diff.z * factor;
    }
    *alpha = 255 - (fadeStep * 30);
    if (*alpha < 0) {
        sSave.valid = false;
        Store();
        sFaroreMarkerData = 0;
        *alpha = 0;
    } else {
        sFaroreMarkerData++;
    }
    return 1.0f + ((f32)fadeStep * 0.2f);
}

static void DrawFaroreMarkerFlash(PlayState* play, f32 yOffset, f32 ratio, s32 alpha) {
    f32 scale = 0.025f * ratio;
    OPEN_DISPS(play->state.gfxCtx);
    POLY_XLU_DISP = Gfx_SetupDL(POLY_XLU_DISP, SETUPDL_25);
    Matrix_Translate(sFaroreMarkerPos.x, sFaroreMarkerPos.y + yOffset, sFaroreMarkerPos.z, MTXMODE_NEW);
    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    Matrix_Mult(&play->billboardMtxF, MTXMODE_APPLY);
    Matrix_Push();
    gDPPipeSync(POLY_XLU_DISP++);
    gDPSetPrimColor(POLY_XLU_DISP++, 128, 128, 255, 255, 200, alpha);
    gDPSetEnvColor(POLY_XLU_DISP++, 100, 200, 0, 255);
    Matrix_RotateZS((s16)(play->gameplayFrames * 1500), MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gEffFlash1DL);
    Matrix_Pop();
    Matrix_RotateZS((s16)~(play->gameplayFrames * 1200), MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gEffFlash1DL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawFaroreMarker(PlayState* play) {
    s32 data = sFaroreMarkerData;
    if (data == 0) {
        return;
    }
    f32 yOffset = 60.0f;
    f32 ratio = 1.0f;
    s32 alpha = 255;
    s32 fadeStep = data - 40;
    if (fadeStep < 0) {
        sFaroreMarkerData = ++data;
        ratio = ABS(data) * 0.025f;
        sFaroreFlyTimer = 60;
        sFaroreFlyProgress = 1.0f;
    } else if (sFaroreFlyTimer != 0) {
        sFaroreFlyTimer--;
    } else if (sFaroreFlyProgress > 0.0f) {
        FlyFaroreMarker(play, &yOffset);
    } else if (fadeStep > 0) {
        ratio = FadeFaroreMarker(play, fadeStep, yOffset, &alpha);
    }
    bool isMarkerRoom = sSave.scene == play->sceneId && sSave.room == play->roomCtx.curRoom.num;
    if (play->csCtx.state == CS_STATE_IDLE && isMarkerRoom) {
        DrawFaroreMarkerFlash(play, yOffset, ratio, alpha);
    }
    if (sFaroreLightNode != NULL) {
        Lights_PointNoGlowSetInfo(&sFaroreLightInfo, sFaroreMarkerPos.x, sFaroreMarkerPos.y + yOffset,
                                  sFaroreMarkerPos.z, 255, 255, 255, 500.0f * ratio);
    }
}

static void DrawOotSpellEffects(void) {
    PlayState* play = gPlayState;
    if (play == NULL || play->pauseCtx.state != PAUSE_STATE_OFF) {
        return;
    }
    DrawFaroreMarker(play);
}
static void DrawSlingshotString(Player* player, s32 limb) {
    PlayState* play = gPlayState;
    if (play == NULL || player != GET_PLAYER(play) || limb != PLAYER_LIMB_RIGHT_HAND ||
        player->actionFunc != AimSling || sSlingSeed == NULL || !sApi->HasResource(sSlingStringDL)) return;
    Vec3f projectedFocus;
    SkinMatrix_Vec3fMtxFMultXYZ(&play->viewProjectionMtxF, &player->actor.focus.pos, &projectedFocus);
    if ((player->stateFlags1 & PLAYER_STATE1_100000) && projectedFocus.z < -4.0f &&
        sApi->HasResource(sSlingFirstPersonDL)) return;

    Vec3f anchor = { 606.0f, 236.0f, 0.0f };
    Vec3f worldAnchor;
    Matrix_MultVec3f(&anchor, &worldAnchor);
    f32 distance = Math_Vec3f_DistXYZ(&worldAnchor, &player->bodyPartsPos[PLAYER_BODYPART_LEFT_HAND]);
    f32 tension = CLAMP((distance - 3.0f) * 1.6f, 0.0f, 1.0f);
    if (tension == 0.0f) return;
    GraphicsContext* gfx = play->state.gfxCtx;
    OPEN_DISPS(gfx);
    Matrix_Push();
    Matrix_Translate(anchor.x, anchor.y, anchor.z, MTXMODE_APPLY);
    Matrix_Scale(1.0f, tension, 1.0f, MTXMODE_APPLY);
    Matrix_RotateZF(-0.2f * tension, MTXMODE_APPLY);
    Gfx_SetupDL25_Xlu(gfx);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, gfx);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sSlingStringDL);
    Matrix_Pop();
    CLOSE_DISPS(gfx);
}
static s32 HoldItem(Player* player, PlayState* play) { return 0; }
// OoT raises the slingshot in hand while Z-targeting, so Link side walks with it; a press aims it.
static void RaiseHeldSling(Player* player, PlayState* play) {
    if (sSlingAimPending || (player->stateFlags3 & PLAYER_STATE3_START_CHANGING_HELD_ITEM)) {
        return;
    }
    const bool isPressed = (play->state.input[0].press.button & SlingButton(player)) != 0;
    if (isPressed || UseTargetCamera(player, play)) {
        StartSlingAim(player, play, isPressed);
    }
}

static s32 HoldSlingshot(Player* player, PlayState* play) {
    if (!sSlingOut) {
        RaiseHeldSling(player, play);
    }
    if (sSlingOut && sSlingTargeted) UpdateTargetedSling(player, play);
    if (!sSlingOut) return 0;
    if (PlayerAnimation_Update(play, &player->skelAnimeUpper) && sSlingLoading) {
        sSlingLoading = false;
        PlayerAnimation_PlayLoop(play, &player->skelAnimeUpper,
                                 (PlayerAnimationHeader*)gPlayerAnim_link_bow_bow_wait);
    }
    return 1;
}

static void DrawSlingshotHand(Player* player, s32 limb, Gfx** displayList, Gfx* original, Vec3f* position) {
    PlayState* play = gPlayState;
    if (play == NULL || player != GET_PLAYER(play) || player->heldItemAction != PLAYER_IA_CUSTOM ||
        !HasSlingshotKey(sApi->GetEquippedCustomItem(player->heldItemButton))) return;

    // The bow model group would draw the Hero's Bow in the right hand whenever the slingshot is not aimed.
    const bool isAiming = sSlingOut && (player->actionFunc == AimSling || sSlingTargeted);
    Vec3f projectedFocus;
    SkinMatrix_Vec3fMtxFMultXYZ(&play->viewProjectionMtxF, &player->actor.focus.pos, &projectedFocus);
    // Match MM's first-person limb pass, using the child display lists OoT chooses there.
    const bool firstPerson = isAiming && (player->stateFlags1 & PLAYER_STATE1_100000) && projectedFocus.z < -4.0f &&
                             sApi->HasResource(sSlingFirstPersonDL) && sApi->HasResource(sSlingShoulderDL);
    if (limb == PLAYER_LIMB_RIGHT_HAND) {
        const char* model = firstPerson && sSlingSeed != NULL ? sSlingFirstPersonDL : sSlingDL;
        if (sApi->HasResource(model)) *displayList = (Gfx*)model;
    } else if (firstPerson && limb == PLAYER_LIMB_RIGHT_SHOULDER) {
        *displayList = (Gfx*)sSlingShoulderDL;
        // The shoulder position is in its parent's scaled space. Lift the
        // entire arm in world Y without changing its aim rotation.
        MtxF* parent = Matrix_GetCurrent();
        f32 scaleSq = SQ(parent->yx) + SQ(parent->yy) + SQ(parent->yz);
        if (scaleSq > 0.0f) {
            f32 lift = SLING_FIRST_PERSON_LIFT / scaleSq;
            position->x += parent->yx * lift;
            position->y += parent->yy * lift;
            position->z += parent->yz * lift;
        }
    } else if (firstPerson && (limb == PLAYER_LIMB_LEFT_FOREARM || limb == PLAYER_LIMB_LEFT_HAND ||
                              limb == PLAYER_LIMB_RIGHT_FOREARM)) {
        *displayList = NULL;
    }
}

static s32 SeedCount(const char* key) { return sSave.seeds; }
static void RefillSeeds(const char* key) { sSave.seeds = SeedCapacity(); Store(); }

static void AddSeeds(const char* key) {
    if (!sApi->IsCustomItemOwned(SLING)) return;
    s16 amount = strcmp(key, "oot.deku_seeds_5") == 0 ? 5 : 30;
    sSave.seeds = MIN(sSave.seeds + amount, SeedCapacity());
    Store();
}

static void SetBulletBag(u8 level) {
    if (RandoInfCustom_IsLogicSimulation() || level == 0) return;
    sBulletBagLevel = MAX(sBulletBagLevel, level);
    sApi->SetCustomItemOwned(SLING, true);
    sApi->StorageSet(MOD, "bullet_bag_level", &sBulletBagLevel, sizeof(sBulletBagLevel));
    RefillSeeds(SLING);
}

static u16 ReadSlingLevel(const SaveContext* save) {
    if (RandoInfCustom_IsLogicSimulation()) return 0;
    return sBulletBagLevel == 4 ? 5 : sBulletBagLevel;
}

static u16 ReadCondensedSlingLevel(const SaveContext* save) {
    if (RandoInfCustom_IsLogicSimulation()) return 0;
    return sBulletBagLevel == 4 ? 2 : (sBulletBagLevel > 0 ? 1 : 0);
}

static void ApplySlingLevel(SaveContext* save, u16 level) {
    SetBulletBag(level == 5 ? 4 : MIN(level, 3));
}

static void ApplyCondensedSlingLevel(SaveContext* save, u16 level) {
    SetBulletBag(level >= 2 ? 4 : level);
}

static void ResolveSlingshotPickup(Actor* actor, PlayState* play, S2HGetItemEntry* entry, const char** key) {
    if (key==NULL || *key == NULL || !S2H_MOD_API_HAS(sApi, GetNextProgressiveItem)) return;
    if (strcmp(*key, SLING) != 0 && strcmp(*key, SLING_PROGRESSIVE) != 0 && strcmp(*key, SLING_CONDENSED) != 0) return;
    bool held=actor && (actor->parent || (actor->id==ACTOR_EN_ITEM00 && actor->params==ITEM00_NOTHING &&
                                          ((EnItem00*)actor)->unk152>=0));
    // Ownership changes while this same actor is still above Link. Freeze the
    // awarded stage from the moment he accepts it, until that actor is destroyed.
    const char* next=held && actor==sSlingPickup ? sSlingPickupStage :
                     sApi->GetNextProgressiveItem(*key,S2H_RANDO_INF_SAVE);
    if (next == NULL) return;
    const S2HCustomItemDefinition* stage = sApi->FindCustomItem(next);
    if (stage == NULL) return;
    if(held){sSlingPickup=actor;sSlingPickupStage=next;}
    *key = next;
    if (entry != NULL) *entry = stage->getItemEntry;
}

static void StartEquipFlight(PlayState* play, u8 button, u16 item, bool* handled) {
    static const char* const keys[] = { SLING, BOOM, HAMMER, DIN, FARORE, NAYRU };
    PauseContext* pause = &play->pauseCtx;
    if (*handled || item != ITEM_CUSTOM || pause->pageIndex != PAUSE_ITEM ||
        button < S2H_ITEM_BUTTON_C_FIRST || button >= S2H_ITEM_BUTTON_C_FIRST + 3) return;
    s16 slot = pause->cursorSlot[PAUSE_ITEM];
    if (slot < 0 || slot >= ITEM_NUM_SLOTS || pause->itemVtx == NULL) return;
    const char* icon = KaleidoItemManager_GetSlotIcon(slot);
    for (u8 i = 0; icon != NULL && i < ARRAY_COUNT(keys); i++) {
        const S2HCustomItemDefinition* definition = sApi->FindCustomItem(keys[i]);
        if (definition == NULL || strcmp(icon, definition->iconPath) != 0) continue;
        sEquipFlight.key = keys[i];
        sEquipFlight.icon = definition->iconPath;
        sEquipFlight.startFrame = play->state.frames;
        sEquipFlight.x = SCREEN_WIDTH / 2 + pause->itemVtx[slot * 4].v.ob[0];
        sEquipFlight.y = SCREEN_HEIGHT / 2 - pause->itemVtx[slot * 4].v.ob[1];
        sEquipFlight.button = button;
        break;
    }
}

static bool HasSlingshotWheel(void) {
    if (!sApi->IsCustomItemOwned(SLING) ||
        gSaveContext.save.saveInfo.inventory.items[SLOT_POWDER_KEG] != ITEM_POWDER_KEG) return false;
    if (CVarGetInteger(SLING_SHOW_KEG, 0)) return true;
    S2HCustomItemPlacement placement;
    return KaleidoItemManager_GetPlacement(SLING, &placement) &&
           placement.page == S2H_ITEM_PAGE_VANILLA && placement.slot == SLOT_POWDER_KEG;
}

static void CycleSlingshotCell(PlayState* play, Input* input, bool* handled) {
    PauseContext* pause = &play->pauseCtx;
    const bool showingKeg = CVarGetInteger(SLING_SHOW_KEG, 0) != 0;
    const bool hasWheel = HasSlingshotWheel();
    if (showingKeg && !hasWheel) {
        CVarSetInteger(SLING_SHOW_KEG, 0);
        KaleidoItemManager_SetPauseVisible(SLING, true);
    }
    const bool focused = hasWheel && pause->pageIndex == PAUSE_ITEM && pause->cursorSpecialPos == 0 &&
                         !pause->itemDescriptionOn && pause->cursorPoint[PAUSE_ITEM] == SLOT_POWDER_KEG &&
                         KaleidoItemManager_IsVanillaPageShown();
    if (focused) {
        if (!sWheelPrompt) sPreviousAAction = play->interfaceCtx.aButtonDoActionDelayed;
        sWheelPrompt = true;
        Interface_SetAButtonDoAction(play, DO_ACTION_DECIDE);
    } else if (sWheelPrompt) {
        sWheelPrompt = false;
        if (play->interfaceCtx.aButtonDoActionDelayed == DO_ACTION_DECIDE) {
            Interface_SetAButtonDoAction(play, sPreviousAAction);
        }
    }
    if (*handled || !focused || !(input->press.button & BTN_A)) return;

    CVarSetInteger(SLING_SHOW_KEG, !showingKeg);
    KaleidoItemManager_SetPauseVisible(SLING, showingKeg);
    Audio_PlaySfx(NA_SE_SY_CURSOR);
    input->press.button &= ~BTN_A;
    *handled = true;
}

static void DrawEquipFlight(PlayState* play) {
    static const s16 targetX[] = { 226, 250, 274 };
    static const s16 targetY[] = { 10, 28, 10 };
    if (sEquipFlight.key == NULL) return;
    u32 elapsed = play->state.frames - sEquipFlight.startFrame;
    const char* equipped = sApi->GetEquippedCustomItem(sEquipFlight.button);
    if (elapsed >= 10 || play->pauseCtx.state != PAUSE_STATE_MAIN ||
        equipped == NULL || strcmp(equipped, sEquipFlight.key) != 0) {
        sEquipFlight.key = NULL;
        return;
    }
    u8 index = sEquipFlight.button - S2H_ITEM_BUTTON_C_FIRST;
    s16 x = sEquipFlight.x + (targetX[index] - sEquipFlight.x) * (s32)elapsed / 10;
    s16 y = sEquipFlight.y + (targetY[index] - sEquipFlight.y) * (s32)elapsed / 10;
    GraphicsContext* gfx = play->state.gfxCtx;
    OPEN_DISPS(gfx);
    Gfx_SetupDL42_Overlay(gfx);
    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 255, 255, 255);
    gDPLoadTextureBlock(OVERLAY_DISP++, sEquipFlight.icon, G_IM_FMT_RGBA, G_IM_SIZ_32b, 32, 32, 0,
                        G_TX_CLAMP, G_TX_CLAMP, 0, 0, G_TX_NOLOD, G_TX_NOLOD);
    gSPTextureRectangle(OVERLAY_DISP++, x * 4, y * 4, (x + 32) * 4, (y + 32) * 4,
                        G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
    CLOSE_DISPS(gfx);
}

static void DrawSeedHud(PlayState* play) {
    static const s16 x[] = { 228, 250, 272 };
    static const s16 y[] = { 35, 51, 35 };
    const s16 alpha[] = { play->interfaceCtx.cLeftAlpha, play->interfaceCtx.cDownAlpha, play->interfaceCtx.cRightAlpha };
    GraphicsContext* gfx = play->state.gfxCtx;
    OPEN_DISPS(gfx);
    for (u8 i = 0; i < ARRAY_COUNT(x); i++) {
        if (alpha[i] == 0 || !HasSlingshotKey(sApi->GetEquippedCustomItem(i + S2H_ITEM_BUTTON_C_FIRST))) continue;
        Gfx_SetupDL39_Overlay(gfx);
        gDPSetEnvColor(OVERLAY_DISP++, 0, 0, 0, 0);
        bool full = sBulletBagLevel == 4 || sSave.seeds == SeedCapacity();
        u8 tone = sSave.seeds ? 255 : 100;
        gDPSetPrimColor(OVERLAY_DISP++, 0, 0, full ? 120 : tone, full ? 255 : tone,
                       full ? 0 : tone, alpha[i]);
        if (sBulletBagLevel == 4) {
            gDPLoadTextureBlock_4b(OVERLAY_DISP++, sInfinityTex, G_IM_FMT_I, 16, 16, 0,
                                   G_TX_CLAMP, G_TX_CLAMP, 0, 0, G_TX_NOLOD, G_TX_NOLOD);
            gSPTextureRectangle(OVERLAY_DISP++, x[i] * 4, (y[i] - 1) * 4,
                                (x[i] + 14) * 4, (y[i] + 8) * 4, G_TX_RENDERTILE, 0, 0, 1170, 1820);
            continue;
        }
        if (sSave.seeds >= 10) {
            OVERLAY_DISP = Gfx_DrawTexRectIA8(OVERLAY_DISP, (TexturePtr)sAmmoDigits[sSave.seeds / 10],
                                              8, 8, x[i], y[i], 8, 8, 1 << 10, 1 << 10);
        }
        OVERLAY_DISP = Gfx_DrawTexRectIA8(OVERLAY_DISP, (TexturePtr)sAmmoDigits[sSave.seeds % 10],
                                          8, 8, x[i] + 6, y[i], 8, 8, 1 << 10, 1 << 10);
    }
    CLOSE_DISPS(gfx);
    DrawEquipFlight(play);
}

static void DrawSlingshotWheelPreview(PlayState* play, s32 slot, Vtx* cell) {
    if (slot != SLOT_POWDER_KEG || play->pauseCtx.pageIndex != PAUSE_ITEM ||
        !KaleidoItemManager_IsVanillaPageShown() || !HasSlingshotWheel()) return;
    const char* icon = CVarGetInteger(SLING_SHOW_KEG, 0) ? sSlingIcon : sKegIcon;
    GraphicsContext* gfx = play->state.gfxCtx;
    const s16 left = cell[0].v.ob[0] - 8;
    const s16 top = cell[0].v.ob[1] - 14;
    Vtx* quad = GRAPH_ALLOC(gfx, 4 * sizeof(Vtx));
    const Vtx corners[] = {
        VTX(left, top, 0, 0, 0, 255, 255, 255, 255),
        VTX(left + 16, top, 0, 32 << 5, 0, 255, 255, 255, 255),
        VTX(left, top - 16, 0, 0, 32 << 5, 255, 255, 255, 255),
        VTX(left + 16, top - 16, 0, 32 << 5, 32 << 5, 255, 255, 255, 255),
    };
    memcpy(quad, corners, sizeof(corners));
    OPEN_DISPS(gfx);
    gSPVertex(POLY_OPA_DISP++, (uintptr_t)quad, 4, 0);
    KaleidoScope_DrawTexQuadRGBA32(gfx, (TexturePtr)icon, 32, 32, 0);
    CLOSE_DISPS(gfx);
}

static void DrawSeedPause(PlayState* play, s32 slot, s32 item, Vtx* cell) {
    DrawSlingshotWheelPreview(play, slot, cell);
    if (item != ITEM_CUSTOM) return;
    const char* icon = KaleidoItemManager_GetSlotIcon(slot);
    if (icon == NULL ||
        strcmp(icon, OOT_ICON("gItemIconSlingshotTex")) != 0) return;
    GraphicsContext* gfx = play->state.gfxCtx;
    const s16 digits[] = { sSave.seeds / 10, sSave.seeds % 10 };
    OPEN_DISPS(gfx);
    gDPPipeSync(POLY_OPA_DISP++);
    bool infinite = sBulletBagLevel == 4;
    bool full = infinite || sSave.seeds == SeedCapacity();
    u8 tone = sSave.seeds ? 255 : 130;
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, full ? 120 : tone, full ? 255 : tone,
                   full ? 0 : tone, play->pauseCtx.alpha);
    for (u8 i = !infinite && digits[0] == 0 ? 1 : 0; i < ARRAY_COUNT(digits); i++) {
        Vtx* quad = GRAPH_ALLOC(gfx, 4 * sizeof(Vtx));
        memcpy(quad, cell, 4 * sizeof(Vtx));
        for (u8 v = 0; v < 4; v++) {
            quad[v].v.ob[0] = cell[0].v.ob[0] + 14 + i * 6 + ((v & 1) ? (infinite ? 14 : 8) : 0);
            quad[v].v.ob[1] = cell[0].v.ob[1] - 22 - ((v & 2) ? 8 : 0);
            quad[v].v.tc[0] = (v & 1) ? (infinite ? 16 : 8) << 5 : 0;
            quad[v].v.tc[1] = (v & 2) ? (infinite ? 16 : 8) << 5 : 0;
        }
        gSPVertex(POLY_OPA_DISP++, (uintptr_t)quad, 4, 0);
        if (infinite) {
            gDPLoadTextureBlock_4b(POLY_OPA_DISP++, sInfinityTex, G_IM_FMT_I, 16, 16, 0,
                                   G_TX_CLAMP, G_TX_CLAMP, 0, 0, G_TX_NOLOD, G_TX_NOLOD);
            gSP1Quadrangle(POLY_OPA_DISP++, 0, 2, 3, 1, 0);
            break;
        }
        POLY_OPA_DISP = Gfx_DrawTexQuadIA8(POLY_OPA_DISP, (TexturePtr)sAmmoDigits[digits[i]], 8, 8, 0);
    }
    CLOSE_DISPS(gfx);
}

static void FaroreArrivalAction(Player* player, PlayState* play) {
    sFaroreArrivalTimer++;
    if (sFaroreArrivalTimer == 20) {
        Vec3f* pos = &player->actor.world.pos;
        Actor_Spawn(&play->actorCtx, play, sWindId, pos->x, pos->y, pos->z, 0, 0, 0, 1);
        sFaroreMarkerData = 41;
    }
    if (sFaroreArrivalTimer > 20) {
        player->actor.draw = sFaroreHiddenDraw;
        player->actor.world.pos.y += 60.0f;
        player->stateFlags1 &= ~PLAYER_STATE1_20000000;
        func_80836988(player, play);
    }
}

static void StartFaroreArrival(PlayState* play) {
    Player* player = GET_PLAYER(play);
    sFaroreReturnPending = false;
    sFaroreArrivalTimer = 0;
    sFaroreHiddenDraw = player->actor.draw;
    player->actor.draw = NULL;
    player->stateFlags1 |= PLAYER_STATE1_20000000;
    Player_SetAction(play, player, FaroreArrivalAction, 0);
}

static void RestoreNayrusLove(PlayState* play) {
    if (sNayruTimer == 0 || sNayrusLove != NULL) {
        return;
    }
    Vec3f* pos = &GET_PLAYER(play)->actor.world.pos;
    gSaveContext.magicState = MAGIC_STATE_METER_FLASH_1;
    Actor_Spawn(&play->actorCtx, play, sNayruId, pos->x, pos->y, pos->z, 0, 0, 0, 0);
}

static bool CanSwingHammer(Player* player) {
    return (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) && !(player->stateFlags1 & PLAYER_STATE1_8000000);
}

// The hammer's Great Fairy's Sword melee type would also start vanilla's 2H slash and combo on the same press.
static void TakeHammerAttack(PlayState* play, Player* player, int32_t action, bool* consumed, bool* startedAction) {
    if (*consumed || !IsHoldingHammer(player)) {
        return;
    }
    *consumed = true;
    *startedAction = action == SOH_PLAYER_ACTION_MELEE && sHammerSwingPending && CanSwingHammer(player);
    if (*startedAction) {
        StartHammerSwing(play, player);
    }
}

static void UpdateHammer(PlayState* play, Player* player) {
    if (!IsHoldingHammer(player)) {
        sHammerSwingPending = false;
        return;
    }
    Z64Items_KeepHeld(sApi);
    UpdateHammerJumpslash(play, player);
    // The melee handler already ran this frame; only a swing in progress keeps the press buffered.
    if (player->actionFunc != HammerAttackAction) {
        sHammerSwingPending = false;
    }
}

static void Tick(Player* player) {
    PlayState* play = gPlayState;
    if (play == NULL || player != GET_PLAYER(play) || play->pauseCtx.state != PAUSE_STATE_OFF) {
        return;
    }
    if (sFarorePromptPending) {
        sFarorePromptPending = false;
        SetCastCutsceneFlags(player, true);
        Player_SetAction_PreserveItemAction(play, player, FarorePromptAction, 1);
        Player_StopHorizontalMovement(player);
    } else if (sFarorePrompt && player->actionFunc != FarorePromptAction) {
        sFarorePrompt = false;
        Message_CloseTextbox(play);
        SetCastCutsceneFlags(player, false);
    }
    if (sFarorePrompt) {
        return;
    }
    if (sActiveCast != CAST_NONE && player->actionFunc != OotCastAction) {
        AbortOotCast(play, player);
    }
    if (sPendingCast != CAST_NONE) {
        StartOotCast(play, player);
    }
    UpdateHammer(play, player);
    const char* held = sApi->GetEquippedCustomItem(player->heldItemButton);
    // The registry finishes the held-item transition after the take-out callback.
    if (sSlingAimPending && player->heldItemAction == PLAYER_IA_CUSTOM && HasSlingshotKey(held) &&
        !(player->stateFlags3 & PLAYER_STATE3_START_CHANGING_HELD_ITEM)) {
        StartSlingAim(player, play, true);
    }
    if (sBoomPhase == BOOM_PENDING && held != NULL && strcmp(held, BOOM) == 0 &&
        player->heldItemAction == PLAYER_IA_CUSTOM &&
        !(player->stateFlags3 & PLAYER_STATE3_START_CHANGING_HELD_ITEM)) {
        RaiseBoom(player, play, true);
    }
    if(sSlingOut && player->actionFunc==AimSling)HoldSlingAimForWheel(player,play);
    if (sBoomPhase == BOOM_AIM) {
        const bool isAiming = player->actionFunc == BoomAimAction || sBoomTargeted;
        if (isAiming && held != NULL && strcmp(held, BOOM) == 0) {
            Z64Items_KeepHeld(sApi);
            if (!sBoomTargeted) {
                UpdateWeaponAim(player, play);
            }
        } else {
            PutBoom(player, play);
        }
    }
}

static void Scene(s16 scene, s8 spawn) {
    sSlingPickup=NULL;
    sSlingPickupStage=NULL;
    sBoom = NULL;
    sBoomPhase = BOOM_STOWED;
    sBoomTargeted = false;
    sHammerSwingPending = false;
    sSlingOut = false;
    sSlingTargeted = false;
    sSlingSeed = NULL;
    sSlingPull = false;
    sSlingAimPending = false;
    sPendingCast = sActiveCast = CAST_NONE;
    sFarorePrompt = false;
    sFarorePromptPending = false;
    sNayrusLove = NULL;
    sCastCamera = CAM_ID_NONE;
    sEquipFlight.key = NULL;
    sWheelPrompt = false;
    Z64Aiming_Drop();
    PlayState* play = gPlayState;
    if (play == NULL) {
        return;
    }
    InitFaroreMarker(play);
    if (sFaroreReturnPending) {
        StartFaroreArrival(play);
    }
    RestoreNayrusLove(play);
}

static void ActorDestroyed(Actor* actor) {
    if(actor==sSlingPickup){sSlingPickup=NULL;sSlingPickupStage=NULL;}
    if (sBoom != NULL && actor != sBoom) {
        OotBoom* boom = (OotBoom*)sBoom;
        if (actor == boom->moveTo) {
            boom->moveTo = NULL;
        }
        if (actor == boom->grabbed) {
            boom->grabbed = NULL;
        }
    }
    if (actor == sSlingSeed) {
        sSlingSeed = NULL;
    }
}

static void DrawSpellGetItem(PlayState* play, S2HGetItemEntry* entry) {
    if (entry->drawItemId >= ARRAY_COUNT(sSpellColors) ||
        !sApi->HasResource(sSpellDiamondDL) || !sApi->HasResource(sSpellOrbDL) ||
        !sApi->HasResource(sSpellColors[entry->drawItemId])) {
        return;
    }
    GraphicsContext* gfx = play->state.gfxCtx;
    s32 frame = play->state.frames;
    OPEN_DISPS(gfx);
    Matrix_Push();
    Gfx_SetupDL25_Xlu(gfx);
    gSPSegment(POLY_XLU_DISP++, 8, Gfx_TwoTexScrollEx(gfx, 0, frame * 2, -frame * 6, 32, 32,
                                                   1, frame, -frame * 2, 32, 32, 2, -6, 1, -2));
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, gfx);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sSpellDiamondDL);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sSpellColors[entry->drawItemId]);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sSpellOrbDL);
    Matrix_Pop();
    CLOSE_DISPS(gfx);
}

static void DrawBulletBag(PlayState* play, S2HGetItemEntry* entry) {
    if (!sApi->HasResource(sBagDL)) return;
    GraphicsContext* gfx = play->state.gfxCtx;
    bool large = entry->drawItemId != 0;
    OPEN_DISPS(gfx);
    Gfx_SetupDL25_Opa(gfx);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, gfx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)(large ? sBagLargeColor : sBagColor));
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sBagDL);
    Gfx_SetupDL25_Xlu(gfx);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, gfx);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)(large ? sBagLargeStringColor : sBagStringColor));
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sBagString);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sBagWriting);
    CLOSE_DISPS(gfx);
}

static void RegisterItem(const char* key, const char* icon, const char* name, const char* pauseText,
                         const char* getItemText, u8 slot, const char* model, s8 spell, S2HCustomItemInitFunc init) {
    S2HCustomItemDefinition item = Z64Items_Define(key, icon, name);
    Z64Items_SetButtons(&item, S2H_CUSTOM_ITEM_C_BUTTON | S2H_CUSTOM_ITEM_DPAD);
    Z64Items_SetForms(&item, S2H_CUSTOM_ITEM_FORM_BIT(PLAYER_FORM_HUMAN));
    Z64Items_SetPlacement(&item, S2H_ITEM_PAGE_VANILLA, slot, 0);
    Z64Items_SetPauseText(&item, pauseText);
    Z64Items_SetTextbox(&item, getItemText);
    Z64Items_SetGetItemModel(&item, model);
    item.getItemScale = 1.0f;
    if (spell >= 0) {
        item.getItemEntry.drawItemId = spell;
        item.getItemEntry.drawFunc = DrawSpellGetItem;
    }
    const S2HCustomItemRandomizer randomizer = {
        sizeof(S2HCustomItemRandomizer), 0, S2H_CUSTOM_ITEM_TYPE_ITEM, 0, strcmp(key, SLING) == 0 ? 0 : 1, key, key, "",
    };
    Z64Items_SetLogic(&item, &randomizer);
    Z64Items_SetCanUse(&item, HasModels);
    Z64Items_SetAction(&item, init, NULL);
    item.flags |= S2H_CUSTOM_ITEM_INSTANT;
    if (strcmp(key, HAMMER) == 0) {
        item.flags &= ~S2H_CUSTOM_ITEM_INSTANT;
        item.update = HoldItem;
        item.meleeWeapon = Z64ITEMS_MELEE_GREAT_FAIRY_SWORD;
        item.modelGroup = PLAYER_MODELGROUP_TWO_HAND_SWORD;
        Z64Items_SetHeldCallbacks(&item, UseHammer, PutHammer, NULL);
    } else if (strcmp(key, SLING) == 0) {
        item.flags &= ~S2H_CUSTOM_ITEM_INSTANT;
        item.update = HoldSlingshot;
        item.putAway = PutSling;
        item.onAcquire = RefillSeeds;
        item.getAmmo = SeedCount;
        item.modelGroup = PLAYER_MODELGROUP_BOW;
        Z64Items_SetFirstPersonModel(&item, sSlingFirstPersonDL);
    } else if (strcmp(key, BOOM) == 0) {
        item.flags &= ~S2H_CUSTOM_ITEM_INSTANT;
        item.update = UpdateBoomHeld;
        Z64Items_SetCanUse(&item, CanUseBoom);
        Z64Items_SetHeldCallbacks(&item, UseBoom, PutBoom, NULL);
    }
    Z64Items_Register(sApi, &item);
}

static void RegisterSlingshotPickup(const char* key, const char* icon, const char* label, const char* message,
                                   const char* model, const S2HCustomItemRandomizer* randomizer,
                                   S2HCustomItemStateFunc onReceive) {
    S2HCustomItemDefinition item = Z64Items_Define(key, icon, NULL);
    item.presentationFlags = S2H_ITEM_HIDE_FROM_KALEIDO | S2H_ITEM_HIDE_FROM_SAVE_EDITOR;
    item.displayName = label;
    item.getItemScale = 1.0f;
    item.onReceive = onReceive;
    Z64Items_SetTextbox(&item, message);
    Z64Items_SetGetItemModel(&item, model);
    if (randomizer != NULL) Z64Items_SetLogic(&item, randomizer);
    if (model == sBagDL) {
        item.getItemEntry.drawItemId = strcmp(icon, OOT_ICON("gItemIconBulletBag50Tex")) == 0;
        item.getItemEntry.drawFunc = DrawBulletBag;
    }
    Z64Items_Register(sApi, &item);
}

static void RegisterSlingshotProgression(void) {
    static const S2HProgressiveLevel regular[] = {
        { SLING }, { "oot.bullet_bag_40" }, { "oot.bullet_bag_50" }, { "oot.bullet_bag_50_extra" },
        { "oot.bullet_bag_infinite" },
    };
    static const S2HProgressiveLevel condensed[] = {
        { "oot.slingshot.condensed_first" }, { "oot.bullet_bag_infinite_condensed" },
    };
    static const S2HCustomItemRandomizer regularPool = {
        .structSize = sizeof(S2HCustomItemRandomizer),
        .flags = S2H_CUSTOM_ITEM_RANDO_ADVANCEMENT | S2H_CUSTOM_ITEM_RANDO_PROGRESSIVE,
        .type = S2H_CUSTOM_ITEM_TYPE_ITEM,
        .poolCount = 4,
        .progressionGroup = SLING_CHAIN,
        .poolCountOption = SLING_INFINITY_OPTION,
        .poolModeOption = SLING_INFINITY_OPTION,
        .poolModeValue = 2,
        .poolModeInvert = true,
    };
    static const S2HCustomItemRandomizer condensedPool = {
        .structSize = sizeof(S2HCustomItemRandomizer),
        .flags = S2H_CUSTOM_ITEM_RANDO_ADVANCEMENT | S2H_CUSTOM_ITEM_RANDO_PROGRESSIVE,
        .type = S2H_CUSTOM_ITEM_TYPE_ITEM,
        .poolCount = 4,
        .progressionGroup = SLING_CONDENSED_CHAIN,
        .poolModeOption = SLING_INFINITY_OPTION,
        .poolModeValue = 2,
    };
    static const char* const slingText = "You got the %rFairy Slingshot%w!&Press %y" GLYPH_C "%w to aim and shoot.^"
                                        "Its Bullet Bag holds %y30 Deku Seeds%w.";
    static const char* const infiniteText = "You got the %gInfinite Bullet Bag%w!&Your slingshot won't run out&of Deku Seeds.";
    const char* slingIcon = OOT_ICON("gItemIconSlingshotTex");
    const char* bagIcon = OOT_ICON("gItemIconBulletBag50Tex");

    if (!S2H_MOD_API_HAS(sApi, RegisterProgressiveChain) || !S2H_MOD_API_HAS(sApi, RegisterRandoOption)) return;
    sApi->RegisterRandoOption(&sInfinityOption);
    RegisterSlingshotPickup(regular[1].itemKey, OOT_ICON("gItemIconBulletBag40Tex"), "Big Bullet Bag",
                            "You got the %yBig Bullet Bag%w!&It holds up to %y40 Deku Seeds%w.", sBagDL, NULL, NULL);
    for (u8 i = 2; i <= 3; i++) {
        RegisterSlingshotPickup(regular[i].itemKey, bagIcon, "Biggest Bullet Bag",
                                "You got the %yBiggest Bullet Bag%w!&It holds up to %y50 Deku Seeds%w.",
                                sBagDL, NULL, NULL);
    }
    RegisterSlingshotPickup(regular[4].itemKey, bagIcon, "Infinite Bullet Bag", infiniteText, sBagDL, NULL, NULL);
    RegisterSlingshotPickup(condensed[0].itemKey, slingIcon, "Fairy Slingshot", slingText, sSlingGetDL, NULL, NULL);
    RegisterSlingshotPickup(condensed[1].itemKey, bagIcon, "Infinite Bullet Bag", infiniteText, sBagDL, NULL, NULL);
    RegisterSlingshotPickup(SLING_PROGRESSIVE, slingIcon, "Progressive Slingshot", slingText, sSlingGetDL, &regularPool, NULL);
    RegisterSlingshotPickup(SLING_CONDENSED, slingIcon, "Progressive Slingshot", slingText, sSlingGetDL, &condensedPool, NULL);
    RegisterSlingshotPickup("oot.deku_seeds_5", OOT_ICON("gItemIconDekuSeedsTex"), "Deku Seeds (5)",
                            "You got %y5 Deku Seeds%w!", sSeedDL, NULL, AddSeeds);
    RegisterSlingshotPickup("oot.deku_seeds_30", OOT_ICON("gItemIconDekuSeedsTex"), "Deku Seeds (30)",
                            "You got %y30 Deku Seeds%w!", sSeedDL, NULL, AddSeeds);

    const S2HProgressiveChain normalChain = {
        .structSize = sizeof(S2HProgressiveChain), .key = SLING_CHAIN, .pickupKey = SLING_PROGRESSIVE,
        .levels = regular, .levelCount = ARRAY_COUNT(regular), .firstLevel = 1,
        .readLevel = ReadSlingLevel, .applyLevel = ApplySlingLevel,
    };
    const S2HProgressiveChain condensedChain = {
        .structSize = sizeof(S2HProgressiveChain), .key = SLING_CONDENSED_CHAIN, .pickupKey = SLING_CONDENSED,
        .levels = condensed, .levelCount = ARRAY_COUNT(condensed), .firstLevel = 1,
        .readLevel = ReadCondensedSlingLevel, .applyLevel = ApplyCondensedSlingLevel,
    };
    sApi->RegisterProgressiveChain(&normalChain);
    sApi->RegisterProgressiveChain(&condensedChain);
}

#include "oot_songs.inc.c"

// Din's dome carries fire arrow damage, so it lights torches and burns webs near Link, not at a distance.
static void GrantOotItemLogic(void) {
    static const Z64LogicSource sSlingshot = { SLING };
    static const Z64LogicSource sCondensedSlingshot = { "oot.slingshot.condensed_first" };
    static const Z64LogicSource sBoomerang = { BOOM };
    static const Z64LogicSource sHammer = { HAMMER };
    static const Z64LogicSource sDinsFire = { DIN, 0, true };
    static const char* const sShooting[] = { "CanUseProjectile", "CanHitSwitch", "CanAttack", "CanDamage", NULL };
    static const char* const sThrowing[] = { "CanUseProjectile", "HookshotOrBoomerang", "CanHitSwitch",
                                             "CanAttack", "CanDamage", NULL };
    static const char* const sSmashing[] = { "BlastOrSmash", "CanHitSwitch", "CanAttack", "CanDamage", NULL };
    static const char* const sBurning[] = { "HasFireSourceWithTorch", "CanAttack", "CanDamage", NULL };

    if (!Z64Logic_Init(sApi)) {
        return;
    }
    Z64Logic_GrantCapabilities(&sSlingshot, sShooting);
    Z64Logic_GrantCapabilities(&sCondensedSlingshot, sShooting);
    Z64Logic_GrantCapabilities(&sBoomerang, sThrowing);
    Z64Logic_GrantCapabilities(&sHammer, sSmashing);
    Z64Logic_GrantBoulderSmashing(&sHammer);
    Z64Logic_GrantCapabilities(&sDinsFire, sBurning);
    Z64Logic_GrantTorchLighting(&sDinsFire);
}

S2H_MOD_EXPORT void ModSetApi(const S2HModApi* api) { sApi = api; }
S2H_MOD_EXPORT const S2HModRequirements* ModGetRequirements(void) { return &sRequirements; }
S2H_MOD_EXPORT void ModInit(void) {
    Load(0);
    S2HActorDefinition actor = { 0 };
    actor.structSize = sizeof(actor);
    actor.key = "oot.boomerang.projectile";
    actor.description = "OoT boomerang";
    actor.category = ACTORCAT_ITEMACTION;
    actor.actorFlags = ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
    actor.objectId = GAMEPLAY_KEEP;
    actor.instanceSize = sizeof(OotBoom);
    actor.init = BoomInit;
    actor.destroy = BoomDestroy;
    actor.update = BoomUpdate;
    actor.draw = BoomDraw;
    sBoomId = sApi->RegisterActor(&actor);
    actor.key = "oot.dins_fire.effect";
    actor.description = "Din's Fire";
    actor.instanceSize = sizeof(OotDinsFire);
    actor.init = DinsFireInit;
    actor.destroy = DinsFireDestroy;
    actor.update = DinsFireUpdate;
    actor.draw = DinsFireDraw;
    sFireId = sApi->RegisterActor(&actor);
    actor.key = "oot.farores_wind.effect";
    actor.description = "Farore's Wind";
    actor.instanceSize = sizeof(OotWindEffect);
    actor.init = WindInit;
    actor.destroy = WindDestroy;
    actor.update = WindUpdate;
    actor.draw = WindDraw;
    sWindId = sApi->RegisterActor(&actor);
    actor.key = "oot.nayrus_love.effect";
    actor.description = "Nayru's Love";
    actor.instanceSize = sizeof(OotNayrusLove);
    actor.init = NayrusLoveInit;
    actor.destroy = NayrusLoveDestroy;
    actor.update = NayrusLoveUpdate;
    actor.draw = NayrusLoveDraw;
    sNayruId = sApi->RegisterActor(&actor);
    if (sBoomId < 0 || sFireId < 0 || sWindId < 0 || sNayruId < 0) {
        return;
    }
    PatchHammerHand();
    RegisterItem(SLING, OOT_ICON("gItemIconSlingshotTex"), OOT_NAME("gFairySlingshotItemNameENGTex"),
                 "%rFairy Slingshot&%wHold %y" GLYPH_C "%w to aim and load.&Release to fire.",
                 "You got the %rFairy Slingshot%w!&Hold %y" GLYPH_C "%w to aim and load a seed.&Release it to fire.^"
                 "Press %g" GLYPH_A "%w, %r" GLYPH_B "%w or %y" GLYPH_C_UP "%w to stop aiming.&"
                 "It holds up to %y30 Deku Seeds%w.", SLOT_POWDER_KEG, sSlingGetDL, -1, TakeSling);
    RegisterItem(BOOM, OOT_ICON("gItemIconBoomerangTex"), OOT_NAME("gBoomerangItemNameENGTex"),
                 "%rBoomerang&%wHold %y" GLYPH_C "%w to aim. Release to throw.",
                 "You got the %rBoomerang%w!&Hold %y" GLYPH_C "%w to aim, then release to&throw it and stun enemies.",
                 SLOT_MAGIC_BEANS, sBoomDL, -1, TakeBoom);
    RegisterItem(HAMMER, OOT_ICON("gItemIconHammerTex"), OOT_NAME("gMegatonHammerItemNameENGTex"),
                 "%rMegaton Hammer&%wPress %y" GLYPH_C "%w to swing the hammer.",
                 "You got the %rMegaton Hammer%w!&Press %y" GLYPH_C "%w to swing the hammer.^"
                 "Its heavy blows hit like a&Goron punch or pound.", SLOT_SWORD_GREAT_FAIRY, sHammerDL, -1,
                 RequestHammerSwing);
    RegisterItem(DIN, OOT_ICON("gItemIconDinsFireTex"), OOT_NAME("gDinsFireItemNameENGTex"),
                 "%rDin's Fire&%wPress %y" GLYPH_C "%w to cast. Costs 12 MP.",
                 "You got %rDin's Fire%w!&Press %y" GLYPH_C "%w to unleash a ring of&flame. It costs %y12 MP%w.",
                 OOT_SLOT_DINS_FIRE, sSpellDiamondDL, 0, CastDin);
    RegisterItem(FARORE, OOT_ICON("gItemIconFaroresWindTex"), OOT_NAME("gFaroresWindItemNameENGTex"),
                 "%gFarore's Wind&%wSet a warp point in a dungeon&for 12 MP. Use it again to&return or dispel it.",
                 "You got %gFarore's Wind%w!&Press %y" GLYPH_C "%w in a dungeon to set&a warp point for %y12 MP%w.^"
                 "Use it again to return to the&point or dispel it.",
                 OOT_SLOT_FARORES_WIND, sSpellDiamondDL, 1, CastFarore);
    RegisterItem(NAYRU, OOT_ICON("gItemIconNayrusLoveTex"), OOT_NAME("gNayrusLoveItemNameENGTex"),
                 "%bNayru's Love&%wPress %y" GLYPH_C "%w to cast. Costs 24 MP.",
                 "You got %bNayru's Love%w!&Press %y" GLYPH_C "%w to cast a protective&aura for one minute. Costs %y24 MP%w.",
                 OOT_SLOT_NAYRUS_LOVE, sSpellDiamondDL, 2, CastNayru);
    RegisterSlingshotProgression();
    GrantOotItemLogic();
    RegisterOotSongs();
    S2H_REGISTER_HOOK(sApi, OnPlayerUpdate, Tick);
    S2H_REGISTER_HOOK(sApi, OnSceneInit, Scene);
    S2H_REGISTER_HOOK(sApi, OnPlayDrawEnd, DrawOotReticle);
    S2H_REGISTER_HOOK(sApi, OnPlayDrawEnd, DrawOotSpellEffects);
    S2H_REGISTER_HOOK(sApi, OnPlayerResolveLimbDraw, DrawSlingshotHand);
    S2H_REGISTER_HOOK(sApi, OnPlayerResolveLimbDraw, DrawOotLeftHand);
    S2H_REGISTER_HOOK(sApi, OnPlayerPostLimbDraw, DrawSlingshotString);
    S2H_REGISTER_HOOK(sApi, OnKaleidoItemDraw, DrawSeedPause);
    S2H_REGISTER_HOOK(sApi, OnInterfaceDrawEnd, DrawSeedHud);
    S2H_REGISTER_HOOK(sApi, OnKaleidoItemEquip, StartEquipFlight);
    S2H_REGISTER_HOOK(sApi, OnKaleidoInput, CycleSlingshotCell);
    S2H_REGISTER_HOOK(sApi, OnResolveCustomGetItem, ResolveSlingshotPickup);
    S2H_REGISTER_HOOK(sApi, OnLoadFile, Load);
    S2H_REGISTER_HOOK(sApi, OnActorDestroy, ActorDestroyed);
    S2H_REGISTER_HOOK(sApi, OnPlayerResolveAnim, ResolveHammerStance);
    S2H_REGISTER_HOOK_FOR_ID(sApi, OnPlayerActionHandler, SOH_PLAYER_ACTION_MELEE, TakeHammerAttack);
    S2H_REGISTER_HOOK_FOR_ID(sApi, OnPlayerActionHandler, SOH_PLAYER_ACTION_SPIN_CHARGE, TakeHammerAttack);
    sApi->RegisterVB(VB_INFLICT_VOID_DAMAGE, WaiveVoid);
}
