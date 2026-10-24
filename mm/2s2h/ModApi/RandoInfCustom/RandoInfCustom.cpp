#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ModApi/ModDungeons/DungeonRegistry.h"
#include "2s2h/ModApi/RandoInfCustom/RandoInfCustom.h"

#include <array>
#include <string>
#include <unordered_map>
#include <vector>

#include <spdlog/spdlog.h>

#include "2s2h/SaveManager/SaveSections.h"

namespace {

constexpr size_t SaveFlagLimit = 4096;
constexpr const char* SaveSection = "randoInfCustom";

std::vector<std::string> sKeys;
std::unordered_map<std::string, uint16_t> sKeysToFlags;
std::array<std::vector<bool>, S2H_RANDO_INF_BANK_MAX> sBanks;
std::vector<std::string> sKeysOfUnloadedMods;
S2HRandoInfBank sActiveBank = S2H_RANDO_INF_SAVE;

bool IsBankValid(S2HRandoInfBank bank) {
    return static_cast<int>(bank) >= 0 && bank < S2H_RANDO_INF_BANK_MAX;
}

bool IsFlagValid(uint16_t flag) {
    return flag != RANDO_INF_CUSTOM_NONE && flag <= sKeys.size();
}

void ClaimUnresolvedKey(const std::string& key, uint16_t flag) {
    for (size_t index = 0; index < sKeysOfUnloadedMods.size(); index++) {
        if (sKeysOfUnloadedMods[index] != key) {
            continue;
        }
        sKeysOfUnloadedMods.erase(sKeysOfUnloadedMods.begin() + index);
        sBanks[S2H_RANDO_INF_SAVE][flag - 1] = true;
        return;
    }
}

nlohmann::json WriteSection() {
    std::vector<std::string> set = sKeysOfUnloadedMods;
    for (uint16_t flag = 1; flag <= sKeys.size(); flag++) {
        if (sBanks[S2H_RANDO_INF_SAVE][flag - 1]) {
            set.push_back(sKeys[flag - 1]);
        }
    }
    nlohmann::json section;
    section["set"] = set;
    return section;
}

void ReadSection(const nlohmann::json& section) {
    RandoInfCustom_ClearBank(S2H_RANDO_INF_SAVE);
    sKeysOfUnloadedMods.clear();
    if (!section.is_object()) {
        return;
    }
    size_t readCount = 0;
    for (const auto& entry : section.value("set", nlohmann::json::array())) {
        if (!entry.is_string() || readCount++ >= SaveFlagLimit) {
            continue;
        }
        const std::string key = entry.get<std::string>();
        const uint16_t flag = RandoInfCustom_Find(key.c_str());
        if (flag == RANDO_INF_CUSTOM_NONE) {
            sKeysOfUnloadedMods.push_back(key);
            continue;
        }
        sBanks[S2H_RANDO_INF_SAVE][flag - 1] = true;
    }
}

} // namespace

uint16_t RandoInfCustom_Register(const char* key) {
    if (key == nullptr || key[0] == '\0') {
        SPDLOG_ERROR("Rejected a custom randomizer flag with no key");
        return RANDO_INF_CUSTOM_NONE;
    }
    const std::string name = key;
    const auto found = sKeysToFlags.find(name);
    if (found != sKeysToFlags.end()) {
        return found->second;
    }
    if (sKeys.size() >= UINT16_MAX - 1) {
        SPDLOG_ERROR("Ran out of custom randomizer flags registering {}", name);
        return RANDO_INF_CUSTOM_NONE;
    }

    sKeys.push_back(name);
    const uint16_t flag = static_cast<uint16_t>(sKeys.size());
    sKeysToFlags[name] = flag;
    for (std::vector<bool>& bank : sBanks) {
        bank.resize(sKeys.size(), false);
    }
    ClaimUnresolvedKey(name, flag);
    return flag;
}

uint16_t RandoInfCustom_Find(const char* key) {
    if (key == nullptr) {
        return RANDO_INF_CUSTOM_NONE;
    }
    const auto found = sKeysToFlags.find(key);
    return found != sKeysToFlags.end() ? found->second : RANDO_INF_CUSTOM_NONE;
}

const char* RandoInfCustom_GetKey(uint16_t flag) {
    return IsFlagValid(flag) ? sKeys[flag - 1].c_str() : nullptr;
}

uint16_t RandoInfCustom_GetCount(void) {
    return static_cast<uint16_t>(sKeys.size());
}

bool RandoInfCustom_Get(S2HRandoInfBank bank, uint16_t flag) {
    if (!IsBankValid(bank) || !IsFlagValid(flag)) {
        return false;
    }
    return sBanks[bank][flag - 1];
}

void RandoInfCustom_Set(S2HRandoInfBank bank, uint16_t flag, bool state) {
    if (!IsBankValid(bank) || !IsFlagValid(flag)) {
        return;
    }
    const bool changed = sBanks[bank][flag - 1] != state;
    sBanks[bank][flag - 1] = state;
    if (changed && bank == S2H_RANDO_INF_SAVE)
        GameInteractor::Instance->ExecuteHooks<GameInteractor::OnRandoInfCustomChange>(sKeys[flag - 1].c_str(), state);
}

void RandoInfCustom_ClearBank(S2HRandoInfBank bank) {
    if (!IsBankValid(bank)) {
        return;
    }
    sBanks[bank].assign(sKeys.size(), false);
}

bool RandoInfCustom_GetByKey(const char* key) {
    return RandoInfCustom_Get(sActiveBank, RandoInfCustom_Find(key));
}

void RandoInfCustom_SetByKey(const char* key, bool state) {
    RandoInfCustom_Set(sActiveBank, RandoInfCustom_Register(key), state);
}

void RandoInfCustom_Init() {
    SaveSections_Register(SaveSection, WriteSection, ReadSection);
}

void RandoInfCustom_BeginLogicSimulation() {
    DungeonRegistry_BeginSeed();
    sBanks[S2H_RANDO_INF_LOGIC] = sBanks[S2H_RANDO_INF_SAVE];
    sActiveBank = S2H_RANDO_INF_LOGIC;
}

void RandoInfCustom_EndLogicSimulation() {
    DungeonRegistry_EndSeed();
    sActiveBank = S2H_RANDO_INF_SAVE;
    RandoInfCustom_ClearBank(S2H_RANDO_INF_LOGIC);
}

S2HRandoInfBank RandoInfCustom_GetActiveBank() {
    return sActiveBank;
}

std::vector<std::string> RandoInfCustom_ListSet() {
    std::vector<std::string> keys = sKeysOfUnloadedMods;
    for (uint16_t flag = 1; flag <= sKeys.size(); ++flag) {
        if (RandoInfCustom_Get(S2H_RANDO_INF_SAVE, flag)) keys.push_back(sKeys[flag - 1]);
    }
    return keys;
}
