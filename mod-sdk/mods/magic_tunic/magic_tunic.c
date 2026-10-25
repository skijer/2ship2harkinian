/**
 * Magic Tunic: while the wallet holds rupees every hit is paid in rupees instead of hearts, and heat and water
 * stop counting down. A third of each payment spills out as real rupees. Broke, it is a heavy black shirt.
 */

#include "z64items.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"

#define MAGIC_TUNIC_KEY "nei.equip.magic_tunic"
#define RUPEE_DRAIN_INTERVAL 30
#define BROKE_SPEED_SCALE 0.5f
#define ABSORB_INTANGIBILITY 20
#define SPILL_RADIUS_MIN 8.0f
#define SPILL_RADIUS_MAX 20.0f
// Rupees collect within 50 units vertically: spawning above that window keeps them from being swallowed at once.
#define SPILL_HEIGHT_MARGIN 40.0f

static const ALIGN_ASSET(2) char sIcon[] = "__OTR__textures/icon_item_custom/gItemIconMagicTunicTex";
static const ALIGN_ASSET(2) char sName[] = "__OTR__textures/item_name_custom/gMagicTunicNameTex";

void func_80833998(Player* player, s32 invincibilityTimer);

static const S2HModApi* sApi;
static s16 sDrainTimer;

static bool IsWorn(void) {
    return CustomEquipRegistry_IsWorn(MAGIC_TUNIC_KEY);
}

// The accumulator holds what the wallet is still paying out, so it counts against the balance.
static s16 GetSpendableRupees(void) {
    return gSaveContext.save.saveInfo.playerData.rupees + gSaveContext.rupeeAccumulator;
}

static bool HasMoney(void) {
    return IsWorn() && GetSpendableRupees() > 0;
}

static bool IsPlayerBusy(PlayState* play, Player* player) {
    return (player->stateFlags1 & PLAYER_STATE1_DEAD) != 0 || Player_InCsMode(play);
}

static s16 TakeSpillDenomination(s16* spill) {
    if (*spill >= 20) {
        *spill -= 20;
        return ITEM00_RUPEE_RED;
    }
    if (*spill >= 5) {
        *spill -= 5;
        return ITEM00_RUPEE_BLUE;
    }
    *spill -= 1;
    return ITEM00_RUPEE_GREEN;
}

static void SpillRupees(PlayState* play, Player* player, s16 paid) {
    s16 spill = (paid * 3 + 9) / 10;
    f32 spawnY = player->actor.world.pos.y + Player_GetHeight(player) + SPILL_HEIGHT_MARGIN;

    while (spill > 0) {
        s16 params = TakeSpillDenomination(&spill);
        s16 angle = (s16)Rand_CenteredFloat(65536.0f);
        f32 radius = SPILL_RADIUS_MIN + Rand_ZeroOne() * (SPILL_RADIUS_MAX - SPILL_RADIUS_MIN);
        Vec3f pos = { player->actor.world.pos.x + Math_CosS(angle) * radius, spawnY,
                      player->actor.world.pos.z + Math_SinS(angle) * radius };

        Item_DropCollectible(play, &pos, params);
    }
}

static void PayForHit(PlayState* play, Player* player, s16 damage) {
    s16 paid = MIN(damage, GetSpendableRupees());

    Rupees_ChangeBy(-paid);
    Audio_PlaySfx(NA_SE_IT_SHIELD_BOUND);
    SpillRupees(play, player, paid);
}

static void FeelAbsorbedHit(Player* player) {
    func_80833998(player, ABSORB_INTANGIBILITY);
    Rumble_Request(0.0f, 180, 20, 100);
}

// Runs before Player_UpdateCommon consumes the hit: paid for, it plays like a block, with no knockback, no hurt
// animation and no voice.
static void AbsorbHit(Player* player, Input* input) {
    PlayState* play = gPlayState;

    if (play == NULL || !HasMoney()) {
        return;
    }
    if ((player->cylinder.base.acFlags & AC_HIT) && player->actor.colChkInfo.damage > 0) {
        PayForHit(play, player, player->actor.colChkInfo.damage);
        player->actor.colChkInfo.damage = 0;
        player->cylinder.base.acFlags &= ~AC_HIT;
        FeelAbsorbedHit(player);
        return;
    }
    if (player->unk_B75 != 0 && player->unk_B74 > 0) {
        PayForHit(play, player, player->unk_B74);
        player->unk_B75 = 0;
        player->unk_B74 = 0;
        FeelAbsorbedHit(player);
    }
}

// Burning floors, falls and voids take health without a collider: the loss is handed back the moment it lands.
static void RefundOtherDamage(int16_t amount) {
    PlayState* play = gPlayState;

    if (amount >= 0 || play == NULL || !HasMoney()) {
        return;
    }
    gSaveContext.save.saveInfo.playerData.health =
        MIN(gSaveContext.save.saveInfo.playerData.health - amount, gSaveContext.save.saveInfo.playerData.healthCapacity);
    PayForHit(play, GET_PLAYER(play), -amount);
}

static void DrainWallet(Player* player) {
    PlayState* play = gPlayState;

    if (play == NULL || !IsWorn() || IsPlayerBusy(play, player)) {
        return;
    }
    if (GetSpendableRupees() <= 0) {
        sDrainTimer = 0;
        return;
    }
    if (++sDrainTimer >= RUPEE_DRAIN_INTERVAL) {
        sDrainTimer = 0;
        Rupees_ChangeBy(-1);
    }
}

static void WeighDownWhenBroke(Player* player, int32_t kind, float* scale) {
    if (kind == SOH_PLAYER_MOTION_STICK_SPEED && IsWorn() && GetSpendableRupees() <= 0) {
        *scale *= BROKE_SPEED_SCALE;
    }
}

static void SkipHeatAndWater(PlayState* play, int16_t* hazard) {
    if (!HasMoney()) {
        return;
    }
    if (*hazard == PLAYER_ENV_HAZARD_HOTROOM || *hazard == PLAYER_ENV_HAZARD_UNDERWATER_FLOOR ||
        *hazard == PLAYER_ENV_HAZARD_UNDERWATER_FREE) {
        *hazard = PLAYER_ENV_HAZARD_NONE;
    }
}

static void ResetDrain(const char* key) {
    sDrainTimer = 0;
}

static void DyeTunic(bool* should, va_list args) {
    va_arg(args, void*);
    Color_RGB8* color=va_arg(args, Color_RGB8*);
    if(!IsWorn() || color==NULL)return;
    if(GetSpendableRupees()>0){color->r=235;color->g=110;color->b=20;}else{color->r=20;color->g=20;color->b=20;}
}

static const char* const sRequiredHooks[] = { "OnPlayerUpdate", "OnPlayerFilterInput", "OnPlayerHealthChange",
                                              "OnPlayerResolveMotionScale", "OnResolveEnvHazard" };

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
    definition.key = MAGIC_TUNIC_KEY;
    definition.iconPath = sIcon;
    definition.namePath = sName;
    definition.pauseText = "%rMagic Tunic&%wWhile you carry rupees, hits cost rupees instead of hearts and heat and "
                           "water stop counting down.";
    definition.getItemText = "You got the %rMagic Tunic%w!&Armor woven from the wallet itself. Every blow is paid in "
                             "rupees and some of them spill out; with an empty purse it only weighs you down.";
    definition.slot = S2H_EQUIP_SLOT_TUNIC;
    definition.page = 0;
    definition.row = S2H_EQUIP_SLOT_TUNIC;
    definition.column = 2;
    definition.formMask = S2H_CUSTOM_ITEM_FORM_BIT(PLAYER_FORM_HUMAN);
    definition.toggles = 1;
    definition.onUnequip = ResetDrain;
    if (!CustomEquipRegistry_Register(&definition)) {
        return;
    }

    sApi->RegisterVB(VB_APPLY_TUNIC_COLOR,DyeTunic);
    S2H_REGISTER_HOOK(sApi, OnPlayerUpdate, DrainWallet);
    S2H_REGISTER_HOOK(sApi, OnPlayerFilterInput, AbsorbHit);
    S2H_REGISTER_HOOK(sApi, OnPlayerHealthChange, RefundOtherDamage);
    S2H_REGISTER_HOOK(sApi, OnPlayerResolveMotionScale, WeighDownWhenBroke);
    S2H_REGISTER_HOOK(sApi, OnResolveEnvHazard, SkipHeatAndWater);
}
