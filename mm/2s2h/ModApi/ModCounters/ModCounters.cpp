#include "2s2h/ModApi/ModCounters/ModCounters.h"

#include <algorithm>
#include <unordered_map>

#include <spdlog/spdlog.h>

#include "2s2h/SaveManager/SaveSections.h"

namespace {

constexpr size_t SaveCounterLimit = 4096;
constexpr const char* SaveSection = "modCounters";

std::vector<ModCounter> sCounters;
std::unordered_map<std::string, int32_t> sValuesOfUnloadedMods;

ModCounter* FindCounter(const char* key) {
    if (key == nullptr) {
        return nullptr;
    }
    auto found = std::find_if(sCounters.begin(), sCounters.end(),
                              [key](const ModCounter& counter) { return counter.key == key; });
    return found != sCounters.end() ? &*found : nullptr;
}

int32_t Clamp(const ModCounter& counter, int32_t value) {
    return std::clamp(value, 0, counter.max);
}

void ResetValues() {
    for (ModCounter& counter : sCounters) {
        counter.value = 0;
    }
    sValuesOfUnloadedMods.clear();
}

void KeepValue(const std::string& key, int32_t value) {
    ModCounter* counter = FindCounter(key.c_str());
    if (counter == nullptr) {
        sValuesOfUnloadedMods[key] = value;
        return;
    }
    counter->value = Clamp(*counter, value);
}

nlohmann::json WriteSection() {
    nlohmann::json values = nlohmann::json::array();
    for (const auto& [key, value] : sValuesOfUnloadedMods) {
        values.push_back({ { "key", key }, { "value", value } });
    }
    for (const ModCounter& counter : sCounters) {
        if (counter.value != 0) {
            values.push_back({ { "key", counter.key }, { "value", counter.value } });
        }
    }
    nlohmann::json section;
    section["values"] = values;
    return section;
}

void ReadSection(const nlohmann::json& section) {
    ResetValues();
    if (!section.is_object()) {
        return;
    }
    size_t readCount = 0;
    for (const auto& entry : section.value("values", nlohmann::json::array())) {
        if (!entry.is_object() || readCount++ >= SaveCounterLimit) {
            continue;
        }
        const std::string key = entry.value("key", "");
        if (!key.empty()) {
            KeepValue(key, entry.value("value", 0));
        }
    }
}

} // namespace

bool ModCounters_Register(const char* key, int32_t max) {
    if (key == nullptr || key[0] == '\0' || max <= 0) {
        SPDLOG_ERROR("Rejected a mod counter with no key or no range");
        return false;
    }
    ModCounter* existing = FindCounter(key);
    if (existing != nullptr) {
        existing->max = max;
        existing->value = Clamp(*existing, existing->value);
        return true;
    }
    sCounters.push_back({ key, max, 0 });
    auto unloaded = sValuesOfUnloadedMods.find(key);
    if (unloaded != sValuesOfUnloadedMods.end()) {
        sCounters.back().value = Clamp(sCounters.back(), unloaded->second);
        sValuesOfUnloadedMods.erase(unloaded);
    }
    return true;
}

int32_t ModCounters_Get(const char* key) {
    const ModCounter* counter = FindCounter(key);
    return counter != nullptr ? counter->value : 0;
}

void ModCounters_Set(const char* key, int32_t value) {
    ModCounter* counter = FindCounter(key);
    if (counter == nullptr) {
        SPDLOG_ERROR("Set the unregistered mod counter {}", key != nullptr ? key : "(null)");
        return;
    }
    counter->value = Clamp(*counter, value);
}

const std::vector<ModCounter>& ModCounters_List() {
    return sCounters;
}

void ModCounters_Init() {
    SaveSections_Register(SaveSection, WriteSection, ReadSection);
}
