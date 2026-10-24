#include "ModLayout.h"

#include <algorithm>
#include <map>
#include <string>

#include <imgui.h>
#include <libultraship/classes.h>
#include <libultraship/bridge/consolevariablebridge.h>
#include <spdlog/spdlog.h>

#include "2s2h/ModApi/CustomItemRegistry/CustomItemRegistry.h"
#include "2s2h/ModApi/CustomEquipRegistry/CustomEquipRegistry.h"

extern "C" {
#include "z64.h"
extern s16 sQuestVtxWidths[];
extern s16 sQuestVtxHeights[];
}

namespace {

constexpr const char* VanillaPageKey = "vanilla";
constexpr uint8_t MaxItemRows = 4;
constexpr uint8_t MaxItemColumns = 6;
constexpr uint8_t QuestPointCount = QUEST_HEART_PIECE + 1;
// Below S2H_ITEM_PAGE_QUEST_VANILLA, clear of the small page numbers mods give their items directly.
constexpr uint16_t FirstItemPageId = 0x80;
constexpr uint16_t ItemPageLimit = S2H_ITEM_PAGE_QUEST_VANILLA - FirstItemPageId;
constexpr uint16_t QuestPageLimit = S2H_ITEM_PAGE_VANILLA - S2H_ITEM_PAGE_QUEST_VANILLA - 1;

struct Page {
    std::string title;
    uint8_t kind;
    uint8_t rows;
    uint8_t columns;
    uint16_t pageId;
};

std::map<std::string, Page> sPages;

bool HasReservedPrefix(const std::string& key) {
    return key == VanillaPageKey || key.rfind("vanilla.", 0) == 0 || key.rfind("user.", 0) == 0 ||
           key.rfind("page.", 0) == 0;
}

uint16_t CountPagesOfKind(uint8_t kind) {
    uint16_t count = 0;
    for (const auto& [key, page] : sPages) {
        count += page.kind == kind ? 1 : 0;
    }
    return count;
}

bool IsValidPage(const S2HLayoutPage* definition) {
    if (definition == nullptr || definition->structSize < sizeof(S2HLayoutPage) || definition->key == nullptr ||
        definition->key[0] == '\0' || HasReservedPrefix(definition->key) || sPages.contains(definition->key)) {
        return false;
    }
    if (definition->kind == S2H_LAYOUT_EQUIPMENT) {
        return definition->rows == 4 && definition->columns >= 1 && definition->columns <= 6 && CountPagesOfKind(S2H_LAYOUT_EQUIPMENT) < 224;
    }
    if (definition->kind == S2H_LAYOUT_COLLECTABLES) {
        return CountPagesOfKind(S2H_LAYOUT_COLLECTABLES) < QuestPageLimit;
    }
    return definition->kind == S2H_LAYOUT_ITEMS && definition->rows >= 1 && definition->rows <= MaxItemRows &&
           definition->columns >= 1 && definition->columns <= MaxItemColumns &&
           CountPagesOfKind(S2H_LAYOUT_ITEMS) < ItemPageLimit;
}

uint16_t GetCellCount(const Page& page) {
    return page.kind == S2H_LAYOUT_COLLECTABLES ? 25 : page.rows * page.columns;
}

// Item pages lay their cells out in the pause grid's 6 columns; quest pages address the quest points directly.
uint8_t GetSlot(const Page& page, uint8_t cell) {
    if (page.kind == S2H_LAYOUT_EQUIPMENT) return (cell / page.columns) * 6 + cell % page.columns;
    if (page.kind == S2H_LAYOUT_COLLECTABLES) {
        return cell;
    }
    return (cell / page.columns) * MaxItemColumns + cell % page.columns;
}

bool PlaceOnPage(const S2HCustomItemDefinition& definition, uint16_t pageId, uint8_t slot) {
    S2HCustomItemPlacement current;
    if (!KaleidoItemManager_GetPlacement(definition.key, &current)) {
        return false;
    }
    // A placement that already moved off the item's own preference is the player's choice and wins over a default.
    const auto* equipment = CustomEquipRegistry_Find(definition.key);
    const uint16_t defaultPage = equipment != nullptr && equipment->slot == S2H_EQUIP_SLOT_UPGRADE ? S2H_ITEM_PAGE_QUEST_VANILLA : equipment != nullptr ? S2H_ITEM_PAGE_EQUIPMENT_FIRST + (equipment->page == S2H_EQUIP_PAGE_VANILLA ? 0 : equipment->page) : definition.preferredPage;
    const int upgradePoints[]={QUEST_SWORD,QUEST_SHIELD,QUEST_QUIVER,QUEST_BOMB_BAG};
    const uint8_t defaultSlot = equipment != nullptr && equipment->slot == S2H_EQUIP_SLOT_UPGRADE ? upgradePoints[std::min<int>(equipment->row,3)] : equipment != nullptr ? equipment->row * 6 + equipment->column : definition.preferredSlot;
    const bool isUntouched = current.page == defaultPage && current.slot == defaultSlot;
    if (!isUntouched) {
        return true;
    }
    const S2HCustomItemPlacement placement = { pageId, slot };
    return KaleidoItemManager_SetPlacement(definition.key, &placement);
}

} // namespace

bool ModLayout_RegisterPage(const S2HLayoutPage* definition) {
    if (!IsValidPage(definition)) {
        return false;
    }
    const bool isQuestPage = definition->kind == S2H_LAYOUT_COLLECTABLES;
    uint16_t pageId = isQuestPage ? S2H_ITEM_PAGE_QUEST_VANILLA + 1 :
                      definition->kind == S2H_LAYOUT_EQUIPMENT ? S2H_ITEM_PAGE_EQUIPMENT_FIRST : 0;
    for (;;) {
        bool occupied = false;
        for (const auto& [key, page] : sPages) occupied |= page.pageId == pageId;
        for (uint32_t i = 0; i < CustomItemRegistry_GetCount(); ++i) {
            S2HCustomItemPlacement placement;
            if (KaleidoItemManager_GetPlacement(CustomItemRegistry_GetAt(i)->key, &placement)) occupied |= placement.page == pageId;
        }
        if (!occupied) break;
        ++pageId;
    }
    if ((definition->kind == S2H_LAYOUT_ITEMS && pageId >= S2H_ITEM_PAGE_QUEST_VANILLA) ||
        (isQuestPage && pageId >= S2H_ITEM_PAGE_VANILLA) || pageId == UINT16_MAX) return false;
    sPages.emplace(definition->key, Page{ definition->title ? definition->title : definition->key, definition->kind,
                                          definition->rows, definition->columns, pageId });
    return true;
}

bool ModLayout_PlaceItem(const char* itemKey, const char* pageKey, uint8_t cell) {
    const S2HCustomItemDefinition* definition = itemKey != nullptr ? CustomItemRegistry_Find(itemKey) : nullptr;
    if (definition == nullptr || pageKey == nullptr) {
        return false;
    }
    if (std::string(pageKey) == VanillaPageKey) {
        const bool isQuestItem = CustomItemRegistry_IsQuestPage(definition->preferredPage);
        const uint16_t vanillaPage = isQuestItem ? S2H_ITEM_PAGE_QUEST_VANILLA : S2H_ITEM_PAGE_VANILLA;
        const uint8_t cellCount = isQuestItem ? QuestPointCount : ITEM_NUM_SLOTS;
        return cell < cellCount && PlaceOnPage(*definition, vanillaPage, cell);
    }

    const auto found = sPages.find(pageKey);
    if (found == sPages.end() || cell >= GetCellCount(found->second)) {
        return false;
    }
    return PlaceOnPage(*definition, found->second.pageId, GetSlot(found->second, cell));
}

bool ModLayout_RegisterVanilla(const S2HLayoutVanilla* entry) {
    SPDLOG_WARN("[ModLayout] Native slot references are not supported in MM ('{}')",
                entry != nullptr && entry->key != nullptr ? entry->key : "");
    return false;
}

bool ModLayout_CyclePage(struct PlayState* play, uint8_t kind) {
    return false;
}

bool ModLayout_DrawPage(struct PlayState* play, uint8_t kind) {
    return false;
}

bool ModLayout_IsActive(uint8_t kind) {
    return false;
}

bool ModLayout_BeginCustomEquip(struct PlayState* play, const char* key, uint8_t button, int16_t x, int16_t y) {
    return false;
}

void ModLayout_CompleteCustomEquip(struct PlayState* play) {
}

const char* ModLayout_GetEquipIcon(void) {
    return nullptr;
}

bool ModLayout_HasMods(void) {
    return !sPages.empty();
}

void ModLayout_DrawEditor(uint8_t kind) {
    if (kind == S2H_LAYOUT_EQUIPMENT) ImGui::TextUnformatted("Equipment pages appear over Masks, in four rows: swords, shields, tunics and boots.");
    for (const auto& [key, page] : sPages) {
        if (page.kind != kind || !ImGui::TreeNode(key.c_str(), "%s (%d cells)", page.title.c_str(), GetCellCount(page))) {
            continue;
        }
        for (uint8_t cell = 0; cell < GetCellCount(page); cell++) {
            const uint8_t slot = GetSlot(page, cell);
            for (uint32_t index = 0; index < KaleidoItemManager_GetSlotItemCount(page.pageId, slot); index++) {
                ImGui::Text("Cell %d: %s", cell, KaleidoItemManager_GetSlotItemAt(page.pageId, slot, index));
            }
        }
        ImGui::TreePop();
    }
}

static std::string CellSetting(uint16_t page, uint8_t slot, const char* field) {
    return "gEnhancements.CustomItemManager.Cells." + std::to_string(page) + "." + std::to_string(slot) + "." + field;
}

bool ModLayout_GetCell(uint16_t page, uint8_t slot, S2HLayoutCell* cell) {
    if (cell == nullptr || slot >= 32) return false;
    *cell = { static_cast<int16_t>(-96 + (slot % 6) * 32), static_cast<int16_t>(56 - (slot / 6) * 32), 28, 28 };
    if (page >= S2H_ITEM_PAGE_QUEST_VANILLA && page < S2H_ITEM_PAGE_VANILLA) {
        if (page == S2H_ITEM_PAGE_QUEST_VANILLA && slot < 23) {
            const int x[] = { 45,78,10,45,80,11,-109,-87,-65,-41,-19,-18,-109,-87,-65,-41,-19,-18,-103,7,82,-110,-54 };
            const int y[] = { 62,42,42,20,-9,-9,-20,-20,-20,-20,-20,-20,2,2,2,2,2,2,54,-44,-44,34,58 };
            cell->x = x[slot]; cell->y = y[slot]; 
            cell->width = sQuestVtxWidths[slot]; cell->height = sQuestVtxHeights[slot];
        } else {
            const int x[] = {74,74,46,18,18,46,-108,-90,-72,-54,-36,-18,-108,-90,-72,-54,-36,-18,20,46,72,-110,-86,-110,-54};
            const int y[] = {38,6,-12,6,38,56,-20,-20,-20,-20,-20,-20,2,2,2,2,2,2,-46,-46,-46,58,58,34,58};
            if (slot >= 25) return false;
            *cell = { static_cast<int16_t>(x[slot]), static_cast<int16_t>(y[slot]), static_cast<uint16_t>(slot >= 6 && slot < 18 ? 16 : 24), 24 };
            if (slot == 24) cell->width = cell->height = 48;
        }
    }
    cell->x = CVarGetInteger(CellSetting(page,slot,"X").c_str(),cell->x);
    cell->y = CVarGetInteger(CellSetting(page,slot,"Y").c_str(),cell->y);
    cell->width = CVarGetInteger(CellSetting(page,slot,"Width").c_str(),cell->width);
    cell->height = CVarGetInteger(CellSetting(page,slot,"Height").c_str(),cell->height);
    return true;
}

bool ModLayout_SetCell(uint16_t page, uint8_t slot, const S2HLayoutCell* cell) {
    if (cell == nullptr || slot >= 32 || cell->x < -128 || cell->x + cell->width > 128 || cell->y > 80 ||
        cell->y - cell->height < -88 || cell->width < 8 || cell->width > 64 || cell->height < 8 || cell->height > 64) return false;
    CVarSetInteger(CellSetting(page,slot,"X").c_str(),cell->x);
    CVarSetInteger(CellSetting(page,slot,"Y").c_str(),cell->y);
    CVarSetInteger(CellSetting(page,slot,"Width").c_str(),cell->width);
    CVarSetInteger(CellSetting(page,slot,"Height").c_str(),cell->height);
    Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
    return true;
}

void ModLayout_ApplyCells(PlayState* play, uint16_t page, Vtx* vertices, uint8_t count) {
    for (uint8_t slot = 0; slot < count; ++slot) {
        S2HLayoutCell cell;
        if (!ModLayout_GetCell(page,slot,&cell)) continue;
        for (int corner = 0; corner < 4; ++corner) {
            Vtx& v = vertices[slot * 4 + corner];
            v.v.ob[0] = cell.x + ((corner & 1) ? cell.width : 0);
            v.v.ob[1] = cell.y + play->pauseCtx.offsetY - ((corner & 2) ? cell.height : 0);
        }
    }
}
