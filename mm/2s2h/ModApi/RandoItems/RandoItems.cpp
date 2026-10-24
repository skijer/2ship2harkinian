#include "2s2h/ModApi/RandoItems/RandoItems.h"

#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

#include <spdlog/spdlog.h>

#include "2s2h/ModApi/CustomItemRegistry/CustomItemRegistry.h"
#include "2s2h/ModApi/RandoOptions/RandoOptions.h"
#include "2s2h/Rando/StaticData/StaticData.h"
#include "ShipUtils.h"

extern "C" {
#include "z64.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
// OPEN_DISPS redeclares these inside its body, which would give them C++ linkage here.
void FrameInterpolation_RecordOpenChild(const void* a, int b);
void FrameInterpolation_RecordCloseChild(void);
}

namespace {

constexpr int32_t FirstModRandoItem = RI_MAX + 1;
constexpr int32_t LastModRandoItem = UINT16_MAX;
constexpr int16_t DrawIconHalfSize = 16;

struct ModItem {
    std::string key;
    int32_t randoItem;
    uint16_t flag;
};

// Deque: the item table keeps the key pointers of every row it was handed.
std::deque<ModItem> sItems;
std::unordered_map<std::string, size_t> sKeysToIndex;
std::unordered_map<int32_t, size_t> sRandoItemsToIndex;

uint32_t HashKey(const std::string& key) {
    uint32_t hash = 2166136261u;
    for (const unsigned char character : key) {
        hash = (hash ^ character) * 16777619u;
    }
    return hash;
}

int32_t ClaimRandoItem(const std::string& key) {
    constexpr uint32_t span = LastModRandoItem - FirstModRandoItem + 1;
    if (sItems.size() >= span) {
        return 0;
    }
    uint32_t offset = HashKey(key) % span;
    while (sRandoItemsToIndex.contains(FirstModRandoItem + static_cast<int32_t>(offset))) {
        offset = (offset + 1) % span;
    }
    return FirstModRandoItem + static_cast<int32_t>(offset);
}

const ModItem* Find(int32_t randoItem) {
    const auto found = sRandoItemsToIndex.find(randoItem);
    return found != sRandoItemsToIndex.end() ? &sItems[found->second] : nullptr;
}

const S2HCustomItemRandomizer* GetRandomizerBlock(const ModItem& item) {
    const S2HCustomItemDefinition* definition = CustomItemRegistry_Find(item.key.c_str());
    return definition != nullptr ? definition->randomizer : nullptr;
}

RandoItemType GetItemType(const S2HCustomItemRandomizer& block) {
    const bool isAdvancement = (block.flags & S2H_CUSTOM_ITEM_RANDO_ADVANCEMENT) != 0;
    return isAdvancement ? RITYPE_MAJOR : RITYPE_LESSER;
}

void BuildRow(const ModItem& item) {
    const S2HCustomItemRandomizer* block = GetRandomizerBlock(item);
    if (block == nullptr) {
        SPDLOG_ERROR("Randomizer item {} lost its definition", item.key);
        return;
    }
    // Mod items have no vanilla ItemId, GetItemId or draw id: their name and icon come from the custom item registry.
    Rando::StaticData::Items[static_cast<RandoItemId>(item.randoItem)] = {
        static_cast<RandoItemId>(item.randoItem), item.key.c_str(), "", item.key.c_str(), GetItemType(*block),
        ITEM_NONE,                                GI_NONE,          GID_NONE
    };
}

S2HRandoInfBank GetActiveBank() {
    return RandoInfCustom_GetActiveBank();
}

} // namespace

int32_t RandoItems_Register(const char* key) {
    if (key == nullptr || key[0] == '\0') {
        SPDLOG_ERROR("Rejected a randomizer item with no key");
        return 0;
    }
    const std::string name = key;
    const auto found = sKeysToIndex.find(name);
    if (found != sKeysToIndex.end()) {
        return sItems[found->second].randoItem;
    }

    const uint16_t flag = RandoInfCustom_Register(key);
    const int32_t randoItem = ClaimRandoItem(name);
    if (flag == RANDO_INF_CUSTOM_NONE || randoItem == 0) {
        return 0;
    }
    sKeysToIndex[name] = sItems.size();
    sRandoItemsToIndex[randoItem] = sItems.size();
    sItems.push_back({ name, randoItem, flag });
    if (GetRandomizerBlock(sItems.back()) != nullptr) {
        BuildRow(sItems.back());
    }
    return randoItem;
}

int32_t RandoItems_GetRandoGet(const char* key) {
    if (key == nullptr) {
        return 0;
    }
    const auto found = sKeysToIndex.find(key);
    return found != sKeysToIndex.end() ? sItems[found->second].randoItem : 0;
}

const char* RandoItems_GetKey(int32_t randoItem) {
    const ModItem* item = Find(randoItem);
    return item != nullptr ? item->key.c_str() : nullptr;
}

bool RandoItems_IsModItem(int32_t randoItem) {
    return Find(randoItem) != nullptr;
}

uint32_t RandoItems_GetCount(void) {
    return static_cast<uint32_t>(sItems.size());
}

int32_t RandoItems_GetAt(uint32_t index) {
    return index < sItems.size() ? sItems[index].randoItem : 0;
}

bool RandoItems_IsOwned(int32_t randoItem, S2HRandoInfBank bank) {
    const ModItem* item = Find(randoItem);
    if (item == nullptr) {
        return false;
    }
    if (bank == S2H_RANDO_INF_SAVE) {
        return CustomItemRegistry_IsOwned(item->key.c_str());
    }
    if (RandoInfCustom_Get(bank, item->flag)) return true;
    for (const auto& candidate : sItems) {
        const auto* pickup = CustomItemRegistry_GetPickupVariant(candidate.key.c_str());
        if (pickup != nullptr && item->key == pickup->itemKey && RandoInfCustom_Get(bank, candidate.flag)) return true;
    }
    return false;
}

void RandoItems_SetOwned(int32_t randoItem, S2HRandoInfBank bank, bool state) {
    const ModItem* item = Find(randoItem);
    if (item == nullptr) {
        return;
    }
    if (bank == S2H_RANDO_INF_SAVE) {
        CustomItemRegistry_SetOwned(item->key.c_str(), state);
        return;
    }
    RandoInfCustom_Set(bank, item->flag, state);
}

void RandoItems_BuildItemTable() {
    for (const ModItem& item : sItems) {
        if (GetRandomizerBlock(item) != nullptr) {
            BuildRow(item);
        }
    }
}

void RandoItems_AddToPool(std::vector<RandoItemId>& itemPool) {
    for (const ModItem& item : sItems) {
        const S2HCustomItemRandomizer* block = GetRandomizerBlock(item);
        if (block == nullptr) {
            continue;
        }
        if (block->poolModeOption != nullptr) {
            const bool match = RandoOptions_GetForSeed(block->poolModeOption) == block->poolModeValue;
            if (match == block->poolModeInvert) continue;
        }
        const uint32_t optionCount =
            block->poolCountOption != nullptr ? RandoOptions_GetForSeed(block->poolCountOption) : 0;
        const uint32_t count = block->poolCount + optionCount;
        if (block->poolSourceCount == 0) {
            itemPool.insert(itemPool.end(), count, static_cast<RandoItemId>(item.randoItem));
            continue;
        }
        for (uint32_t i = 0; i < count; i++) {
            itemPool.push_back(static_cast<RandoItemId>(block->poolSources[Ship_Random(0, block->poolSourceCount)]));
        }
    }
}

void RandoItems_Give(RandoItemId randoItemId) {
    const ModItem* item = Find(randoItemId);
    if (item == nullptr) {
        return;
    }
    const S2HRandoInfBank bank = GetActiveBank();
    if (bank == S2H_RANDO_INF_SAVE) {
        CustomItemRegistry_ReceiveGetItem(item->key.c_str());
    } else {
        RandoItems_SetOwned(randoItemId, bank, true);
    }
}

void RandoItems_Remove(RandoItemId randoItemId) {
    RandoItems_SetOwned(randoItemId, GetActiveBank(), false);
}

// The registry owns the real state, so a logic simulation starts from it rather than from the flags alone.
void RandoItems_SeedLogicBank() {
    for (const ModItem& item : sItems) {
        RandoInfCustom_Set(S2H_RANDO_INF_LOGIC, item.flag, CustomItemRegistry_IsOwned(item.key.c_str()));
    }
}

const char* RandoItems_GetIconPath(RandoItemId randoItemId) {
    const ModItem* item = Find(randoItemId);
    return item != nullptr ? CustomItemRegistry_ResolveTexture(item->key.c_str(), S2H_ITEM_ICON_INVENTORY) : nullptr;
}

// Outside the anonymous namespace: OPEN_DISPS redeclares the interpolation hooks in its body, and inside one they
// get C++ linkage and fail to link.
void RandoItems_Draw(RandoItemId randoItemId) {
    if (gPlayState == nullptr) return;
    const char* key = RandoItems_GetKey(randoItemId);
    const auto* definition = CustomItemRegistry_Find(key);
    if (definition != nullptr && definition->getItemModelPath != nullptr) {
        CustomItemRegistry_DrawAcquisition(key, gPlayState);
        return;
    }
    const char* icon = RandoItems_GetIconPath(randoItemId);
    if (icon == nullptr || gPlayState == nullptr) {
        return;
    }

    OPEN_DISPS(gPlayState->state.gfxCtx);

    Gfx_SetupDL25_Xlu(gPlayState->state.gfxCtx);
    Matrix_ReplaceRotation(&gPlayState->billboardMtxF);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, gPlayState->state.gfxCtx);

    Vtx* vertices = static_cast<Vtx*>(GRAPH_ALLOC(gPlayState->state.gfxCtx, 4 * sizeof(Vtx)));
    const int16_t half = DrawIconHalfSize;
    vertices[0] = VTX(-half, half, 0, 0, 0, 255, 255, 255, 255);
    vertices[1] = VTX(half, half, 0, 32 << 5, 0, 255, 255, 255, 255);
    vertices[2] = VTX(-half, -half, 0, 0, 32 << 5, 255, 255, 255, 255);
    vertices[3] = VTX(half, -half, 0, 32 << 5, 32 << 5, 255, 255, 255, 255);

    gDPSetCombineMode(POLY_XLU_DISP++, G_CC_MODULATERGBA, G_CC_MODULATERGBA);
    gDPLoadTextureBlock(POLY_XLU_DISP++, icon, G_IM_FMT_RGBA, G_IM_SIZ_32b, 32, 32, 0, G_TX_NOMIRROR | G_TX_WRAP,
                        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gSPVertex(POLY_XLU_DISP++, reinterpret_cast<uintptr_t>(vertices), 4, 0);
    gSP2Triangles(POLY_XLU_DISP++, 0, 1, 2, 0, 1, 3, 2, 0);

    CLOSE_DISPS(gPlayState->state.gfxCtx);
}
