#include <math.h>
#include <string.h>

#include "z64items.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "objects/gameplay_keep/gameplay_keep.h"

#define DEKU_LEAF_KEY "nei.deku_leaf"
#define ROC_BOOTS_KEY "nei.equip.roc_boots"
#define DEKU_LEAF_CANOPY_SCALE 0.16f
#define DEKU_LEAF_CANOPY_LIFT 6.0f
#define DEKU_LEAF_HELD_SCALE 0.08f
#define DEKU_LEAF_SWEPT_SCALE 0.25f
#define DEKU_LEAF_FALL_SPEED -1.0f
#define DEKU_LEAF_GLIDE_SPEED 6.0f
#define DEKU_LEAF_GLIDE_ACCEL 0.5f
#define DEKU_LEAF_GLIDE_DRAIN_INTERVAL 7
#define DEKU_LEAF_GLIDE_MAGIC 1
#define DEKU_LEAF_SWING_MAGIC 1
#define DEKU_LEAF_SWING_SPEED 2.0f
// Frames of the 39-frame swing: it stalls at its farthest reach on 8, the leaf is drawn wide from 10, and the
// gust leaves it on 15.
#define DEKU_LEAF_APEX_FRAME 8.0f
#define DEKU_LEAF_APEX_HOLD 5
#define DEKU_LEAF_GUST_FRAME 15.0f
#define DEKU_LEAF_WIDE_FRAME_FIRST 10.0f
#define DEKU_LEAF_WIDE_FRAME_LAST 22.0f
#define DEKU_LEAF_GUST_SPEED 26.0f
#define DEKU_LEAF_GUST_LIFE 14
#define DEKU_LEAF_GUST_DAMAGE 2
#define DEKU_LEAF_GUST_KNOCKBACK 40.0f
#define DEKU_LEAF_GUST_LIFT 6.0f
#define DEKU_LEAF_GUST_START 40.0f
#define DEKU_LEAF_GUST_HEIGHT 25.0f
#define DEKU_LEAF_COLLIDER_RADIUS 55
#define DEKU_LEAF_COLLIDER_HEIGHT 60
#define DEKU_LEAF_WIND_MOTES 3
#define DEKU_LEAF_AIR_BALL_MOTES 6

// A player in any of these is busy with something that owns them; the leaf does not answer meanwhile.
#define DEKU_LEAF_BLOCKING_STATES \
    (PLAYER_STATE1_DEAD | PLAYER_STATE1_TALKING | PLAYER_STATE1_CARRYING_ACTOR | PLAYER_STATE1_8000000)

static const ALIGN_ASSET(2) char sIconTex[] = "__OTR__textures/icon_item_custom/gItemIconDekuLeafTex";
static const ALIGN_ASSET(2) char sNameTex[] = "__OTR__textures/item_name_custom/gDekuLeafNameTex";
static const ALIGN_ASSET(2) char sLeafDL[] = "__OTR__objects/object_nei_deku_leaf/g_dekuleaf_dl";
static const ALIGN_ASSET(2) char sSwingAnimPath[] = "__OTR__misc/link_animetion/gPlayerAnim_nei_dekuleaf_blow";

static const u32 sItemButtons[S2H_ITEM_BUTTON_COUNT] = { BTN_B,      BTN_CLEFT, BTN_CDOWN, BTN_CRIGHT, 0,
                                                         BTN_DRIGHT, BTN_DLEFT, BTN_DDOWN, BTN_DUP };

static Color_RGBA8 sWindColor = { 220, 220, 220, 160 };
static Color_RGBA8 sAirBallPrimColor = { 195, 225, 235, 160 };
static Color_RGBA8 sAirBallEnvColor = { 150, 200, 220, 100 };

// DMG_UNBLOCKABLE is a trap: no pot or crate accepts it, and a damage table is read at the HIGHEST flag set,
// which nearly every foe leaves at zero. The sword's is the flag they all answer to.
#define DEKU_LEAF_GUST_FLAGS (DMG_SWORD | DMG_ZORA_BOOMERANG)

static ColliderCylinderInit sGustColliderInit = {
    {
        COL_MATERIAL_NONE,
        AT_ON | AT_TYPE_PLAYER,
        AC_NONE,
        OC1_NONE,
        OC2_TYPE_PLAYER,
        COLSHAPE_CYLINDER,
    },
    {
        ELEM_MATERIAL_UNK0,
        { DEKU_LEAF_GUST_FLAGS, 0x00, DEKU_LEAF_GUST_DAMAGE },
        { 0x00000000, 0x00, 0x00 },
        ATELEM_ON | ATELEM_SFX_NONE,
        ACELEM_NONE,
        OCELEM_NONE,
    },
    { DEKU_LEAF_COLLIDER_RADIUS, DEKU_LEAF_COLLIDER_HEIGHT, 0, { 0, 0, 0 } },
};

static const char* const sRequiredHooks[] = { "OnPlayerUpdate", "OnActorDrawEnd", "OnSceneInit" };
static const S2HModRequirements sRequirements = { sizeof(S2HModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const S2HModApi* sApi;
static ColliderCylinder sGustCollider;
static bool sIsGliding;
static bool sHasGustLeft;
static bool sHasHeldApex;
static s16 sApexHold;
static Vec3f sGustPos;
static s16 sGustYaw;
static s16 sGustLife;
static s16 sDrainTimer;
static u8 sPreviousInvincibility;

extern PlayerAnimationHeader* ResourceMgr_LoadPlayerAnimAsHeader(const char* animPath);
s32 Player_SetAction(PlayState* play, Player* player, PlayerActionFunc actionFunc, s32 flags);
void Player_StopHorizontalMovement(Player* player);
void Player_Anim_PlayOnce(PlayState* play, Player* player, PlayerAnimationHeader* anim);
void func_8083B1A0(Player* player, PlayState* play);

static void SwingAction(Player* player, PlayState* play);

static u32 FindEquippedButtonMask(void) {
    for (u8 button = 0; button < S2H_ITEM_BUTTON_COUNT; button++) {
        const char* equipped = sApi->GetEquippedCustomItem(button);
        if (equipped != NULL && strcmp(equipped, DEKU_LEAF_KEY) == 0) {
            return sItemButtons[button];
        }
    }
    return 0;
}

static bool HasMagic(s16 amount) {
    return gSaveContext.save.saveInfo.playerData.isMagicAcquired &&
           gSaveContext.save.saveInfo.playerData.magic >= amount;
}

// The meter's own consume state machine serves one request at a time and beeps at the rest, which a leaf
// that drains while it glides would trip on its next gust. A point off the meter is the whole cost here.
static bool SpendMagic(s16 amount) {
    if (!HasMagic(amount)) {
        return false;
    }
    gSaveContext.save.saveInfo.playerData.magic -= amount;
    return true;
}

static bool IsSwinging(Player* player) {
    return player->actionFunc == (PlayerActionFunc)SwingAction;
}

static bool IsGrounded(Player* player) {
    return (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) != 0;
}

static void SpawnWindMotes(PlayState* play, Vec3f* origin, s16 yaw) {
    for (u8 mote = 0; mote < DEKU_LEAF_WIND_MOTES; mote++) {
        s16 spread = (s16)Rand_CenteredFloat(0x2000);
        f32 reach = 10.0f + Rand_ZeroOne() * 20.0f;
        f32 speed = 15.0f + Rand_ZeroOne() * 10.0f;
        Vec3f pos = { origin->x + Math_SinS(yaw + spread) * reach, origin->y + Rand_CenteredFloat(15.0f),
                      origin->z + Math_CosS(yaw + spread) * reach };
        Vec3f velocity = { Math_SinS(yaw + spread) * speed, Rand_CenteredFloat(2.0f), Math_CosS(yaw + spread) * speed };
        Vec3f accel = { velocity.x * -0.05f, -0.1f, velocity.z * -0.05f };

        EffectSsDust_Spawn(play, 0, &pos, &velocity, &accel, &sWindColor, &sWindColor, 80, 20, 10, 0);
    }
}

// What the gust throws flies out wrapped in its own puff of wind.
static void SpawnAirBall(PlayState* play, Vec3f* pos) {
    for (u8 mote = 0; mote < DEKU_LEAF_AIR_BALL_MOTES; mote++) {
        s16 angle = (s16)Rand_CenteredFloat(65535.0f);
        f32 radius = 8.0f + Rand_ZeroFloat(14.0f);
        Vec3f motePos = { pos->x + Math_SinS(angle) * radius, pos->y + 10.0f + Rand_CenteredFloat(16.0f),
                          pos->z + Math_CosS(angle) * radius };
        Vec3f velocity = { Math_SinS(angle) * 3.0f, Rand_CenteredFloat(1.5f), Math_CosS(angle) * 3.0f };
        Vec3f accel = { 0.0f, 0.3f, 0.0f };

        EffectSsDust_Spawn(play, 0, &motePos, &velocity, &accel, &sAirBallPrimColor, &sAirBallEnvColor, 200, 25, 12,
                           0);
    }
}

static void LaunchGust(Player* player) {
    sGustYaw = player->actor.shape.rot.y;
    sGustPos.x = player->actor.world.pos.x + Math_SinS(sGustYaw) * DEKU_LEAF_GUST_START;
    sGustPos.y = player->actor.world.pos.y + DEKU_LEAF_GUST_HEIGHT;
    sGustPos.z = player->actor.world.pos.z + Math_CosS(sGustYaw) * DEKU_LEAF_GUST_START;
    sGustLife = DEKU_LEAF_GUST_LIFE;
    sGustCollider.base.atFlags &= ~AT_HIT;
}

static void BurstGust(PlayState* play) {
    SpawnAirBall(play, &sGustPos);
    Audio_PlaySfx_AtPos(&sGustPos, NA_SE_EV_PLANT_BROKEN);
    sGustLife = 0;
    sGustCollider.base.atFlags &= ~(AT_ON | AT_HIT);
}

// What the wind reaches is thrown the way the wind was already going, not away from where it came from.
static void ThrowHitActor(PlayState* play, Actor* hit) {
    hit->world.rot.y = sGustYaw;
    hit->speed = DEKU_LEAF_GUST_KNOCKBACK;
    hit->velocity.x = Math_SinS(sGustYaw) * DEKU_LEAF_GUST_KNOCKBACK;
    hit->velocity.z = Math_CosS(sGustYaw) * DEKU_LEAF_GUST_KNOCKBACK;
    hit->velocity.y = DEKU_LEAF_GUST_LIFT;
    SpawnAirBall(play, &hit->world.pos);
}

static bool HasHitWall(PlayState* play, Vec3f* from, Vec3f* to) {
    CollisionPoly* poly = NULL;
    s32 bgId;
    Vec3f hit;

    if (!BgCheck_EntityLineTest1(&play->colCtx, from, to, &hit, &poly, true, true, true, true, &bgId)) {
        return false;
    }
    *to = hit;
    return true;
}

// The hit only shows up in the flags once the frame's collision pass has run, so it is read on the next tick,
// by which time whatever it was may already be gone.
static Actor* TakeGustHit(void) {
    Actor* hit = sGustCollider.base.at;

    if (!(sGustCollider.base.atFlags & AT_HIT)) {
        return NULL;
    }
    sGustCollider.base.atFlags &= ~AT_HIT;
    return hit != NULL && hit->update != NULL ? hit : NULL;
}

static void AdvanceGust(PlayState* play) {
    Vec3f previous = sGustPos;
    Actor* hit = TakeGustHit();

    if (hit != NULL) {
        ThrowHitActor(play, hit);
        BurstGust(play);
        return;
    }
    sGustPos.x += Math_SinS(sGustYaw) * DEKU_LEAF_GUST_SPEED;
    sGustPos.z += Math_CosS(sGustYaw) * DEKU_LEAF_GUST_SPEED;
    SpawnWindMotes(play, &sGustPos, sGustYaw);
    if (HasHitWall(play, &previous, &sGustPos)) {
        BurstGust(play);
        return;
    }
    sGustCollider.dim.pos.x = (s16)sGustPos.x;
    sGustCollider.dim.pos.y = (s16)sGustPos.y;
    sGustCollider.dim.pos.z = (s16)sGustPos.z;
    sGustCollider.base.atFlags |= AT_ON | AT_TYPE_PLAYER;
    CollisionCheck_SetAT(play, &play->colChkCtx, &sGustCollider.base);
    if (--sGustLife <= 0) {
        sGustLife = 0;
        sGustCollider.base.atFlags &= ~AT_ON;
    }
}

// The wind-up hangs at its farthest reach before the leaf sweeps, and the swing picks up where it left off.
static void HoldSwingAtApex(Player* player) {
    if (sApexHold > 0) {
        sApexHold--;
        player->skelAnime.playSpeed = 0.0f;
        return;
    }
    if (!sHasHeldApex && player->skelAnime.curFrame >= DEKU_LEAF_APEX_FRAME) {
        sHasHeldApex = true;
        sApexHold = DEKU_LEAF_APEX_HOLD;
        player->skelAnime.playSpeed = 0.0f;
        return;
    }
    player->skelAnime.playSpeed = DEKU_LEAF_SWING_SPEED;
}

static void SwingAction(Player* player, PlayState* play) {
    HoldSwingAtApex(player);
    if (PlayerAnimation_Update(play, &player->skelAnime)) {
        func_8083B1A0(player, play);
        return;
    }
    Player_StopHorizontalMovement(player);
    if (!sHasGustLeft && player->skelAnime.curFrame >= DEKU_LEAF_GUST_FRAME) {
        sHasGustLeft = true;
        SpendMagic(DEKU_LEAF_SWING_MAGIC);
        LaunchGust(player);
        Audio_PlaySfx_AtPos(&player->actor.world.pos, NA_SE_IT_SWORD_SWING);
        // The effort lands on the beat the gust leaves, as it does on a jump slash.
        Player_PlaySfx(player, NA_SE_VO_LI_SWORD_N);
    }
}

static bool CanSwingLeaf(Player* player, PlayState* play) {
    return sApi->GetActiveForm() == NULL && IsGrounded(player) &&
           !(player->stateFlags1 & DEKU_LEAF_BLOCKING_STATES) &&
           (player->meleeWeaponState == PLAYER_MELEE_WEAPON_STATE_0) && HasMagic(DEKU_LEAF_SWING_MAGIC);
}

// The gust is timed by the animation, so without the resource the leaf never swings at all.
static void SwingLeaf(PlayState* play, Player* player) {
    PlayerAnimationHeader* swing = ResourceMgr_LoadPlayerAnimAsHeader(sSwingAnimPath);

    if (swing == NULL || IsSwinging(player) || !CanSwingLeaf(player, play)) {
        return;
    }
    Collider_InitCylinder(play, &sGustCollider);
    Collider_SetCylinder(play, &sGustCollider, &player->actor, &sGustColliderInit);
    sHasGustLeft = false;
    sHasHeldApex = false;
    sApexHold = 0;
    sGustLife = 0;
    Player_SetAction(play, player, (PlayerActionFunc)SwingAction, 0);
    Player_StopHorizontalMovement(player);
    Player_Anim_PlayOnce(play, player, swing);
    player->skelAnime.playSpeed = DEKU_LEAF_SWING_SPEED;
    Audio_PlaySfx_AtPos(&player->actor.world.pos, NA_SE_PL_CHANGE_ARMS);
    Player_PlaySfx(player, NA_SE_VO_LI_AUTO_JUMP);
}

static void StartGliding(Player* player) {
    sIsGliding = true;
    sDrainTimer = 0;
    Audio_PlaySfx_AtPos(&player->actor.world.pos, NA_SE_PL_CHANGE_ARMS);
}

// The rustle is asked for frame by frame, so letting go of the glide is what silences it.
static void StopGliding(void) {
    AudioSfx_StopById(NA_SE_PL_PLANT_MOVE - SFX_FLAG);
    AudioSfx_StopById(NA_SE_PL_PLANT_MOVE);
    sIsGliding = false;
}

static bool DrainGlideMagic(void) {
    if (++sDrainTimer < DEKU_LEAF_GLIDE_DRAIN_INTERVAL) {
        return true;
    }
    sDrainTimer = 0;
    return SpendMagic(DEKU_LEAF_GLIDE_MAGIC);
}

// Link's yaw already follows the stick in the air, so a steady forward drift is all the leaf has to add.
static void UpdateGlide(Player* player, PlayState* play) {
    // Both hands held over the head is the carry pose, and it reads as holding the leaf up into the wind.
    PlayerAnimationHeader* glide = (PlayerAnimationHeader*)dgPlayerAnim_link_normal_carryB_wait;

    if (!BEN_ANIM_EQUAL(player->skelAnime.animation, dgPlayerAnim_link_normal_carryB_wait)) {
        PlayerAnimation_Change(play, &player->skelAnime, glide, 1.0f, 0.0f, Animation_GetLastFrame(glide),
                               ANIMMODE_LOOP, -4.0f);
    }
    // Roc's Boots halve every fall; the leaf pins its own, so it is halved here.
    f32 fallSpeed = sApi->IsEquipmentWorn(ROC_BOOTS_KEY) ? DEKU_LEAF_FALL_SPEED * 0.5f : DEKU_LEAF_FALL_SPEED;

    if (player->actor.velocity.y < fallSpeed) {
        player->actor.velocity.y = fallSpeed;
    }
    if (player->speedXZ < DEKU_LEAF_GLIDE_SPEED) {
        Math_StepToF(&player->speedXZ, DEKU_LEAF_GLIDE_SPEED, DEKU_LEAF_GLIDE_ACCEL);
    }
    // The rustle of the bean plant's own leaf, asked for every frame the way that plant asks for it.
    Actor_PlaySfx_Flagged2(&player->actor, NA_SE_PL_PLANT_MOVE - SFX_FLAG);
}

static bool WasJustHurt(Player* player) {
    bool isHurt = player->invincibilityTimer > 0 && sPreviousInvincibility == 0;

    sPreviousInvincibility = player->invincibilityTimer;
    return isHurt;
}

static bool CanKeepGliding(Player* player, PlayState* play, u32 buttonMask) {
    return !IsGrounded(player) && (play->state.input[0].cur.button & buttonMask) &&
           !(player->stateFlags1 & DEKU_LEAF_BLOCKING_STATES) && !WasJustHurt(player);
}

// Both the swing and the glide read the button here, not through vanilla's item path: that one only runs on the
// frames the current action func updates the upper body, so a press made on any other frame was simply lost.
// The gust flies on from here too, so it outlives the swing that threw it.
static void UpdateDekuLeaf(Player* player) {
    PlayState* play = gPlayState;
    u32 buttonMask = FindEquippedButtonMask();

    if (play == NULL) {
        StopGliding();
        return;
    }
    // A transformation which allows the leaf owns its form-specific behavior; the leaf is the Human's.
    if (sApi->GetActiveForm() != NULL || player->transformation != PLAYER_FORM_HUMAN) {
        StopGliding();
        return;
    }
    if (sGustLife > 0) {
        AdvanceGust(play);
    }
    if (buttonMask == 0) {
        StopGliding();
        return;
    }
    if (sIsGliding) {
        if (!CanKeepGliding(player, play, buttonMask) || !DrainGlideMagic()) {
            StopGliding();
            return;
        }
        UpdateGlide(player, play);
        return;
    }
    sPreviousInvincibility = player->invincibilityTimer;
    if (!(play->state.input[0].press.button & buttonMask) || IsSwinging(player) || sApi->GetWorldSpeed() == 0.0f ||
        (player->stateFlags1 & DEKU_LEAF_BLOCKING_STATES)) {
        return;
    }
    if (IsGrounded(player)) {
        SwingLeaf(play, player);
        return;
    }
    if (HasMagic(DEKU_LEAF_GLIDE_MAGIC)) {
        StartGliding(player);
    }
}

// The leaf's own display list clears lighting for everything drawn after it, so the mode is put back by hand.
static void DrawLeaf(PlayState* play, Vec3f* pos, f32 yaw, f32 scale) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    Matrix_Translate(pos->x, pos->y, pos->z, MTXMODE_NEW);
    Matrix_RotateYF(yaw + M_PIf, MTXMODE_APPLY);
    Matrix_RotateXF(-M_PIf / 2.0f, MTXMODE_APPLY);
    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sLeafDL);
    gSPSetGeometryMode(POLY_OPA_DISP++, G_CULL_BACK | G_LIGHTING);
    CLOSE_DISPS(play->state.gfxCtx);
}

// A canopy over both hands, not a leaf at his chest: anchored anywhere lower it cuts through Link as he falls.
static void DrawCanopy(Player* player, PlayState* play) {
    Vec3f* leftHand = &player->bodyPartsPos[PLAYER_BODYPART_LEFT_HAND];
    Vec3f* rightHand = &player->bodyPartsPos[PLAYER_BODYPART_RIGHT_HAND];
    Vec3f canopy = { (leftHand->x + rightHand->x) * 0.5f, (leftHand->y + rightHand->y) * 0.5f + DEKU_LEAF_CANOPY_LIFT,
                     (leftHand->z + rightHand->z) * 0.5f };

    DrawLeaf(play, &canopy, BINANG_TO_RAD(player->actor.shape.rot.y), DEKU_LEAF_CANOPY_SCALE);
}

static void DrawLeafInHand(Player* player, PlayState* play) {
    Vec3f* forearm = &player->bodyPartsPos[PLAYER_BODYPART_LEFT_FOREARM];
    Vec3f hand = player->bodyPartsPos[PLAYER_BODYPART_LEFT_HAND];
    f32 yaw = Math_Atan2F_XY(hand.z - forearm->z, hand.x - forearm->x);
    bool isSweeping = player->skelAnime.curFrame >= DEKU_LEAF_WIDE_FRAME_FIRST &&
                      player->skelAnime.curFrame <= DEKU_LEAF_WIDE_FRAME_LAST;

    DrawLeaf(play, &hand, yaw, isSweeping ? DEKU_LEAF_SWEPT_SCALE : DEKU_LEAF_HELD_SCALE);
}

static void DrawDekuLeaf(Actor* actor, PlayState* play) {
    Player* player = (Player*)actor;

    if (player->stateFlags2 & PLAYER_STATE2_20000000) {
        return;
    }
    if (sIsGliding) {
        DrawCanopy(player, play);
        return;
    }
    if (IsSwinging(player)) {
        DrawLeafInHand(player, play);
    }
}

// The scene rebuilds the player, and the collider still points at the old one until the next swing sets it.
static void DropGlideOnSceneChange(s16 sceneId, s8 spawnNum) {
    StopGliding();
    sGustLife = 0;
    sPreviousInvincibility = 0;
}

// Shuffled once by the randomizer; no vanilla logic needs it.
static const S2HCustomItemRandomizer sRandomizerItem = {
    sizeof(S2HCustomItemRandomizer), 0, S2H_CUSTOM_ITEM_TYPE_ITEM, 0, 1, DEKU_LEAF_KEY, DEKU_LEAF_KEY, "",
};

S2H_MOD_EXPORT void ModSetApi(const S2HModApi* api) {
    sApi = api;
}

S2H_MOD_EXPORT const S2HModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

S2H_MOD_EXPORT void ModInit(void) {
    if (!S2H_MOD_API_HAS(sApi, RegisterCustomItem)) {
        return;
    }

    S2HCustomItemDefinition leaf = Z64Items_Define(DEKU_LEAF_KEY, sIconTex, sNameTex);

    Z64Items_SetButtons(&leaf, S2H_CUSTOM_ITEM_C_BUTTON | S2H_CUSTOM_ITEM_DPAD);
    Z64Items_SetPlacement(&leaf, 0, 6, 0);
    Z64Items_SetForms(&leaf, S2H_CUSTOM_ITEM_FORM_BIT(PLAYER_FORM_HUMAN));
    Z64Items_SetTextbox(&leaf, "You got the %rDeku Leaf%w!&A leaf of the Great Deku Tree,&wide enough to ride the "
                               "wind.^"
                               "On the ground, a %yC button%w beats it into a&%ggust%w that stuns what stands&ahead "
                               "and hurls it away.^"
                               "In the air, hold it to open the leaf&and %gglide%w. Every moment aloft&spends a "
                               "little %gmagic%w.");
    Z64Items_SetPauseText(&leaf, "%rDeku Leaf&%wPress a %yC button%w to fan a gust. Hold it&in midair to glide.");
    Z64Items_SetCanUse(&leaf, CanSwingLeaf);
    leaf.flags |= S2H_CUSTOM_ITEM_INSTANT;
    Z64Items_SetGetItemModel(&leaf, sLeafDL);
    leaf.getItemScale = 0.26666667f;

    Z64Items_SetLogic(&leaf, &sRandomizerItem);
    if (!Z64Items_Register(sApi, &leaf)) {
        return;
    }
    S2H_REGISTER_HOOK(sApi, OnPlayerUpdate, UpdateDekuLeaf);
    S2H_REGISTER_HOOK(sApi, OnSceneInit, DropGlideOnSceneChange);
    S2H_REGISTER_HOOK_FOR_ID(sApi, OnActorDrawEnd, ACTOR_PLAYER, DrawDekuLeaf);
}
