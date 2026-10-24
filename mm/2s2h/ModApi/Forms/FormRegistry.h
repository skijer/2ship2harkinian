#ifndef S2H_MOD_API_FORM_REGISTRY_H
#define S2H_MOD_API_FORM_REGISTRY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "z64.h"

typedef enum {
    S2H_FORM_KIND_LINK,
    S2H_FORM_KIND_TAKEOVER,
} S2HFormKind;

typedef enum {
    S2H_FORM_ACTION_VANILLA,
    S2H_FORM_ACTION_BLOCKED,
    S2H_FORM_ACTION_STARTED,
} S2HFormActionResult;

typedef int32_t (*S2HFormActionFunc)(PlayState* play, Player* player);
typedef void (*S2HFormPlayerFunc)(PlayState* play, Player* player);

// action is an SOHPlayerAction (2s2h/ModApi/Player/PlayerHookTypes.h), the index of a Player_ActionHandler_*.
typedef struct {
    int32_t action;
    S2HFormActionFunc run;
} S2HFormActionOverride;

#define S2H_FORM_ANIM_TYPE_ALL (-1)

typedef struct {
    int32_t group;
    int32_t animType;
    PlayerAnimationHeader* anim;
} S2HFormAnimOverride;

typedef struct {
    uint32_t structSize;
    const char* key;
    const char* label;
    S2HFormKind kind;
    // Replaces the Human row of sPlayerAgeProperties while the form is in effect.
    const PlayerAgeProperties* body;
    const S2HFormActionOverride* actions;
    uint32_t actionCount;
    const S2HFormAnimOverride* anims;
    uint32_t animCount;
    float motionScale;
    uint16_t blockedButtons;
    S2HFormPlayerFunc onEnter;
    S2HFormPlayerFunc onExit;
    S2HFormPlayerFunc update;
    S2HFormPlayerFunc draw;
    const char* item;
    // Kept for the shared definition layout: the transformation pose is MM's own cl_setmask.
    PlayerAnimationHeader* transformAnim;
    const char* transformMask;
    const char* transformMaskClimax;
    uint16_t transformVoiceSfx;
    // Mirror root of a Link-rig model: <modelPath>/object_link_child/gLinkChildSkel or object_link_boy/gLinkAdultSkel.
    const char* modelPath;
    float rootScaleAdult;
    float rootScaleChild;
    float rootDrop;
    float height;
    // equipType is EQUIP_TYPE_SWORD or EQUIP_TYPE_SHIELD; return value to wear instead of value.
    uint16_t (*resolveEquipment)(int32_t equipType, uint16_t value);
    bool (*allowsButtonItem)(uint16_t item, const char* customKey);
    PlayerAnimationHeader* transformOffAnim;
    Vec3f transformGlowOffset;
    float transformGlowScale;
} S2HFormDefinition;

#define S2H_FORM_DEFINITION_MIN_SIZE (offsetof(S2HFormDefinition, draw) + sizeof(((S2HFormDefinition*)0)->draw))

bool FormRegistry_Register(const S2HFormDefinition* definition);
const S2HFormDefinition* FormRegistry_Find(const char* key);
uint32_t FormRegistry_GetCount(void);
const S2HFormDefinition* FormRegistry_GetAt(uint32_t index);
bool FormRegistry_SetActive(const char* key);
bool FormRegistry_Toggle(const char* key);
bool FormRegistry_ApplyActive(const char* key);
const char* FormRegistry_GetActiveKey(void);
bool FormRegistry_IsActive(const char* key);

// Player_GetHeight (z_actor.c), Human case: the form's own height, or height itself when none applies.
float FormRegistry_ResolveHeight(Player* player, float height);

#ifdef __cplusplus
}

void FormRegistry_Init();

// Same arguments as the Ship hooks OnPlayerActionHandler and OnPlayerResolveMotionScale (ID SOH_PLAYER_MOTION_STICK_SPEED).
void FormRegistry_RunActionOverride(PlayState* play, Player* player, int32_t action, bool* consumed,
                                    bool* startedAction);
void FormRegistry_ResolveMotionScale(Player* player, int32_t kind, float* scale);
#endif

#endif
