#include "CustomEquipRegistry.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <spdlog/spdlog.h>
#include <ship/Context.h>
#include <ship/debug/Console.h>

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ModApi/ConsoleArguments.h"
#include "2s2h/SaveManager/SaveSections.h"

extern "C" {
#include "z64.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
}

namespace {

constexpr const char* SaveSection = "customEquipment";
constexpr size_t OwnedKeyLimit = 4096;
constexpr size_t MinDefinitionSize = offsetof(S2HCustomEquipDefinition, randomizer) + sizeof(void*);

struct RegisteredEquip {
    std::string owner;
    std::string key;
    std::string iconPath;
    std::string namePath;
    std::string pauseText;
    std::string getItemText;
    std::string getItemModelPath;
    S2HCustomEquipDefinition definition = {};
};

std::string sCurrentMod;
std::vector<std::unique_ptr<RegisteredEquip>> sEquipment;
std::unordered_map<std::string, RegisteredEquip*> sByKey;
std::unordered_set<std::string> sOwned;
std::unordered_set<std::string> sToggled;
std::array<std::string, S2H_EQUIP_SLOT_MAX> sWorn;
// MM keeps no vanilla ownership bits: the equipped value is the only record of the sword and shield the player has.
std::array<uint16_t, S2H_EQUIP_SLOT_MAX> sRestoreValue = {};
std::array<uint16_t, S2H_EQUIP_SLOT_MAX> sAppliedValue = {};
std::string sSwordOnButton;
bool sIsSwordButtonDirty = false;

RegisteredEquip* Find(const std::string& key) {
    auto entry = sByKey.find(key);
    return entry == sByKey.end() ? nullptr : entry->second;
}

void SetString(const char*& target, std::string& storage, const char* value) {
    storage = value == nullptr ? "" : value;
    target = storage.empty() ? nullptr : storage.c_str();
}

bool IsVanillaSlot(uint8_t slot) {
    return slot == S2H_EQUIP_SLOT_SWORD || slot == S2H_EQUIP_SLOT_SHIELD;
}

bool IsWearableSlot(uint8_t slot) {
    return slot < S2H_EQUIP_SLOT_UPGRADE;
}

uint16_t ReadVanillaValue(uint8_t slot) {
    return GET_CUR_EQUIP_VALUE(slot == S2H_EQUIP_SLOT_SWORD ? EQUIP_TYPE_SWORD : EQUIP_TYPE_SHIELD);
}

uint16_t GetVanillaValueLimit(uint8_t slot) {
    return slot == S2H_EQUIP_SLOT_SWORD ? EQUIP_VALUE_SWORD_MAX : EQUIP_VALUE_SHIELD_MAX;
}

bool IsFormAllowed(const S2HCustomEquipDefinition* definition) {
    if (definition == nullptr) {
        return false;
    }
    return definition->formMask == 0 ||
           (definition->formMask & S2H_CUSTOM_ITEM_FORM_BIT(gSaveContext.save.playerForm)) != 0;
}

void WriteVanillaValue(uint8_t slot, uint16_t value) {
    SET_EQUIP_VALUE(slot == S2H_EQUIP_SLOT_SWORD ? EQUIP_TYPE_SWORD : EQUIP_TYPE_SHIELD, value);
    if (gPlayState != nullptr && GET_PLAYER(gPlayState) != nullptr) {
        Player_SetEquipmentData(gPlayState, GET_PLAYER(gPlayState));
    }
}

uint8_t GetSwordItem(uint16_t swordValue) {
    if (swordValue == EQUIP_VALUE_SWORD_NONE) {
        return ITEM_NONE;
    }
    return ITEM_SWORD_KOKIRI + swordValue - EQUIP_VALUE_SWORD_KOKIRI;
}

bool IsSwordButtonItem(uint8_t item) {
    return item == ITEM_NONE || (item >= ITEM_SWORD_KOKIRI && item <= ITEM_SWORD_DEITY);
}

void UnbindSwordFromButton() {
    if (sSwordOnButton.empty()) {
        return;
    }
    // A vanilla gift overwrites the B item but leaves the stale key behind, so a null binding is also ours to clear.
    const char* boundKey = CustomItemRegistry_GetEquippedKey(S2H_ITEM_BUTTON_B);
    if (boundKey == nullptr || sSwordOnButton == boundKey) {
        CustomItemRegistry_Unequip(S2H_ITEM_BUTTON_B);
    }
    sSwordOnButton.clear();
}

void RestoreVanillaSwordButton() {
    if (!IsSwordButtonItem(BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_B))) {
        return;
    }
    BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_B) = GetSwordItem(ReadVanillaValue(S2H_EQUIP_SLOT_SWORD));
    if (gPlayState != nullptr) {
        Interface_LoadItemIconImpl(gPlayState, EQUIP_SLOT_B);
    }
}

bool BindMeleeSwordToButton(const RegisteredEquip* sword) {
    if (sword == nullptr || sword->definition.meleeWeapon == 0) {
        return false;
    }
    if (!CustomItemRegistry_IsOwned(sword->key.c_str())) {
        CustomItemRegistry_SetOwned(sword->key.c_str(), true);
    }
    if (!CustomItemRegistry_Equip(S2H_ITEM_BUTTON_B, sword->key.c_str())) {
        return false;
    }
    sSwordOnButton = sword->key;
    return true;
}

// B belongs to the form, and only the human one carries the sword, so the work waits until Link is human again.
void ReconcileSwordButton() {
    if (!sIsSwordButtonDirty || gSaveContext.save.playerForm != PLAYER_FORM_HUMAN) {
        return;
    }
    sIsSwordButtonDirty = false;
    if (BindMeleeSwordToButton(Find(sWorn[S2H_EQUIP_SLOT_SWORD]))) {
        return;
    }
    UnbindSwordFromButton();
    RestoreVanillaSwordButton();
}

// Wears incoming in the slot, or empties it when null. A vanilla slot is snapshotted on the first piece worn and
// handed back exactly on the last one taken off, so wearing custom gear never grants or removes the vanilla piece.
void SwitchWorn(uint8_t slot, RegisteredEquip* incoming, bool shouldRestoreVanilla) {
    if (!IsWearableSlot(slot)) {
        return;
    }
    const std::string outgoingKey = sWorn[slot];
    if (incoming == nullptr ? outgoingKey.empty() : outgoingKey == incoming->key) {
        return;
    }

    sWorn[slot] = incoming != nullptr ? incoming->key : std::string();
    const RegisteredEquip* outgoing = Find(outgoingKey);
    if (outgoing != nullptr && outgoing->definition.onUnequip != nullptr) {
        outgoing->definition.onUnequip(outgoing->key.c_str());
    }

    if (IsVanillaSlot(slot)) {
        if (incoming != nullptr) {
            if (outgoingKey.empty()) {
                sRestoreValue[slot] = ReadVanillaValue(slot);
            }
            WriteVanillaValue(slot, incoming->definition.vanillaBase);
        } else if (shouldRestoreVanilla) {
            WriteVanillaValue(slot, sRestoreValue[slot]);
        }
        sAppliedValue[slot] = ReadVanillaValue(slot);
    }
    if (slot == S2H_EQUIP_SLOT_SWORD) {
        sIsSwordButtonDirty = true;
    }

    if (incoming != nullptr && incoming->definition.onEquip != nullptr) {
        incoming->definition.onEquip(incoming->key.c_str());
    }
}

void WatchWornEquipment(Player*) {
    for (uint8_t slot = 0; slot < S2H_EQUIP_SLOT_UPGRADE; slot++) {
        if (sWorn[slot].empty()) {
            continue;
        }
        if (Find(sWorn[slot]) == nullptr || !CustomEquipRegistry_IsOwned(sWorn[slot].c_str())) {
            SwitchWorn(slot, nullptr, true);
            continue;
        }
        if (IsVanillaSlot(slot) && ReadVanillaValue(slot) != sAppliedValue[slot]) {
            SwitchWorn(slot, nullptr, false);
        }
    }
    ReconcileSwordButton();
}

// A vanilla gift of the same value as the worn piece is invisible to the watcher, so the gift itself is the signal.
void ReleaseSlotTakenByVanillaGift(u8 item) {
    if (item >= ITEM_SWORD_KOKIRI && item <= ITEM_SWORD_GILDED) {
        SwitchWorn(S2H_EQUIP_SLOT_SWORD, nullptr, false);
    } else if (item >= ITEM_SHIELD_HERO && item <= ITEM_SHIELD_MIRROR) {
        SwitchWorn(S2H_EQUIP_SLOT_SHIELD, nullptr, false);
    }
}

nlohmann::json WriteSection() {
    nlohmann::json section;
    section["owned"] = std::vector<std::string>(sOwned.begin(), sOwned.end());
    section["toggled"] = std::vector<std::string>(sToggled.begin(), sToggled.end());
    nlohmann::json worn = nlohmann::json::array();
    for (size_t slot = 0; slot < sWorn.size(); ++slot) {
        if (!sWorn[slot].empty()) {
            worn.push_back({ { "slot", slot }, { "key", sWorn[slot] }, { "restore", sRestoreValue[slot] } });
        }
    }
    section["worn"] = worn;
    return section;
}

void ReadKeySet(const nlohmann::json& section, const char* name, std::unordered_set<std::string>& keys) {
    keys.clear();
    for (const auto& key : section.value(name, nlohmann::json::array())) {
        if (key.is_string() && keys.size() < OwnedKeyLimit) {
            keys.insert(key.get<std::string>());
        }
    }
}

void ReadSection(const nlohmann::json& section) {
    sOwned.clear();
    sToggled.clear();
    for (auto& key : sWorn) {
        key.clear();
    }
    sRestoreValue.fill(0);
    sAppliedValue.fill(0);
    sSwordOnButton.clear();
    sIsSwordButtonDirty = false;
    if (!section.is_object()) {
        return;
    }

    ReadKeySet(section, "owned", sOwned);
    ReadKeySet(section, "toggled", sToggled);
    for (const auto& entry : section.value("worn", nlohmann::json::array())) {
        const size_t slot = entry.value("slot", sWorn.size());
        if (slot >= sWorn.size() || !IsWearableSlot(static_cast<uint8_t>(slot))) {
            continue;
        }
        sWorn[slot] = entry.value("key", "");
        sRestoreValue[slot] = entry.value("restore", 0);
        const RegisteredEquip* worn = Find(sWorn[slot]);
        sAppliedValue[slot] = worn != nullptr ? worn->definition.vanillaBase : 0;
    }
    sIsSwordButtonDirty = !sWorn[S2H_EQUIP_SLOT_SWORD].empty();
}

void MarkOwnedFromPickup(const char* key) {
    CustomEquipRegistry_SetOwned(key, true);
}

// The companion item is what `give custom_item`, the randomizer and the B button see of a piece of equipment.
static void ToggleCompanionEquipment(PlayState* play, Player* player) {
    const char* key = CustomItemRegistry_GetEquippedKey(player->heldItemButton);
    const auto* equip = CustomEquipRegistry_Find(key);
    if (equip == nullptr) return;
    if (equip->slot == S2H_EQUIP_SLOT_UPGRADE) CustomEquipRegistry_SetToggle(key, !CustomEquipRegistry_IsToggleOn(key));
    else if (CustomEquipRegistry_IsWorn(key)) CustomEquipRegistry_TakeOff(equip->slot);
    else CustomEquipRegistry_Wear(key);
    Audio_PlaySfx(NA_SE_SY_DECIDE);
}

bool RegisterCompanionItem(const RegisteredEquip& equip, const S2HCustomItemRandomizer* randomizer) {
    const S2HCustomEquipDefinition& definition = equip.definition;
    S2HCustomItemDefinition item = {};
    item.structSize = sizeof(item);
    item.key = definition.key;
    item.iconPath = definition.iconPath;
    item.namePath = definition.namePath;
    item.pauseText = definition.pauseText;
    item.getItemEntry = definition.getItemEntry;
    item.getItemText = definition.getItemText;
    item.getItemModelPath = definition.getItemModelPath;
    item.randomizer = randomizer;
    item.formMask = definition.formMask;
    item.presentationFlags = S2H_ITEM_HIDE_FROM_SAVE_EDITOR;
    item.flags = S2H_CUSTOM_ITEM_WEARABLE | S2H_CUSTOM_ITEM_EQUIPPABLE | S2H_CUSTOM_ITEM_INSTANT |
                 S2H_CUSTOM_ITEM_C_BUTTON | S2H_CUSTOM_ITEM_DPAD;
    item.init = ToggleCompanionEquipment;
    item.preferredPage = definition.page == S2H_EQUIP_PAGE_VANILLA ? S2H_ITEM_PAGE_VANILLA :
                         static_cast<uint8_t>(std::min<int>(2 + definition.page, S2H_ITEM_PAGE_QUEST_VANILLA - 1));
    item.preferredSlot = static_cast<uint8_t>(std::min<int>(definition.row * 6 + definition.column, ITEM_NUM_SLOTS - 1));
    item.onReceive = MarkOwnedFromPickup;
    if (definition.meleeWeapon != 0) {
        item.flags &= ~S2H_CUSTOM_ITEM_INSTANT;
        item.flags |= S2H_CUSTOM_ITEM_B_BUTTON;
        item.meleeWeapon = definition.meleeWeapon;
    }
    if (definition.presentationFlags & S2H_EQUIP_OWNERSHIP_ONLY) {
        item.flags = S2H_CUSTOM_ITEM_WEARABLE;
        item.init = nullptr;
        item.meleeWeapon = 0;
    }
    return CustomItemRegistry_Register(&item);
}

bool IsValidDefinition(const S2HCustomEquipDefinition* definition) {
    if (definition == nullptr || definition->structSize < MinDefinitionSize || definition->key == nullptr ||
        definition->key[0] == '\0' || definition->slot >= S2H_EQUIP_SLOT_MAX) {
        SPDLOG_ERROR("[CustomEquipRegistry] Mod '{}' passed an unusable equipment definition", sCurrentMod);
        return false;
    }
    if (sByKey.contains(definition->key)) {
        SPDLOG_ERROR("[CustomEquipRegistry] Key '{}' is already registered", definition->key);
        return false;
    }
    const bool hasMeleeWeapon = definition->structSize > offsetof(S2HCustomEquipDefinition, meleeWeapon) &&
                                definition->meleeWeapon != 0;
    const bool hasIcon = definition->iconPath != nullptr && definition->iconPath[0] != '\0';
    if (hasMeleeWeapon && (definition->slot != S2H_EQUIP_SLOT_SWORD || definition->meleeWeapon >= PLAYER_MELEEWEAPON_MAX ||
                           !hasIcon)) {
        SPDLOG_ERROR("[CustomEquipRegistry] '{}' needs a sword slot, a valid melee weapon and an icon to take B",
                     definition->key);
        return false;
    }
    if (IsVanillaSlot(definition->slot) && definition->vanillaBase >= GetVanillaValueLimit(definition->slot)) {
        SPDLOG_ERROR("[CustomEquipRegistry] '{}' has a vanilla base value out of range", definition->key);
        return false;
    }
    return true;
}

bool WearCommand(std::shared_ptr<Ship::Console>, const std::vector<std::string>& args, std::string* output) {
    if (args.size() != 2) {
        if (output != nullptr) {
            *output = "Usage: wear \"<custom equipment key>\"";
        }
        return true;
    }
    const std::string key = ModApi_UnquoteConsoleArgument(args[1]);
    const bool isWorn = CustomEquipRegistry_Wear(key.c_str());
    if (output != nullptr) {
        *output = isWorn ? "Wearing " + key : "Not owned, wrong form or unknown key";
    }
    return !isWorn;
}

} // namespace

bool CustomEquipRegistry_Register(const S2HCustomEquipDefinition* definition) {
    if (!IsValidDefinition(definition)) {
        return false;
    }

    auto entry = std::make_unique<RegisteredEquip>();
    entry->owner = sCurrentMod;
    entry->key = definition->key;
    std::memcpy(&entry->definition, definition, std::min<size_t>(definition->structSize, sizeof(entry->definition)));
    entry->definition.structSize = sizeof(entry->definition);
    entry->definition.key = entry->key.c_str();
    SetString(entry->definition.iconPath, entry->iconPath, definition->iconPath);
    SetString(entry->definition.namePath, entry->namePath, definition->namePath);
    SetString(entry->definition.pauseText, entry->pauseText, definition->pauseText);
    SetString(entry->definition.getItemText, entry->getItemText, definition->getItemText);
    SetString(entry->definition.getItemModelPath, entry->getItemModelPath, definition->getItemModelPath);

    if (!RegisterCompanionItem(*entry, definition->randomizer)) {
        return false;
    }
    // The companion item copied the randomizer data; the caller's pointer must not outlive this call.
    entry->definition.randomizer = nullptr;
    sByKey[entry->key] = entry.get();
    sEquipment.push_back(std::move(entry));
    return true;
}

const char* CustomEquipRegistry_GetWorn(uint8_t slot) {
    if (CustomItemRegistry_GetDrawingPlayer() != nullptr) {
        for (const auto& [key, equip] : sByKey) {
            if (equip->definition.slot == slot && CustomItemRegistry_IsWorn(key.c_str())) return key.c_str();
        }
        return nullptr;
    }
    if (slot >= S2H_EQUIP_SLOT_MAX || sWorn[slot].empty()) {
        return nullptr;
    }
    return sWorn[slot].c_str();
}

bool CustomEquipRegistry_IsWorn(const char* key) {
    if (CustomItemRegistry_GetDrawingPlayer() != nullptr) return CustomItemRegistry_IsWorn(key);
    if (key == nullptr) {
        return false;
    }
    return std::any_of(sWorn.begin(), sWorn.end(), [key](const std::string& worn) { return worn == key; });
}

bool CustomEquipRegistry_IsOwned(const char* key) {
    return key != nullptr && sOwned.contains(key);
}

bool CustomEquipRegistry_SetOwned(const char* key, bool owned) {
    if (key == nullptr || !key[0]) return false;
    const bool changed = sOwned.contains(key) != owned;
    if (changed) GameInteractor::Instance->ExecuteHooks<GameInteractor::OnCustomEquipOwnedChange>(key, owned);
    RegisteredEquip* entry = key == nullptr ? nullptr : Find(key);
    if (entry == nullptr) {
        if (owned) sOwned.insert(key); else sOwned.erase(key);
        return true;
    }
    CustomItemRegistry_SetOwned(entry->key.c_str(), owned);
    if (owned) {
        sOwned.insert(entry->key);
        return true;
    }
    sOwned.erase(entry->key);
    if (CustomEquipRegistry_IsWorn(entry->key.c_str())) {
        SwitchWorn(entry->definition.slot, nullptr, true);
    }
    sToggled.erase(entry->key);
    return true;
}

bool CustomEquipRegistry_Wear(const char* key) {
    RegisteredEquip* entry = key == nullptr ? nullptr : Find(key);
    if (entry == nullptr || (entry->definition.presentationFlags & S2H_EQUIP_OWNERSHIP_ONLY) || !CustomEquipRegistry_IsOwned(entry->key.c_str()) || !IsFormAllowed(&entry->definition)) {
        return false;
    }
    if (entry->definition.slot == S2H_EQUIP_SLOT_UPGRADE) {
        return CustomEquipRegistry_SetToggle(entry->key.c_str(), true);
    }
    SwitchWorn(entry->definition.slot, entry, true);
    return true;
}

void CustomEquipRegistry_TakeOff(uint8_t slot) {
    SwitchWorn(slot, nullptr, true);
}

bool CustomEquipRegistry_IsToggleOn(const char* key) {
    return key != nullptr && sToggled.contains(key);
}

bool CustomEquipRegistry_SetToggle(const char* key, bool on) {
    RegisteredEquip* entry = key == nullptr ? nullptr : Find(key);
    if (entry == nullptr || (entry->definition.presentationFlags & S2H_EQUIP_OWNERSHIP_ONLY) || !CustomEquipRegistry_IsOwned(entry->key.c_str())) {
        return false;
    }
    if (on && !entry->definition.toggles) {
        return false;
    }
    if (on) {
        sToggled.insert(entry->key);
        if (entry->definition.onEquip != nullptr) {
            entry->definition.onEquip(entry->key.c_str());
        }
        return true;
    }
    sToggled.erase(entry->key);
    if (entry->definition.onUnequip != nullptr) {
        entry->definition.onUnequip(entry->key.c_str());
    }
    return true;
}

uint32_t CustomEquipRegistry_GetCount(void) {
    return static_cast<uint32_t>(sEquipment.size());
}

const S2HCustomEquipDefinition* CustomEquipRegistry_GetAt(uint32_t index) {
    return index < sEquipment.size() ? &sEquipment[index]->definition : nullptr;
}

const S2HCustomEquipDefinition* CustomEquipRegistry_Find(const char* key) {
    const RegisteredEquip* entry = key == nullptr ? nullptr : Find(key);
    return entry == nullptr ? nullptr : &entry->definition;
}

bool CustomEquipRegistry_IsFormAllowed(const char* key) {
    return IsFormAllowed(CustomEquipRegistry_Find(key));
}

void CustomEquipRegistry_Init(void) {
    SaveSections_Register(SaveSection, WriteSection, ReadSection);

    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerUpdate>(WatchWornEquipment);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerUpdate>([](Player*) {
        for (const auto& [key, _] : sByKey) {
            CustomItemRegistry_SetWorn(key.c_str(), CustomEquipRegistry_IsWorn(key.c_str()) || sToggled.contains(key));
        }
    });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnItemGive>(ReleaseSlotTakenByVanillaGift);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnConsoleGive>(
        [](PlayState*, const char* type, const char* key, bool* handled, bool* given) {
            if (type == nullptr || std::string(type) != "custom_equipment") {
                return;
            }
            *handled = true;
            *given = CustomEquipRegistry_SetOwned(key, true);
        });

    Ship::Context::GetRawInstance()->GetConsole()->AddCommand(
        "wear", { WearCommand, "Wears an owned piece of custom equipment", { { "key", Ship::ArgumentType::TEXT } } });
}

void CustomEquipRegistry_SetCurrentMod(const std::string& name) {
    sCurrentMod = name;
}

uint16_t CustomEquipRegistry_GetVanillaBase(uint8_t slot) {
    if (!IsVanillaSlot(slot)) return 0;
    return sWorn[slot].empty() ? ReadVanillaValue(slot) : sRestoreValue[slot];
}

std::vector<std::string> CustomEquipRegistry_ListOwned() { return {sOwned.begin(), sOwned.end()}; }
