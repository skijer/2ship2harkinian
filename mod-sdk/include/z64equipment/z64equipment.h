#ifndef Z64EQUIPMENT_H
#define Z64EQUIPMENT_H

// OoT equipment behaviour for one mod's custom equipment pieces, built on MM's own systems. Include it in one
// translation unit, give pieces a role with Z64Equipment_SetRole and call Z64Equipment_Install from ModInit.
// Swords and shields need no role: their vanillaBase already makes MM treat them as its own.

#include <stdarg.h>
#include <stdbool.h>

#include "2s2h/ModApi/ModApi.h"
#include "2s2h/ModApi/CustomEquipRegistry/CustomEquipRegistry.h"
#include "2s2h/ModApi/Player/PlayerHookTypes.h"
#include "2s2h/BenPort.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"

typedef enum {
    Z64EQUIPMENT_ROLE_NONE,
    Z64EQUIPMENT_TUNIC_HEAT,   // fire hits, burning floors and lava do nothing (Goron Tunic)
    Z64EQUIPMENT_TUNIC_WATER,  // no underwater timer, the Zora's fast swim and barrier (Zora Tunic)
    Z64EQUIPMENT_BOOTS_IRON,   // heavy on land; sinks and walks the bottom; currents and ice do not move Link
    Z64EQUIPMENT_BOOTS_HOVER,  // a short glide past ledges and every floor slides (Hover Boots)
    Z64EQUIPMENT_SHIELD_WOOD,  // burns away when it blocks fire (Deku Shield)
} Z64EquipmentRole;

#define Z64EQUIPMENT_MAX_PIECES 16
// OoT's iron boots on land: a walking cap, heavy gravity and slow turns (NEI's tuning of MM's boot registers).
#define Z64EQUIPMENT_IRON_RUN_LIMIT 300
#define Z64EQUIPMENT_IRON_GRAVITY -160
#define Z64EQUIPMENT_IRON_TURN 500
// OoT's hoverBootsTimer: frames Link keeps his height after walking off a ledge.
#define Z64EQUIPMENT_HOVER_FRAMES 19
#define Z64EQUIPMENT_HOVER_RING_DL "__OTR__objects/oot_gameplay_keep/gHoverBootsCircleDL"
// MM numbers hit effects without names; 1 is fire (what func_8083456C sets Link burning on).
#define Z64EQUIPMENT_HIT_EFFECT_FIRE 1

typedef struct {
    const char* key;
    Z64EquipmentRole role;
} Z64EquipmentPiece;

static Z64EquipmentPiece sZ64EquipmentPieces[Z64EQUIPMENT_MAX_PIECES];
static u32 sZ64EquipmentPieceCount;
static bool sZ64EquipmentIronApplied;
static s16 sZ64EquipmentHoverFrame = -1;
static f32 sZ64EquipmentHoverHeight;

static inline bool Z64Equipment_SetRole(const char* key, Z64EquipmentRole role) {
    if (key == NULL || sZ64EquipmentPieceCount >= Z64EQUIPMENT_MAX_PIECES) {
        return false;
    }
    sZ64EquipmentPieces[sZ64EquipmentPieceCount].key = key;
    sZ64EquipmentPieces[sZ64EquipmentPieceCount].role = role;
    sZ64EquipmentPieceCount++;
    return true;
}

// OoT equipment is human Link's: other forms keep their own bodies.
static inline bool Z64Equipment_IsRoleWorn(Player* player, Z64EquipmentRole role) {
    if (player == NULL || player->transformation != PLAYER_FORM_HUMAN) {
        return false;
    }
    for (u32 i = 0; i < sZ64EquipmentPieceCount; i++) {
        if (sZ64EquipmentPieces[i].role == role && CustomEquipRegistry_IsWorn(sZ64EquipmentPieces[i].key)) {
            return true;
        }
    }
    return false;
}

static inline const char* Z64Equipment_FindWornKey(Player* player, Z64EquipmentRole role) {
    if (player == NULL || player->transformation != PLAYER_FORM_HUMAN) {
        return NULL;
    }
    for (u32 i = 0; i < sZ64EquipmentPieceCount; i++) {
        if (sZ64EquipmentPieces[i].role == role && CustomEquipRegistry_IsWorn(sZ64EquipmentPieces[i].key)) {
            return sZ64EquipmentPieces[i].key;
        }
    }
    return NULL;
}

static inline Z64EquipmentRole Z64Equipment_GetAbilityRole(SOHPlayerAbility ability) {
    switch (ability) {
        case SOH_PLAYER_ABILITY_HEAT_PROOF:
            return Z64EQUIPMENT_TUNIC_HEAT;
        case SOH_PLAYER_ABILITY_ZORA_SWIM:
        case SOH_PLAYER_ABILITY_WATER_BREATHING:
            return Z64EQUIPMENT_TUNIC_WATER;
        case SOH_PLAYER_ABILITY_BOTTOM_WALK:
        case SOH_PLAYER_ABILITY_ANCHORED:
            return Z64EQUIPMENT_BOOTS_IRON;
        case SOH_PLAYER_ABILITY_SLIDE:
            return Z64EQUIPMENT_BOOTS_HOVER;
        default:
            return Z64EQUIPMENT_ROLE_NONE;
    }
}

// Only ever grants: another mod's piece or the form itself may already have said yes.
static void Z64Equipment_LendFormAbility(bool* should, va_list args) {
    Player* player = va_arg(args, Player*);
    SOHPlayerAbility ability = (SOHPlayerAbility)va_arg(args, int);
    Z64EquipmentRole role = Z64Equipment_GetAbilityRole(ability);

    if (role != Z64EQUIPMENT_ROLE_NONE && Z64Equipment_IsRoleWorn(player, role)) {
        *should = true;
    }
}

// MM sinks and walks the bottom on Zora underwater boots; on land the boot registers are re-tuned each frame
// because MM rewrites them whenever it recomputes the boots.
static void Z64Equipment_UpdateIronBoots(PlayState* play, Player* player) {
    bool worn = Z64Equipment_IsRoleWorn(player, Z64EQUIPMENT_BOOTS_IRON);

    if (!worn) {
        if (sZ64EquipmentIronApplied) {
            sZ64EquipmentIronApplied = false;
            if (player->currentBoots == PLAYER_BOOTS_ZORA_UNDERWATER) {
                player->currentBoots = PLAYER_BOOTS_HYLIAN;
            }
            func_80123140(play, player);
        }
        return;
    }
    sZ64EquipmentIronApplied = true;
    bool deepWater = (player->stateFlags1 & PLAYER_STATE1_8000000) &&
                     (player->actor.depthInWater > player->ageProperties->unk_24);
    if (deepWater) {
        player->currentBoots = PLAYER_BOOTS_ZORA_UNDERWATER;
    } else if (player->currentBoots == PLAYER_BOOTS_ZORA_UNDERWATER) {
        player->currentBoots = PLAYER_BOOTS_HYLIAN;
    }
    func_80123140(play, player);
    if (player->currentBoots < PLAYER_BOOTS_ZORA_UNDERWATER) {
        R_RUN_SPEED_LIMIT = Z64EQUIPMENT_IRON_RUN_LIMIT;
        REG(68) = Z64EQUIPMENT_IRON_GRAVITY;
        REG(27) = Z64EQUIPMENT_IRON_TURN;
    }
}

// Walking off a ledge holds Link's height for Z64EQUIPMENT_HOVER_FRAMES; a jump spends the glide.
static void Z64Equipment_UpdateHoverBoots(Player* player) {
    bool grounded = (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) != 0;

    if (!Z64Equipment_IsRoleWorn(player, Z64EQUIPMENT_BOOTS_HOVER) || (player->stateFlags1 & PLAYER_STATE1_8000000)) {
        sZ64EquipmentHoverFrame = -1;
        return;
    }
    if (grounded) {
        sZ64EquipmentHoverFrame = 0;
        sZ64EquipmentHoverHeight = player->actor.world.pos.y;
        return;
    }
    bool canGlide = (sZ64EquipmentHoverFrame >= 0) && (sZ64EquipmentHoverFrame < Z64EQUIPMENT_HOVER_FRAMES) &&
                    (player->actor.velocity.y <= 0.0f) && !(player->stateFlags1 & PLAYER_STATE1_40000);
    if (!canGlide) {
        sZ64EquipmentHoverFrame = -1;
        return;
    }
    player->actor.world.pos.y = sZ64EquipmentHoverHeight;
    player->actor.velocity.y = 0.0f;
    player->fallStartHeight = (s16)sZ64EquipmentHoverHeight;
    sZ64EquipmentHoverFrame++;
}

static void Z64Equipment_OnPlayerUpdate(Player* player) {
    if (gPlayState == NULL || player != GET_PLAYER(gPlayState)) {
        return;
    }
    Z64Equipment_UpdateIronBoots(gPlayState, player);
    Z64Equipment_UpdateHoverBoots(player);
}

// OoT's Goron Tunic shrugs fire off entirely: no damage, no burning, no knockback.
static void Z64Equipment_OnResolveDamage(Actor* victim, ColliderElement* attack, f32* damage) {
    if (gPlayState == NULL || attack == NULL || victim != &GET_PLAYER(gPlayState)->actor) {
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    if (attack->atDmgInfo.effect != Z64EQUIPMENT_HIT_EFFECT_FIRE ||
        !Z64Equipment_IsRoleWorn(player, Z64EQUIPMENT_TUNIC_HEAT)) {
        return;
    }
    *damage = 0.0f;
    player->actor.colChkInfo.damage = 0;
    player->actor.colChkInfo.acHitEffect = 0;
    player->cylinder.base.acFlags &= ~AC_HIT;
}

static void Z64Equipment_OnShieldBlocked(PlayState* play, Player* player, Actor* attacker) {
    ColliderElement* hit = player->shieldQuad.elem.acHitElem;
    const char* woodenShield = Z64Equipment_FindWornKey(player, Z64EQUIPMENT_SHIELD_WOOD);

    if (woodenShield == NULL || hit == NULL || hit->atDmgInfo.effect != Z64EQUIPMENT_HIT_EFFECT_FIRE) {
        return;
    }
    CustomEquipRegistry_TakeOff(S2H_EQUIP_SLOT_SHIELD);
    CustomEquipRegistry_SetOwned(woodenShield, false);
    Actor_PlaySfx(&player->actor, NA_SE_EV_FLAME_IGNITION);
}

static void Z64Equipment_DrawHoverRing(Actor* actor, PlayState* play) {
    Player* player = GET_PLAYER(play);
    if (actor != &player->actor || sZ64EquipmentHoverFrame <= 0 ||
        !ResourceMgr_FileExists(Z64EQUIPMENT_HOVER_RING_DL)) {
        return;
    }
    u8 alpha = (u8)(255 * (Z64EQUIPMENT_HOVER_FRAMES - sZ64EquipmentHoverFrame) / Z64EQUIPMENT_HOVER_FRAMES);

    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    Matrix_Translate(player->actor.world.pos.x, player->actor.world.pos.y + 2.0f, player->actor.world.pos.z,
                     MTXMODE_NEW);
    Matrix_Scale(4.0f, 4.0f, 4.0f, MTXMODE_APPLY);
    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               (uintptr_t)Gfx_TwoTexScroll(play->state.gfxCtx, 0, 0, 0, 16, 32, 1, 0,
                                           (play->gameplayFrames * -15) % 128, 16, 32));
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gDPSetPrimColor(POLY_XLU_DISP++, 0x80, 0x80, 255, 255, 255, alpha);
    gDPSetEnvColor(POLY_XLU_DISP++, 120, 90, 30, 128);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)Z64EQUIPMENT_HOVER_RING_DL);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

static inline bool Z64Equipment_Install(const S2HModApi* api) {
    if (api == NULL) {
        return false;
    }
    api->RegisterVB(VB_PLAYER_USE_FORM_ABILITY, Z64Equipment_LendFormAbility);
    S2H_REGISTER_HOOK(api, OnPlayerUpdate, Z64Equipment_OnPlayerUpdate);
    S2H_REGISTER_HOOK(api, OnCollisionResolveDamage, Z64Equipment_OnResolveDamage);
    S2H_REGISTER_HOOK(api, OnPlayerShieldBlocked, Z64Equipment_OnShieldBlocked);
    S2H_REGISTER_HOOK_FOR_ID(api, OnActorDrawEnd, ACTOR_PLAYER, Z64Equipment_DrawHoverRing);
    return true;
}

#endif
