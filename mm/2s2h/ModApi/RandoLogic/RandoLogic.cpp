#include "2s2h/ModApi/ProgressiveItems/ProgressiveItems.h"
#include "2s2h/ModApi/RandoLogic/RandoLogic.h"

#include <algorithm>
#include <array>
#include <functional>
#include <list>
#include <string>
#include <unordered_map>
#include <vector>

#include <spdlog/spdlog.h>

#include "2s2h/ModApi/CustomItemRegistry/CustomItemRegistry.h"
#include "2s2h/ModApi/RandoInfCustom/RandoInfCustom.h"
#include "2s2h/ModApi/RandoItems/RandoItems.h"
#include "2s2h/ModApi/RandoOptions/RandoOptions.h"
#include "2s2h/Rando/Logic/Logic.h"

bool gRandoLogicHasCapabilityGrants = false;

namespace {

using Condition = std::function<bool()>;

const char* const kCapabilityNames[RLC_MAX] = {
#define RANDO_LOGIC_CAPABILITY_NAME(name, token) #name,
    RANDO_LOGIC_CAPABILITIES(RANDO_LOGIC_CAPABILITY_NAME)
#undef RANDO_LOGIC_CAPABILITY_NAME
};

const char* const kCapabilityTokens[RLC_MAX] = {
#define RANDO_LOGIC_CAPABILITY_TOKEN(name, token) token,
    RANDO_LOGIC_CAPABILITIES(RANDO_LOGIC_CAPABILITY_TOKEN)
#undef RANDO_LOGIC_CAPABILITY_TOKEN
};


struct Selector {
    uint32_t targets;
    std::string capabilityToken;
    std::string conditionText;
    int32_t checkType;
    int32_t region;
    int32_t scene;
    int32_t entranceTo;
    int32_t logicEvent;
    std::vector<int32_t> checks;
};

struct Rule {
    std::string name;
    S2HLogicEffect effect;
    S2HLogicCondition condition;
    void* userData;
    std::vector<Selector> include;
    std::vector<Selector> exclude;
    bool evaluating;
};

struct WorldEntry {
    uint32_t target;
    int32_t id;
    int32_t region;
    int32_t scene;
    int32_t entranceTo;
    int32_t checkType;
    const std::string* condition;
};

struct WrappedCondition {
    Condition* target;
    Condition original;
};

const std::string kNoCondition;

struct Grant {
    std::string owner;
    S2HLogicCondition condition;
    void* userData;
    std::vector<Selector> include;
    std::vector<Selector> exclude;
    bool evaluating;
};
std::unordered_map<int32_t, std::vector<Grant>> sItemGrants, sEnemyGrants;
std::vector<WorldEntry> sEntryStack;
std::array<std::vector<Grant>, RLC_MAX> sCapabilityGrants;
std::array<bool, RLC_MAX> sCapabilityEvaluating{};
std::list<Rule> sRules;

std::vector<std::vector<Rule*>> sLocationRules;
std::vector<std::vector<Rule*>> sEventRules;
std::unordered_map<uint64_t, std::vector<Rule*>> sEntranceRules;
std::vector<WrappedCondition> sWrappedConditions;

uint64_t EntranceKey(int32_t fromRegion, int32_t toRegion) {
    return (static_cast<uint64_t>(static_cast<uint32_t>(fromRegion)) << 32) | static_cast<uint32_t>(toRegion);
}

int32_t FindCapability(const char* capability) {
    if (capability == nullptr) {
        return -1;
    }
    for (int32_t index = 0; index < RLC_MAX; index++) {
        if (std::string(kCapabilityNames[index]) == capability) {
            return index;
        }
    }
    return -1;
}

Rule* FindRule(const std::string& name) {
    for (Rule& rule : sRules) {
        if (rule.name == name) {
            return &rule;
        }
    }
    return nullptr;
}

std::string FindCapabilityToken(const char* capability) {
    const int32_t index = FindCapability(capability);
    return index >= 0 ? kCapabilityTokens[index] : std::string();
}

Selector ReadSelector(const S2HLogicSelector& source) {
    Selector selector{};
    selector.targets = source.targets;
    selector.capabilityToken = FindCapabilityToken(source.capability);
    if (source.conditionText != nullptr) {
        selector.conditionText = source.conditionText;
    }
    selector.checkType = source.checkType;
    selector.region = source.region;
    selector.scene = source.scene;
    selector.entranceTo = source.entranceTo;
    selector.logicEvent = source.logicEvent;
    for (uint32_t index = 0; index < source.checkCount; index++) {
        selector.checks.push_back(source.checks[index]);
    }
    return selector;
}

bool MatchesSelector(const Selector& selector, const WorldEntry& entry) {
    if (selector.targets != S2H_LOGIC_TARGET_ALL && (selector.targets & entry.target) == 0) {
        return false;
    }
    if (selector.region >= 0 && selector.region != entry.region) {
        return false;
    }
    if (selector.scene >= 0 && selector.scene != entry.scene) {
        return false;
    }
    if (selector.entranceTo >= 0 && selector.entranceTo != entry.entranceTo) {
        return false;
    }
    if (selector.checkType >= 0 && selector.checkType != entry.checkType) {
        return false;
    }
    if (selector.logicEvent >= 0 && (entry.target != S2H_LOGIC_TARGET_EVENTS || selector.logicEvent != entry.id)) {
        return false;
    }
    if (!selector.checks.empty()) {
        if (entry.target != S2H_LOGIC_TARGET_LOCATIONS) {
            return false;
        }
        if (std::find(selector.checks.begin(), selector.checks.end(), entry.id) == selector.checks.end()) {
            return false;
        }
    }
    if (!selector.capabilityToken.empty() && entry.condition->find(selector.capabilityToken) == std::string::npos) {
        return false;
    }
    if (!selector.conditionText.empty() && entry.condition->find(selector.conditionText) == std::string::npos) {
        return false;
    }
    return true;
}

bool MatchesAny(const std::vector<Selector>& selectors, const WorldEntry& entry) {
    return std::any_of(selectors.begin(), selectors.end(),
                       [&entry](const Selector& selector) { return MatchesSelector(selector, entry); });
}

bool IsInScope(const Grant& grant) {
    if (sEntryStack.empty()) {
        return grant.include.empty();
    }
    const WorldEntry& entry = sEntryStack.back();
    if (!grant.include.empty() && !MatchesAny(grant.include, entry)) {
        return false;
    }
    return !MatchesAny(grant.exclude, entry);
}

// A grant whose condition asks for what it grants answers false to itself instead of recursing.
bool AnyGrantHolds(std::vector<Grant>& grants) {
    for (Grant& grant : grants) {
        if (grant.evaluating || !IsInScope(grant)) {
            continue;
        }
        grant.evaluating = true;
        bool holds = grant.condition(grant.userData);
        grant.evaluating = false;
        if (holds) {
            return true;
        }
    }
    return false;
}

bool AnyMappedGrantHolds(std::unordered_map<int32_t, std::vector<Grant>>& grants, int32_t key) {
    auto found = grants.find(key);
    return found != grants.end() && AnyGrantHolds(found->second);
}

Grant ReadGrant(const S2HLogicGrant& source) {
    Grant grant{};
    grant.owner = source.owner;
    grant.condition = source.condition;
    grant.userData = source.userData;
    for (uint32_t index = 0; index < source.includeCount; index++) {
        grant.include.push_back(ReadSelector(source.include[index]));
    }
    for (uint32_t index = 0; index < source.excludeCount; index++) {
        grant.exclude.push_back(ReadSelector(source.exclude[index]));
    }
    return grant;
}

void RemoveOwnerGrants(std::vector<Grant>& grants, const std::string& owner) {
    grants.erase(
        std::remove_if(grants.begin(), grants.end(), [&owner](const Grant& grant) { return grant.owner == owner; }),
        grants.end());
}

class EntryScope {
  public:
    explicit EntryScope(const WorldEntry& entry) {
        sEntryStack.push_back(entry);
    }
    ~EntryScope() {
        sEntryStack.pop_back();
    }
};

bool SelectsEntry(const Rule& rule, const WorldEntry& entry) {
    return MatchesAny(rule.include, entry) && !MatchesAny(rule.exclude, entry);
}

void AddRuleToBucket(std::vector<Rule*>& bucket, Rule* rule) {
    if (bucket.empty() || bucket.back() != rule) {
        bucket.push_back(rule);
    }
}

int32_t GetCheckType(RandoCheckId check) {
    const auto found = Rando::StaticData::Checks.find(check);
    return found != Rando::StaticData::Checks.end() ? found->second.randoCheckType : -1;
}

void IndexLocations(Rule& rule, Rando::Logic::RandoRegion& region, WorldEntry entry) {
    entry.target = S2H_LOGIC_TARGET_LOCATIONS;
    entry.entranceTo = -1;
    for (auto& [check, logic] : region.checks) {
        entry.id = check;
        entry.checkType = GetCheckType(check);
        entry.condition = &logic.second;
        if (SelectsEntry(rule, entry)) {
            AddRuleToBucket(sLocationRules[check], &rule);
        }
    }
}

void IndexEvents(Rule& rule, Rando::Logic::RandoRegion& region, WorldEntry entry) {
    entry.target = S2H_LOGIC_TARGET_EVENTS;
    entry.entranceTo = -1;
    entry.checkType = -1;
    entry.condition = &kNoCondition;
    for (auto& event : region.events) {
        entry.id = event.first;
        if (SelectsEntry(rule, entry)) {
            AddRuleToBucket(sEventRules[event.first], &rule);
        }
    }
}

void IndexEntrance(Rule& rule, RandoRegionId from, RandoRegionId to, WorldEntry entry) {
    entry.entranceTo = to;
    if (SelectsEntry(rule, entry)) {
        AddRuleToBucket(sEntranceRules[EntranceKey(from, to)], &rule);
    }
}

void IndexEntrances(Rule& rule, RandoRegionId regionId, Rando::Logic::RandoRegion& region, WorldEntry entry) {
    entry.target = S2H_LOGIC_TARGET_ENTRANCES;
    entry.id = -1;
    entry.checkType = -1;
    for (auto& [connectedRegionId, logic] : region.connections) {
        entry.condition = &logic.second;
        IndexEntrance(rule, regionId, connectedRegionId, entry);
    }
    for (auto& [exitId, regionExit] : region.exits) {
        const RandoRegionId connectedRegionId = Rando::Logic::GetRegionIdFromEntrance(exitId);
        if (connectedRegionId == RR_MAX) {
            continue;
        }
        entry.condition = &regionExit.conditionString;
        IndexEntrance(rule, regionId, connectedRegionId, entry);
    }
}

void IndexWorld() {
    sLocationRules.assign(RC_MAX, {});
    sEventRules.assign(RE_MAX, {});
    sEntranceRules.clear();
    for (Rule& rule : sRules) {
        for (auto& [regionId, region] : Rando::Logic::Regions) {
            WorldEntry entry{};
            entry.region = regionId;
            entry.scene = region.sceneId;
            IndexLocations(rule, region, entry);
            IndexEvents(rule, region, entry);
            IndexEntrances(rule, regionId, region, entry);
        }
    }
}

void WrapCondition(Condition& condition, const WorldEntry& entry, std::function<bool(bool)> resolve) {
    sWrappedConditions.push_back({&condition, condition});
    const Condition original = condition;
    condition = [original, entry, resolve]() {
        EntryScope scope(entry);
        return resolve(original());
    };
}
void WrapWorld() {
    for (auto& [regionId, region] : Rando::Logic::Regions) {
        for (auto& [check, logic] : region.checks) {
            WorldEntry entry{S2H_LOGIC_TARGET_LOCATIONS, check, regionId, region.sceneId, -1, GetCheckType(check), &logic.second};
            WrapCondition(logic.first, entry, [check](bool result) { return RandoLogic_ResolveLocation(check, result); });
        }
        for (auto& event : region.events) {
            const auto id = event.first;
            WorldEntry entry{S2H_LOGIC_TARGET_EVENTS, id, regionId, region.sceneId, -1, -1, &kNoCondition};
            WrapCondition(event.second, entry, [id](bool result) { return RandoLogic_ResolveEvent(id, result); });
        }
        for (auto& [to, logic] : region.connections) {
            WorldEntry entry{S2H_LOGIC_TARGET_ENTRANCES, -1, regionId, region.sceneId, to, -1, &logic.second};
            WrapCondition(logic.first, entry, [regionId, to](bool result) { return RandoLogic_ResolveEntrance(regionId, to, result); });
        }
        for (auto& [exit, regionExit] : region.exits) {
            const auto to = Rando::Logic::GetRegionIdFromEntrance(exit);
            if (to == RR_MAX) continue;
            WorldEntry entry{S2H_LOGIC_TARGET_ENTRANCES, -1, regionId, region.sceneId, to, -1, &regionExit.conditionString};
            WrapCondition(regionExit.condition, entry, [regionId, to](bool result) { return RandoLogic_ResolveEntrance(regionId, to, result); });
        }
    }
}

void UnwrapWorld() {
    for (WrappedCondition& wrapped : sWrappedConditions) {
        *wrapped.target = wrapped.original;
    }
    sWrappedConditions.clear();
    sLocationRules.clear();
    sEventRules.clear();
    sEntranceRules.clear();
}

bool HoldsCondition(Rule& rule) {
    if (rule.condition == nullptr) {
        return true;
    }
    if (rule.evaluating) {
        return false;
    }
    rule.evaluating = true;
    const bool holds = rule.condition(rule.userData);
    rule.evaluating = false;
    return holds;
}

bool ApplyRules(const std::vector<Rule*>& rules, bool vanillaResult) {
    bool result = vanillaResult;
    for (Rule* rule : rules) {
        switch (rule->effect) {
            case S2H_LOGIC_ALLOW:
                result = result || HoldsCondition(*rule);
                break;
            case S2H_LOGIC_REQUIRE:
                result = result && HoldsCondition(*rule);
                break;
            case S2H_LOGIC_DENY:
                result = false;
                break;
            case S2H_LOGIC_REPLACE:
                result = HoldsCondition(*rule);
                break;
            default:
                SPDLOG_ERROR("Logic rule {} has unknown effect {}", rule->name, static_cast<int>(rule->effect));
                break;
        }
    }
    return result;
}

bool CanBecomeForm(PlayerTransformation form) {
    using namespace Rando::Logic;
    switch (form) {
        case PLAYER_FORM_HUMAN:
            return CAN_BE_HUMAN;
        case PLAYER_FORM_DEKU:
            return CAN_BE_DEKU;
        case PLAYER_FORM_GORON:
            return CAN_BE_GORON;
        case PLAYER_FORM_ZORA:
            return CAN_BE_ZORA;
        case PLAYER_FORM_FIERCE_DEITY:
            return CAN_BE_DEITY;
        default:
            return false;
    }
}

bool CanAnyFormUseModItem(int32_t randoItem) {
    const S2HCustomItemDefinition* definition = CustomItemRegistry_Find(RandoItems_GetKey(randoItem));
    if (definition == nullptr) {
        return false;
    }
    if (definition->formMask == 0) {
        return true;
    }
    for (int32_t form = 0; form < PLAYER_FORM_MAX; form++) {
        const bool isAllowed = (definition->formMask & S2H_CUSTOM_ITEM_FORM_BIT(form)) != 0;
        if (isAllowed && CanBecomeForm(static_cast<PlayerTransformation>(form))) {
            return true;
        }
    }
    return false;
}

bool EvaluateCapability(RandoLogicCapability capability) {
    using namespace Rando::Logic;
    switch (capability) {
        case RLC_BlastOrSmash:
            return CAN_USE_EXPLOSIVE || CAN_BE_GORON;
        case RLC_HasExplosives:
            return CAN_USE_EXPLOSIVE;
        case RLC_HasFireSource:
            return CAN_USE_MAGIC_ARROW(FIRE);
        case RLC_HasFireSourceWithTorch:
            return CAN_LIGHT_TORCH_NEAR_ANOTHER;
        case RLC_CanHitSwitch:
            return CAN_USE_SWORD || CAN_USE_PROJECTILE || CAN_USE_EXPLOSIVE || CAN_BE_GORON;
        case RLC_CanUseSword:
            return CAN_USE_SWORD;
        case RLC_CanUseHumanSword:
            return CAN_USE_HUMAN_SWORD;
        case RLC_CanAttack:
            return CAN_USE_SWORD || CAN_USE_PROJECTILE || CAN_USE_EXPLOSIVE || CAN_BE_GORON || CAN_BE_ZORA ||
                   CAN_BE_DEKU;
        case RLC_CanDamage:
            return EvaluateCapability(RLC_CanAttack) || HAS_ITEM(ITEM_DEKU_STICK);
        case RLC_CanShield:
            return GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD) > EQUIP_VALUE_SHIELD_NONE;
        case RLC_CanUseProjectile:
            return CAN_USE_PROJECTILE;
        case RLC_HookshotOrBoomerang:
            return HAS_ITEM(ITEM_HOOKSHOT) || CAN_BE_ZORA;
        case RLC_SummonEpona:
            return CAN_RIDE_EPONA;
        case RLC_ScarecrowsSong:
            return HAS_ITEM(ITEM_OCARINA_OF_TIME) && canPlaySong(OCARINA_SONG_SCARECROW_SPAWN);
        case RLC_CanOpenUnderwaterChest:
            return CAN_BE_ZORA && CAN_USE_ABILITY(SWIM);
        case RLC_HasMagic:
            return HAS_MAGIC;
        case RLC_CanBeHuman:
            return CAN_BE_HUMAN;
        case RLC_CanBeDeku:
            return CAN_BE_DEKU;
        case RLC_CanBeGoron:
            return CAN_BE_GORON;
        case RLC_CanBeZora:
            return CAN_BE_ZORA;
        case RLC_CanBeFierceDeity:
            return CAN_BE_DEITY;
        default:
            SPDLOG_ERROR("Unknown logic capability {}", static_cast<int>(capability));
            return false;
    }
}

} // namespace

bool RandoLogic_GrantCapability(const char* owner, const char* capability, S2HLogicCondition condition,
                                void* userData) {
    const int32_t index = FindCapability(capability);
    if (index < 0 || owner == nullptr || condition == nullptr) {
        SPDLOG_ERROR("Rejected a grant of logic capability {}", capability != nullptr ? capability : "(null)");
        return false;
    }
    sCapabilityGrants[index].push_back({ owner, condition, userData });
    gRandoLogicHasCapabilityGrants = true;
    return true;
}

bool RandoLogic_Grant(const S2HLogicGrant* definition) {
    if (!definition || definition->structSize < sizeof(*definition) || !definition->owner || !definition->condition ||
        definition->includeCount > 4096 || definition->excludeCount > 4096 ||
        (definition->includeCount && !definition->include) || (definition->excludeCount && !definition->exclude)) return false;
    for (uint32_t i = 0; i < definition->includeCount + definition->excludeCount; ++i) {
        const auto& selector = i < definition->includeCount ? definition->include[i] : definition->exclude[i - definition->includeCount];
        if (selector.structSize < sizeof(selector) || selector.checkCount > 4096 || (selector.checkCount && !selector.checks)) return false;
    }
    switch (definition->kind) {
        case S2H_LOGIC_GRANT_CAPABILITY: {
            int32_t index = FindCapability(definition->capability);
            if (index < 0) return false;
            sCapabilityGrants[index].push_back(ReadGrant(*definition));
            gRandoLogicHasCapabilityGrants = true;
            return true;
        }
        case S2H_LOGIC_GRANT_ITEM:
            if (definition->value <= RI_UNKNOWN || definition->value > UINT16_MAX) return false;
            sItemGrants[definition->value].push_back(ReadGrant(*definition));
            return true;
        case S2H_LOGIC_GRANT_ENEMY_KILL:
            if (definition->value < 0 || definition->value > INT16_MAX) return false;
            sEnemyGrants[definition->value].push_back(ReadGrant(*definition));
            return true;
        default: return false;
    }
}
bool RandoLogic_IsItemGranted(int32_t item) { return AnyMappedGrantHolds(sItemGrants, item); }
bool RandoLogic_IsEnemyGranted(int32_t enemy) { return AnyMappedGrantHolds(sEnemyGrants, enemy); }
bool RandoLogic_IsNativeItemGranted(uint16_t item) {
    for (auto& [id, grants] : sItemGrants) {
        const auto found = Rando::StaticData::Items.find(static_cast<RandoItemId>(id));
        if (found != Rando::StaticData::Items.end() && found->second.itemId == item && AnyGrantHolds(grants)) return true;
    }
    return false;
}

bool RandoLogic_RegisterRule(const S2HLogicRule* definition) {
    if (definition == nullptr || definition->structSize < sizeof(S2HLogicRule) || definition->name == nullptr) {
        SPDLOG_ERROR("Rejected a logic rule with no name or an older struct");
        return false;
    }
    if (definition->includeCount == 0) {
        SPDLOG_ERROR("Logic rule {} selects nothing", definition->name);
        return false;
    }

    Rule rule{};
    rule.name = definition->name;
    rule.effect = definition->effect;
    rule.condition = definition->condition;
    rule.userData = definition->userData;
    for (uint32_t index = 0; index < definition->includeCount; index++) {
        rule.include.push_back(ReadSelector(definition->include[index]));
    }
    for (uint32_t index = 0; index < definition->excludeCount; index++) {
        rule.exclude.push_back(ReadSelector(definition->exclude[index]));
    }

    Rule* existing = FindRule(rule.name);
    if (existing != nullptr) {
        *existing = rule;
    } else {
        sRules.push_back(rule);
    }
    return true;
}

void RandoLogic_RemoveOwner(const char* name) {
    if (name == nullptr) {
        return;
    }
    const std::string owner = name;
    for (auto* map : {&sItemGrants, &sEnemyGrants}) {
        for (auto& [_, grants] : *map) std::erase_if(grants, [&owner](const Grant& grant) { return grant.owner == owner; });
    }
    sRules.remove_if([&owner](const Rule& rule) { return rule.name == owner; });
    for (std::vector<Grant>& grants : sCapabilityGrants) {
        grants.erase(std::remove_if(grants.begin(), grants.end(),
                                    [&owner](const Grant& grant) { return grant.owner == owner; }),
                     grants.end());
    }
}

bool RandoLogic_IsCapabilityGranted(RandoLogicCapability capability) {
    if (capability >= RLC_MAX || sCapabilityGrants[capability].empty() || sCapabilityEvaluating[capability]) {
        return false;
    }
    sCapabilityEvaluating[capability] = true;
    const bool granted = AnyGrantHolds(sCapabilityGrants[capability]);
    sCapabilityEvaluating[capability] = false;
    return granted;
}

bool RandoLogic_ResolveLocation(int32_t check, bool vanillaResult) {
    if (check < 0 || check >= static_cast<int32_t>(sLocationRules.size())) {
        return vanillaResult;
    }
    return ApplyRules(sLocationRules[check], vanillaResult);
}

bool RandoLogic_ResolveEntrance(int32_t fromRegion, int32_t toRegion, bool vanillaResult) {
    const auto found = sEntranceRules.find(EntranceKey(fromRegion, toRegion));
    if (found == sEntranceRules.end()) {
        return vanillaResult;
    }
    return ApplyRules(found->second, vanillaResult);
}

bool RandoLogic_ResolveEvent(int32_t logicEvent, bool vanillaResult) {
    if (logicEvent < 0 || logicEvent >= static_cast<int32_t>(sEventRules.size())) {
        return vanillaResult;
    }
    return ApplyRules(sEventRules[logicEvent], vanillaResult);
}

bool RandoLogic_HasLogicItem(int32_t randoItem) {
    if (RandoLogic_IsItemGranted(randoItem)) return true;
    bool progressive = false;
    if (ProgressiveItems_HasLogicItem(randoItem, &progressive)) return progressive;
    if (DungeonRegistry_FindItem(randoItem) >= 0) { return DungeonRegistry_HasItem(randoItem); }
    if (RandoItems_IsModItem(randoItem)) {
        return RandoItems_IsOwned(randoItem, RandoInfCustom_GetActiveBank());
    }
    const auto found = Rando::StaticData::Items.find(static_cast<RandoItemId>(randoItem));
    if (found == Rando::StaticData::Items.end() || found->second.itemId == ITEM_NONE) {
        return false;
    }
    return Item_CheckObtainability(found->second.itemId) != ITEM_NONE;
}

bool RandoLogic_CanUseItem(int32_t randoItem) {
    if (DungeonRegistry_FindItem(randoItem) >= 0) { return DungeonRegistry_HasItem(randoItem); }
    if (!RandoLogic_HasLogicItem(randoItem)) {
        return false;
    }
    if (RandoItems_IsModItem(randoItem)) {
        return CanAnyFormUseModItem(randoItem);
    }
    return CanBecomeForm(PLAYER_FORM_HUMAN);
}

bool RandoLogic_IsChild(void) {
    return CanBecomeForm(PLAYER_FORM_HUMAN);
}

bool RandoLogic_IsAdult(void) {
    return CanBecomeForm(PLAYER_FORM_DEKU) || CanBecomeForm(PLAYER_FORM_GORON) || CanBecomeForm(PLAYER_FORM_ZORA) ||
           CanBecomeForm(PLAYER_FORM_FIERCE_DEITY);
}

bool RandoLogic_IsAtDay(void) {
    using namespace Rando::Logic;
    return IS_DAY();
}

bool RandoLogic_IsAtNight(void) {
    using namespace Rando::Logic;
    return IS_NIGHT();
}

bool RandoLogic_HasCapability(const char* capability) {
    const int32_t index = FindCapability(capability);
    if (index < 0) {
        return false;
    }
    const RandoLogicCapability resolved = static_cast<RandoLogicCapability>(index);
    return RandoLogic_IsCapabilityGranted(resolved) || EvaluateCapability(resolved);
}

RandoSeedGenerationScope::RandoSeedGenerationScope() {
    ProgressiveItems_ResetLogic();
    RandoInfCustom_BeginLogicSimulation();
    RandoItems_SeedLogicBank();
    RandoOptions_BeginSeedGeneration();
    IndexWorld();
    WrapWorld();
}

RandoSeedGenerationScope::~RandoSeedGenerationScope() {
    UnwrapWorld();
    RandoOptions_EndSeedGeneration();
    RandoInfCustom_EndLogicSimulation();
    ProgressiveItems_ResetLogic();
}
