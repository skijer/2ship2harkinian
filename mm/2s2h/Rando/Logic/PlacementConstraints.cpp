#include "Logic.h"
#include "Rando/StaticData/StaticData.h"
#include "2s2h/ModApi/RandoItems/RandoItems.h"

#include <algorithm>

namespace Rando {

namespace Logic {

int SceneIdToDungeon(SceneId sceneId) {
    const int32_t custom = DungeonRegistry_FindScene(sceneId);
    if (custom >= 0) {
        return custom + 9;
    }
    switch (sceneId) {
        case SCENE_MITURIN:
        case SCENE_MITURIN_BS:
            return DUNGEON_SCENE_INDEX_WOODFALL_TEMPLE;
        case SCENE_HAKUGIN:
        case SCENE_HAKUGIN_BS:
            return DUNGEON_SCENE_INDEX_SNOWHEAD_TEMPLE;
        case SCENE_SEA:
        case SCENE_SEA_BS:
            return DUNGEON_SCENE_INDEX_GREAT_BAY_TEMPLE;
        case SCENE_INISIE_N:
        case SCENE_INISIE_R:
        case SCENE_INISIE_BS:
            return DUNGEON_SCENE_INDEX_STONE_TOWER_TEMPLE;
        default:
            return -1;
    }
}

int RandoItemIdToDungeon(RandoItemId itemId);

namespace {

int GetCheckDungeon(RandoCheckId checkId) {
    return SceneIdToDungeon(Rando::StaticData::Checks[checkId].sceneId);
}

int GetItemDungeon(RandoItemId itemId) {
    if (const S2HKeyRing* keyRing = RandoItems_GetKeyRing(itemId); keyRing != nullptr) {
        return keyRing->dungeon;
    }
    switch (itemId) {
        case RI_WOODFALL_SMALL_KEY:
        case RI_WOODFALL_BOSS_KEY:
        case RI_WOODFALL_STRAY_FAIRY:
            return DUNGEON_SCENE_INDEX_WOODFALL_TEMPLE;
        case RI_SNOWHEAD_SMALL_KEY:
        case RI_SNOWHEAD_BOSS_KEY:
        case RI_SNOWHEAD_STRAY_FAIRY:
            return DUNGEON_SCENE_INDEX_SNOWHEAD_TEMPLE;
        case RI_GREAT_BAY_SMALL_KEY:
        case RI_GREAT_BAY_BOSS_KEY:
        case RI_GREAT_BAY_STRAY_FAIRY:
            return DUNGEON_SCENE_INDEX_GREAT_BAY_TEMPLE;
        case RI_STONE_TOWER_SMALL_KEY:
        case RI_STONE_TOWER_BOSS_KEY:
        case RI_STONE_TOWER_STRAY_FAIRY:
            return DUNGEON_SCENE_INDEX_STONE_TOWER_TEMPLE;
        default:
            return -1;
    }
}

uint32_t GetMajoraSoulPlacement() {
    switch (RANDO_SAVE_OPTIONS[RO_SHUFFLE_MAJORA_SOUL]) {
        case RO_MAJORA_SOUL_ANY_DUNGEON:
            return RO_DUNGEON_ITEM_ANY_DUNGEON;
        case RO_MAJORA_SOUL_OVERWORLD:
            return RO_DUNGEON_ITEM_OVERWORLD;
        default:
            return RO_DUNGEON_ITEM_ANYWHERE;
    }
}

uint32_t GetItemPlacement(RandoItemId itemId) {
    if (itemId == RI_SOUL_BOSS_MAJORA) {
        return GetMajoraSoulPlacement();
    }
    if (GetItemDungeon(itemId) < 0) {
        return RO_DUNGEON_ITEM_ANYWHERE;
    }
    switch (Rando::StaticData::Items[itemId].randoItemType) {
        case RITYPE_SMALL_KEY:
            return RANDO_SAVE_OPTIONS[RO_PLACEMENT_SMALL_KEYS];
        case RITYPE_BOSS_KEY:
            return RANDO_SAVE_OPTIONS[RO_PLACEMENT_BOSS_KEYS];
        case RITYPE_STRAY_FAIRY:
            return RANDO_SAVE_OPTIONS[RO_PLACEMENT_STRAY_FAIRIES];
        default:
            return RO_DUNGEON_ITEM_ANYWHERE;
    }
}

bool IsPlacementMet(uint32_t placement, int itemDungeon, int checkDungeon) {
    switch (placement) {
        case RO_DUNGEON_ITEM_OWN_DUNGEON:
            return checkDungeon == itemDungeon;
        case RO_DUNGEON_ITEM_ANY_DUNGEON:
            return checkDungeon >= 0;
        case RO_DUNGEON_ITEM_OVERWORLD:
            return checkDungeon < 0;
        default:
            return true;
    }
}

bool IsPreplacedAs(RandoItemId itemId, uint32_t placement) {
    if (placement == RO_DUNGEON_ITEM_OWN_DUNGEON) {
        return RandoItemIdToDungeon(itemId) >= 0;
    }
    return GetItemPlacement(itemId) == placement;
}

template <class ItemTest, class CheckTest>
size_t FindForcedItem(const std::vector<RandoItemId>& itemPool, const std::vector<RandoCheckId>& checkPool,
                      ItemTest isBound, CheckTest isInGroup) {
    const auto boundItems = std::count_if(itemPool.begin(), itemPool.end(), isBound);
    if (boundItems == 0) {
        return itemPool.size();
    }
    const auto groupChecks = 1 + std::count_if(checkPool.begin(), checkPool.end(), isInGroup);
    if (boundItems < groupChecks) {
        return itemPool.size();
    }
    for (size_t i = itemPool.size(); i-- > 0;) {
        if (isBound(itemPool[i])) {
            return i;
        }
    }
    return itemPool.size();
}

size_t FindForcedItemForCheck(const std::vector<RandoItemId>& itemPool, const std::vector<RandoCheckId>& checkPool,
                              int dungeon) {
    if (dungeon < 0) {
        return FindForcedItem(
            itemPool, checkPool,
            [](RandoItemId itemId) { return GetItemPlacement(itemId) == RO_DUNGEON_ITEM_OVERWORLD; },
            [](RandoCheckId other) { return GetCheckDungeon(other) < 0; });
    }
    const size_t ownDungeonItem = FindForcedItem(
        itemPool, checkPool, [dungeon](RandoItemId itemId) { return RandoItemIdToDungeon(itemId) == dungeon; },
        [dungeon](RandoCheckId other) { return GetCheckDungeon(other) == dungeon; });
    if (ownDungeonItem < itemPool.size()) {
        return ownDungeonItem;
    }
    return FindForcedItem(
        itemPool, checkPool, [](RandoItemId itemId) { return GetItemPlacement(itemId) == RO_DUNGEON_ITEM_ANY_DUNGEON; },
        [](RandoCheckId other) { return GetCheckDungeon(other) >= 0; });
}

} // namespace

int RandoItemIdToDungeon(RandoItemId itemId) {
    if (DungeonRegistry_FindItem(itemId) >= 0) {
        return DungeonRegistry_ConfinedItem(itemId);
    }
    return GetItemPlacement(itemId) == RO_DUNGEON_ITEM_OWN_DUNGEON ? GetItemDungeon(itemId) : -1;
}

bool IsItemAllowedAtCheck(RandoItemId itemId, RandoCheckId checkId) {
    const int checkDungeon = GetCheckDungeon(checkId);
    if (!DungeonRegistry_ItemAllowed(itemId, checkDungeon >= 0)) {
        return false;
    }
    if (DungeonRegistry_FindItem(itemId) >= 0) {
        const int confinedDungeon = DungeonRegistry_ConfinedItem(itemId);
        return confinedDungeon < 0 || confinedDungeon == checkDungeon;
    }
    return IsPlacementMet(GetItemPlacement(itemId), GetItemDungeon(itemId), checkDungeon);
}

size_t SelectItemForCheck(const std::vector<RandoItemId>& itemPool, const std::vector<RandoCheckId>& checkPool,
                          RandoCheckId checkId) {
    const size_t forcedItem = FindForcedItemForCheck(itemPool, checkPool, GetCheckDungeon(checkId));
    if (forcedItem < itemPool.size()) {
        return forcedItem;
    }
    for (size_t i = itemPool.size(); i-- > 0;) {
        if (IsItemAllowedAtCheck(itemPool[i], checkId)) {
            return i;
        }
    }
    return itemPool.size();
}

void PreplaceConfinedItems(std::vector<RandoCheckId>& checkPool, std::vector<RandoItemId>& itemPool) {
    std::set<RandoCheckId> placedChecks;
    std::set<size_t> placedItems;
    for (const uint32_t placement :
         { RO_DUNGEON_ITEM_OWN_DUNGEON, RO_DUNGEON_ITEM_ANY_DUNGEON, RO_DUNGEON_ITEM_OVERWORLD }) {
        for (size_t i = 0; i < itemPool.size(); i++) {
            if (!IsPreplacedAs(itemPool[i], placement)) {
                continue;
            }
            std::vector<RandoCheckId> candidates;
            for (RandoCheckId checkId : checkPool) {
                if (!placedChecks.contains(checkId) && IsItemAllowedAtCheck(itemPool[i], checkId)) {
                    candidates.push_back(checkId);
                }
            }
            if (candidates.empty()) {
                continue;
            }
            const RandoCheckId checkId = candidates[Ship_Random(0, candidates.size())];
            RANDO_SAVE_CHECKS[checkId].shuffled = true;
            RANDO_SAVE_CHECKS[checkId].randoItemId = itemPool[i];
            placedChecks.insert(checkId);
            placedItems.insert(i);
        }
    }

    std::vector<RandoItemId> remainingItems;
    for (size_t i = 0; i < itemPool.size(); i++) {
        if (!placedItems.contains(i)) {
            remainingItems.push_back(itemPool[i]);
        }
    }
    itemPool = std::move(remainingItems);
    std::erase_if(checkPool, [&placedChecks](RandoCheckId checkId) { return placedChecks.contains(checkId); });
}

void PlaceVanillaDungeonItems(RandoSaveInfo& saveInfo, std::vector<RandoCheckId>& checkPool,
                              std::vector<RandoItemId>& itemPool) {
    std::vector<RandoCheckId> remainingChecks;
    for (RandoCheckId checkId : checkPool) {
        const RandoItemId vanillaItem = Rando::StaticData::Checks[checkId].randoItemId;
        const auto pooledItem = std::find(itemPool.begin(), itemPool.end(), vanillaItem);
        if (GetItemPlacement(vanillaItem) != RO_DUNGEON_ITEM_VANILLA || pooledItem == itemPool.end()) {
            remainingChecks.push_back(checkId);
            continue;
        }
        itemPool.erase(pooledItem);
        RandoSaveCheck& saveCheck = DungeonRegistry_SaveCheck(saveInfo, checkId);
        saveCheck.shuffled = true;
        saveCheck.randoItemId = vanillaItem;
    }
    checkPool = std::move(remainingChecks);
}

} // namespace Logic

} // namespace Rando
