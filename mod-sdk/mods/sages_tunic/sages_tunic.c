/**
 * Sage's Tunic: one resistance for every medallion Link carries. Water: ice. Fire: heat and flame.
 * Light: electricity. Shadow: grabs and paralysis. Spirit: falls. Forest: wind.
 */

#include "z64items.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"

#define SAGES_TUNIC_KEY "nei.equip.sages_tunic"
#define HIT_EFFECT_FIRE 1
#define HIT_EFFECT_ICE 2
#define HIT_EFFECT_ELECTRIC 3
#define HIT_EFFECT_SHOCK 7
#define HIT_EFFECT_BURN 9
#define KNOCKBACK_SHOCK 4
#define KNOCKBACK_LARGE 3

static const ALIGN_ASSET(2) char sIcon[] = "__OTR__textures/icon_item_custom/gItemIconSagesTunicTex";
static const ALIGN_ASSET(2) char sName[] = "__OTR__textures/item_name_custom/gSagesTunicNameTex";

s32 func_808340AC(FloorType floorType);

typedef enum {
    SAGES_RESIST_ICE,
    SAGES_RESIST_FIRE,
    SAGES_RESIST_THUNDER,
    SAGES_RESIST_STUN,
    SAGES_RESIST_FALL,
    SAGES_RESIST_WIND,
    SAGES_RESIST_MAX,
} SagesResistance;

static const char* const sResistanceMedallions[SAGES_RESIST_MAX] = {
    "nei.medallion.water",  "nei.medallion.fire",   "nei.medallion.light",
    "nei.medallion.shadow", "nei.medallion.spirit", "nei.medallion.forest",
};

static const S2HModApi* sApi;
static s32 (*sVanillaGrabPlayer)(PlayState* play, Player* player);

static bool IsWorn(void) {
    return CustomEquipRegistry_IsWorn(SAGES_TUNIC_KEY);
}

static bool Resists(SagesResistance resistance) {
    return IsWorn() && sApi->IsCustomItemOwned(sResistanceMedallions[resistance]);
}

static bool IsLocalPlayer(Actor* actor) {
    return gPlayState != NULL && actor == &GET_PLAYER(gPlayState)->actor;
}

// Heat rooms count down only while the hazard reads as heat.
static void ResistHeat(PlayState* play, int16_t* hazard) {
    if (*hazard == PLAYER_ENV_HAZARD_HOTROOM && Resists(SAGES_RESIST_FIRE)) {
        *hazard = PLAYER_ENV_HAZARD_NONE;
    }
}

// Skipping the assignment keeps the previous floor type, so a burning floor is walked like any other.
static void ResistBurningFloors(bool* should, va_list args) {
    Player* player = va_arg(args, Player*);
    PlayState* play = gPlayState;
    FloorType floorType;

    if (play == NULL || !Resists(SAGES_RESIST_FIRE) || !(player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) ||
        player->actor.floorPoly == NULL) {
        return;
    }
    floorType = SurfaceType_GetFloorType(&play->colCtx, player->actor.floorPoly, player->actor.floorBgId);
    if (floorType == FLOOR_TYPE_9 || func_808340AC(floorType) >= 0) {
        *should = false;
    }
}

// The hit effect is decided in the AT pass and read by the player next update; clearing it here keeps the damage
// and drops the freeze, the shock or the flames.
static void ResistElementalHit(Actor* victim, ColliderElement* attack, float* damage) {
    u8 effect;

    if (!IsLocalPlayer(victim)) {
        return;
    }
    effect = victim->colChkInfo.acHitEffect;
    if ((effect == HIT_EFFECT_ICE && Resists(SAGES_RESIST_ICE)) ||
        ((effect == HIT_EFFECT_ELECTRIC || effect == HIT_EFFECT_SHOCK) && Resists(SAGES_RESIST_THUNDER)) ||
        ((effect == HIT_EFFECT_FIRE || effect == HIT_EFFECT_BURN) && Resists(SAGES_RESIST_FIRE))) {
        victim->colChkInfo.acHitEffect = 0;
    }
}

// An enemy shoving Link with a shock leaves the same knockback without the jolt.
static void ResistShockingKnockback(Player* player, Input* input) {
    if (player->unk_B75 == KNOCKBACK_SHOCK && Resists(SAGES_RESIST_THUNDER)) {
        player->unk_B75 = KNOCKBACK_LARGE;
    }
}

static void ResistFallDamage(bool* should, va_list args) {
    if (Resists(SAGES_RESIST_FALL)) {
        *should = false;
    }
}

static void ResistParalysis(bool* should, va_list args) {
    if (Resists(SAGES_RESIST_STUN)) {
        *should = false;
    }
}

// Like Likes, Wallmasters and the bosses that swallow Link all seize him through this one pointer.
static s32 GrabUnlessResisted(PlayState* play, Player* player) {
    if (Resists(SAGES_RESIST_STUN)) {
        return false;
    }
    return sVanillaGrabPlayer(play, player);
}

static void WrapGrabPlayer(PlayState* play) {
    if (play->grabPlayer == GrabUnlessResisted) {
        return;
    }
    sVanillaGrabPlayer = play->grabPlayer;
    play->grabPlayer = GrabUnlessResisted;
}

// The current sets the player's wind from its own update; this runs straight after it.
static void ResistCurrentWind(Actor* current) {
    PlayState* play = gPlayState;
    Player* player;

    if (play == NULL) {
        return;
    }
    player = GET_PLAYER(play);
    if (player->windSpeed == 0.0f || !Resists(SAGES_RESIST_WIND)) {
        return;
    }
    player->windSpeed = 0.0f;
}

static void TickTunic(Player* player) {
    PlayState* play = gPlayState;

    if (play == NULL) {
        return;
    }
    WrapGrabPlayer(play);
}

static void DyeTunic(bool* should, va_list args) {
    va_arg(args, void*);
    Color_RGB8* color=va_arg(args, Color_RGB8*);
    if(!IsWorn() || color==NULL)return;
    color->r=235;color->g=240;color->b=245;
}

static const char* const sRequiredHooks[] = {
    "OnPlayerUpdate", "OnPlayerFilterInput", "OnCollisionResolveDamage", "OnResolveEnvHazard", "OnActorUpdate",
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

    definition.structSize = sizeof(definition);
    definition.key = SAGES_TUNIC_KEY;
    definition.iconPath = sIcon;
    definition.namePath = sName;
    definition.pauseText = "%rSage's Tunic&%wEach medallion you carry shields you from one danger: ice, fire, "
                           "lightning, grabs, falls and wind.";
    definition.getItemText = "You got the %rSage's Tunic%w!&Woven for the Sages of the Temples. Every medallion you "
                             "hold becomes a resistance you wear, and the cloth shines with its colour when it saves "
                             "you.";
    definition.slot = S2H_EQUIP_SLOT_TUNIC;
    definition.page = 0;
    definition.row = S2H_EQUIP_SLOT_TUNIC;
    definition.column = 3;
    definition.formMask = S2H_CUSTOM_ITEM_FORM_BIT(PLAYER_FORM_HUMAN);
    definition.toggles = 1;
    if (!CustomEquipRegistry_Register(&definition)) {
        return;
    }

    sApi->RegisterVB(VB_APPLY_TUNIC_COLOR,DyeTunic);
    S2H_REGISTER_HOOK(sApi, OnPlayerUpdate, TickTunic);
    S2H_REGISTER_HOOK(sApi, OnPlayerFilterInput, ResistShockingKnockback);
    S2H_REGISTER_HOOK(sApi, OnCollisionResolveDamage, ResistElementalHit);
    S2H_REGISTER_HOOK(sApi, OnResolveEnvHazard, ResistHeat);
    S2H_REGISTER_HOOK_FOR_ID(sApi, OnActorUpdate, ACTOR_OBJ_WIND, ResistCurrentWind);
    sApi->RegisterVB(VB_SET_STATIC_FLOOR_TYPE, ResistBurningFloors);
    sApi->RegisterVB(VB_RECIEVE_FALL_DAMAGE, ResistFallDamage);
    sApi->RegisterVB(VB_LIKE_LIKE_GRAB_PLAYER, ResistParalysis);
    sApi->RegisterVB(VB_REDEAD_GIBDO_FREEZE_LINK, ResistParalysis);
}
