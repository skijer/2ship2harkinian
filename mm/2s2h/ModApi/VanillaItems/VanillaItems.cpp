#include "VanillaItems.h"

#include <vector>

#include <spdlog/spdlog.h>

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ModApi/CustomItemRegistry/CustomItemRegistry.h"

extern "C" {
#include "z64.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
}

// z_player.c's GetItemEntry is local to that file; this mirrors its 6-byte layout to read sGetItemTable.
struct PlayerGetItemTableEntry {
    u8 itemId;
    u8 field;
    s8 gid;
    u8 textId;
    u16 objectId;
};
static_assert(sizeof(PlayerGetItemTableEntry) == 6, "z_player.c GetItemEntry layout changed");
extern "C" PlayerGetItemTableEntry sGetItemTable[GI_MAX - 1];

namespace {

struct BlockedItem {
    uint16_t vanillaItem;
    int32_t randoItem;
};

std::vector<BlockedItem> sBlockedItems;

const S2HCustomItemDefinition* FindClaim(uint16_t vanillaItem) {
    const S2HCustomItemDefinition* claim = CustomItemRegistry_FindReplacement(vanillaItem);
    if (claim == nullptr || claim->vanillaMode == S2H_VANILLA_ITEM_UPGRADE) {
        return nullptr;
    }
    return claim;
}

const S2HCustomItemDefinition* FindRandoClaim(int32_t randoItem) {
    if (randoItem == 0) {
        return nullptr;
    }
    for (uint32_t index = 0; index < CustomItemRegistry_GetCount(); index++) {
        const S2HCustomItemDefinition* definition = CustomItemRegistry_GetAt(index);
        if (definition != nullptr && definition->randoItem == randoItem &&
            definition->vanillaMode != S2H_VANILLA_ITEM_UPGRADE) {
            return definition;
        }
    }
    return nullptr;
}

bool IsBlockedVanillaItem(uint16_t vanillaItem) {
    for (const BlockedItem& blocked : sBlockedItems) {
        if (blocked.vanillaItem == vanillaItem) {
            return true;
        }
    }
    return false;
}

bool IsBlockedRandoItem(int32_t randoItem) {
    if (randoItem == 0) {
        return false;
    }
    for (const BlockedItem& blocked : sBlockedItems) {
        if (blocked.randoItem == randoItem) {
            return true;
        }
    }
    return false;
}

bool GrantClaimedItem(const S2HCustomItemDefinition* claim, bool isBlocked) {
    if (claim == nullptr) {
        return isBlocked;
    }
    if (claim->vanillaMode == S2H_VANILLA_ITEM_REPLACE) {
        CustomItemRegistry_SetOwned(claim->key, true);
    }
    return true;
}

void ResolveItemGive(u8 item, bool* should) {
    if (GrantClaimedItem(FindClaim(item), IsBlockedVanillaItem(item))) {
        *should = false;
    }
}

// Item_Give is not the only way in (old saves, cheats), so claimed items are swept instead of checked every frame.
void SweepInventory() {
    for (size_t slot = 0; slot < ARRAY_COUNT(gSaveContext.save.saveInfo.inventory.items); slot++) {
        const uint16_t item = gSaveContext.save.saveInfo.inventory.items[slot];
        if (item == ITEM_NONE) {
            continue;
        }
        const S2HCustomItemDefinition* claim = FindClaim(item);
        if (claim == nullptr && !IsBlockedVanillaItem(item)) {
            continue;
        }
        Inventory_DeleteItem(item, (uint16_t)slot);
        if (claim != nullptr && claim->vanillaMode == S2H_VANILLA_ITEM_REPLACE) {
            CustomItemRegistry_SetOwned(claim->key, true);
        }
    }
}

} // namespace

void VanillaItems_Init() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::ShouldItemGive>(ResolveItemGive);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSaveLoad>([](s16) { SweepInventory(); });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnKaleidoUpdate>(
        [](PauseContext*) { SweepInventory(); });
}

bool VanillaItems_Block(uint16_t vanillaItem, int32_t randoItem) {
    if (vanillaItem == 0) {
        SPDLOG_ERROR("[VanillaItems] Cannot block item 0");
        return false;
    }
    if (!IsBlockedVanillaItem(vanillaItem)) {
        sBlockedItems.push_back({ vanillaItem, randoItem });
    }
    return true;
}

const char* VanillaItems_GetCustomKey(uint16_t vanillaItem) {
    const S2HCustomItemDefinition* claim = FindClaim(vanillaItem);
    return claim != nullptr && claim->vanillaMode == S2H_VANILLA_ITEM_REPLACE ? claim->key : nullptr;
}

const char* VanillaItems_GetCustomKeyForRandoItem(int32_t randoItem) {
    const S2HCustomItemDefinition* claim = FindRandoClaim(randoItem);
    return claim != nullptr && claim->vanillaMode == S2H_VANILLA_ITEM_REPLACE ? claim->key : nullptr;
}

bool VanillaItems_GrantRandoItem(int32_t randoItem) {
    return GrantClaimedItem(FindRandoClaim(randoItem), IsBlockedRandoItem(randoItem));
}

bool VanillaItems_Give(PlayState* play, uint16_t vanillaItem) {
    if (play == nullptr || vanillaItem >= ITEM_CUSTOM) {
        return false;
    }
    Item_Give(play, (u8)vanillaItem);
    return true;
}

// MM has no custom drop actor: En_Item00 would need a custom collectible type first.
bool VanillaItems_Drop(PlayState* play, uint16_t vanillaItem) {
    return false;
}

bool VanillaItems_IsVanillaSuppressed(uint16_t vanillaItem) {
    return FindClaim(vanillaItem) != nullptr || IsBlockedVanillaItem(vanillaItem);
}

bool VanillaItems_IsRandoItemSuppressed(int32_t randoItem) {
    return FindRandoClaim(randoItem) != nullptr || IsBlockedRandoItem(randoItem);
}

bool VanillaItems_GetItemEntry(int16_t getItemId, S2HGetItemEntry* entry) {
    if (entry == nullptr) {
        return false;
    }
    *entry = {};
    if (getItemId <= GI_NONE || getItemId >= GI_MAX) {
        return false;
    }
    const PlayerGetItemTableEntry& source = sGetItemTable[getItemId - 1];
    entry->itemId = source.itemId;
    entry->field = source.field;
    entry->gi = source.gid;
    entry->textId = source.textId;
    entry->objectId = source.objectId;
    entry->getItemId = getItemId;
    entry->gid = ABS_ALT(source.gid) - 1;
    entry->collectable = true;
    entry->drawItemId = source.itemId;
    return true;
}
