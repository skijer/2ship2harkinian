#include "BossRegistry.h"
#include "LogicExpr.h"

#include <algorithm>
#include <array>
#include <map>
#include <random>
#include <set>
#include <vector>
#include <spdlog/spdlog.h>
#include <libultraship/bridge/consolevariablebridge.h>
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ModApi/RandoLogic/RandoLogic.h"
#include "2s2h/ModApi/RandoOptions/RandoOptions.h"
#include "2s2h/Rando/Logic/Logic.h"
#include "2s2h/SaveManager/SaveSections.h"
#include "2s2h/Unbound/SceneDB.h"

namespace {
struct Slot {
    const char* name;
    s16 scene;
    s16 actor;
    const char* condition;
};
const std::array<Slot, S2H_BOSS_SLOT_MAX> slots = {{
    { "Odolwa", SCENE_MITURIN_BS, ACTOR_BOSS_01, "CanKillEnemy(ACTOR_BOSS_01)" },
    { "Goht", SCENE_HAKUGIN_BS, ACTOR_BOSS_HAKUGIN, "CanKillEnemy(ACTOR_BOSS_HAKUGIN)" },
    { "Gyorg", SCENE_SEA_BS, ACTOR_BOSS_03, "CanKillEnemy(ACTOR_BOSS_03)" },
    { "Twinmold", SCENE_INISIE_BS, ACTOR_BOSS_02, "CanKillEnemy(ACTOR_BOSS_02)" },
    { "Majora", SCENE_LAST_BS, ACTOR_BOSS_07, "CanKillEnemy(ACTOR_BOSS_07)" },
}};
struct Boss {
    std::string owner;
    std::string name;
    std::string scene;
    std::string enabledCvar;
    std::string logicText;
    LogicExpr logic;
    u32 allowed;
    s32 preferred;
    bool compiled = false;
    bool validLogic = false;
};
using Assignment = std::array<std::string, S2H_BOSS_SLOT_MAX>;
std::map<std::string, Boss> bosses;
std::string currentMod;
Assignment saveAssignment;
std::set<Actor*> reported;

bool Enabled(const Boss& boss) {
    return boss.enabledCvar.empty() || CVarGetInteger(boss.enabledCvar.c_str(), 0) != 0;
}
bool Allowed(const Boss& boss, s32 slot) {
    return slot >= 0 && slot < S2H_BOSS_SLOT_MAX && (boss.allowed & S2H_BOSS_SLOT_BIT(slot));
}
Assignment Assign(bool randomize, u32 seed) {
    Assignment result;
    std::mt19937 random(seed);
    for (const auto& [key, boss] : bosses) {
        if (!Enabled(boss) || (!boss.scene.empty() && !SceneDB_FindScene(boss.scene))) {
            continue;
        }
        std::vector<s32> available;
        for (s32 slot = 0; slot < S2H_BOSS_SLOT_MAX; slot++) {
            if (Allowed(boss, slot) && result[slot].empty()) {
                available.push_back(slot);
            }
        }
        if (available.empty()) {
            SPDLOG_WARN("[BossRegistry] '{}' has no free compatible slot", key);
            continue;
        }
        s32 selected = available.front();
        if (randomize) {
            selected = available[random() % available.size()];
        } else if (Allowed(boss, boss.preferred) && result[boss.preferred].empty()) {
            selected = boss.preferred;
        }
        result[selected] = key;
    }
    return result;
}
const Assignment& Current() {
    if (IS_RANDO) {
        return saveAssignment;
    }
    static Assignment defaults;
    defaults = Assign(false, 0);
    return defaults;
}
s32 FindSlot(s32 scene) {
    for (s32 slot = 0; slot < S2H_BOSS_SLOT_MAX; slot++) {
        if (slots[slot].scene == scene) { return slot; }
    }
    return -1;
}
Boss* Owner(s32 slot) {
    if (slot < 0 || slot >= S2H_BOSS_SLOT_MAX) { return nullptr; }
    const auto found = bosses.find(Current()[slot]);
    if (found == bosses.end() || (!found->second.scene.empty() && !SceneDB_FindScene(found->second.scene))) {
        return nullptr;
    }
    return &found->second;
}
bool Evaluate(void* data) {
    const s32 slot = static_cast<s32>(reinterpret_cast<uintptr_t>(data));
    Boss* boss = Owner(slot);
    if (boss == nullptr || boss->logicText.empty()) {
        return Rando::Logic::CanKillEnemy(static_cast<ActorId>(slots[slot].actor));
    }
    if (!boss->compiled) {
        std::string error;
        boss->validLogic = boss->logic.Compile(boss->logicText, error);
        boss->compiled = true;
        if (!boss->validLogic) {
            SPDLOG_ERROR("[BossRegistry] '{}' logic: {}", boss->name, error);
        }
    }
    return boss->validLogic && boss->logic.Evaluate();
}
nlohmann::json WriteAssignment() {
    nlohmann::json result = nlohmann::json::object();
    const auto& assignment = Current();
    for (s32 slot = 0; slot < S2H_BOSS_SLOT_MAX; slot++) {
        if (!assignment[slot].empty()) { result[slots[slot].name] = assignment[slot]; }
    }
    return result;
}
void ReadAssignment(const nlohmann::json& source) {
    saveAssignment = {};
    std::set<std::string> used;
    if (!source.is_object()) { return; }
    for (s32 slot = 0; slot < S2H_BOSS_SLOT_MAX; slot++) {
        const auto value = source.find(slots[slot].name);
        if (value == source.end() || !value->is_string()) { continue; }
        const std::string key = value->get<std::string>();
        const auto found = bosses.find(key);
        if (used.insert(key).second && (found == bosses.end() || Allowed(found->second, slot))) {
            saveAssignment[slot] = key;
        }
        if (found == bosses.end() || !Allowed(found->second, slot) ||
            (!found->second.scene.empty() && !SceneDB_FindScene(found->second.scene))) {
            SPDLOG_WARN("[BossRegistry] Saved boss '{}' for {} is unavailable; using vanilla", key, slots[slot].name);
        }
    }
}
}

extern "C" bool BossRegistry_Register(const S2HBossDefinition* definition) {
    if (definition == nullptr || definition->structSize < sizeof(S2HBossDefinition) ||
        definition->key == nullptr || definition->key[0] == '\0' ||
        (definition->allowedSlots & ((1u << S2H_BOSS_SLOT_MAX) - 1)) == 0) {
        return false;
    }
    const auto existing = bosses.find(definition->key);
    if (existing != bosses.end()) {
        SPDLOG_ERROR("[BossRegistry] Duplicate boss key '{}'", definition->key);
        return false;
    }
    Boss boss;
    boss.owner = currentMod;
    boss.name = definition->displayName ? definition->displayName : definition->key;
    boss.scene = definition->scene ? definition->scene : "";
    boss.enabledCvar = definition->enabledCvar ? definition->enabledCvar : "";
    boss.logicText = definition->logic ? definition->logic : "";
    boss.allowed = definition->allowedSlots & ((1u << S2H_BOSS_SLOT_MAX) - 1);
    boss.preferred = definition->preferredSlot;
    bosses.emplace(definition->key, std::move(boss));
    return true;
}
extern "C" int32_t BossRegistry_GetSlot(const char* key) {
    if (key == nullptr || key[0] == '\0') { return -1; }
    const auto& assignment = Current();
    for (s32 slot = 0; slot < S2H_BOSS_SLOT_MAX; slot++) {
        if (assignment[slot] == key) { return slot; }
    }
    return -1;
}
extern "C" const char* BossRegistry_GetSceneBoss(int16_t sceneId) {
    const s32 slot = FindSlot(sceneId);
    if (slot < 0 || Owner(slot) == nullptr) { return nullptr; }
    const auto found = bosses.find(Current()[slot]);
    return found == bosses.end() ? nullptr : found->first.c_str();
}
extern "C" int16_t BossRegistry_GetSlotScene(int32_t slot) {
    return slot >= 0 && slot < S2H_BOSS_SLOT_MAX ? slots[slot].scene : -1;
}
extern "C" int32_t BossRegistry_ResolveSceneSource(int32_t sceneId) {
    const Boss* boss = Owner(FindSlot(sceneId));
    if (boss != nullptr && !boss->scene.empty()) {
        const auto scene = SceneDB_FindScene(boss->scene);
        if (scene) { return *scene; }
    }
    return sceneId;
}
extern "C" void BossRegistry_ReportDefeated(Actor* boss) {
    if (boss == nullptr || gPlayState == nullptr) { return; }
    const s32 slot = FindSlot(gPlayState->sceneId);
    if (slot < 0 || Owner(slot) == nullptr || !reported.insert(boss).second) { return; }
    GameInteractor_ExecuteOnBossDefeated(slots[slot].actor);
}
void BossRegistry_SetCurrentMod(const std::string& name) {
    currentMod = name;
}
void BossRegistry_AssignForSeed(uint32_t seed) {
    saveAssignment = Assign(RandoOptions_GetForSeed("unbound.bossShuffle") != 0, seed);
}
void BossRegistry_WriteSpoiler(nlohmann::json& spoiler) {
    spoiler["unboundBosses"] = WriteAssignment();
}
void BossRegistry_ParseSpoiler(const nlohmann::json& spoiler) {
    const auto found = spoiler.find("unboundBosses");
    ReadAssignment(found == spoiler.end() ? nlohmann::json() : *found);
}
void BossRegistry_Init() {
    static const char* choices[] = { "Off", "On" };
    S2HModRandoOption option = { sizeof(option), "unbound.bossShuffle", "Mod boss shuffle",
        "Assign registered bosses to random compatible MM boss slots; Off uses their preferred slots.",
        choices, 2, 0, S2H_MOD_RANDO_WIDGET_COMBOBOX };
    RandoOptions_Register(&option);
    SaveSections_Register("unboundBosses", WriteAssignment, ReadAssignment);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>([](s16 scene, s8 spawn) { reported.clear(); });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnActorDestroy>([](Actor* actor) { reported.erase(actor); });
    for (s32 slot = 0; slot < S2H_BOSS_SLOT_MAX; slot++) {
        S2HLogicSelector selector = {};
        selector.structSize = sizeof(selector);
        selector.targets = S2H_LOGIC_TARGET_LOCATIONS | S2H_LOGIC_TARGET_ENTRANCES;
        selector.scene = slots[slot].scene;
        selector.conditionText = slots[slot].condition;
        selector.region = selector.entranceTo = selector.checkType = selector.logicEvent = S2H_LOGIC_ANY;
        const std::string name = std::string("unboundBoss:") + slots[slot].name;
        S2HLogicRule rule = {};
        rule.structSize = sizeof(rule);
        rule.name = name.c_str();
        rule.effect = S2H_LOGIC_REPLACE;
        rule.condition = Evaluate;
        rule.userData = reinterpret_cast<void*>(static_cast<uintptr_t>(slot));
        rule.include = &selector;
        rule.includeCount = 1;
        RandoLogic_RegisterRule(&rule);
    }
}
