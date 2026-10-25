#include "KaleidoItemManager.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <libultraship/bridge/consolevariablebridge.h>
#include <libultraship/classes.h>

#include "2s2h/CustomMessage/CustomMessage.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ModApi/CustomEquipRegistry/CustomEquipRegistry.h"

extern "C" {
#include "z64.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "overlays/kaleido_scope/ovl_kaleido_scope/z_kaleido_scope.h"
#include "2s2h_assets.h"
void func_801514B0(PlayState* play, u16 textId, u8 arg2);
// OPEN_DISPS redeclares these inside its body, which would give them C++ linkage here.
void FrameInterpolation_RecordOpenChild(const void* a, int b);
void FrameInterpolation_RecordCloseChild(void);
}

namespace {

#define CVAR_CUSTOM_ITEM_MANAGER(name) "gEnhancements.CustomItemManager." name

constexpr uint8_t SlotCount = 32;
constexpr uint8_t GridColumns = 6;
constexpr int32_t StickThreshold = 30;
constexpr int32_t WheelIconSize = 16;

struct PoolEntry {
    uint16_t vanillaItem;
    std::string customKey;
};

struct ItemPool {
    std::string key;
    std::vector<PoolEntry> entries;
};

using WheelItems = std::vector<const S2HCustomItemDefinition*>;

struct ItemPage {
    uint16_t page;
    std::array<WheelItems, SlotCount> slots;
};

std::deque<ItemPool> sPools;
std::vector<ItemPage> sVisiblePages;
ItemPage sVanillaClaims = { S2H_ITEM_PAGE_VANILLA, {} };
std::optional<uint32_t> sLayoutFrame;
std::optional<uint16_t> sShownPage;
std::optional<uint16_t> sMaskPage;
int32_t sMaskWheelSlot = -1;
std::map<std::pair<uint16_t, uint8_t>, std::string> sWheelSelection;
std::string sSelectedKey;
bool sWasAPressConsumed = false;
int32_t sCyclingSlot = -1;
int32_t sItemAHold = 0;
int32_t sMaskAHold = 0;

const ItemPool* FindPool(const char* key) {
    if (key == nullptr) {
        return nullptr;
    }
    for (const auto& pool : sPools) {
        if (pool.key == key) {
            return &pool;
        }
    }
    return nullptr;
}

std::string PlacementCvar(const char* key, const char* field) {
    std::string result = CVAR_CUSTOM_ITEM_MANAGER("Layout.");
    constexpr char digits[] = "0123456789abcdef";
    for (const unsigned char* cursor = reinterpret_cast<const unsigned char*>(key); *cursor != 0; ++cursor) {
        result += digits[*cursor >> 4];
        result += digits[*cursor & 15];
    }
    return result + "." + field;
}

void PlayPauseSfx(u16 sfxId) {
    Audio_PlaySfx(sfxId);
}

const ItemPage* FindVisiblePage(uint16_t page) {
    for (const auto& visible : sVisiblePages) {
        if (visible.page == page) {
            return &visible;
        }
    }
    return nullptr;
}

void SortByPriority(std::array<WheelItems, SlotCount>& slots) {
    for (auto& items : slots) {
        std::stable_sort(items.begin(), items.end(),
                         [](const S2HCustomItemDefinition* a, const S2HCustomItemDefinition* b) {
                             return a->priority > b->priority;
                         });
    }
}

void BuildVisiblePages() {
    std::map<uint16_t, std::array<WheelItems, SlotCount>> pages;
    if (CustomEquipRegistry_GetCount() != 0) pages[S2H_ITEM_PAGE_EQUIPMENT_FIRST];
    for (uint32_t index = 0; index < CustomItemRegistry_GetCount(); ++index) {
        const S2HCustomItemDefinition* definition = CustomItemRegistry_GetAt(index);
        S2HCustomItemPlacement placement;
        if (definition == nullptr || CustomItemRegistry_IsVanillaUpgrade(definition) ||
            !CustomItemRegistry_IsOwned(definition->key) ||
            !KaleidoItemManager_GetPlacement(definition->key, &placement)) {
            continue;
        }
        if (CustomItemRegistry_IsQuestPage(placement.page)) {
            continue;
        }
        pages[placement.page][placement.slot].push_back(definition);
    }

    for (auto& items : sVanillaClaims.slots) {
        items.clear();
    }
    auto claims = pages.find(S2H_ITEM_PAGE_VANILLA);
    if (claims != pages.end()) {
        sVanillaClaims.slots = std::move(claims->second);
        SortByPriority(sVanillaClaims.slots);
        pages.erase(claims);
    }

    sVisiblePages.clear();
    for (auto& [page, slots] : pages) {
        SortByPriority(slots);
        sVisiblePages.push_back({ page, std::move(slots) });
    }
    if (sShownPage.has_value() && FindVisiblePage(*sShownPage) == nullptr) {
        sShownPage.reset();
    }
}

void RefreshVisiblePages() {
    const uint32_t frame = gPlayState != nullptr ? gPlayState->state.frames : 0;
    if (sLayoutFrame == frame) {
        return;
    }
    sLayoutFrame = frame;
    BuildVisiblePages();
}

const ItemPage* GetShownPage() {
    RefreshVisiblePages();
    return sShownPage.has_value() ? FindVisiblePage(*sShownPage) : nullptr;
}

const ItemPage* GetShownOrVanillaPage() {
    const ItemPage* page = GetShownPage();
    return page != nullptr ? page : &sVanillaClaims;
}

const S2HCustomItemDefinition* GetWheelSelection(const ItemPage& page, uint8_t slot) {
    const WheelItems& items = page.slots[slot];
    if (items.empty()) {
        return nullptr;
    }
    const auto selection = sWheelSelection.find({ page.page, slot });
    const std::string wheelSetting = CVAR_CUSTOM_ITEM_MANAGER("Wheel.") + std::to_string(page.page) + "." + std::to_string(slot);
    const char* saved = CVarGetString(wheelSetting.c_str(), "");
    if (selection != sWheelSelection.end()) {
        for (const auto* item : items) {
            if (selection->second == item->key) {
                return item;
            }
        }
    }
    for (const auto* item : items) if (std::strcmp(saved, item->key) == 0) return item;
    return items.front();
}

const S2HCustomItemDefinition* GetShownSlotItem(uint8_t slot) {
    return slot < SlotCount ? GetWheelSelection(*GetShownOrVanillaPage(), slot) : nullptr;
}

const S2HCustomItemDefinition* GetCursorShownItem(PlayState* play) {
    const int32_t slot = play->pauseCtx.cursorPoint[PAUSE_ITEM];
    return slot >= 0 && slot < SlotCount ? GetShownSlotItem(static_cast<uint8_t>(slot)) : nullptr;
}

void MoveCursorToFirstItem(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;
    for (uint8_t slot = 0; slot < SlotCount; ++slot) {
        if (KaleidoItemManager_GetSlotItem(slot) == ITEM_NONE) {
            continue;
        }
        pauseCtx->cursorPoint[PAUSE_ITEM] = slot;
        pauseCtx->cursorXIndex[PAUSE_ITEM] = slot % GridColumns;
        pauseCtx->cursorYIndex[PAUSE_ITEM] = slot / GridColumns;
        pauseCtx->cursorSlot[PAUSE_ITEM] = slot;
        pauseCtx->namedItem = PAUSE_ITEM_NONE;
        pauseCtx->nameDisplayTimer = 0;
        return;
    }
}

void EquipOnSameButtons(const char* previousKey, const char* nextKey) {
    for (uint8_t button = 0; button < S2H_ITEM_BUTTON_COUNT; ++button) {
        const char* equipped = CustomItemRegistry_GetEquippedKey(button);
        if (equipped == nullptr || std::strcmp(equipped, previousKey) != 0) {
            continue;
        }
        if (!CustomItemRegistry_Equip(button, nextKey)) {
            CustomItemRegistry_Unequip(button);
        }
    }
}

void TurnWheel(PlayState* play, const ItemPage& page, uint8_t slot, int32_t step) {
    const WheelItems& items = page.slots[slot];
    const S2HCustomItemDefinition* previous = GetWheelSelection(page, slot);
    const auto position = std::find(items.begin(), items.end(), previous) - items.begin();
    const auto count = static_cast<std::ptrdiff_t>(items.size());
    const S2HCustomItemDefinition* next = items[(position + step + count) % count];
    sWheelSelection[{ page.page, slot }] = next->key;
    EquipOnSameButtons(previous->key, next->key);
    play->pauseCtx.namedItem = PAUSE_ITEM_NONE;
}

void SyncSelectedItem(PlayState* play) {
    const S2HCustomItemDefinition* shown = GetCursorShownItem(play);
    const std::string key = shown != nullptr ? shown->key : std::string();
    const auto* selected = CustomItemRegistry_GetPauseItem();
    if (key == sSelectedKey && (selected == nullptr ? key.empty() : key == selected->key)) {
        return;
    }
    sSelectedKey = key;
    CustomItemRegistry_SelectPauseItem(key.empty() ? nullptr : key.c_str());
    play->pauseCtx.namedItem = PAUSE_ITEM_NONE;
}

void EquipCursorItem(bool* should, va_list args) {
    const u16 cursorSlot = va_arg(args, int);
    const u16 cursorItem = va_arg(args, int);
    if (cursorItem != ITEM_CUSTOM || gPlayState == nullptr) {
        return;
    }
    const S2HCustomItemDefinition* definition = GetShownSlotItem(static_cast<uint8_t>(cursorSlot));
    const bool isEquipped = definition != nullptr &&
                            CustomItemRegistry_Equip(KaleidoItemManager_ToHeldButton(gPlayState->pauseCtx.equipTargetCBtn),
                                                     definition->key);
    PlayPauseSfx(isEquipped ? NA_SE_SY_DECIDE : NA_SE_SY_ERROR);
    *should = false;
}

void DisplayCursorItemText(bool* should, va_list args) {
    const u16* cursorItem = va_arg(args, u16*);
    if (*cursorItem != ITEM_CUSTOM || gPlayState == nullptr) {
        return;
    }
    *should = false;
    if (sWasAPressConsumed) {
        return;
    }
    const PauseContext* pauseCtx = &gPlayState->pauseCtx;
    KaleidoItemManager_OpenPauseDescription(gPlayState, CustomItemRegistry_GetPauseItem(),
                                            pauseCtx->cursorYIndex[PAUSE_ITEM]);
}

void ResetShownPage() {
    bool migrated = false;
    for (uint32_t index = 0; index < CustomItemRegistry_GetCount(); ++index) {
        const auto* definition = CustomItemRegistry_GetAt(index);
        const auto version = PlacementCvar(definition->key, "DispositionVersion");
        int oldVersion=CVarGetInteger(version.c_str(),0);if(oldVersion>=2)continue;
        const auto pageKey=PlacementCvar(definition->key,"Page");const auto slotKey=PlacementCvar(definition->key,"Slot");
        int oldPage=CVarGetInteger(pageKey.c_str(),-1),oldSlot=CVarGetInteger(slotKey.c_str(),-1);
        const auto* equip=CustomEquipRegistry_Find(definition->key);
        if(equip&&equip->slot==S2H_EQUIP_SLOT_UPGRADE) {
            const int points[]={QUEST_SWORD,QUEST_SHIELD,QUEST_QUIVER,QUEST_BOMB_BAG};
            CVarSetInteger(pageKey.c_str(),S2H_ITEM_PAGE_QUEST_VANILLA);
            CVarSetInteger(slotKey.c_str(),points[std::min<int>(equip->row,3)]);
        } else if(oldVersion==1&&oldPage>=S2H_ITEM_PAGE_EQUIPMENT_FIRST&&oldSlot>=0&&oldSlot<24) {
            CVarSetInteger(slotKey.c_str(),oldSlot/6*6+std::max(0,oldSlot%6-2));
        }
        bool medal=std::strncmp(definition->key,"nei.medallion.",14)==0;
        bool stone=std::strncmp(definition->key,"nei.spiritual_stone.",20)==0;
        if((oldPage==1||oldPage==S2H_ITEM_PAGE_QUEST_VANILLA+1)&&
           ((medal&&oldSlot==definition->preferredSlot+3)||(stone&&oldSlot==definition->preferredSlot-18))) {
            CVarSetInteger(pageKey.c_str(),S2H_ITEM_PAGE_QUEST_VANILLA+1);
            CVarSetInteger(slotKey.c_str(),definition->preferredSlot);
        }
        CVarSetInteger(version.c_str(),2);
        migrated = true;
    }
    if (migrated) Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
    sShownPage.reset();
    sMaskPage.reset();
    sMaskWheelSlot = -1;
    sLayoutFrame.reset();
    sWheelSelection.clear();
    sSelectedKey.clear();
    sCyclingSlot = -1;
}

void RegisterHooks() {
    REGISTER_VB_SHOULD(VB_KALEIDO_EQUIP_ITEM_TO_BUTTON, { EquipCursorItem(should, args); });
    REGISTER_VB_SHOULD(VB_KALEIDO_DISPLAY_ITEM_TEXT, { DisplayCursorItemText(should, args); });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSaveLoad>([](s16) { ResetShownPage(); });
}

} // namespace

uint8_t KaleidoItemManager_ToHeldButton(uint16_t equipTargetCBtn) {
    return equipTargetCBtn <= PAUSE_EQUIP_C_RIGHT ? equipTargetCBtn + S2H_ITEM_BUTTON_C_FIRST
                                                  : equipTargetCBtn - PAUSE_EQUIP_D_RIGHT + S2H_ITEM_BUTTON_DPAD_FIRST;
}

// Outside the anonymous namespace: OPEN_DISPS redeclares the interpolation hooks in its body, and inside one they
// get C++ linkage and fail to link.
static void DrawNeighborIcon(PlayState* play, uint8_t slot, bool isRight, const S2HCustomItemDefinition* neighbor,
                             bool mask = false) {
    const char* icon = CustomItemRegistry_ResolveTexture(neighbor->key, S2H_ITEM_ICON_INVENTORY);
    if (icon == nullptr) {
        return;
    }
    const Vtx* cell = &(mask ? play->pauseCtx.maskVtx : play->pauseCtx.itemVtx)[slot * 4];
    const int16_t left = cell[0].v.ob[0] + (isRight ? 32 - WheelIconSize / 2 : -WheelIconSize / 2);
    const int16_t top = cell[0].v.ob[1] - 32 + WheelIconSize + 2;
    Vtx* vertices = static_cast<Vtx*>(GRAPH_ALLOC(play->state.gfxCtx, 4 * sizeof(Vtx)));
    const Vtx corners[4] = {
        VTX(left, top, 0, 0, 0, 255, 255, 255, 255),
        VTX(left + WheelIconSize, top, 0, 32 << 5, 0, 255, 255, 255, 255),
        VTX(left, top - WheelIconSize, 0, 0, 32 << 5, 255, 255, 255, 255),
        VTX(left + WheelIconSize, top - WheelIconSize, 0, 32 << 5, 32 << 5, 255, 255, 255, 255),
    };
    std::copy(corners, corners + 4, vertices);

    OPEN_DISPS(play->state.gfxCtx);
    gSPVertex(POLY_OPA_DISP++, reinterpret_cast<uintptr_t>(vertices), 4, 0);
    KaleidoScope_DrawTexQuadRGBA32(play->state.gfxCtx, const_cast<char*>(icon), 32, 32, 0);
    CLOSE_DISPS(play->state.gfxCtx);
}

void KaleidoItemManager_Init() {
    RegisterHooks();
}

bool KaleidoItemManager_RegisterPool(const char* key, const S2HItemPoolEntry* entries, uint32_t count) {
    if (key == nullptr || key[0] == '\0' || FindPool(key) != nullptr || (count != 0 && entries == nullptr)) {
        return false;
    }
    ItemPool pool;
    pool.key = key;
    for (uint32_t index = 0; index < count; ++index) {
        const auto& entry = entries[index];
        if (entry.customKey != nullptr && entry.customKey[0] != '\0') {
            pool.entries.push_back({ ITEM_NONE, entry.customKey });
        } else if (entry.vanillaItem < ITEM_CUSTOM) {
            pool.entries.push_back({ entry.vanillaItem, "" });
        } else {
            return false;
        }
    }
    sPools.push_back(std::move(pool));
    return true;
}

uint32_t KaleidoItemManager_GetPoolCount(void) {
    return static_cast<uint32_t>(sPools.size());
}

const char* KaleidoItemManager_GetPoolKey(uint32_t index) {
    return index < sPools.size() ? sPools[index].key.c_str() : nullptr;
}

uint32_t KaleidoItemManager_GetPoolSize(const char* key) {
    const ItemPool* pool = FindPool(key);
    return pool == nullptr ? 0 : static_cast<uint32_t>(pool->entries.size());
}

bool KaleidoItemManager_GetPoolEntry(const char* key, uint32_t index, S2HItemPoolEntry* entry) {
    const ItemPool* pool = FindPool(key);
    if (pool == nullptr || entry == nullptr || index >= pool->entries.size()) {
        return false;
    }
    const PoolEntry& stored = pool->entries[index];
    *entry = { stored.vanillaItem, stored.customKey.empty() ? nullptr : stored.customKey.c_str() };
    return true;
}

bool KaleidoItemManager_GetPlacement(const char* key, S2HCustomItemPlacement* placement) {
    const S2HCustomItemDefinition* definition = CustomItemRegistry_Find(key);
    if (definition == nullptr || placement == nullptr || CVarGetInteger(PlacementCvar(key, "Hidden").c_str(), 0) ||
        (definition->presentationFlags & S2H_ITEM_HIDE_FROM_KALEIDO) != 0) {
        return false;
    }
    const S2HCustomEquipDefinition* equip = CustomEquipRegistry_Find(key);
    const int defaultPage = equip != nullptr && equip->slot == S2H_EQUIP_SLOT_UPGRADE ? S2H_ITEM_PAGE_QUEST_VANILLA : equip != nullptr ? S2H_ITEM_PAGE_EQUIPMENT_FIRST +
        (equip->page == S2H_EQUIP_PAGE_VANILLA ? 0 : equip->page) : definition->preferredPage;
    const int page = CVarGetInteger(PlacementCvar(key, "Page").c_str(), defaultPage);
    const int upgradePoints[] = { QUEST_SWORD, QUEST_SHIELD, QUEST_QUIVER, QUEST_BOMB_BAG };
    const int defaultSlot = equip != nullptr && equip->slot == S2H_EQUIP_SLOT_UPGRADE ? upgradePoints[std::min<int>(equip->row,3)] : equip != nullptr ? std::min<int>(equip->row * 6 + equip->column, 23) : definition->preferredSlot;
    const int slot = CVarGetInteger(PlacementCvar(key, "Slot").c_str(), defaultSlot);
    placement->page = static_cast<uint16_t>(std::clamp(page, 0, static_cast<int>(UINT16_MAX)));
    const int cellCount=CustomItemRegistry_IsQuestPage(placement->page)?(placement->page==S2H_ITEM_PAGE_QUEST_VANILLA?QUEST_HEART_PIECE+1:25):ITEM_NUM_SLOTS;
    placement->slot = static_cast<uint8_t>(std::clamp(slot,0,cellCount-1));
    return true;
}

bool KaleidoItemManager_SetPlacement(const char* key, const S2HCustomItemPlacement* placement) {
    if (placement == nullptr || placement->slot >= SlotCount || CustomItemRegistry_Find(key) == nullptr) {
        return false;
    }
    if ((placement->page==S2H_ITEM_PAGE_QUEST_VANILLA&&placement->slot>QUEST_HEART_PIECE)||(CustomItemRegistry_IsQuestPage(placement->page) && placement->page != S2H_ITEM_PAGE_QUEST_VANILLA && placement->slot >= 25) || placement->page == UINT16_MAX || (!CustomItemRegistry_IsQuestPage(placement->page) && placement->slot >= ITEM_NUM_SLOTS)) return false;
    CVarClear(PlacementCvar(key, "Hidden").c_str());
    CVarSetInteger(PlacementCvar(key, "Page").c_str(), placement->page);
    CVarSetInteger(PlacementCvar(key, "Slot").c_str(), placement->slot);
    CVarSetInteger(PlacementCvar(key, "DispositionVersion").c_str(), 2);
    sLayoutFrame.reset();
    Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
    return true;
}

void KaleidoItemManager_ResetPlacement(const char* key) {
    if (CustomItemRegistry_Find(key) == nullptr) {
        return;
    }
    CVarClear(PlacementCvar(key, "Hidden").c_str());
    CVarClear(PlacementCvar(key, "Page").c_str());
    CVarClear(PlacementCvar(key, "Slot").c_str());
    sLayoutFrame.reset();
    Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
}

uint32_t KaleidoItemManager_GetSlotItemCount(uint16_t page, uint8_t slot) {
    RefreshVisiblePages();
    const ItemPage* visible = FindVisiblePage(page);
    return visible != nullptr && slot < SlotCount ? static_cast<uint32_t>(visible->slots[slot].size()) : 0;
}

const char* KaleidoItemManager_GetSlotItemAt(uint16_t page, uint8_t slot, uint32_t index) {
    if (index >= KaleidoItemManager_GetSlotItemCount(page, slot)) {
        return nullptr;
    }
    return FindVisiblePage(page)->slots[slot][index]->key;
}

bool KaleidoItemManager_IsVanillaPageShown(void) {
    return GetShownPage() == nullptr;
}

uint16_t KaleidoItemManager_ResolveSlotItem(uint8_t slot, uint16_t vanillaItem) {
    if (slot >= SlotCount) {
        return vanillaItem;
    }
    if (GetShownSlotItem(slot) != nullptr) {
        return ITEM_CUSTOM;
    }
    return KaleidoItemManager_IsVanillaPageShown() ? vanillaItem : ITEM_NONE;
}

uint16_t KaleidoItemManager_GetSlotItem(uint8_t slot) {
    if (slot >= ITEM_NUM_SLOTS) {
        return ITEM_NONE;
    }
    const uint8_t vanillaSlot = KaleidoItemManager_GetVanillaSlot(slot);
    return KaleidoItemManager_ResolveSlotItem(slot, gSaveContext.save.saveInfo.inventory.items[vanillaSlot]);
}

// MM draws and indexes its item grid by slot, so there is no cell permutation to undo yet.
uint8_t KaleidoItemManager_GetVanillaSlot(uint8_t cell) {
    return cell;
}

void* KaleidoItemManager_GetSlotIcon(uint8_t slot) {
    const S2HCustomItemDefinition* definition = GetShownSlotItem(slot);
    const char* icon = definition != nullptr ? CustomItemRegistry_ResolveTexture(definition->key, S2H_ITEM_ICON_INVENTORY)
                                             : nullptr;
    return const_cast<char*>(icon != nullptr ? icon : gEmptyTexture);
}

bool KaleidoItemManager_IsSlotFormAllowed(uint8_t slot) {
    const S2HCustomItemDefinition* definition = GetShownSlotItem(slot);
    return definition != nullptr && CustomItemRegistry_IsFormAllowed(definition->key);
}

const char* KaleidoItemManager_GetPauseNameTexture(void) {
    const S2HCustomItemDefinition* definition = CustomItemRegistry_GetPauseItem();
    return definition == nullptr ? nullptr : CustomItemRegistry_ResolveTexture(definition->key, S2H_ITEM_NAME_TEXTURE);
}

bool KaleidoItemManager_OpenPauseDescription(PlayState* play, const S2HCustomItemDefinition* definition,
                                             int16_t cursorRow) {
    if (play == nullptr || definition == nullptr || definition->pauseText == nullptr) {
        return false;
    }
    CustomMessage::SetActiveCustomMessage(definition->pauseText);
    play->pauseCtx.itemDescriptionOn = true;
    func_801514B0(play, CUSTOM_MESSAGE_ID, cursorRow < 2 ? 3 : 1);
    return true;
}

bool KaleidoItemManager_IsPauseEffectActive(const S2HCustomItemDefinition* definition) {
    if(definition==nullptr)return true;
    if(definition->isPauseActive)return definition->isPauseActive(definition->key);
    const auto* equip=CustomEquipRegistry_Find(definition->key);
    return equip==nullptr || equip->slot!=S2H_EQUIP_SLOT_UPGRADE || !equip->toggles || CustomEquipRegistry_IsToggleOn(equip->key);
}

bool KaleidoItemManager_IsSlotEffectActive(uint8_t slot) {
    return KaleidoItemManager_IsPauseEffectActive(GetShownSlotItem(slot));
}

void KaleidoItemManager_ToggleEquipment(const S2HCustomItemDefinition* definition) {
    if(definition && definition->onPauseToggle) {
        definition->onPauseToggle(definition->key);
        PlayPauseSfx(NA_SE_SY_DECIDE);
        return;
    }
    const S2HCustomEquipDefinition* equip = definition == nullptr ? nullptr : CustomEquipRegistry_Find(definition->key);
    if (equip == nullptr) {
        return;
    }
    bool changed = true;
    if (equip->slot == S2H_EQUIP_SLOT_UPGRADE) {
        changed = CustomEquipRegistry_SetToggle(equip->key, !CustomEquipRegistry_IsToggleOn(equip->key));
    } else if (CustomEquipRegistry_IsWorn(equip->key)) {
        CustomEquipRegistry_TakeOff(equip->slot);
    } else {
        changed = CustomEquipRegistry_Wear(equip->key);
    }
    PlayPauseSfx(changed ? NA_SE_SY_DECIDE : NA_SE_SY_ERROR);
}

bool KaleidoItemManager_CycleItemPage(PlayState* play) {
    RefreshVisiblePages();
    if (play == nullptr || sVisiblePages.empty() || sCyclingSlot != -1) {
        return false;
    }
    std::vector<ItemPage> itemPages;
    for (const auto& page : sVisiblePages) {
        if (page.page < S2H_ITEM_PAGE_EQUIPMENT_FIRST) itemPages.push_back(page);
    }
    if (itemPages.empty()) return false;
    auto next = itemPages.begin();
    if (sShownPage.has_value()) {
        next = std::find_if(itemPages.begin(), itemPages.end(),
                            [](const ItemPage& visible) { return visible.page == *sShownPage; });
        next = next == itemPages.end() ? itemPages.end() : next + 1;
    }
    if (next == itemPages.end()) {
        sShownPage.reset();
    } else {
        sShownPage = next->page;
    }
    PlayPauseSfx(NA_SE_SY_WIN_SCROLL_RIGHT);
    if (play->pauseCtx.cursorSpecialPos == 0) {
        MoveCursorToFirstItem(play);
    }
    return true;
}

// A on a cell with several items starts or stops turning its wheel; the stick turns it and the cursor stays put.
void KaleidoItemManager_HandleWheel(PlayState* play) {
    const ItemPage* page = GetShownOrVanillaPage();
    int slot = play->pauseCtx.cursorPoint[PAUSE_ITEM];
    auto* input = CONTROLLER1(&play->state);
    if (slot < 0 || slot >= ITEM_NUM_SLOTS) return;
    if (page->slots[slot].size() < 2) {
        if(input->press.button & BTN_A) KaleidoItemManager_ToggleEquipment(GetWheelSelection(*page,slot));
        sCyclingSlot = -1; sItemAHold = 0; return;
    }
    if (input->press.button & BTN_A) {
        TurnWheel(play,*page,slot,1);
        PlayPauseSfx(NA_SE_SY_CURSOR);
        sWasAPressConsumed = true;
        sItemAHold = 0;
    }
    if (input->cur.button & BTN_A) {
        if (++sItemAHold >= 10) sCyclingSlot = slot;
    } else { if(sItemAHold>=10)KaleidoItemManager_ToggleEquipment(GetWheelSelection(*page,slot)); sCyclingSlot = -1; sItemAHold = 0; }
    if (sCyclingSlot == slot) {
        int step = play->pauseCtx.stickAdjX > StickThreshold ? 1 : play->pauseCtx.stickAdjX < -StickThreshold ? -1 : 0;
        if (step) TurnWheel(play,*page,slot,step);
        play->pauseCtx.stickAdjX = play->pauseCtx.stickAdjY = 0;
    }
}

// Follow edited positions rather than assuming that cell numbers remain in a rectangular grid.
static int SpatialCell(uint16_t page, int slot, int dx, int dy) {
    S2HLayoutCell current; ModLayout_GetCell(page,slot,&current);
    float best = 100000.0f; int result = slot;
    for (int candidate=0; candidate<ITEM_NUM_SLOTS; ++candidate) {
        if (candidate==slot) continue;
        S2HLayoutCell cell; ModLayout_GetCell(page,candidate,&cell);
        float x = cell.x+cell.width/2.0f-current.x-current.width/2.0f;
        float y = cell.y-cell.height/2.0f-current.y+current.height/2.0f;
        float along=x*dx+y*dy, across=dx?std::abs(y):std::abs(x);
        if(along>1.0f && along+across*3.0f<best) {best=along+across*3.0f;result=candidate;}
    }
    return result;
}

// L turns to the next extra item page; it is free on the item page (R and Z already turn the pause pages).
void KaleidoItemManager_Update(PlayState* play) {
    sWasAPressConsumed = false;
    RefreshVisiblePages();
    if (CHECK_BTN_ALL(CONTROLLER1(&play->state)->press.button, BTN_L) &&
        !CVarGetInteger("gDeveloperTools.DebugEnabled", 0) && play->pauseCtx.cursorSpecialPos == 0) {
        KaleidoItemManager_CycleItemPage(play);
    }
    KaleidoItemManager_HandleWheel(play);
    auto* pause=&play->pauseCtx;
    if(pause->cursorSpecialPos==0 && (std::abs(pause->stickAdjX)>StickThreshold || std::abs(pause->stickAdjY)>StickThreshold)) {
        int dx=pause->stickAdjX>StickThreshold?1:pause->stickAdjX<-StickThreshold?-1:0;
        int dy=dx?0:pause->stickAdjY>StickThreshold?1:-1;
        const auto* shown=GetShownOrVanillaPage();
        int old=pause->cursorPoint[PAUSE_ITEM], next=SpatialCell(shown->page,old,dx,dy);
        if(next!=old) {pause->cursorPoint[PAUSE_ITEM]=pause->cursorSlot[PAUSE_ITEM]=next;
            pause->cursorXIndex[PAUSE_ITEM]=next%6;pause->cursorYIndex[PAUSE_ITEM]=next/6;PlayPauseSfx(NA_SE_SY_CURSOR);}
        else if(dx) KaleidoScope_MoveCursorToSpecialPos(play,dx<0?PAUSE_CURSOR_PAGE_LEFT:PAUSE_CURSOR_PAGE_RIGHT);
        pause->stickAdjX=pause->stickAdjY=0;
    }
    SyncSelectedItem(play);
}

void KaleidoItemManager_DrawWheels(PlayState* play) {
    const ItemPage* page = GetShownOrVanillaPage();
    for (uint8_t slot = 0; slot < SlotCount; ++slot) {
        const WheelItems& items = page->slots[slot];
        if (items.size() < 2) {
            continue;
        }
        const auto count = static_cast<std::ptrdiff_t>(items.size());
        const auto position = std::find(items.begin(), items.end(), GetWheelSelection(*page, slot)) - items.begin();
        const S2HCustomItemDefinition* left = items[(position + count - 1) % count];
        const S2HCustomItemDefinition* right = items[(position + 1) % count];
        DrawNeighborIcon(play, slot, false, left);
        if (right != left) {
            DrawNeighborIcon(play, slot, true, right);
        }
    }
}

namespace {
const ItemPage* MaskEquipmentPage() {
    RefreshVisiblePages();
    const ItemPage* page = sMaskPage.has_value() ? FindVisiblePage(*sMaskPage) : nullptr;
    if (page == nullptr) sMaskPage.reset();
    return page;
}
}

static int NativeEquipmentItem(int slot) {
    if(slot==0) {int value=CustomEquipRegistry_GetVanillaBase(S2H_EQUIP_SLOT_SWORD);return value?ITEM_SWORD_KOKIRI+value-1:ITEM_NONE;}
    if(slot==6) {int value=CustomEquipRegistry_GetVanillaBase(S2H_EQUIP_SLOT_SHIELD);return value?ITEM_SHIELD_HERO+value-1:ITEM_NONE;}
    return ITEM_NONE;
}

bool KaleidoMaskEquipment_Update(PlayState* play) {
    PauseContext* pause = &play->pauseCtx;
    Input* input = CONTROLLER1(&play->state);
    const ItemPage* page = MaskEquipmentPage();
    if (pause->state != PAUSE_STATE_MAIN || pause->mainState != PAUSE_MAIN_STATE_IDLE ||
        pause->pageIndex != PAUSE_MASK || pause->itemDescriptionOn) return page != nullptr;
    if (CHECK_BTN_ALL(input->press.button, BTN_L) && !CVarGetInteger("gDeveloperTools.DebugEnabled", 0)) {
        auto next = std::find_if(sVisiblePages.begin(), sVisiblePages.end(), [](const ItemPage& candidate) {
            return candidate.page >= S2H_ITEM_PAGE_EQUIPMENT_FIRST &&
                   (!sMaskPage.has_value() || candidate.page > *sMaskPage);
        });
        if (next == sVisiblePages.end()) sMaskPage.reset();
        else sMaskPage = next->page;
        sMaskWheelSlot = -1;
        page = MaskEquipmentPage();
        pause->cursorSpecialPos = 0;
        pause->namedItem = PAUSE_ITEM_NONE;
        PlayPauseSfx(NA_SE_SY_WIN_SCROLL_RIGHT);
        if (page == nullptr) {
            CustomItemRegistry_SelectPauseItem(nullptr);
            pause->cursorItem[PAUSE_MASK] = PAUSE_ITEM_NONE;
            return false;
        }
        for (int slot = 0; slot < SlotCount; ++slot) {
            if (!page->slots[slot].empty()) {
                pause->cursorPoint[PAUSE_MASK] = slot;
                break;
            }
        }
    }
    if (page == nullptr) {
        if (pause->cursorSpecialPos == 0 &&
            (std::abs(pause->stickAdjX) > StickThreshold || std::abs(pause->stickAdjY) > StickThreshold)) {
            const int dx = pause->stickAdjX > StickThreshold ? 1 : pause->stickAdjX < -StickThreshold ? -1 : 0;
            const int dy = dx ? 0 : pause->stickAdjY > StickThreshold ? 1 : -1;
            const int old = pause->cursorPoint[PAUSE_MASK];
            const int next = SpatialCell(UINT16_MAX, old, dx, dy);
            if (next != old) {
                pause->cursorPoint[PAUSE_MASK] = pause->cursorSlot[PAUSE_MASK] = next;
                pause->cursorXIndex[PAUSE_MASK] = next % GridColumns;
                pause->cursorYIndex[PAUSE_MASK] = next / GridColumns;
                PlayPauseSfx(NA_SE_SY_CURSOR);
            } else if (dx) {
                KaleidoScope_MoveCursorToSpecialPos(play, dx < 0 ? PAUSE_CURSOR_PAGE_LEFT : PAUSE_CURSOR_PAGE_RIGHT);
            }
            pause->stickAdjX = pause->stickAdjY = 0;
        }
        return false;
    }
    int slot = std::clamp<int>(pause->cursorPoint[PAUSE_MASK], 0, SlotCount - 1);
    if (pause->cursorSpecialPos != 0) {
        if ((pause->cursorSpecialPos == PAUSE_CURSOR_PAGE_LEFT && pause->stickAdjX > StickThreshold) ||
            (pause->cursorSpecialPos == PAUSE_CURSOR_PAGE_RIGHT && pause->stickAdjX < -StickThreshold)) {
            pause->cursorSpecialPos = 0;
        } else {
            CustomItemRegistry_SelectPauseItem(nullptr);
            pause->cursorItem[PAUSE_MASK] = PAUSE_ITEM_NONE;
            return true;
        }
    } else if (sMaskWheelSlot == slot) {
        int step = pause->stickAdjX > StickThreshold ? 1 : (pause->stickAdjX < -StickThreshold ? -1 : 0);
        if (step && page->slots[slot].size() > 1) {
            TurnWheel(play, *page, slot, step);
            PlayPauseSfx(NA_SE_SY_CURSOR);
        }
        pause->stickAdjX = pause->stickAdjY = 0;
    } else {
        int dx=pause->stickAdjX>StickThreshold?1:pause->stickAdjX<-StickThreshold?-1:0;
        int dy=dx?0:pause->stickAdjY>StickThreshold?1:pause->stickAdjY<-StickThreshold?-1:0;
        int next=(dx||dy)?SpatialCell(page->page,slot,dx,dy):slot;
        if(next==slot && dx) {
            KaleidoScope_MoveCursorToSpecialPos(play,dx<0?PAUSE_CURSOR_PAGE_LEFT:PAUSE_CURSOR_PAGE_RIGHT);
            CustomItemRegistry_SelectPauseItem(nullptr);pause->cursorItem[PAUSE_MASK]=PAUSE_ITEM_NONE;return true;
        }
        if(next!=slot)PlayPauseSfx(NA_SE_SY_CURSOR);
        slot=next;
    }
    pause->cursorPoint[PAUSE_MASK] = pause->cursorSlot[PAUSE_MASK] = slot;
    pause->cursorXIndex[PAUSE_MASK] = slot % GridColumns;
    pause->cursorYIndex[PAUSE_MASK] = slot / GridColumns;
    const S2HCustomItemDefinition* item = GetWheelSelection(*page, slot);
    const auto* previous = CustomItemRegistry_GetPauseItem();
    if ((previous == nullptr) != (item == nullptr) ||
        (previous != nullptr && item != nullptr && std::strcmp(previous->key, item->key) != 0)) {
        pause->namedItem = PAUSE_ITEM_NONE;
        pause->nameDisplayTimer = 0;
    }
    CustomItemRegistry_SelectPauseItem(item == nullptr ? nullptr : item->key);
    pause->cursorItem[PAUSE_MASK] = item == nullptr ? PAUSE_ITEM_NONE : ITEM_CUSTOM;
    pause->cursorColorSet = PAUSE_CURSOR_COLOR_SET_YELLOW;
    pause->nameColorSet = PAUSE_NAME_COLOR_SET_WHITE;
    if (item == nullptr) {
        int native=NativeEquipmentItem(slot);
        pause->cursorItem[PAUSE_MASK]=native==ITEM_NONE?PAUSE_ITEM_NONE:native;
        if(native!=ITEM_NONE && (input->press.button & BTN_A)) {
            CustomEquipRegistry_TakeOff(slot==0?S2H_EQUIP_SLOT_SWORD:S2H_EQUIP_SLOT_SHIELD);
            PlayPauseSfx(NA_SE_SY_DECIDE);
        }
        return true;
    }
    if (CHECK_BTN_ALL(input->press.button, BTN_CUP)) {
        KaleidoItemManager_OpenPauseDescription(play,item,slot / GridColumns);
    }
    if (input->press.button & BTN_A) {
        sMaskAHold = 0;
        if (page->slots[slot].size() > 1) TurnWheel(play,*page,slot,1);
        else KaleidoItemManager_ToggleEquipment(item);
    }
    if (input->cur.button & BTN_A) {
        if (++sMaskAHold >= 10 && page->slots[slot].size() > 1) sMaskWheelSlot = slot;
    } else {
        if (sMaskWheelSlot == slot) KaleidoItemManager_ToggleEquipment(GetWheelSelection(*page,slot));
        sMaskWheelSlot = -1; sMaskAHold = 0;
    }
    const uint32_t buttons[] = { BTN_CLEFT, BTN_CDOWN, BTN_CRIGHT, BTN_DRIGHT, BTN_DLEFT, BTN_DDOWN, BTN_DUP };
    for (int i = 0; i < 7; ++i) {
        if (i >= 3 && !CVarGetInteger("gEnhancements.Dpad.DpadEquips", 0)) continue;
        if (CHECK_BTN_ALL(input->press.button, buttons[i])) {
            bool equipped = CustomItemRegistry_Equip(i < 3 ? i + 1 : i + 2, item->key);
            PlayPauseSfx(equipped ? NA_SE_SY_DECIDE : NA_SE_SY_ERROR);
        }
    }
    return true;
}

bool KaleidoMaskEquipment_Draw(PlayState* play) {
    const ItemPage* page = MaskEquipmentPage();
    if (page == nullptr) return false;
    PauseContext* pause = &play->pauseCtx;
    ModLayout_ApplyCells(play,page->page,pause->maskVtx,ITEM_NUM_SLOTS);
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL42_Opa(play->state.gfxCtx);
    gDPSetCombineMode(POLY_OPA_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    for (int slot = 0; slot < SlotCount; ++slot) {
        const auto* item = GetWheelSelection(*page, slot);
        bool owned = item != nullptr;
        if (item == nullptr) {
            int native=NativeEquipmentItem(slot);
            if(native!=ITEM_NONE) {
                gDPSetPrimColor(POLY_OPA_DISP++,0,0,255,255,255,pause->alpha);
                gSPVertex(POLY_OPA_DISP++,reinterpret_cast<uintptr_t>(&pause->maskVtx[slot*4]),4,0);
                KaleidoScope_DrawTexQuadRGBA32(play->state.gfxCtx,gItemIcons[native],32,32,0);
            }
            continue;
        }
        const char* icon = CustomItemRegistry_ResolveTexture(item->key, S2H_ITEM_ICON_INVENTORY);
        if (icon == nullptr) continue;
        bool worn = CustomEquipRegistry_IsWorn(item->key) || CustomEquipRegistry_IsToggleOn(item->key);
        gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, worn ? 160 : 255, 255, worn ? 160 : 255, KaleidoItemManager_IsPauseEffectActive(item) ? pause->alpha : pause->alpha/2);
        bool restricted = !owned || !CustomItemRegistry_IsFormAllowed(item->key);
        if (restricted) {
            gDPSetGrayscaleColor(POLY_OPA_DISP++, 109, 109, 109, 255);
            gSPGrayscale(POLY_OPA_DISP++, true);
        }
        gSPVertex(POLY_OPA_DISP++, reinterpret_cast<uintptr_t>(&pause->maskVtx[slot * 4]), 4, 0);
        KaleidoScope_DrawTexQuadRGBA32(play->state.gfxCtx, const_cast<char*>(icon), 32, 32, 0);
        if (restricted) gSPGrayscale(POLY_OPA_DISP++, false);
    }
    KaleidoScope_SetCursorVtxPos(pause, std::clamp<int>(pause->cursorSlot[PAUSE_MASK], 0, SlotCount - 1) * 4,
                               pause->maskVtx);
    CLOSE_DISPS(play->state.gfxCtx);
    for (uint8_t slot = 0; slot < SlotCount; ++slot) {
        const auto& items = page->slots[slot];
        if (items.size() < 2) continue;
        const auto count = static_cast<std::ptrdiff_t>(items.size());
        const auto position = std::find(items.begin(), items.end(), GetWheelSelection(*page, slot)) - items.begin();
        DrawNeighborIcon(play, slot, false, items[(position + count - 1) % count], true);
        DrawNeighborIcon(play, slot, true, items[(position + 1) % count], true);
    }
    return true;
}

void KaleidoItemManager_SetPauseVisible(const char* key, bool visible) {
    CVarSetInteger(PlacementCvar(key,"Hidden").c_str(), visible ? 0 : 1);
    sLayoutFrame.reset();
    Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
}
bool KaleidoItemManager_SetWheelSelection(uint16_t page, uint8_t slot, const char* key) {
    if (CustomItemRegistry_Find(key) == nullptr) return false;
    sWheelSelection[{page,slot}] = key;
    const std::string setting = CVAR_CUSTOM_ITEM_MANAGER("Wheel.") + std::to_string(page) + "." + std::to_string(slot);
    CVarSetString(setting.c_str(),key);
    Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
    return true;
}
void KaleidoItemManager_ApplyGeometry(PlayState* play) {
    const auto* page = GetShownPage();
    ModLayout_ApplyCells(play,page == nullptr ? S2H_ITEM_PAGE_VANILLA : page->page,play->pauseCtx.itemVtx,ITEM_NUM_SLOTS);
}
