#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "z64items.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "2s2h/Rando/Types.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "overlays/actors/ovl_En_Bom/z_en_bom.h"
#include "overlays/actors/ovl_En_Bom_Chu/z_en_bom_chu.h"

#define KAFEI_MASK_KEY "nei.mask_kafei"
#define KAFEI_FORM_KEY "nei.kafei"
#define KAFEI_MODEL_PATH "objects/forms/kafei"
#define KAFEI_MASK_PAGE 1
#define KAFEI_MASK_SLOT 14

#define STICK_MAX 60.0f
#define STICK_MOVING 10.0f

#define SPRINT_SPEED_SCALE 1.5f
#define SPRINT_LEG_CYCLE_SCALE 1.4f
#define LEG_CYCLE_FRAMES 29.0f
#define STAMINA_WHEEL_FRAMES 220.0f
#define STAMINA_REFILL_PER_FRAME 2.2f
#define STAMINA_REFILL_DELAY 18

// SW97 guards below this speed, backing up included, so a coasting halt still blocks.
#define GUARD_STILL_SPEED 0.1f

#define MOVING_SLASH_MIN_SPEED 2.0f

#define OCARINA_NOTE_NONE OCARINA_PITCH_NONE
// PlayerAnimation advances R_UPDATE_RATE * 0.5 clip frames per update, and the rate is always 3.
#define CLIP_FRAMES_PER_UPDATE 1.5f
// Player_DrawImpl reads the face from this joint: eyes + 1 in the low nibble, mouth + 1 in the high one.
#define FACE_JOINT PLAYER_LIMB_MAX
#define WHISTLE_FACE (((2 + 1) << 4) | (0 + 1))

// MM leaves these bits unnamed: dead, talking, loading, getting an item, carrying, swimming, first person, ledge
// climb, ladder, horse, damage knockback and the textbox freeze.
#define BUSY_STATE1                                                                                          \
    (PLAYER_STATE1_DEAD | PLAYER_STATE1_TALKING | PLAYER_STATE1_200 | PLAYER_STATE1_400 |                    \
     PLAYER_STATE1_CARRYING_ACTOR | PLAYER_STATE1_8000000 | PLAYER_STATE1_100000 | PLAYER_STATE1_4 |         \
     PLAYER_STATE1_200000 | PLAYER_STATE1_800000 | PLAYER_STATE1_4000000 | PLAYER_STATE1_20000000)
#define STATE1_SHIELDING PLAYER_STATE1_400000
#define STATE1_SWIMMING PLAYER_STATE1_8000000

#define LANDMINE_FUSE 500
#define LANDMINE_REARM 200
#define LANDMINE_FALL_SPEED 8.0f

// The stamina wheel is built in hundredths of a unit and scaled down, so the ring can use integer vertices.
#define WHEEL_SEGMENTS 32
#define WHEEL_RADIUS 1200
#define WHEEL_THICKNESS 340
#define WHEEL_GAP 200
#define WHEEL_SCALE 0.01f
#define WHEEL_HEAD_CLEARANCE 25.0f

#define ANIM_SW97_DIR "__OTR__misc/link_animetion/gPlayerAnim_mhr_sw97_"

static const ALIGN_ASSET(2) char sIconTex[] = "__OTR__icon_item_static_yar/gItemIconKafeisMaskTex";
static const ALIGN_ASSET(2) char sNameTex[] = "__OTR__item_name_static/gItemNameKafeisMaskENGTex";
static const ALIGN_ASSET(2) char sMaskDL[] = "__OTR__objects/object_mask_kerfay/gKafeisMaskDL";
static const ALIGN_ASSET(2) char sGetItemDL[] = "__OTR__objects/object_gi_mask05/gGiKafeiMaskDL";
static const ALIGN_ASSET(2) char sLandmineIconTex[] = "__OTR__textures/icon_item_custom/gItemIconLandmineTex";
static const ALIGN_ASSET(2) char sWhistleRaisePath[] = ANIM_SW97_DIR "reed_whistle_start";
static const ALIGN_ASSET(2) char sWhistlePlayPath[] = ANIM_SW97_DIR "reed_whistle_loop";
static const ALIGN_ASSET(2) char sWhistleLowerPath[] = ANIM_SW97_DIR "reed_whistle_end";
static const ALIGN_ASSET(2) char sAdultEmptyHandDL[] = "__OTR__" KAFEI_MODEL_PATH
                                                       "/object_link_boy/gLinkAdultRightHandNearDL";
static const ALIGN_ASSET(2) char sChildEmptyHandDL[] = "__OTR__" KAFEI_MODEL_PATH
                                                       "/object_link_child/gLinkChildRightHandNearDL";

// SW97's landmine, converted from the prototype's gameplay_keep.
static const ALIGN_ASSET(2) char sLandmineSphereDL[] = "__OTR__objects/object_landmine/gLandMineSphereDL";
static const ALIGN_ASSET(2) char sLandminePlaneDL[] = "__OTR__objects/object_landmine/gLandMinePlaneDL";
static const ALIGN_ASSET(2) char sLandmineSpikesDL[] = "__OTR__objects/object_landmine/gLandMineSphere004DL";

static const char* const sRequiredHooks[] = {
    "OnPlayerActionHandler", "OnPlayerResolveMotionScale", "OnPlayerPostLimbDraw", "OnPlayerResolveLimbDraw",
    "OnOcarinaInput",        "OnActorInit",                "OnActorUpdate",         "ShouldActorDraw",
    "OnActorDrawEnd",        "OnSceneInit",
};
static const S2HModRequirements sRequirements = { sizeof(S2HModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

s32 Player_SetAction(PlayState* play, Player* this, PlayerActionFunc actionFunc, s32 arg3);
void func_8083B1A0(Player* player, PlayState* play);
void func_80834DB8(Player* player, PlayerAnimationHeader* anim, f32 lift, PlayState* play);
void func_8082FA5C(PlayState* play, Player* player, s32 meleeWeaponState);
bool Player_IsZTargeting(Player* player);
extern PlayerAnimationHeader* ResourceMgr_LoadPlayerAnimAsHeader(const char* animPath);
extern Gfx* ResourceMgr_LoadGfxByName(const char* path);
extern HOST_DATA u8 sCurOcarinaPitch;

static const S2HModApi* sApi;

static bool CanAct(Player* player) {
    return !(player->stateFlags1 & BUSY_STATE1);
}

static bool IsOnGround(Player* player) {
    return (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) != 0;
}

static f32 GetStickMagnitude(Input* input) {
    f32 x = input->rel.stick_x;
    f32 y = input->rel.stick_y;

    return MIN(sqrtf((x * x) + (y * y)), STICK_MAX);
}

// ---- stamina sprint ----

static f32 sStamina;
static bool sIsSprinting;
static bool sIsWinded;
static s16 sRefillDelay;
static f32 sLegCycle;

static bool IsDoubleDefenseAcquired(void) {
    return gSaveContext.save.saveInfo.playerData.doubleDefense != 0;
}

static f32 GetStaminaCapacity(void) {
    return IsDoubleDefenseAcquired() ? (STAMINA_WHEEL_FRAMES * 2.0f) : STAMINA_WHEEL_FRAMES;
}

static void ResetStamina(void) {
    sStamina = GetStaminaCapacity();
    sIsSprinting = false;
    sIsWinded = false;
    sRefillDelay = 0;
}

// Running out costs the whole recovery: Kafei stays doubled over until the wheels are full again, like BotW.
static void WindedAction(Player* player, PlayState* play) {
    player->speedXZ = 0.0f;
    player->actor.speed = 0.0f;
    PlayerAnimation_Update(play, &player->skelAnime);
    if (!sIsWinded) {
        func_8083B1A0(player, play);
    }
}

// There is no out-of-breath clip, so the heavy landing stands in: Link doubled over, recovering.
static void StartWinded(PlayState* play, Player* player) {
    PlayerAnimationHeader* landing = (PlayerAnimationHeader*)gPlayerAnim_link_normal_landing;

    sIsWinded = true;
    Player_SetAction(play, player, WindedAction, 0);
    PlayerAnimation_Change(play, &player->skelAnime, landing, 1.0f, 0.0f, Animation_GetLastFrame(landing),
                           ANIMMODE_ONCE, -6.0f);
}

static bool WantsToSprint(Player* player, Input* input) {
    return CHECK_BTN_ALL(input->cur.button, BTN_A) && IsOnGround(player) && CanAct(player) &&
           !(player->stateFlags1 & STATE1_SHIELDING) && GetStickMagnitude(input) >= STICK_MOVING;
}

static void RefillStamina(void) {
    if (sRefillDelay > 0) {
        sRefillDelay--;
        return;
    }
    sStamina = MIN(sStamina + STAMINA_REFILL_PER_FRAME, GetStaminaCapacity());
}

static void UpdateStamina(PlayState* play, Player* player, Input* input) {
    if (sIsWinded && sStamina >= GetStaminaCapacity()) {
        sIsWinded = false;
    }
    sIsSprinting = !sIsWinded && sStamina > 0.0f && WantsToSprint(player, input);
    if (!sIsSprinting) {
        RefillStamina();
        return;
    }

    sStamina -= 1.0f;
    sRefillDelay = STAMINA_REFILL_DELAY;
    if (sStamina <= 0.0f) {
        sStamina = 0.0f;
        sIsSprinting = false;
        StartWinded(play, player);
    }
}

// unk_B38 is the phase every locomotion action loads the legs from, and the speed cap on it stops a faster run
// from speeding the legs up, so it is stepped again here, after the cap.
static void QuickenLegCycle(Player* player) {
    f32 step = player->unk_B38 - sLegCycle;

    if (step > LEG_CYCLE_FRAMES * 0.5f) {
        step -= LEG_CYCLE_FRAMES;
    } else if (step < -LEG_CYCLE_FRAMES * 0.5f) {
        step += LEG_CYCLE_FRAMES;
    }
    if (sIsSprinting) {
        player->unk_B38 += step * (SPRINT_LEG_CYCLE_SCALE - 1.0f);
        if (player->unk_B38 >= LEG_CYCLE_FRAMES) {
            player->unk_B38 -= LEG_CYCLE_FRAMES;
        } else if (player->unk_B38 < 0.0f) {
            player->unk_B38 += LEG_CYCLE_FRAMES;
        }
    }
    sLegCycle = player->unk_B38;
}

static void ScaleSprintSpeed(Player* player, s32 kind, f32* scale) {
    if (sIsSprinting) {
        *scale *= SPRINT_SPEED_SCALE;
    }
}

static u8 GetWheelCount(void) {
    return IsDoubleDefenseAcquired() ? 2 : 1;
}

static f32 GetWheelFill(u8 wheel) {
    f32 remaining = sStamina - ((f32)wheel * STAMINA_WHEEL_FRAMES);

    return CLAMP(remaining / STAMINA_WHEEL_FRAMES, 0.0f, 1.0f);
}

// Hidden while full and idle, so the wheel does not sit over Kafei's head forever.
static bool IsStaminaWheelVisible(void) {
    return sIsSprinting || sIsWinded || sStamina < GetStaminaCapacity();
}

static void SetWheelVertex(Vtx* vertex, f32 angle, s32 radius, const Color_RGBA8* color) {
    vertex->v.ob[0] = (s32)(cosf(angle) * radius);
    vertex->v.ob[1] = (s32)(sinf(angle) * radius);
    vertex->v.ob[2] = 0;
    vertex->v.flag = 0;
    vertex->v.tc[0] = 0;
    vertex->v.tc[1] = 0;
    vertex->v.cn[0] = color->r;
    vertex->v.cn[1] = color->g;
    vertex->v.cn[2] = color->b;
    vertex->v.cn[3] = color->a;
}

// An arc from twelve o'clock, clockwise, as inner/outer vertex pairs drawn one quad at a time.
static void DrawWheelArc(PlayState* play, s32 radius, f32 fill, const Color_RGBA8* color) {
    s32 segments = (s32)ceilf(fill * WHEEL_SEGMENTS);

    if (segments <= 0) {
        return;
    }
    Vtx* ring = GRAPH_ALLOC(play->state.gfxCtx, (segments + 1) * 2 * sizeof(Vtx));
    if (ring == NULL) {
        return;
    }
    for (s32 i = 0; i <= segments; i++) {
        f32 angle = (M_PIf * 0.5f) - ((M_PIf * 2.0f) * fill * ((f32)i / segments));

        SetWheelVertex(&ring[i * 2], angle, radius - WHEEL_THICKNESS, color);
        SetWheelVertex(&ring[(i * 2) + 1], angle, radius, color);
    }

    OPEN_DISPS(play->state.gfxCtx);
    for (s32 i = 0; i < segments; i++) {
        gSPVertex(POLY_XLU_DISP++, (uintptr_t)&ring[i * 2], 4, 0);
        gSP2Triangles(POLY_XLU_DISP++, 0, 1, 3, 0, 0, 3, 2, 0);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawStaminaWheel(Actor* actor, PlayState* play) {
    static const Color_RGBA8 sTrackColor = { 0, 0, 0, 140 };
    static const Color_RGBA8 sFillColor = { 120, 230, 110, 235 };
    static const Color_RGBA8 sWindedColor = { 230, 70, 60, 235 };
    Player* player = (Player*)actor;

    if (play->pauseCtx.state != 0 || !IsStaminaWheelVisible() || !sApi->IsFormActive(KAFEI_FORM_KEY)) {
        return;
    }
    Vec3f* head = &player->bodyPartsPos[PLAYER_BODYPART_HEAD];

    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    Matrix_Translate(head->x, head->y + WHEEL_HEAD_CLEARANCE, head->z, MTXMODE_NEW);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    Matrix_Scale(WHEEL_SCALE, WHEEL_SCALE, WHEEL_SCALE, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, Matrix_Finalize(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    Matrix_Pop();
    gDPPipeSync(POLY_XLU_DISP++);
    gSPTexture(POLY_XLU_DISP++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_OFF);
    gDPSetCombineMode(POLY_XLU_DISP++, G_CC_SHADE, G_CC_SHADE);
    // No depth test: like the BotW wheel it reads through walls, and Kafei's own head never hides it.
    gDPSetRenderMode(POLY_XLU_DISP++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gSPClearGeometryMode(POLY_XLU_DISP++,
                         G_ZBUFFER | G_CULL_BOTH | G_FOG | G_LIGHTING | G_TEXTURE_GEN | G_TEXTURE_GEN_LINEAR);
    gSPSetGeometryMode(POLY_XLU_DISP++, G_SHADE | G_SHADING_SMOOTH);
    CLOSE_DISPS(play->state.gfxCtx);

    for (u8 wheel = 0; wheel < GetWheelCount(); wheel++) {
        s32 radius = WHEEL_RADIUS - (wheel * (WHEEL_THICKNESS + WHEEL_GAP));

        DrawWheelArc(play, radius, 1.0f, &sTrackColor);
        DrawWheelArc(play, radius, GetWheelFill(wheel), sIsWinded ? &sWindedColor : &sFillColor);
    }
}

// ---- standalone shield ----

static Vec3f sHandShieldCorners[4] = {
    { -4500.0f, -3000.0f, -600.0f },
    { 1500.0f, -3000.0f, -600.0f },
    { -4500.0f, 3000.0f, -600.0f },
    { 1500.0f, 3000.0f, -600.0f },
};
static const u8 sShieldMaterials[PLAYER_SHIELD_MAX] = { COL_MATERIAL_METAL, COL_MATERIAL_WOOD, COL_MATERIAL_METAL };

static bool IsGuardingPassively(Player* player) {
    return player->rightHandType == PLAYER_MODELTYPE_RH_SHIELD && player->currentShield != PLAYER_SHIELD_NONE &&
           player->actor.scale.y >= 0.0f && !(player->stateFlags1 & STATE1_SHIELDING) &&
           player->speedXZ <= GUARD_STILL_SPEED;
}

// SW97's shield blocks on its own while Link stands still. Vanilla only registers the shield quad under
// the shielding state; registering it here is enough, since the player's damage check reads AC_BOUNCED.
static void GuardWhileStill(Player* player, s32 limbIndex) {
    PlayState* play = gPlayState;

    if (limbIndex != PLAYER_LIMB_RIGHT_HAND || !IsGuardingPassively(player) || !sApi->IsFormActive(KAFEI_FORM_KEY)) {
        return;
    }
    Vec3f corners[4];

    Matrix_Push();
    Matrix_Put(&player->shieldMf);
    for (s32 i = 0; i < 4; i++) {
        Matrix_MultVec3f(&sHandShieldCorners[i], &corners[i]);
    }
    Matrix_Pop();

    player->shieldQuad.base.colMaterial = sShieldMaterials[player->currentShield];
    Collider_SetQuadVertices(&player->shieldQuad, &corners[0], &corners[1], &corners[2], &corners[3]);
    CollisionCheck_SetAC(play, &play->colChkCtx, &player->shieldQuad.base);
    CollisionCheck_SetAT(play, &play->colChkCtx, &player->shieldQuad.base);
}

// ---- slashing without breaking stride ----

typedef enum {
    SLASH_SWORD,
    SLASH_STICK,
    SLASH_NONE,
} SlashClip;

typedef struct {
    const char* path;
    u8 arcStart;
    u8 arcEnd;
} SlashArc;

// The arcs are measured: the frames where the arm's angular velocity clears 30% of the clip's peak.
static const SlashArc sSlashArcs[] = {
    { ANIM_SW97_DIR "move_sword_slash", 1, 5 },
    { ANIM_SW97_DIR "move_stick_slash", 2, 4 },
};

static PlayerAnimationHeader* sSlashAnims[ARRAY_COUNT(sSlashArcs)];
static u8 sSlash = SLASH_NONE;
static u8 sSlashFrame;

static SlashClip GetSlashClip(Player* player) {
    switch (player->heldItemAction) {
        case PLAYER_IA_SWORD_KOKIRI:
        case PLAYER_IA_SWORD_RAZOR:
        case PLAYER_IA_SWORD_GILDED:
            return SLASH_SWORD;
        case PLAYER_IA_DEKU_STICK:
            return SLASH_STICK;
        default:
            return SLASH_NONE;
    }
}

static bool IsStickOnButton(PlayState* play, Player* player, EquipSlot slot, u16 button) {
    return Player_GetItemOnButton(play, player, slot) == ITEM_DEKU_STICK &&
           CHECK_BTN_ALL(play->state.input[0].press.button, button);
}

static bool IsStickOnDpad(PlayState* play, Player* player, DpadEquipSlot slot, u16 button) {
    return Player_Dpad_GetItemOnButton(play, player, slot) == ITEM_DEKU_STICK &&
           CHECK_BTN_ALL(play->state.input[0].press.button, button);
}

// The sword swings on B; a Deku Stick swings on the button it is equipped to, where B would swap in the sword.
static bool IsSwingPressed(PlayState* play, Player* player) {
    if (player->heldItemAction != PLAYER_IA_DEKU_STICK) {
        return CHECK_BTN_ALL(play->state.input[0].press.button, BTN_B);
    }
    return IsStickOnButton(play, player, EQUIP_SLOT_C_LEFT, BTN_CLEFT) ||
           IsStickOnButton(play, player, EQUIP_SLOT_C_DOWN, BTN_CDOWN) ||
           IsStickOnButton(play, player, EQUIP_SLOT_C_RIGHT, BTN_CRIGHT) ||
           IsStickOnDpad(play, player, EQUIP_SLOT_D_UP, BTN_DUP) ||
           IsStickOnDpad(play, player, EQUIP_SLOT_D_DOWN, BTN_DDOWN) ||
           IsStickOnDpad(play, player, EQUIP_SLOT_D_LEFT, BTN_DLEFT) ||
           IsStickOnDpad(play, player, EQUIP_SLOT_D_RIGHT, BTN_DRIGHT);
}

static bool IsStriding(Player* player, Input* input) {
    return IsOnGround(player) && player->speedXZ >= MOVING_SLASH_MIN_SPEED &&
           GetStickMagnitude(input) >= STICK_MOVING;
}

static void EndMovingSlash(PlayState* play, Player* player) {
    if (sSlash == SLASH_NONE) {
        return;
    }
    sSlash = SLASH_NONE;
    if (player->meleeWeaponState != 0) {
        func_8082FA5C(play, player, 0);
    }
}

// The vanilla attack takes the whole body and brakes to a stop, so while moving it is refused and the SW97
// clip drives only the sword arm from the form's update; Link's own locomotion keeps the legs and the speed.
static int32_t SlashWhileMoving(PlayState* play, Player* player) {
    Input* input = &play->state.input[0];

    if (!IsSwingPressed(play, player) || (player->stateFlags1 & STATE1_SHIELDING)) {
        return S2H_FORM_ACTION_VANILLA;
    }
    if (sSlash != SLASH_NONE) {
        return S2H_FORM_ACTION_BLOCKED;
    }
    SlashClip clip = GetSlashClip(player);
    if (clip == SLASH_NONE || sSlashAnims[clip] == NULL || !CanAct(player) || !IsStriding(player, input)) {
        return S2H_FORM_ACTION_VANILLA;
    }

    sSlash = clip;
    sSlashFrame = 0;
    // func_8082FA5C picks its swing sound and grunt off these two.
    player->meleeWeaponAnimation = PLAYER_MWA_FORWARD_SLASH_1H;
    player->unk_ADD = 0;
    return S2H_FORM_ACTION_BLOCKED;
}

static void UpdateMovingSlash(PlayState* play, Player* player) {
    if (sSlash == SLASH_NONE) {
        return;
    }
    if (!IsOnGround(player) || !CanAct(player) || GetSlashClip(player) == SLASH_NONE) {
        EndMovingSlash(play, player);
        return;
    }
    const SlashArc* arc = &sSlashArcs[sSlash];
    PlayerAnimationHeader* anim = sSlashAnims[sSlash];
    // SetLoadFrame copies the face channel after the limbs, hence the extra entry.
    Vec3s pose[PLAYER_LIMB_MAX + 1];

    // The walk or run pose is already in jointTable by now: only the sword arm changes.
    AnimTaskQueue_AddLoadPlayerFrame(play, anim, sSlashFrame, PLAYER_LIMB_MAX, pose);
    player->skelAnime.jointTable[PLAYER_LIMB_LEFT_SHOULDER] = pose[PLAYER_LIMB_LEFT_SHOULDER];
    player->skelAnime.jointTable[PLAYER_LIMB_LEFT_FOREARM] = pose[PLAYER_LIMB_LEFT_FOREARM];
    player->skelAnime.jointTable[PLAYER_LIMB_LEFT_HAND] = pose[PLAYER_LIMB_LEFT_HAND];

    // The blade damages straight off the hand matrix, so arming it over the arc is all the hit detection needed.
    bool bladeLive = sSlashFrame >= arc->arcStart && sSlashFrame <= arc->arcEnd;
    if (bladeLive != (player->meleeWeaponState != 0)) {
        func_8082FA5C(play, player, bladeLive);
    }

    sSlashFrame++;
    if (sSlashFrame > (u8)Animation_GetLastFrame(anim)) {
        EndMovingSlash(play, player);
    }
}

// ---- A button ----

// A roll on the press, so a moving A belongs to the sprint from the frame it goes down. Standing still it
// stays Link's: that same handler puts the sword away.
static int32_t SprintInsteadOfRoll(PlayState* play, Player* player) {
    Input* input = &play->state.input[0];
    bool isSprintPress = CHECK_BTN_ALL(input->cur.button, BTN_A) && GetStickMagnitude(input) >= STICK_MOVING;

    return isSprintPress ? S2H_FORM_ACTION_BLOCKED : S2H_FORM_ACTION_VANILLA;
}

static bool CanJumpFromFloor(PlayState* play, Player* player) {
    return play->roomCtx.curRoom.type != ROOM_TYPE_INDOORS && player->actor.floorPoly != NULL &&
           SurfaceType_GetFloorType(&play->colCtx, player->actor.floorPoly, player->actor.floorBgId) !=
               FLOOR_TYPE_7 &&
           SurfaceType_GetFloorEffect(&play->colCtx, player->actor.floorPoly, player->actor.floorBgId) !=
               FLOOR_EFFECT_1;
}

// A under Z-target is a plain jump, never a jump slash: vanilla's own jump, with its animation and launch.
static int32_t JumpInsteadOfJumpslash(PlayState* play, Player* player) {
    const s8 stick = player->controlStickDirections[player->controlStickDataIndex];

    if (!CHECK_BTN_ALL(play->state.input[0].press.button, BTN_A) || stick > PLAYER_STICK_DIR_FORWARD ||
        !Player_IsZTargeting(player) || !CanJumpFromFloor(play, player)) {
        return S2H_FORM_ACTION_VANILLA;
    }
    func_80834DB8(player, (PlayerAnimationHeader*)gPlayerAnim_link_normal_jump, REG(69) / 100.0f, play);
    return S2H_FORM_ACTION_STARTED;
}

static const S2HFormActionOverride sActions[] = {
    { SOH_PLAYER_ACTION_ROLL, SprintInsteadOfRoll },
    { SOH_PLAYER_ACTION_MELEE, SlashWhileMoving },
    { SOH_PLAYER_ACTION_ZTARGET_A, JumpInsteadOfJumpslash },
};

// ---- the whistle ----

typedef enum {
    WHISTLE_IDLE,
    WHISTLE_RAISING,
    WHISTLE_PLAYING,
    WHISTLE_LOWERING,
} WhistlePhase;

static PlayerAnimationHeader* sWhistleRaise;
static PlayerAnimationHeader* sWhistlePlay;
static PlayerAnimationHeader* sWhistleLower;
static u8 sWhistlePhase;
static bool sIsOcarinaOut;
static bool sIsInstrumentLent;
static volatile bool sWasNoteStruck;
static u8 sLastNote = OCARINA_NOTE_NONE;

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

static void ChangeWhistleClip(PlayState* play, Player* player, PlayerAnimationHeader* anim, u8 mode,
                              f32 morphFrames) {
    PlayerAnimation_Change(play, &player->skelAnime, anim, 1.0f, 0.0f, Animation_GetLastFrame(anim), mode,
                           morphFrames);
}

// Vanilla's ocarina action waits for the end of its intro and outro clips; the whistle's own clips take
// their place, so that wait is what raises and lowers the hand.
static bool SwapOcarinaClip(PlayState* play, Player* player) {
    if (IsPlayingOcarinaClip(player, "okarina_start")) {
        ChangeWhistleClip(play, player, sWhistleRaise, ANIMMODE_ONCE, -6.0f);
        sWhistlePhase = WHISTLE_RAISING;
        return true;
    }
    if (IsPlayingOcarinaClip(player, "okarina_end") && sWhistleLower != NULL) {
        ChangeWhistleClip(play, player, sWhistleLower, ANIMMODE_ONCE, -6.0f);
        sWhistlePhase = WHISTLE_LOWERING;
        return true;
    }
    return false;
}

// Park on the play clip at speed 0; each note replays the gesture. Freezing with playSpeed instead of
// re-arming frame 0 keeps the pose from snapping back between notes. It loops: that same action reopens the
// ocarina whenever a clip reports its end, which would wipe the notes of the song being played.
static void PoseWhistle(PlayState* play, Player* player) {
    f32 playEnd = Animation_GetLastFrame(sWhistlePlay) - CLIP_FRAMES_PER_UPDATE;

    if (player->skelAnime.animation != sWhistlePlay) {
        ChangeWhistleClip(play, player, sWhistlePlay, ANIMMODE_LOOP, -4.0f);
        player->skelAnime.playSpeed = 0.0f;
        sWhistlePhase = WHISTLE_PLAYING;
    }
    bool isGestureDone = player->skelAnime.curFrame >= playEnd;
    if (sWasNoteStruck) {
        if (isGestureDone) {
            player->skelAnime.curFrame = 0.0f;
        }
        player->skelAnime.playSpeed = 1.0f;
    } else if (isGestureDone) {
        player->skelAnime.playSpeed = 0.0f;
    }
    sWasNoteStruck = false;
    player->skelAnime.jointTable[FACE_JOINT].x = WHISTLE_FACE;
}

static void StopWhistling(PlayState* play, Player* player) {
    bool isHandUp = sWhistlePhase == WHISTLE_RAISING || sWhistlePhase == WHISTLE_PLAYING;
    if (isHandUp && sWhistleLower != NULL) {
        ChangeWhistleClip(play, player, sWhistleLower, ANIMMODE_ONCE, -6.0f);
    }
    sWhistlePhase = WHISTLE_IDLE;
    sWasNoteStruck = false;
    // The ocarina session already closed, and vanilla leaves the instrument off between sessions.
    if (sIsInstrumentLent) {
        AudioOcarina_SetInstrument(OCARINA_INSTRUMENT_OFF);
        sIsInstrumentLent = false;
    }
}

// The ocarina stays out across the message system's session, so the latch arms on the instrument in hand and
// only lets go once no message is up.
static void UpdateWhistle(PlayState* play, Player* player) {
    bool isSessionOpen = play->msgCtx.msgMode != MSGMODE_NONE;

    if (IsHoldingOcarina(player)) {
        sIsOcarinaOut = true;
    } else if (!isSessionOpen) {
        sIsOcarinaOut = false;
    }
    if (!sIsOcarinaOut) {
        if (sWhistlePhase != WHISTLE_IDLE || sIsInstrumentLent) {
            StopWhistling(play, player);
        }
        return;
    }
    if (sWhistleRaise == NULL || sWhistlePlay == NULL || SwapOcarinaClip(play, player)) {
        return;
    }
    bool isRaising = sWhistlePhase == WHISTLE_RAISING && player->skelAnime.animation == sWhistleRaise;
    if (isRaising || sWhistlePhase == WHISTLE_LOWERING) {
        return;
    }
    if (isSessionOpen) {
        // Re-applied every frame: the message system sets the instrument again around opening, and the last
        // write wins.
        AudioOcarina_SetInstrument(OCARINA_INSTRUMENT_WHISTLING_FLUTE);
        sIsInstrumentLent = true;
    }
    // A raise that ended hands over to the gesture even before an actor opens the session it was played for.
    if (isSessionOpen || sWhistlePhase == WHISTLE_RAISING) {
        PoseWhistle(play, player);
    }
}

// The ocarina is baked into the hand, so Kafei whistles with the empty hand of his own model.
static void HideOcarina(Player* player, s32 limbIndex, Gfx** dList, Gfx* limbDList, Vec3f* pos) {
    if (limbIndex != PLAYER_LIMB_RIGHT_HAND || !sIsOcarinaOut || !IsHoldingOcarina(player)) {
        return;
    }
    const char* emptyHand = sApi->HasResource(sChildEmptyHandDL) ? sChildEmptyHandDL : sAdultEmptyHandDL;

    if (sApi->HasResource(emptyHand)) {
        *dList = ResourceMgr_LoadGfxByName(emptyHand);
    }
}

// ---- landmines ----

typedef struct {
    const char* dl;
    f32 scale;
    f32 pivot;
    bool isYawFromCamera;
    bool isPitchFromCamera;
} LandminePiece;

// The prototype aims each piece its own way: the dome turns with the camera in yaw, the band is a full
// billboard hung 5 units up, and the spikes follow the actor. Scales are on top of the chu's own 0.01.
static const LandminePiece sLandminePieces[] = {
    { sLandmineSphereDL, 95.0f, 0.0f, true, false },
    { sLandminePlaneDL, 105.0f, 5.0f, true, true },
    { sLandmineSpikesDL, 1.05f, 0.0f, false, false },
};

static EnBomChuActionFunc sAwaitRelease;

static bool IsLandmineTriggered(EnBomChu* chu) {
    Collider* collider = &chu->collider.base;

    return (collider->acFlags & AC_HIT) ||
           ((collider->ocFlags1 & OC1_HIT) && collider->oc != NULL && collider->oc->category == ACTORCAT_ENEMY);
}

static void LandmineVanish(EnBomChu* chu, PlayState* play) {
    if (chu->timer != 0) {
        chu->timer--;
    }
    if (chu->timer == 0) {
        Actor_Kill(&chu->actor);
    }
}

static void DetonateLandmine(EnBomChu* chu, PlayState* play) {
    EnBom* bomb = (EnBom*)Actor_Spawn(&play->actorCtx, play, ACTOR_EN_BOM, chu->actor.world.pos.x,
                                      chu->actor.world.pos.y, chu->actor.world.pos.z, BOMB_EXPLOSIVE_TYPE_BOMB, 0,
                                      0, BOMB_TYPE_BODY);
    if (bomb != NULL) {
        bomb->timer = 0;
    }
    chu->timer = 1;
    chu->actor.speed = 0.0f;
    chu->actionFunc = LandmineVanish;
}

// A mine lies where it was dropped and waits for an enemy to step on it or for something to hit it.
static void LandmineLie(EnBomChu* chu, PlayState* play) {
    if (IsLandmineTriggered(chu)) {
        DetonateLandmine(chu, play);
        return;
    }
    chu->actor.speed = 0.0f;
    chu->visualJitter = 0.0f;
    // The fuse only drives the blink now; recycled before it reaches the fast "about to blow" rate.
    chu->timer--;
    if (chu->timer < LANDMINE_REARM) {
        chu->timer = LANDMINE_FUSE;
    }

    Actor_UpdateBgCheckInfo(play, &chu->actor, 5.0f, 5.0f, 0.0f, UPDBGCHECKINFO_FLAG_1 | UPDBGCHECKINFO_FLAG_2 |
                                                                     UPDBGCHECKINFO_FLAG_4 | UPDBGCHECKINFO_FLAG_8 |
                                                                     UPDBGCHECKINFO_FLAG_10);
    // A constant drop rather than gravity: EnBomChu has no field to accumulate a fall speed in.
    if (chu->actor.world.pos.y > chu->actor.floorHeight) {
        chu->actor.world.pos.y = MAX(chu->actor.world.pos.y - LANDMINE_FALL_SPEED, chu->actor.floorHeight);
    }
}

static bool IsLandmine(EnBomChu* chu) {
    return chu->actionFunc == LandmineLie || chu->actionFunc == LandmineVanish;
}

// OnActorInit runs after the chu's own init, which set the 120-frame fuse and the hold-in-hand action.
static void ArmLandmine(Actor* actor) {
    EnBomChu* chu = (EnBomChu*)actor;

    sAwaitRelease = chu->actionFunc;
    if (sApi->IsFormActive(KAFEI_FORM_KEY)) {
        chu->timer = LANDMINE_FUSE;
    }
}

// Released, a chu starts crawling at speed 8; the crawl steps itself by rewriting world.pos, so braking it
// from outside is inert. Its action is swapped for the mine's on that very frame instead.
static void LayLandmine(Actor* actor) {
    EnBomChu* chu = (EnBomChu*)actor;

    if (chu->actionFunc == sAwaitRelease || IsLandmine(chu) || !chu->isMoving ||
        !sApi->IsFormActive(KAFEI_FORM_KEY)) {
        return;
    }
    chu->actionFunc = LandmineLie;
    chu->isMoving = false;
    chu->visualJitter = 0.0f;
}

static void DrawLandminePiece(PlayState* play, Actor* actor, const LandminePiece* piece, u8 fade) {
    Camera* camera = GET_ACTIVE_CAM(play);
    s16 yaw = piece->isYawFromCamera ? Camera_GetCamDirYaw(camera) : actor->shape.rot.y;
    s16 pitch = piece->isPitchFromCamera ? -Camera_GetCamDirPitch(camera) : actor->shape.rot.x;

    Matrix_Push();
    Matrix_Translate(actor->world.pos.x, actor->world.pos.y + piece->pivot, actor->world.pos.z, MTXMODE_NEW);
    Matrix_RotateYF(BINANG_TO_RAD(yaw), MTXMODE_APPLY);
    Matrix_RotateXF(BINANG_TO_RAD(pitch), MTXMODE_APPLY);
    Matrix_RotateZF(BINANG_TO_RAD(actor->shape.rot.z), MTXMODE_APPLY);
    Matrix_Translate(0.0f, -piece->pivot, 0.0f, MTXMODE_APPLY);
    Matrix_Scale(actor->scale.x * piece->scale, actor->scale.y * piece->scale, actor->scale.z * piece->scale,
                 MTXMODE_APPLY);

    OPEN_DISPS(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, Matrix_Finalize(play->state.gfxCtx), G_MTX_MODELVIEW | G_MTX_NOPUSH | G_MTX_LOAD);
    // Per piece: the band's own list forces PRIM to black and would carry it into the next piece.
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, fade, fade, 255);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)piece->dl);
    CLOSE_DISPS(play->state.gfxCtx);

    Matrix_Pop();
}

// The chu's own blink, as EnBomChu_Draw computes it, so the mine still warns.
static f32 GetFuseBlink(s16 timer) {
    s32 halfPeriod = timer >= 40 ? 10 : (timer >= 10 ? 5 : 1);
    s32 blink = timer >= 40 ? timer % 20 : (timer >= 10 ? timer % 10 : timer & 1);

    if (blink > halfPeriod) {
        blink = (2 * halfPeriod) - blink;
    }
    return blink / (f32)halfPeriod;
}

static void DrawLandmine(Actor* actor, bool* shouldDraw) {
    EnBomChu* chu = (EnBomChu*)actor;
    PlayState* play = gPlayState;
    bool isHeldByKafei = chu->actionFunc == sAwaitRelease && sApi->IsFormActive(KAFEI_FORM_KEY);

    if (!isHeldByKafei && !IsLandmine(chu)) {
        return;
    }
    *shouldDraw = false;
    if (chu->actionFunc == LandmineVanish) {
        return;
    }
    u8 fade = (u8)(255.0f - (GetFuseBlink(chu->timer) * 170.0f));

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    CLOSE_DISPS(play->state.gfxCtx);

    for (s32 i = 0; i < ARRAY_COUNT(sLandminePieces); i++) {
        DrawLandminePiece(play, actor, &sLandminePieces[i], fade);
    }
}

// ---- form ----

static void LoadAnims(void) {
    for (s32 i = 0; i < ARRAY_COUNT(sSlashArcs); i++) {
        sSlashAnims[i] = ResourceMgr_LoadPlayerAnimAsHeader(sSlashArcs[i].path);
    }
    sWhistleRaise = ResourceMgr_LoadPlayerAnimAsHeader(sWhistleRaisePath);
    sWhistlePlay = ResourceMgr_LoadPlayerAnimAsHeader(sWhistlePlayPath);
    sWhistleLower = ResourceMgr_LoadPlayerAnimAsHeader(sWhistleLowerPath);
}

static void EnterKafei(PlayState* play, Player* player) {
    ResetStamina();
    sLegCycle = player->unk_B38;
    sSlash = SLASH_NONE;
    sWhistlePhase = WHISTLE_IDLE;
    sIsOcarinaOut = false;
}

static void ExitKafei(PlayState* play, Player* player) {
    EndMovingSlash(play, player);
    StopWhistling(play, player);
    sIsOcarinaOut = false;
    ResetStamina();
    if (player->actionFunc == WindedAction) {
        func_8083B1A0(player, play);
    }
}

static void UpdateKafei(PlayState* play, Player* player) {
    Input* input = &play->state.input[0];

    UpdateMovingSlash(play, player);
    UpdateStamina(play, player, input);
    QuickenLegCycle(player);
    UpdateWhistle(play, player);
}

// The new scene brings a new player: whatever pointed at the old one's state starts over.
static void ForgetScene(s16 sceneId, s8 spawnNum) {
    sSlash = SLASH_NONE;
    sWhistlePhase = WHISTLE_IDLE;
    sIsOcarinaOut = false;
    sWasNoteStruck = false;
    sLastNote = OCARINA_NOTE_NONE;
    if (sIsInstrumentLent) {
        AudioOcarina_SetInstrument(OCARINA_INSTRUMENT_OFF);
        sIsInstrumentLent = false;
    }
}

// ---- mask ----

static bool CanWearMask(Player* player, PlayState* play) {
    return IsOnGround(player) && !(player->stateFlags1 & (STATE1_SWIMMING | PLAYER_STATE1_100000));
}

static void WearMask(PlayState* play, Player* player) {
    sApi->ToggleForm(KAFEI_FORM_KEY);
}

static const S2HCustomItemRandomizer sRandomizerItem = {
    sizeof(S2HCustomItemRandomizer), 0, S2H_CUSTOM_ITEM_TYPE_ITEM, 0, 1, KAFEI_MASK_KEY, KAFEI_MASK_KEY, "",
};

static void RegisterMask(void) {
    S2HCustomItemDefinition mask = Z64Items_Define(KAFEI_MASK_KEY, sIconTex, sNameTex);

    Z64Items_SetButtons(&mask, S2H_CUSTOM_ITEM_C_BUTTON | S2H_CUSTOM_ITEM_DPAD);
    mask.flags |= S2H_CUSTOM_ITEM_INSTANT | S2H_CUSTOM_ITEM_WEARABLE;
    mask.disabledIconSurfaces |= S2H_ITEM_ICON_SAVE_EDITOR;
    Z64Items_SetPlacement(&mask, KAFEI_MASK_PAGE, KAFEI_MASK_SLOT, 0);
    Z64Items_SetReplaces(&mask, ITEM_MASK_KAFEIS_MASK);
    Z64Items_SetVanillaMode(&mask, S2H_VANILLA_ITEM_REPLACE, RI_MASK_KAFEIS_MASK);
    if (sApi->HasResource(sGetItemDL)) {
        Z64Items_SetGetItemModel(&mask, sGetItemDL);
    }
    Z64Items_SetTextbox(&mask, "You got %rKafei's Mask%w!&The face of Anju's missing groom. Wear it with %y\xA1%w "
                               "and you become Kafei: he sprints until he runs out of breath, whistles instead of "
                               "playing the ocarina and lays %rlandmines%w instead of bombchus.");
    Z64Items_SetPauseText(&mask, "%rKafei's Mask&%wPress %y\xA1%w to become Kafei. Hold %y\x9F%w to sprint; standing "
                                 "still, his shield guards on its own.");
    Z64Items_SetCanUse(&mask, CanWearMask);
    Z64Items_SetAction(&mask, WearMask, NULL);
    Z64Items_SetLogic(&mask, &sRandomizerItem);
    Z64Items_Register(sApi, &mask);
}

// Kafei is Link-sized and keeps Link's equipment, items and voice: only the body and the moves change.
static void RegisterForm(void) {
    S2HFormDefinition kafei = { 0 };

    kafei.structSize = sizeof(kafei);
    kafei.key = KAFEI_FORM_KEY;
    kafei.label = "Kafei";
    kafei.kind = S2H_FORM_KIND_LINK;
    kafei.item = KAFEI_MASK_KEY;
    kafei.modelPath = KAFEI_MODEL_PATH;
    kafei.transformMask = sApi->HasResource(sMaskDL) ? sMaskDL : NULL;
    kafei.transformVoiceSfx = NA_SE_VO_LI_FALL_L;
    kafei.actions = sActions;
    kafei.actionCount = ARRAY_COUNT(sActions);
    kafei.onEnter = EnterKafei;
    kafei.onExit = ExitKafei;
    kafei.update = UpdateKafei;

    sApi->RegisterForm(&kafei);
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
    S2H_REGISTER_HOOK_FOR_ID(sApi, OnPlayerResolveMotionScale, SOH_PLAYER_MOTION_STICK_SPEED, ScaleSprintSpeed);
    S2H_REGISTER_HOOK(sApi, OnPlayerPostLimbDraw, GuardWhileStill);
    S2H_REGISTER_HOOK(sApi, OnPlayerResolveLimbDraw, HideOcarina);
    S2H_REGISTER_HOOK(sApi, OnOcarinaInput, NoteStruck);
    S2H_REGISTER_HOOK_FOR_ID(sApi, OnActorDrawEnd, ACTOR_PLAYER, DrawStaminaWheel);
    S2H_REGISTER_HOOK_FOR_ID(sApi, OnActorInit, ACTOR_EN_BOM_CHU, ArmLandmine);
    S2H_REGISTER_HOOK_FOR_ID(sApi, OnActorUpdate, ACTOR_EN_BOM_CHU, LayLandmine);
    S2H_REGISTER_HOOK_FOR_ID(sApi, ShouldActorDraw, ACTOR_EN_BOM_CHU, DrawLandmine);
    S2H_REGISTER_HOOK(sApi, OnSceneInit, ForgetScene);
}
