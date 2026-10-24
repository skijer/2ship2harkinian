#ifndef SOH_MOD_API_PLAYER_HOOK_TYPES_H
#define SOH_MOD_API_PLAYER_HOOK_TYPES_H

// Shipwright names, kept so a mod reads the same on both ports. Each action is the index of the matching
// Player_ActionHandler_* in z_player.c; the order is checked there with static asserts.
typedef enum {
    SOH_PLAYER_ACTION_FIRST_PERSON,
    SOH_PLAYER_ACTION_DOOR,
    SOH_PLAYER_ACTION_RECEIVE_ITEM,
    SOH_PLAYER_ACTION_MOUNT_HORSE,
    SOH_PLAYER_ACTION_TALK,
    SOH_PLAYER_ACTION_WALL,
    SOH_PLAYER_ACTION_ROLL,
    SOH_PLAYER_ACTION_MELEE,
    SOH_PLAYER_ACTION_SPIN_CHARGE,
    SOH_PLAYER_ACTION_THROW,
    SOH_PLAYER_ACTION_ZTARGET_A,
    SOH_PLAYER_ACTION_SHIELD,
    SOH_PLAYER_ACTION_LEDGE,
    SOH_PLAYER_ACTION_ITEM_ON_ACTOR,
    SOH_PLAYER_ACTION_ZORA_SWIM_TOGGLE,
    SOH_PLAYER_ACTION_MAX,
} SOHPlayerAction;

typedef enum {
    SOH_PLAYER_MOTION_STICK_SPEED,
    SOH_PLAYER_MOTION_SPEED_CAP,
    SOH_PLAYER_MOTION_WATER_SPEED,
} SOHPlayerMotionKind;

// What VB_PLAYER_USE_FORM_ABILITY asks: a form's trait another form or a piece of equipment may borrow.
typedef enum {
    SOH_PLAYER_ABILITY_HEAT_PROOF,      // Goron: burning floors and lava do not hurt or send back
    SOH_PLAYER_ABILITY_ZORA_SWIM,       // Zora: A boosts instead of diving, R raises the barrier
    SOH_PLAYER_ABILITY_WATER_BREATHING, // Zora: no underwater timer
    SOH_PLAYER_ABILITY_BOTTOM_WALK,     // walking the bottom with land animations, lifting and using items
    SOH_PLAYER_ABILITY_ANCHORED,        // water currents do not push and ice does not slide
    SOH_PLAYER_ABILITY_SLIDE,           // every floor slides like ice
} SOHPlayerAbility;

typedef enum {
    SOH_PLAYER_ANIM_SITE_HOP,
    SOH_PLAYER_ANIM_SITE_DAMAGE,
    SOH_PLAYER_ANIM_SITE_FIDGET,
    SOH_PLAYER_ANIM_SITE_FALL,
} SOHPlayerAnimSite;

typedef enum {
    SOH_ACTOR_SFX_VOICE,
    SOH_ACTOR_SFX_STEP,
    SOH_ACTOR_SFX_JUMP,
    SOH_ACTOR_SFX_LAND,
    SOH_ACTOR_SFX_GENERIC,
    SOH_ACTOR_SFX_FLAGGED,
} SOHActorSfxKind;

#endif
