/**
 * Divine Shield: a wooden shield fire cannot burn. A block inside the first frames of raising it freezes every
 * enemy in the room, the way a Deku Nut would.
 */

#include "z64items.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "objects/object_link_child/object_link_child.h"

#define DIVINE_SHIELD_KEY "nei.equip.divine_shield"
#define PARRY_WINDOW 10
#define FREEZE_FRAMES 40
#define SPARKLES_PER_ENEMY 6
// The model is authored in the shield limb's own space, about 6000 units across.
#define SHIELD_SCALE 44.2f
#define SHIELD_ROT_X (-95.0f * (M_PIf / 180.0f))
#define SHIELD_ROT_Y (-27.0f * (M_PIf / 180.0f))
#define SHIELD_ROT_Z (-99.0f * (M_PIf / 180.0f))
#define SHIELD_OFF_X (-508.0f)
#define SHIELD_OFF_Y (-372.0f)
#define SHIELD_OFF_Z (-5.0f)

static const ALIGN_ASSET(2) char sIcon[] = "__OTR__textures/icon_item_custom/gItemIconGoddessShieldTex";
static const ALIGN_ASSET(2) char sName[] = "__OTR__textures/item_name_custom/gGoddessShieldNameTex";
static const ALIGN_ASSET(2) char sShieldDL[] = "__OTR__objects/object_nei_divine_shield/g_divine_shield_dl";

static const S2HModApi* sApi;

static struct {
    s16 raiseTimer;
    bool wasShielding;
    s8 hiddenVanillaShield;
} sDivine;

static bool IsWorn(void) {
    return CustomEquipRegistry_IsWorn(DIVINE_SHIELD_KEY);
}

static bool IsShieldOnBack(Player* player) {
    return player->sheathType == PLAYER_MODELTYPE_SHEATH_14 || player->sheathType == PLAYER_MODELTYPE_SHEATH_15;
}

static bool IsShieldInHand(Player* player) {
    return player->rightHandType == PLAYER_MODELTYPE_RH_SHIELD;
}

static void TrackRaise(Player* player) {
    PlayState* play = gPlayState;
    bool isShielding;

    if (play == NULL || player != GET_PLAYER(play) || !IsWorn()) {
        return;
    }
    isShielding = (player->stateFlags1 & PLAYER_STATE1_400000) != 0;
    if (isShielding && !sDivine.wasShielding) {
        sDivine.raiseTimer = 0;
    }
    sDivine.wasShielding = isShielding;
    if (isShielding) {
        sDivine.raiseTimer++;
    }
}

static void FreezeRoom(Player* player, PlayState* play) {
    Color_RGBA8 prim = { 200, 220, 255, 255 };
    Color_RGBA8 env = { 100, 150, 255, 0 };
    Vec3f accel = { 0.0f, 0.0f, 0.0f };

    for (Actor* enemy = play->actorCtx.actorLists[ACTORCAT_ENEMY].first; enemy != NULL; enemy = enemy->next) {
        enemy->freezeTimer = FREEZE_FRAMES;
        Actor_SetColorFilter(enemy, COLORFILTER_COLORFLAG_BLUE, 0xF8, COLORFILTER_BUFFLAG_OPA, FREEZE_FRAMES);
        for (s32 i = 0; i < SPARKLES_PER_ENEMY; i++) {
            Vec3f pos = { enemy->world.pos.x + Rand_CenteredFloat(60.0f),
                          enemy->world.pos.y + 20.0f + Rand_ZeroFloat(40.0f),
                          enemy->world.pos.z + Rand_CenteredFloat(60.0f) };
            Vec3f vel = { Rand_CenteredFloat(3.0f), Rand_ZeroFloat(2.0f) + 1.0f, Rand_CenteredFloat(3.0f) };

            EffectSsKirakira_SpawnSmall(play, &pos, &vel, &accel, &prim, &env);
        }
    }
    Audio_PlaySfx_AtPos(&player->actor.world.pos, NA_SE_IT_SHIELD_REFLECT_SW);
}

static void ParryOnBlock(PlayState* play, Player* player, Actor* attacker) {
    if (IsWorn() && attacker != NULL && attacker != &player->actor && sDivine.raiseTimer <= PARRY_WINDOW) {
        FreezeRoom(player, play);
    }
}

// The hand model carries the shield, so it is swapped for the bare fist. The back shield is drawn by the sheath
// limb's post draw from currentShield, so that is cleared until DrawOnLimb restores it.
static void HideVanillaShield(Player* player, int32_t limbIndex, Gfx** dList, Gfx* limbDList, Vec3f* pos) {
    if (!IsWorn() || player->transformation != PLAYER_FORM_HUMAN || player->currentShield == PLAYER_SHIELD_NONE) {
        return;
    }
    if (limbIndex == PLAYER_LIMB_RIGHT_HAND && IsShieldInHand(player)) {
        *dList = (Gfx*)gLinkHumanRightHandClosedDL;
    } else if (limbIndex == PLAYER_LIMB_SHEATH && IsShieldOnBack(player)) {
        sDivine.hiddenVanillaShield = player->currentShield;
        player->currentShield = PLAYER_SHIELD_NONE;
    }
}

// On XLU on purpose: the model leaves its combiner on the pipe, and on OPA that blacks out every limb after it.
static void DrawShield(PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    Matrix_Translate(SHIELD_OFF_X, SHIELD_OFF_Y, SHIELD_OFF_Z, MTXMODE_APPLY);
    Matrix_RotateXF(SHIELD_ROT_X, MTXMODE_APPLY);
    Matrix_RotateYF(SHIELD_ROT_Y, MTXMODE_APPLY);
    Matrix_RotateZF(SHIELD_ROT_Z, MTXMODE_APPLY);
    Matrix_Scale(SHIELD_SCALE, SHIELD_SCALE, SHIELD_SCALE, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sShieldDL);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

static void RestoreVanillaShield(Player* player) {
    if (sDivine.hiddenVanillaShield != PLAYER_SHIELD_NONE) {
        player->currentShield = sDivine.hiddenVanillaShield;
        sDivine.hiddenVanillaShield = PLAYER_SHIELD_NONE;
    }
}

// The vanilla base is the Hero's Shield, which collides as metal; this one is wood.
static void DrawOnLimb(Player* player, int32_t limbIndex) {
    PlayState* play = gPlayState;

    if (limbIndex == PLAYER_LIMB_SHEATH) {
        RestoreVanillaShield(player);
    }
    if (play == NULL || !IsWorn() || player != GET_PLAYER(play) || player->transformation != PLAYER_FORM_HUMAN ||
        player->currentShield == PLAYER_SHIELD_NONE) {
        return;
    }
    if (limbIndex == PLAYER_LIMB_RIGHT_HAND && IsShieldInHand(player)) {
        player->shieldQuad.base.colMaterial = COL_MATERIAL_WOOD;
        DrawShield(play);
    } else if (limbIndex == PLAYER_LIMB_SHEATH && IsShieldOnBack(player) && !IsShieldInHand(player) &&
               player->rightHandType != PLAYER_MODELTYPE_RH_FF) {
        DrawShield(play);
    }
}

static void ForgetRaise(const char* key) {
    sDivine.raiseTimer = 0;
    sDivine.wasShielding = false;
}

static const char* const sRequiredHooks[] = { "OnPlayerUpdate", "OnPlayerShieldBlocked", "OnPlayerResolveLimbDraw",
                                              "OnPlayerPostLimbDraw" };

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

    definition.structSize = sizeof(definition);
    definition.key = DIVINE_SHIELD_KEY;
    definition.iconPath = sIcon;
    definition.namePath = sName;
    definition.pauseText = "%rDivine Shield&%wBlock with %y\xA3%w right as you raise it and every enemy nearby freezes. "
                           "Fire cannot burn it.";
    definition.getItemText = "You got the %rDivine Shield%w!&Wood blessed by the goddess: flames will not take it, and "
                             "a guard raised just in time freezes whatever dares to strike it.";
    definition.slot = S2H_EQUIP_SLOT_SHIELD;
    definition.page = 0;
    definition.row = S2H_EQUIP_SLOT_SHIELD;
    definition.column = 1;
    definition.formMask = S2H_CUSTOM_ITEM_FORM_BIT(PLAYER_FORM_HUMAN);
    definition.vanillaBase = PLAYER_SHIELD_HEROS_SHIELD;
    definition.toggles = 1;
    definition.onUnequip = ForgetRaise;
    definition.getItemModelPath = sShieldDL;
    if (!CustomEquipRegistry_Register(&definition)) {
        return;
    }

    S2H_REGISTER_HOOK(sApi, OnPlayerUpdate, TrackRaise);
    S2H_REGISTER_HOOK(sApi, OnPlayerShieldBlocked, ParryOnBlock);
    S2H_REGISTER_HOOK(sApi, OnPlayerResolveLimbDraw, HideVanillaShield);
    S2H_REGISTER_HOOK(sApi, OnPlayerPostLimbDraw, DrawOnLimb);
}
