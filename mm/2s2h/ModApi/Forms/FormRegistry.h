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
    PlayerAnimationHeader* transformAnim;
    const char* transformMask;
    const char* transformMaskClimax;
    uint16_t transformVoiceSfx;
    const char* modelPath;
    float rootScaleAdult;
    float rootScaleChild;
    float rootDrop;
    float height;
    uint16_t (*resolveEquipment)(int32_t equipType, uint16_t value);
    bool (*allowsButtonItem)(uint16_t item, const char* customKey);
    PlayerAnimationHeader* transformOffAnim;
    Vec3f transformGlowOffset;
    float transformGlowScale;
    bool isAdult;
} S2HFormDefinition;

#define S2H_FORM_DEFINITION_MIN_SIZE (offsetof(S2HFormDefinition, draw) + sizeof(((S2HFormDefinition*)0)->draw))

#define S2H_FORM_KEY_HUMAN "nei.human"
#define S2H_FORM_KEY_DEKU "nei.deku"
#define S2H_FORM_KEY_GORON "nei.goron"
#define S2H_FORM_KEY_ZORA "nei.zora"
#define S2H_FORM_KEY_FIERCE_DEITY "nei.fierce_deity"
#define S2H_FORM_KEY_ADULT "nei.adult"

bool FormRegistry_Register(const S2HFormDefinition* definition);
const S2HFormDefinition* FormRegistry_Find(const char* key);
uint32_t FormRegistry_GetCount(void);
const S2HFormDefinition* FormRegistry_GetAt(uint32_t index);
bool FormRegistry_SetActive(const char* key);
bool FormRegistry_Toggle(const char* key);
bool FormRegistry_ApplyActive(const char* key);
const char* FormRegistry_GetActiveKey(void);
bool FormRegistry_IsActive(const char* key);
bool FormRegistry_IsSuspended(void);
const char* FormRegistry_GetCurrentKey(void);
const char* FormRegistry_GetNativeKey(uint8_t playerForm);
bool FormRegistry_IsCurrentFormIn(const char* const* formKeys, uint32_t formKeyCount);
bool FormRegistry_IsCurrentFormAdult(void);

float FormRegistry_ResolveHeight(Player* player, float height);

#ifdef __cplusplus
}

void FormRegistry_Init();

void FormRegistry_RunActionOverride(PlayState* play, Player* player, int32_t action, bool* consumed,
                                    bool* startedAction);
void FormRegistry_ResolveMotionScale(Player* player, int32_t kind, float* scale);
#endif

#endif
