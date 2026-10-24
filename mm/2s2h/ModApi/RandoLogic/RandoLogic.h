#ifndef S2H_MOD_API_RANDO_LOGIC_H
#define S2H_MOD_API_RANDO_LOGIC_H

#include <stdbool.h>
#include <stdint.h>

// X(name, macro): the macro is the token of Rando/Logic/Logic.h that capability selectors look for in a condition.
#define RANDO_LOGIC_CAPABILITIES(X)                      \
    X(BlastOrSmash, "CAN_USE_EXPLOSIVE")                 \
    X(HasExplosives, "CAN_USE_EXPLOSIVE")                \
    X(HasFireSource, "CAN_USE_MAGIC_ARROW(FIRE)")        \
    X(HasFireSourceWithTorch, "CAN_LIGHT_TORCH_NEAR_ANOTHER") \
    X(CanHitSwitch, "")                                  \
    X(CanUseSword, "CAN_USE_SWORD")                      \
    X(CanUseHumanSword, "CAN_USE_HUMAN_SWORD")           \
    X(CanAttack, "")                                     \
    X(CanDamage, "")                                     \
    X(CanShield, "EQUIP_TYPE_SHIELD")                    \
    X(CanUseProjectile, "CAN_USE_PROJECTILE")            \
    X(HookshotOrBoomerang, "ITEM_HOOKSHOT")              \
    X(SummonEpona, "CAN_RIDE_EPONA")                     \
    X(ScarecrowsSong, "CAN_HOOK_SCARECROW")              \
    X(CanOpenUnderwaterChest, "CAN_USE_ABILITY(SWIM)")   \
    X(HasMagic, "HAS_MAGIC")                             \
    X(CanBeHuman, "CAN_BE_HUMAN")                        \
    X(CanBeDeku, "CAN_BE_DEKU")                          \
    X(CanBeGoron, "CAN_BE_GORON")                        \
    X(CanBeZora, "CAN_BE_ZORA")                          \
    X(CanBeFierceDeity, "CAN_BE_DEITY")

typedef enum {
#define RANDO_LOGIC_CAPABILITY_ID(name, token) RLC_##name,
    RANDO_LOGIC_CAPABILITIES(RANDO_LOGIC_CAPABILITY_ID)
#undef RANDO_LOGIC_CAPABILITY_ID
        RLC_MAX,
} RandoLogicCapability;

typedef enum {
    S2H_LOGIC_ALLOW,
    S2H_LOGIC_REQUIRE,
    S2H_LOGIC_DENY,
    S2H_LOGIC_REPLACE,
} S2HLogicEffect;

#define S2H_LOGIC_TARGET_LOCATIONS (1 << 0)
#define S2H_LOGIC_TARGET_ENTRANCES (1 << 1)
#define S2H_LOGIC_TARGET_EVENTS (1 << 2)
#define S2H_LOGIC_TARGET_ALL 0
#define S2H_LOGIC_ANY (-1)

// checkType is a RandoCheckType, region and entranceTo are RandoRegionIds, scene is a SceneId, logicEvent is a
// RandoEvent and checks are RandoCheckIds. Events carry no condition text, so only the other fields can select them.
typedef struct {
    uint32_t structSize;
    uint32_t targets;
    const char* capability;
    const char* conditionText;
    int32_t checkType;
    int32_t region;
    int32_t scene;
    int32_t entranceTo;
    int32_t logicEvent;
    const int32_t* checks;
    uint32_t checkCount;
} S2HLogicSelector;

typedef bool (*S2HLogicCondition)(void* userData);

typedef struct {
    uint32_t structSize;
    const char* name;
    S2HLogicEffect effect;
    S2HLogicCondition condition;
    void* userData;
    const S2HLogicSelector* include;
    uint32_t includeCount;
    const S2HLogicSelector* exclude;
    uint32_t excludeCount;
} S2HLogicRule;

typedef enum {
    S2H_LOGIC_GRANT_CAPABILITY,
    S2H_LOGIC_GRANT_ITEM,
    S2H_LOGIC_GRANT_ENEMY_KILL,
} S2HLogicGrantKind;

// While condition holds, a capability, a vanilla item (value = RandoItemId, for HasItem and CanUse) or the kill
// of an enemy (value = ActorId) counts as had. include/exclude scope it like a rule's selectors; no
// include means everywhere. A check's scope knows its scene and type, not its region.
typedef struct {
    uint32_t structSize;
    const char* owner;
    S2HLogicGrantKind kind;
    const char* capability;
    int32_t value;
    S2HLogicCondition condition;
    void* userData;
    const S2HLogicSelector* include;
    uint32_t includeCount;
    const S2HLogicSelector* exclude;
    uint32_t excludeCount;
} S2HLogicGrant;

#ifdef __cplusplus
extern "C" {
#endif

bool RandoLogic_Grant(const S2HLogicGrant* grant);
bool RandoLogic_IsItemGranted(int32_t item);
bool RandoLogic_IsNativeItemGranted(uint16_t item);
bool RandoLogic_IsEnemyGranted(int32_t enemy);
bool RandoLogic_GrantCapability(const char* owner, const char* capability, S2HLogicCondition condition, void* userData);
bool RandoLogic_RegisterRule(const S2HLogicRule* rule);
void RandoLogic_RemoveOwner(const char* name);

bool RandoLogic_CanUseItem(int32_t randoItem);
bool RandoLogic_HasLogicItem(int32_t randoItem);
// MM has no ages: Child is "Link can be Human", Adult is "Link can be any transformed form".
bool RandoLogic_IsChild(void);
bool RandoLogic_IsAdult(void);
bool RandoLogic_IsAtDay(void);
bool RandoLogic_IsAtNight(void);
bool RandoLogic_HasCapability(const char* capability);

extern bool gRandoLogicHasCapabilityGrants;
bool RandoLogic_IsCapabilityGranted(RandoLogicCapability capability);
bool RandoLogic_ResolveLocation(int32_t check, bool vanillaResult);
bool RandoLogic_ResolveEntrance(int32_t fromRegion, int32_t toRegion, bool vanillaResult);
bool RandoLogic_ResolveEvent(int32_t logicEvent, bool vanillaResult);

#ifdef __cplusplus
}

// Everything the mod API changes while a seed is generated, undone when the scope ends: the world's conditions
// are wrapped with the registered rules, and mod item ownership and options switch to their generation state.
class RandoSeedGenerationScope {
  public:
    RandoSeedGenerationScope();
    ~RandoSeedGenerationScope();
    RandoSeedGenerationScope(const RandoSeedGenerationScope&) = delete;
    RandoSeedGenerationScope& operator=(const RandoSeedGenerationScope&) = delete;
};
#endif

#define LOGIC_MOD_GRANTS(name)                                                          \
    if (gRandoLogicHasCapabilityGrants && RandoLogic_IsCapabilityGranted(RLC_##name)) { \
        return true;                                                                    \
    }

#define LOGIC_GRANTED(name) (gRandoLogicHasCapabilityGrants && RandoLogic_IsCapabilityGranted(RLC_##name))

#endif
