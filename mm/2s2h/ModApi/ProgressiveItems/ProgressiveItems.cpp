#include "ProgressiveItems.h"
#include <algorithm>
#include <array>
#include <limits>
#include <map>
#include <string>
#include <vector>
#include "2s2h/SaveManager/SaveSections.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ModApi/CustomItemRegistry/CustomItemRegistry.h"
#include "2s2h/ModApi/RandoItems/RandoItems.h"
#include "2s2h/Rando/Rando.h"
extern "C" {
#include "variables.h"
}
namespace {
struct Counter {
    uint32_t copies = 0;
    uint16_t baseline = 0;
    bool initialized = false;
};
struct Chain {
    std::string key, pickup;
    std::vector<std::string> levels;
    std::vector<S2HProgressiveAlias> aliases;
    uint16_t first = 1, seedFirst = 1;
    S2HProgressiveReadLevel read = nullptr;
    S2HProgressiveApplyLevel apply = nullptr;
    uint16_t (*readFirst)(void) = nullptr;
    std::array<Counter, 2> counters;
};
std::map<std::string, Chain> chains;
bool applying = false;
struct ApplyingScope {
    bool previous = applying;
    ApplyingScope() { applying = true; }
    ~ApplyingScope() { applying = previous; }
};
constexpr const char* section = "progressiveItems";
// MM's generator operates on a temporary gSaveContext; counters still live in separate banks.
SaveContext* BankSave(S2HRandoInfBank) { return &gSaveContext; }
uint16_t Level(const Chain& c, S2HRandoInfBank bank, SaveContext* save) {
    const auto& counter = c.counters[bank];
    uint64_t level = counter.initialized ? counter.baseline : (save ? c.read(save) : 0);
    if (counter.initialized && counter.copies)
        level = std::max<uint64_t>(level + counter.copies, static_cast<uint64_t>(c.seedFirst) + counter.copies - 1);
    if (bank == S2H_RANDO_INF_SAVE && save)
        level = std::max<uint64_t>(level, c.read(save));
    return static_cast<uint16_t>(std::min<uint64_t>(c.levels.size(), level));
}
Chain* ForKey(const char* key, uint16_t* explicitLevel = nullptr) {
    if (!key)
        return nullptr;
    for (auto& [_, c] : chains) {
        if (c.key == key || c.pickup == key) {
            if (explicitLevel)
                *explicitLevel = 0;
            return &c;
        }
        for (size_t i = 0; i < c.levels.size(); ++i)
            if (c.levels[i] == key) {
                if (explicitLevel)
                    *explicitLevel = static_cast<uint16_t>(i + 1);
                return &c;
            }
    }
    return nullptr;
}
Chain* ForRando(int32_t id, uint16_t* floor = nullptr) {
    if (const char* key = RandoItems_GetKey(id))
        return ForKey(key, floor);
    for (auto& [_, c] : chains)
        for (const auto& a : c.aliases)
            if (a.randoItem == id) {
                if (floor)
                    *floor = std::min(a.firstLevel, c.seedFirst);
                return &c;
            }
    return nullptr;
}
uint16_t Next(const Chain& c, S2HRandoInfBank bank, SaveContext* save, uint16_t floor = 0) {
    return static_cast<uint16_t>(
        std::min<size_t>(c.levels.size(), std::max<uint32_t>(Level(c, bank, save) + 1, floor ? floor : c.seedFirst)));
}
void Change(Chain& c, S2HRandoInfBank bank, SaveContext* save, bool add, uint16_t floor = 0) {
    if (!save)
        return;
    auto& n = c.counters[bank];
    if (!n.initialized) {
        n.baseline = c.read(save);
        n.initialized = true;
    }
    if (add && bank == S2H_RANDO_INF_SAVE) {
        uint16_t native = c.read(save);
        uint64_t own = n.baseline + (uint64_t)n.copies;
        if (n.copies)
            own = std::max<uint64_t>(own, (uint64_t)c.seedFirst + n.copies - 1);
        if (native > own)
            n.baseline = static_cast<uint16_t>(n.baseline + native - own);
    }
    if (add) {
        if (n.copies < UINT32_MAX)
            n.copies++;
        uint16_t actual = Level(c, bank, save);
        if (floor > actual)
            n.copies += floor - actual;
    } else if (n.copies)
        n.copies--;
    ApplyingScope scope;
    c.apply(save, Level(c, bank, save));
}
nlohmann::json WriteSection() {
    nlohmann::json result = nlohmann::json::array();
    for (const auto& [key, chain] : chains) {
        const auto& n = chain.counters[S2H_RANDO_INF_SAVE];
        result.push_back({{"key", key}, {"copies", n.initialized ? n.copies : 0u},
            {"baseline", n.initialized ? n.baseline : chain.read(&gSaveContext)}, {"first", chain.seedFirst}});
    }
    return result;
}
void ReadSection(const nlohmann::json& section) {
    for (auto& [_, chain] : chains) chain.counters[S2H_RANDO_INF_SAVE] = {};
    if (!section.is_array()) return;
    for (const auto& row : section) {
        if (!row.is_object()) continue;
        auto found = chains.find(row.value("key", std::string()));
        if (found == chains.end()) continue;
        auto& chain = found->second;
        chain.counters[S2H_RANDO_INF_SAVE] = {row.value("copies", 0u), row.value("baseline", uint16_t(0)), true};
        chain.seedFirst = std::clamp<uint16_t>(row.value("first", chain.first), 1, static_cast<uint16_t>(chain.levels.size()));
    }
}
} // namespace
bool ProgressiveItems_Register(const S2HProgressiveChain* d) {
    if (!d || d->structSize < offsetof(S2HProgressiveChain, readFirstLevel) || !d->key || !d->key[0] || !d->pickupKey ||
        !d->pickupKey[0] || !d->levels || !d->readLevel || !d->applyLevel || !d->levelCount || d->levelCount > 256 ||
        !d->firstLevel || d->firstLevel > d->levelCount || (d->aliasCount && !d->aliases) || chains.contains(d->key))
        return false;
    if (ForKey(d->key) || ForKey(d->pickupKey) || std::string(d->key) == d->pickupKey)
        return false;
    Chain c;
    c.key = d->key;
    c.pickup = d->pickupKey;
    c.first = c.seedFirst = d->firstLevel;
    c.read = d->readLevel;
    c.apply = d->applyLevel;
    if (d->structSize >= sizeof(*d))
        c.readFirst = d->readFirstLevel;
    for (uint16_t i = 0; i < d->levelCount; ++i) {
        const char* key = d->levels[i].itemKey;
        if (!key || !key[0] || c.key == key || c.pickup == key || !CustomItemRegistry_Find(key) || ForKey(key))
            return false;
        if (std::find(c.levels.begin(), c.levels.end(), key) != c.levels.end())
            return false;
        c.levels.emplace_back(key);
    }
    if (!CustomItemRegistry_Find(c.pickup.c_str()))
        return false;
    for (uint16_t i = 0; i < d->aliasCount; ++i) {
        const auto& a = d->aliases[i];
        if (!a.firstLevel || a.firstLevel > d->levelCount || ForRando(a.randoItem))
            return false;
        for (const auto& prior : c.aliases)
            if (prior.randoItem == a.randoItem || (a.vanillaItem != UINT16_MAX && prior.vanillaItem == a.vanillaItem))
                return false;
        for (const auto& [_, other] : chains)
            for (const auto& prior : other.aliases)
                if (a.vanillaItem != UINT16_MAX && prior.vanillaItem == a.vanillaItem)
                    return false;
        c.aliases.push_back(a);
    }
    std::string key = c.key;
    chains.emplace(key, std::move(c));
    return true;
}
bool ProgressiveItems_IsSource(int32_t item) {
    return ForRando(item) != nullptr;
}
bool ProgressiveItems_CheckObtainability(int32_t item, bool* result) {
    auto* chain = ForRando(item);
    if (!chain || !result)
        return false;
    *result = Level(*chain, S2H_RANDO_INF_SAVE, &gSaveContext) < chain->levels.size();
    return true;
}

uint16_t ProgressiveItems_GetLevel(const char* key, S2HRandoInfBank bank) {
    auto* c = ForKey(key);
    return c && bank >= S2H_RANDO_INF_SAVE && bank < S2H_RANDO_INF_BANK_MAX ? Level(*c, bank, BankSave(bank)) : 0;
}
const char* ProgressiveItems_GetNextItem(const char* key, S2HRandoInfBank bank) {
    uint16_t floor = 0;
    auto* c = ForKey(key, &floor);
    if (!c || bank < S2H_RANDO_INF_SAVE || bank >= S2H_RANDO_INF_BANK_MAX)
        return nullptr;
    return c->levels[Next(*c, bank, BankSave(bank), floor) - 1].c_str();
}
const char* ProgressiveItems_ResolvePickup(const char* key, uint16_t vanillaItem) {
    uint16_t floor = 0;
    auto* c = ForKey(key, &floor);
    if (!c && vanillaItem != UINT16_MAX)
        for (auto& [_, candidate] : chains)
            for (const auto& a : candidate.aliases)
                if (a.vanillaItem == vanillaItem) {
                    c = &candidate;
                    floor = a.firstLevel;
                    break;
                }
    return c ? c->levels[Next(*c, S2H_RANDO_INF_SAVE, &gSaveContext, floor) - 1].c_str() : nullptr;
}
bool ProgressiveItems_ReceiveCustom(const char* key) {
    uint16_t floor = 0;
    auto* c = ForKey(key, &floor);
    if (!c || applying)
        return false;
    Change(*c, RandoInfCustom_GetActiveBank(), &gSaveContext, true, floor);
    return true;
}
bool ProgressiveItems_GiveVanilla(uint16_t item) {
    if (applying)
        return false;
    for (auto& [_, c] : chains)
        for (const auto& a : c.aliases)
            if (a.vanillaItem == item) {
                if (RandoInfCustom_GetActiveBank() == S2H_RANDO_INF_SAVE) {
                    const auto level = Next(c, S2H_RANDO_INF_SAVE, &gSaveContext, a.firstLevel);
                    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnCustomItemReceive>(c.levels[level - 1].c_str());
                }
                Change(c, RandoInfCustom_GetActiveBank(), &gSaveContext, true, a.firstLevel);
                return true;
            }
    return false;
}
bool ProgressiveItems_GiveRando(int32_t item) {
    if (applying)
        return false;
    uint16_t floor = 0;
    auto* c = ForRando(item, &floor);
    if (!c)
        return false;
    Change(*c, RandoInfCustom_GetActiveBank(), &gSaveContext, true, floor);
    return true;
}
bool ProgressiveItems_ChangeRando(int32_t item, bool state) {
    if (applying) return false;
    uint16_t floor = 0;
    auto* chain = ForRando(item, &floor);
    if (!chain) return false;
    if (state && RandoInfCustom_GetActiveBank() == S2H_RANDO_INF_SAVE) {
        const auto level = Next(*chain, S2H_RANDO_INF_SAVE, &gSaveContext, floor);
        GameInteractor::Instance->ExecuteHooks<GameInteractor::OnCustomItemReceive>(chain->levels[level - 1].c_str());
    }
    Change(*chain, RandoInfCustom_GetActiveBank(), &gSaveContext, state, floor);
    return true;
}

bool ProgressiveItems_HasLogicItem(int32_t item, bool* result) {
    uint16_t floor = 0;
    auto* chain = ForRando(item, &floor);
    if (!chain || !result || !RandoItems_IsModItem(item)) return false;
    *result = Level(*chain, RandoInfCustom_GetActiveBank(), &gSaveContext) >= (floor ? floor : 1);
    return true;
}

const char* ProgressiveItems_ResolveRando(int32_t item, S2HRandoInfBank bank) {
    if (bank < S2H_RANDO_INF_SAVE || bank >= S2H_RANDO_INF_BANK_MAX) return nullptr;
    uint16_t floor = 0;
    auto* chain = ForRando(item, &floor);
    return chain ? chain->levels[Next(*chain, bank, &gSaveContext, floor) - 1].c_str() : nullptr;
}

void ProgressiveItems_ConfigurePool(const std::vector<RandoItemId>& pool) {
    for (auto& [_, chain] : chains) {
        chain.seedFirst = chain.first;
        for (const auto& alias : chain.aliases) {
            if (std::find(pool.begin(), pool.end(), alias.randoItem) != pool.end())
                chain.seedFirst = std::min(chain.seedFirst, alias.firstLevel);
        }
        if (chain.readFirst) {
            const uint16_t first = chain.readFirst();
            if (first) chain.seedFirst = std::clamp<uint16_t>(first, 1, static_cast<uint16_t>(chain.levels.size()));
        }
    }
}
void ProgressiveItems_ResetLogic() {
    for (auto& [_, c] : chains)
        c.counters[S2H_RANDO_INF_LOGIC] = {};
}
void ProgressiveItems_Init() {
    SaveSections_Register(section, WriteSection, ReadSection);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSaveLoad>([](s16) {
        std::vector<RandoItemId> placed;
        if (IS_RANDO) for (int i = 0; i < RC_MAX; ++i) placed.push_back(RANDO_SAVE_CHECKS[i].randoItemId);
        ProgressiveItems_ConfigurePool(placed);
    });
}

nlohmann::json ProgressiveItems_NetworkState() { return WriteSection(); }
void ProgressiveItems_MergeNetworkState(const nlohmann::json& state) {
    if (!state.is_array() || state.size() > 4096) return;
    for (const auto& row : state) {
        if (!row.is_object()) continue;
        auto found = chains.find(row.value("key", std::string()));
        if (found == chains.end()) continue;
        auto& chain = found->second;
        const uint64_t copies = row.value("copies", 0u);
        const uint64_t baseline = row.value("baseline", uint16_t(0));
        const uint16_t first = std::clamp<uint16_t>(row.value("first", chain.first), 1, static_cast<uint16_t>(chain.levels.size()));
        uint64_t incoming = baseline + copies;
        if (copies) incoming = std::max<uint64_t>(incoming, first + copies - 1);
        const uint16_t level = static_cast<uint16_t>(std::min<uint64_t>(chain.levels.size(),
            std::max<uint64_t>(Level(chain, S2H_RANDO_INF_SAVE, &gSaveContext), incoming)));
        chain.counters[S2H_RANDO_INF_SAVE] = {0, level, true};
        chain.seedFirst = std::min(chain.seedFirst, first);
        ApplyingScope scope;
        chain.apply(&gSaveContext, level);
    }
}
