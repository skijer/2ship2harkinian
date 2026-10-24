#include "2s2h/ModApi/ProgressiveItems/ProgressiveItems.h"
#include "CustomItemRegistry.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <spdlog/spdlog.h>
#include <ship/Context.h>
#include <ship/debug/Console.h>

#include "2s2h/CustomMessage/CustomMessage.h"
#include "2s2h/CustomItem/CustomItem.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ModApi/ConsoleArguments.h"
#include "2s2h/ModApi/RandoItems/RandoItems.h"
#include "2s2h/SaveManager/SaveSections.h"
#include "2s2h/Rando/Rando.h"
#include "2s2h/Rando/DrawFuncs.h"

extern "C" {
#include "z64.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
void Player_InitItemAction(PlayState* play, Player* player, PlayerItemAction itemAction);
void Player_InitItemActionWithAnim(PlayState* play, Player* player, PlayerItemAction itemAction);
uint8_t ResourceMgr_FileExists(const char* path);
// OPEN_DISPS redeclares these inside its body, which would give them C++ linkage here.
void FrameInterpolation_RecordOpenChild(const void* a, int b);
void FrameInterpolation_RecordCloseChild(void);
}

namespace {

constexpr uint16_t RuntimeIdBase = 0x400;
constexpr uint32_t HeldUpdateGraceFrames = 4;
constexpr size_t OwnedItemLimit = 4096;
constexpr const char* SaveSection = "customItems";

struct RegisteredCustomItem {
    std::string owner;
    std::string key;
    std::string iconPath;
    std::string namePath;
    std::string pauseText;
    std::string getItemText;
    std::string heldModelPath;
    std::string getItemModelPath;
    std::string firstPersonModelPath;
    std::string appearanceModelPath;
    std::string displayName;
    std::string logicKey;
    std::string hintKey;
    std::string progressionGroup;
    std::string poolCountOption;
    std::string poolModeOption;
    std::vector<int32_t> poolSources;
    S2HCustomItemRandomizer randomizer = {};
    S2HCustomItemDefinition definition = {};
    uint16_t runtimeId = 0;
};

struct RegisteredPickupVariant {
    std::string key, itemKey, variantKey;
    S2HCustomPickupVariant definition = {};
};
std::unordered_map<std::string, std::unique_ptr<RegisteredPickupVariant>> sPickupVariants;

std::string sCurrentMod;
std::unordered_map<std::string, std::unique_ptr<RegisteredCustomItem>> sCustomItems;
std::unordered_map<uint16_t, RegisteredCustomItem*> sRuntimeItems;
std::unordered_map<uint16_t, RegisteredCustomItem*> sReplacements;
std::unordered_set<std::string> sOwnedItems;
std::unordered_set<std::string> sWornItems;
Player* sDrawingPlayer = nullptr;
std::unordered_map<Actor*, std::string> sActorItems;
std::array<std::array<std::string, S2H_ITEM_BUTTON_COUNT>, PLAYER_FORM_MAX> sEquippedKeys;
std::string sActiveItem;
std::string sHeldItem;
std::string sPauseItem;
std::string sMessageItem;
uint32_t sHeldUpdateFrame = 0;
uint16_t sNextRuntimeId = RuntimeIdBase;

const S2HCustomItemDefinition* FindDefinition(const std::string& key) {
    auto item = sCustomItems.find(key);
    return item == sCustomItems.end() ? nullptr : &item->second->definition;
}

void SetString(const char*& target, std::string& storage, const char* value) {
    storage = value == nullptr ? "" : value;
    target = storage.empty() ? nullptr : storage.c_str();
}

bool IsReplacement(const S2HCustomItemDefinition& definition) {
    return (definition.flags & S2H_CUSTOM_ITEM_REPLACES_VANILLA) != 0;
}

bool IsDpadButton(uint8_t button) {
    return button >= S2H_ITEM_BUTTON_DPAD_FIRST && button <= S2H_ITEM_BUTTON_DPAD_LAST;
}

bool IsValidButton(uint8_t button) {
    return button == S2H_ITEM_BUTTON_B || (button >= S2H_ITEM_BUTTON_C_FIRST && button <= S2H_ITEM_BUTTON_C_LAST) ||
           IsDpadButton(button);
}

// Only B differs per form; C and D-pad buttons are shared by every form.
std::string& EquippedKey(uint8_t button) {
    return sEquippedKeys[button == S2H_ITEM_BUTTON_B ? CUR_FORM : 0][button];
}

uint8_t GetVanillaButtonItem(uint8_t button) {
    return IsDpadButton(button) ? DPAD_GET_CUR_FORM_BTN_ITEM(HELD_ITEM_TO_DPAD(button)) : GET_CUR_FORM_BTN_ITEM(button);
}

uint8_t GetVanillaButtonSlot(uint8_t button) {
    return IsDpadButton(button) ? DPAD_GET_CUR_FORM_BTN_SLOT(HELD_ITEM_TO_DPAD(button)) : GET_CUR_FORM_BTN_SLOT(button);
}

void SetVanillaButton(uint8_t button, uint8_t item, uint8_t slot) {
    if (IsDpadButton(button)) {
        DPAD_SET_CUR_FORM_BTN_ITEM(HELD_ITEM_TO_DPAD(button), item);
        DPAD_SET_CUR_FORM_BTN_SLOT(HELD_ITEM_TO_DPAD(button), slot);
    } else {
        SET_CUR_FORM_BTN_ITEM(button, item);
        SET_CUR_FORM_BTN_SLOT(button, slot);
    }
}

void ReloadButtonIcon(uint8_t button) {
    if (gPlayState == nullptr) {
        return;
    }
    if (IsDpadButton(button)) {
        Interface_Dpad_LoadItemIconImpl(gPlayState, HELD_ITEM_TO_DPAD(button));
    } else {
        Interface_LoadItemIconImpl(gPlayState, button);
    }
}

void SyncButton(uint8_t button) {
    const std::string& key = EquippedKey(button);
    if (!key.empty() && FindDefinition(key) != nullptr) {
        SetVanillaButton(button, ITEM_CUSTOM, SLOT_NONE);
        return;
    }
    if (GetVanillaButtonItem(button) == ITEM_CUSTOM && key.empty()) {
        SetVanillaButton(button, ITEM_NONE, SLOT_NONE);
    }
}

void SyncAllButtons() {
    for (uint8_t button = 0; button < S2H_ITEM_BUTTON_COUNT; ++button) {
        if (IsValidButton(button)) {
            SyncButton(button);
        }
    }
}

nlohmann::json WriteSection() {
    nlohmann::json section;
    section["owned"] = std::vector<std::string>(sOwnedItems.begin(), sOwnedItems.end());
    nlohmann::json equipped = nlohmann::json::array();
    for (size_t form = 0; form < sEquippedKeys.size(); ++form) {
        for (size_t button = 0; button < S2H_ITEM_BUTTON_COUNT; ++button) {
            const std::string& key = sEquippedKeys[form][button];
            if (!key.empty()) {
                equipped.push_back({ { "form", form }, { "button", button }, { "key", key } });
            }
        }
    }
    section["equipped"] = equipped;
    section["pickupLocations"] = nlohmann::json::array();
    if (IS_RANDO) {
        for (const auto& [id, check] : Rando::StaticData::Checks) {
            const char* key = RandoItems_GetKey(RANDO_SAVE_CHECKS[id].randoItemId);
            if (key != nullptr && CustomItemRegistry_GetPickupVariant(key) != nullptr) {
                section["pickupLocations"].push_back({ {"check", (int32_t)id}, {"key", key} });
            }
        }
    }
    return section;
}

void ReadSection(const nlohmann::json& section) {
    sOwnedItems.clear();
    sWornItems.clear();
    for (auto& forms : sEquippedKeys) {
        for (auto& key : forms) {
            key.clear();
        }
    }
    sActiveItem.clear();
    sHeldItem.clear();
    sPauseItem.clear();
    sMessageItem.clear();
    if (!section.is_object()) {
        return;
    }
    for (const auto& key : section.value("owned", nlohmann::json::array())) {
        if (key.is_string() && sOwnedItems.size() < OwnedItemLimit) {
            sOwnedItems.insert(key.get<std::string>());
        }
    }
    for (const auto& entry : section.value("equipped", nlohmann::json::array())) {
        const size_t form = entry.value("form", PLAYER_FORM_MAX);
        const size_t button = entry.value("button", S2H_ITEM_BUTTON_COUNT);
        if (form < PLAYER_FORM_MAX && button < S2H_ITEM_BUTTON_COUNT && IsValidButton((uint8_t)button)) {
            sEquippedKeys[form][button] = entry.value("key", "");
        }
    }
    for (const auto& location : section.value("pickupLocations", nlohmann::json::array())) {
        const int32_t check = location.value("check", -1);
        const std::string key = location.value("key", "");
        const int32_t item = RandoItems_GetRandoGet(key.c_str());
        if (item != 0 && Rando::StaticData::Checks.contains((RandoCheckId)check)) {
            RANDO_SAVE_CHECKS[check].randoItemId = (RandoItemId)item;
        }
    }
    // Migrate former separately equipable wand rewards into their parent slot.
    for (auto& form : sEquippedKeys) for (auto& key : form) {
        const auto* variant = CustomItemRegistry_GetPickupVariant(key.c_str());
        if (variant != nullptr) key = variant->itemKey;
    }
    for (const auto& [key, pickup] : sPickupVariants) {
        if (sOwnedItems.contains(key)) sOwnedItems.insert(pickup->itemKey);
    }

}

const S2HCustomItemDefinition* GetEquippedDefinition(uint8_t button) {
    return IsValidButton(button) ? FindDefinition(EquippedKey(button)) : nullptr;
}

void SwapOutOfOtherButtons(uint8_t targetButton, const std::string& key) {
    for (uint8_t other = 0; other < S2H_ITEM_BUTTON_COUNT; ++other) {
        if (other == targetButton || !IsValidButton(other) || GetVanillaButtonItem(other) != ITEM_CUSTOM ||
            EquippedKey(other) != key) {
            continue;
        }
        const uint8_t item = GetVanillaButtonItem(targetButton);
        const uint8_t slot = GetVanillaButtonSlot(targetButton);
        EquippedKey(other) = item == ITEM_CUSTOM ? EquippedKey(targetButton) : std::string();
        SetVanillaButton(other, item, slot);
        ReloadButtonIcon(other);
    }
}

S2HItemIconSurface ButtonIconSurface(uint8_t button) {
    if (button == S2H_ITEM_BUTTON_B) {
        return S2H_ITEM_ICON_B_BUTTON;
    }
    return IsDpadButton(button) ? S2H_ITEM_ICON_DPAD : S2H_ITEM_ICON_C_BUTTON;
}

const char* FindButtonItemKey(uint8_t button) {
    const uint8_t item = GetVanillaButtonItem(button);
    if (item == ITEM_CUSTOM) {
        return EquippedKey(button).c_str();
    }
    const S2HCustomItemDefinition* replacement = CustomItemRegistry_FindReplacement(item);
    return replacement == nullptr ? nullptr : replacement->key;
}

void ApplyButtonIcon(uint8_t heldButton, uint8_t segmentIndex, bool* should) {
    if (gPlayState == nullptr) {
        return;
    }
    const char* path = CustomItemRegistry_ResolveTexture(FindButtonItemKey(heldButton), ButtonIconSurface(heldButton));
    if (path == nullptr) {
        return;
    }
    gPlayState->interfaceCtx.iconItemSegment[segmentIndex] = const_cast<char*>(path);
    *should = false;
}

void PutAwayHeldItem(PlayState* play, Player* player) {
    if (sHeldItem.empty()) {
        return;
    }
    const S2HCustomItemDefinition* definition = FindDefinition(sHeldItem);
    sHeldItem.clear();
    if (definition != nullptr && definition->putAway != nullptr && player != nullptr) {
        definition->putAway(reinterpret_cast<struct Player*>(player), reinterpret_cast<struct PlayState*>(play));
    }
    if (play == nullptr || player == nullptr || player->heldItemAction != PLAYER_IA_CUSTOM) {
        return;
    }
    player->heldItemId = ITEM_NONE;
    player->nextModelGroup = Player_ActionToModelGroup(player, PLAYER_IA_NONE);
    Player_InitItemActionWithAnim(play, player, PLAYER_IA_NONE);
}

// A held item whose upper action stopped running (cutscene, damage) is put away rather than left stuck in the hand.
void PutAwayStalledHeldItem(Player* player) {
    if (sHeldItem.empty() || gPlayState == nullptr) {
        return;
    }
    const S2HCustomItemDefinition* definition = FindDefinition(sHeldItem);
    if (definition == nullptr || definition->update == nullptr) {
        return;
    }
    const bool isStillCustom = player->heldItemAction == PLAYER_IA_CUSTOM;
    const bool isUpdating = (gPlayState->state.frames - sHeldUpdateFrame) <= HeldUpdateGraceFrames;
    if (!isStillCustom || !isUpdating) {
        PutAwayHeldItem(gPlayState, player);
    }
}

uint16_t ParseVanillaItem(const std::string& argument) {
    char* end = nullptr;
    const long item = std::strtol(argument.c_str(), &end, 0);
    if (end == argument.c_str() || *end != '\0' || item <= 0 || item > UINT16_MAX) {
        return 0;
    }
    return (uint16_t)item;
}

bool GiveCommand(std::shared_ptr<Ship::Console>, const std::vector<std::string>& args, std::string* output) {
    if (args.size() == 2) {
        const uint16_t item = ParseVanillaItem(args[1]);
        const bool given = item != 0 && VanillaItems_Give(gPlayState, item);
        if (output != nullptr) {
            *output = given ? "Given item " + args[1] : "Unknown item id or no active game";
        }
        return !given;
    }
    if (args.size() != 3 && args.size() != 4) {
        if (output != nullptr) {
            *output = "Usage: give custom_item \"<key>\" [variant] | give <vanilla item id>";
        }
        return true;
    }
    const bool isCustomItem = args[1] == "custom_item";
    std::string key = ModApi_UnquoteConsoleArgument(args[2]);
    if (isCustomItem && args.size() == 4) {
        const std::string variant = ModApi_UnquoteConsoleArgument(args[3]);
        const char* resolved = CustomItemRegistry_FindPickupKey(key.c_str(), variant.c_str());
        key = resolved != nullptr ? resolved : "";
    }
    bool handled = false;
    bool given = false;
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnConsoleGive>(gPlayState, args[1].c_str(), key.c_str(),
                                                                          &handled, &given);
    if (!handled && isCustomItem) {
        given = CustomItemRegistry_Give(gPlayState, key.c_str());
    }
    if (output != nullptr) {
        *output = given ? "Given " + key : "Unknown " + args[1] + " or no active game";
    }
    return !given;
}

} // namespace

static const char* ResolveDroppedItemKey(Actor* actor, PlayState* play) {
    return GameInteractor_ExecuteOnResolveCustomGetItem(actor, play, CustomItemRegistry_GetActorItem(actor));
}

static void ReceiveDroppedItem(Actor* actor, PlayState* play) {
    const char* key = ResolveDroppedItemKey(actor, play);
    const auto* definition = CustomItemRegistry_Find(key);
    if (definition == nullptr) {
        return;
    }
    sMessageItem = definition->key;
    CustomMessage::Entry entry;
    entry.textboxType = 2;
    if (definition->iconPath != nullptr && ResourceMgr_FileExists(definition->iconPath)) {
        entry.icon = ITEM_OCARINA_OF_TIME;
        entry.iconPath = definition->iconPath;
    }
    CustomMessage::SetActiveCustomMessage(definition->getItemText != nullptr ? definition->getItemText
                                                                           : definition->key, entry);
    CustomItemRegistry_ReceiveGetItem(key);
}

void CustomItemRegistry_DrawAcquisition(const char* key, PlayState* play) {
    const auto* definition = CustomItemRegistry_Find(key);
    if (definition == nullptr || play == nullptr) {
        return;
    }
    const auto* pickup = CustomItemRegistry_GetPickupVariant(key);
    if (pickup != nullptr && pickup->drawSoulFlame) {
        // The acquisition caller already applies its 30x scale; match the soul flame's 0.06 world scale.
        DrawEnLight({ pickup->flameRed, pickup->flameGreen, pickup->flameBlue },
                    { 0.13333333f, 0.13333333f, 0.13333333f });
        return;
    }
    S2HGetItemEntry entry = definition->getItemEntry;
    if (entry.drawFunc != nullptr) {
        entry.drawFunc(play, &entry);
        return;
    }
    const bool hasModel = definition->getItemModelPath != nullptr && ResourceMgr_FileExists(definition->getItemModelPath);
    if (!hasModel && entry.gi != 0) {
        GetItem_Draw(play, entry.gid);
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    if (hasModel) {
        const float scale = definition->getItemScale > 0.0f ? definition->getItemScale : 0.13333333f;
        Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
        Gfx_SetupDL25_Opa(play->state.gfxCtx);
        MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)definition->getItemModelPath);
    } else if (definition->iconPath != nullptr && ResourceMgr_FileExists(definition->iconPath)) {
        // Icon-only mods remain visible without pretending they have a get-item model.
        static Vtx vertices[] = {
            {{ {-24, -24, 0}, 0, {0, 1024}, {255, 255, 255, 255} }},
            {{ {24, -24, 0}, 0, {1024, 1024}, {255, 255, 255, 255} }},
            {{ {24, 24, 0}, 0, {1024, 0}, {255, 255, 255, 255} }},
            {{ {-24, 24, 0}, 0, {0, 0}, {255, 255, 255, 255} }},
        };
        Matrix_ReplaceRotation(&play->billboardMtxF);
        Gfx_SetupDL25_Xlu(play->state.gfxCtx);
        gSPClearGeometryMode(POLY_XLU_DISP++, G_LIGHTING | G_CULL_BACK);
        gDPSetCombineMode(POLY_XLU_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, 255);
        gSPTexture(POLY_XLU_DISP++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
        gDPLoadTextureBlock(POLY_XLU_DISP++, definition->iconPath, G_IM_FMT_RGBA, G_IM_SIZ_32b, 32, 32, 0,
                           G_TX_CLAMP, G_TX_CLAMP, 5, 5, G_TX_NOLOD, G_TX_NOLOD);
        MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
        gSPVertex(POLY_XLU_DISP++, (uintptr_t)vertices, 4, 0);
        gSP2Triangles(POLY_XLU_DISP++, 0, 1, 2, 0, 0, 2, 3, 0);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawDroppedItem(Actor* actor, PlayState* play) {
    Matrix_Scale(30.0f, 30.0f, 30.0f, MTXMODE_APPLY);
    CustomItemRegistry_DrawAcquisition(ResolveDroppedItemKey(actor, play), play);
}

void CustomItemRegistry_PrepareAcquisitionMessage(const char* key) {
    const auto* definition = CustomItemRegistry_Find(key);
    if (definition == nullptr) return;
    sMessageItem = definition->key;
    CustomMessage::Entry entry;
    entry.textboxType = 2;
    if (definition->iconPath != nullptr && ResourceMgr_FileExists(definition->iconPath)) {
        entry.icon = ITEM_OCARINA_OF_TIME;
        entry.iconPath = definition->iconPath;
    }
    CustomMessage::SetActiveCustomMessage(definition->getItemText != nullptr ? definition->getItemText : definition->key, entry);
}

static bool DropCommand(std::shared_ptr<Ship::Console>, const std::vector<std::string>& args, std::string* output) {
    const bool valid = (args.size() == 3 || args.size() == 4) && args[1] == "custom_item";
    std::string key = valid ? ModApi_UnquoteConsoleArgument(args[2]) : std::string();
    if (valid && args.size() == 4) {
        const std::string variant = ModApi_UnquoteConsoleArgument(args[3]);
        const char* resolved = CustomItemRegistry_FindPickupKey(key.c_str(), variant.c_str());
        key = resolved != nullptr ? resolved : "";
    }
    bool dropped = false;
    if (valid && gPlayState != nullptr && CustomItemRegistry_Find(key.c_str()) != nullptr) {
        Player* player = GET_PLAYER(gPlayState);
        const Vec3f& pos = player->actor.world.pos;
        EnItem00* item = CustomItem::Spawn(pos.x + Math_SinS(player->actor.shape.rot.y) * 60.0f, pos.y + 20.0f,
                                         pos.z + Math_CosS(player->actor.shape.rot.y) * 60.0f, 0,
                                         CustomItem::GIVE_ITEM_CUTSCENE | CustomItem::TOSS_ON_SPAWN, 0,
                                         ReceiveDroppedItem, DrawDroppedItem);
        dropped = item != nullptr;
        if (dropped) {
            CustomItemRegistry_BindActor(&item->actor, key.c_str());
        }
    }
    if (output != nullptr) {
        *output = dropped ? "Dropped " + key
                          : "Usage: drop custom_item \"<key>\" [variant] (registered item and active game required)";
    }
    return !dropped;
}

namespace {

const std::array<const char*, S2H_ITEM_BUTTON_COUNT> ButtonNames = { "b",      "cleft", "cdown", "cright", "",
                                                                     "dright", "dleft", "ddown", "dup" };

bool EquipCommand(std::shared_ptr<Ship::Console>, const std::vector<std::string>& args, std::string* output) {
    uint8_t button = S2H_ITEM_BUTTON_COUNT;
    const bool isGrammarRight = args.size() == 4 && args[1] == "custom_item";
    const std::string key = isGrammarRight ? ModApi_UnquoteConsoleArgument(args[2]) : std::string();
    for (uint8_t index = 0; isGrammarRight && index < S2H_ITEM_BUTTON_COUNT; ++index) {
        if (ButtonNames[index][0] != '\0' && args[3] == ButtonNames[index]) {
            button = index;
        }
    }
    const bool equipped =
        button < S2H_ITEM_BUTTON_COUNT && gPlayState != nullptr && CustomItemRegistry_Equip(button, key.c_str());
    if (output != nullptr) {
        *output = equipped ? "Equipped " + key
                           : "Usage: equip custom_item \"<key>\" <b|cleft|cdown|cright|dup|ddown|dleft|dright> "
                             "(item must be owned, fit the button and the current form)";
    }
    return !equipped;
}

void RegisterConsole() {
    auto console = Ship::Context::GetRawInstance()->GetConsole();
    console->AddCommand("drop", { DropCommand, "Drops a custom item with its acquisition model and textbox",
                                  { { "custom_item", Ship::ArgumentType::TEXT },
                                    { "key", Ship::ArgumentType::TEXT },
                                    { "variant", Ship::ArgumentType::TEXT, true } } });
    console->AddCommand("equip", { EquipCommand,
                                   "Equips an owned custom item on a button",
                                   { { "custom_item", Ship::ArgumentType::TEXT },
                                     { "key", Ship::ArgumentType::TEXT },
                                     { "button", Ship::ArgumentType::TEXT } } });
    console->AddCommand("give", { GiveCommand,
                                  "Gives a registered custom item or equipment, or a vanilla item by id",
                                  { { "custom_item|custom_equipment|item id", Ship::ArgumentType::TEXT },
                                    { "key", Ship::ArgumentType::TEXT, true },
                                    { "variant", Ship::ArgumentType::TEXT, true } } });
}

void RegisterButtonIconHandlers() {
    REGISTER_VB_SHOULD(VB_INTERFACE_LOAD_ITEM_ICON, {
        const uint8_t button = va_arg(args, int);
        ApplyButtonIcon(button, button, should);
    });
    REGISTER_VB_SHOULD(VB_INTERFACE_LOAD_DPAD_ITEM_ICON, {
        const uint8_t dpadButton = va_arg(args, int);
        ApplyButtonIcon(DPAD_TO_HELD_ITEM(dpadButton), DPAD_BUTTON(dpadButton) + EQUIP_SLOT_MAX, should);
    });
}

bool IsValidDefinition(const S2HCustomItemDefinition* definition) {
    if (definition == nullptr || definition->structSize < S2H_CUSTOM_ITEM_DEFINITION_MIN_SIZE ||
        definition->key == nullptr || definition->key[0] == '\0') {
        SPDLOG_ERROR("[CustomItemRegistry] Mod '{}' supplied an invalid custom item", sCurrentMod);
        return false;
    }
    if ((definition->flags & S2H_CUSTOM_ITEM_EQUIPPABLE) != 0 &&
        (definition->iconPath == nullptr || definition->iconPath[0] == '\0')) {
        SPDLOG_ERROR("[CustomItemRegistry] Equippable custom item '{}' requires an icon path", definition->key);
        return false;
    }
    if (sCustomItems.contains(definition->key)) {
        SPDLOG_ERROR("[CustomItemRegistry] Mod '{}' tried to register duplicate custom item '{}'", sCurrentMod,
                     definition->key);
        return false;
    }
    if (!IsReplacement(*definition)) {
        return true;
    }
    if (definition->structSize < S2H_CUSTOM_ITEM_REPLACEMENT_MIN_SIZE || definition->replacesItem >= ITEM_CUSTOM) {
        SPDLOG_ERROR("[CustomItemRegistry] Custom item '{}' must replace an inventory item", definition->key);
        return false;
    }
    auto replaced = sReplacements.find(definition->replacesItem);
    if (replaced != sReplacements.end()) {
        SPDLOG_ERROR("[CustomItemRegistry] Custom item '{}' cannot replace item {}: '{}' already does", definition->key,
                     definition->replacesItem, replaced->second->key);
        return false;
    }
    return true;
}

// Copies the caller's randomizer data so the pointer in the stored definition outlives the mod's stack.
bool StoreRandomizer(RegisteredCustomItem& item, const S2HCustomItemRandomizer* randomizer) {
    item.definition.randomizer = nullptr;
    if (randomizer == nullptr) {
        return true;
    }
    if (randomizer->structSize < S2H_CUSTOM_ITEM_RANDOMIZER_MIN_SIZE) {
        SPDLOG_ERROR("[CustomItemRegistry] Custom item '{}' supplied invalid randomizer data", item.key);
        return false;
    }
    std::memcpy(&item.randomizer, randomizer, std::min<size_t>(randomizer->structSize, sizeof(item.randomizer)));
    item.randomizer.structSize = sizeof(item.randomizer);
    item.poolSources.clear();
    for (uint16_t i = 0; item.randomizer.poolSources != nullptr && i < item.randomizer.poolSourceCount; i++) {
        const int32_t source = item.randomizer.poolSources[i];
        if (source <= RI_NONE || source >= RI_MAX) {
            SPDLOG_ERROR("[CustomItemRegistry] Invalid native pool source for '{}'", item.key);
            return false;
        }
        item.poolSources.push_back(source);
    }
    item.randomizer.poolSources = item.poolSources.empty() ? nullptr : item.poolSources.data();
    item.randomizer.poolSourceCount = static_cast<uint16_t>(item.poolSources.size());
    SetString(item.randomizer.logicKey, item.logicKey, item.randomizer.logicKey);
    SetString(item.randomizer.hintKey, item.hintKey, item.randomizer.hintKey);
    SetString(item.randomizer.progressionGroup, item.progressionGroup, item.randomizer.progressionGroup);
    SetString(item.randomizer.poolCountOption, item.poolCountOption, item.randomizer.poolCountOption);
    SetString(item.randomizer.poolModeOption, item.poolModeOption, item.randomizer.poolModeOption);
    item.definition.randomizer = &item.randomizer;
    return true;
}

// A custom item aimed in first person (z64aiming) turns the view with the stick, never Link's legs, as vanilla first
// person does. The aim itself reads the raw pad, so only Link's copy is cleared.
void HoldStillWhileAiming(Player* player, Input* input) {
    const bool isAimingCustomItem =
        (player->heldItemAction == PLAYER_IA_CUSTOM) && (player->stateFlags1 & PLAYER_STATE1_100000);
    if (!isAimingCustomItem) {
        return;
    }
    input->cur.stick_x = 0;
    input->cur.stick_y = 0;
    input->rel.stick_x = 0;
    input->rel.stick_y = 0;
}

} // namespace

void CustomItemRegistry_Init() {
    SaveSections_Register(SaveSection, WriteSection, ReadSection);
    RegisterConsole();
    RegisterButtonIconHandlers();
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnActorDestroy>(
        [](Actor* actor) { CustomItemRegistry_UnbindActor(actor); });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerUpdate>(PutAwayStalledHeldItem);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerFilterInput>(HoldStillWhileAiming);
    CustomItemRegistry_RegisterAppearanceDrawing();
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSaveLoad>([](s16) { SyncAllButtons(); });
}

const S2HCustomPickupVariant* CustomItemRegistry_GetPickupVariant(const char* key) {
    const auto found = sPickupVariants.find(key != nullptr ? key : "");
    return found == sPickupVariants.end() ? nullptr : &found->second->definition;
}

const char* CustomItemRegistry_FindPickupKey(const char* itemKey, const char* variantKey) {
    if (itemKey == nullptr || variantKey == nullptr) return nullptr;
    for (const auto& [key, pickup] : sPickupVariants) {
        if (pickup->itemKey == itemKey && pickup->variantKey == variantKey) return key.c_str();
    }
    return nullptr;
}

static void ReceivePickupVariant(const char* key) {
    const auto* pickup = CustomItemRegistry_GetPickupVariant(key);
    if (pickup == nullptr) return;
    CustomItemRegistry_SetOwned(pickup->itemKey, true);
    if (pickup->onReceive != nullptr) pickup->onReceive(pickup->itemKey, pickup->variantKey);
}

bool CustomItemRegistry_RegisterPickupVariant(const S2HCustomPickupVariant* variant) {
    if (variant == nullptr || variant->structSize < offsetof(S2HCustomPickupVariant, drawSoulFlame) ||
        variant->key == nullptr || variant->itemKey == nullptr || variant->variantKey == nullptr ||
        variant->variantKey[0] == '\0' || sPickupVariants.contains(variant->key)) return false;
    const auto* parent = CustomItemRegistry_Find(variant->itemKey);
    if (parent == nullptr || CustomItemRegistry_GetPickupVariant(variant->itemKey) != nullptr) return false;
    S2HCustomItemDefinition definition = {};
    definition.structSize = sizeof(definition);
    definition.key = variant->key;
    definition.flags = S2H_CUSTOM_ITEM_INSTANT;
    definition.presentationFlags = S2H_ITEM_HIDE_FROM_KALEIDO | S2H_ITEM_HIDE_FROM_SAVE_EDITOR;
    definition.iconPath = variant->iconPath != nullptr ? variant->iconPath : parent->iconPath;
    definition.getItemModelPath = variant->getItemModelPath != nullptr ? variant->getItemModelPath : parent->getItemModelPath;
    definition.getItemText = variant->getItemText != nullptr ? variant->getItemText : parent->getItemText;
    definition.getItemScale = variant->getItemScale > 0 ? variant->getItemScale : parent->getItemScale;
    definition.randomizer = variant->randomizer;
    definition.onReceive = ReceivePickupVariant;
    if (!CustomItemRegistry_Register(&definition)) return false;
    auto stored = std::make_unique<RegisteredPickupVariant>();
    stored->key = variant->key; stored->itemKey = variant->itemKey; stored->variantKey = variant->variantKey;
    // Old callers have no flame tail; preserve their model rendering without reading past their struct.
    memcpy(&stored->definition, variant, std::min<size_t>(variant->structSize, sizeof(stored->definition)));
    stored->definition.structSize = sizeof(stored->definition);
    stored->definition.key = stored->key.c_str();
    stored->definition.itemKey = stored->itemKey.c_str();
    stored->definition.variantKey = stored->variantKey.c_str();
    const auto* acquisition = CustomItemRegistry_Find(variant->key);
    stored->definition.iconPath = acquisition->iconPath;
    stored->definition.getItemModelPath = acquisition->getItemModelPath;
    stored->definition.getItemText = acquisition->getItemText;
    stored->definition.getItemScale = acquisition->getItemScale;
    stored->definition.randomizer = acquisition->randomizer;
    sPickupVariants.emplace(stored->key, std::move(stored));
    return true;
}

bool CustomItemRegistry_Register(const S2HCustomItemDefinition* definition) {
    if (!IsValidDefinition(definition)) {
        return false;
    }
    if (sNextRuntimeId == UINT16_MAX) {
        SPDLOG_ERROR("[CustomItemRegistry] Runtime item id space is exhausted");
        return false;
    }

    auto item = std::make_unique<RegisteredCustomItem>();
    item->owner = sCurrentMod;
    item->key = definition->key;
    item->runtimeId = sNextRuntimeId;
    std::memcpy(&item->definition, definition, std::min<size_t>(definition->structSize, sizeof(item->definition)));
    item->definition.structSize = sizeof(item->definition);
    item->definition.key = item->key.c_str();
    SetString(item->definition.iconPath, item->iconPath, definition->iconPath);
    SetString(item->definition.namePath, item->namePath, definition->namePath);
    SetString(item->definition.pauseText, item->pauseText, definition->pauseText);
    SetString(item->definition.getItemText, item->getItemText, item->definition.getItemText);
    SetString(item->definition.heldModelPath, item->heldModelPath, item->definition.heldModelPath);
    SetString(item->definition.getItemModelPath, item->getItemModelPath, item->definition.getItemModelPath);
    SetString(item->definition.firstPersonModelPath, item->firstPersonModelPath,
              item->definition.firstPersonModelPath);
    SetString(item->definition.appearance.modelPath, item->appearanceModelPath,
              item->definition.appearance.modelPath);
    SetString(item->definition.displayName, item->displayName, item->definition.displayName);
    if (!StoreRandomizer(*item, definition->randomizer)) {
        return false;
    }

    RegisteredCustomItem* registered = item.get();
    sRuntimeItems.emplace(registered->runtimeId, registered);
    sCustomItems.emplace(registered->key, std::move(item));
    ++sNextRuntimeId;
    if (IsReplacement(registered->definition)) {
        sReplacements.emplace(registered->definition.replacesItem, registered);
    }
    for (uint8_t button = 0; button < S2H_ITEM_BUTTON_COUNT; ++button) {
        if (IsValidButton(button) && EquippedKey(button) == registered->key) {
            SyncButton(button);
        }
    }
    if (registered->definition.randomizer != nullptr) {
        RandoItems_Register(registered->key.c_str());
    }
    SPDLOG_INFO("[CustomItemRegistry] Mod '{}' registered custom item '{}' as {}", registered->owner, registered->key,
                registered->runtimeId);
    return true;
}

const S2HCustomItemDefinition* CustomItemRegistry_Find(const char* key) {
    return key == nullptr ? nullptr : FindDefinition(key);
}

std::string CustomItemRegistry_GetOwner(const char* key) {
    auto item = sCustomItems.find(key == nullptr ? "" : key);
    return item == sCustomItems.end() ? std::string() : item->second->owner;
}

const S2HGetItemEntry* CustomItemRegistry_FindGetItemEntry(const char* key) {
    const S2HCustomItemDefinition* definition = CustomItemRegistry_Find(key);
    return definition == nullptr ? nullptr : &definition->getItemEntry;
}

uint16_t CustomItemRegistry_GetRuntimeId(const char* key) {
    auto item = sCustomItems.find(key == nullptr ? "" : key);
    return item == sCustomItems.end() ? 0 : item->second->runtimeId;
}

const S2HCustomItemDefinition* CustomItemRegistry_FindByRuntimeId(uint16_t runtimeId) {
    auto item = sRuntimeItems.find(runtimeId);
    return item == sRuntimeItems.end() ? nullptr : &item->second->definition;
}

uint32_t CustomItemRegistry_GetCount(void) {
    return (uint32_t)sCustomItems.size();
}

const S2HCustomItemDefinition* CustomItemRegistry_GetAt(uint32_t index) {
    return index >= sRuntimeItems.size() ? nullptr : CustomItemRegistry_FindByRuntimeId(RuntimeIdBase + index);
}

const S2HCustomItemDefinition* CustomItemRegistry_FindReplacement(uint16_t vanillaItem) {
    auto replacement = sReplacements.find(vanillaItem);
    return replacement == sReplacements.end() ? nullptr : &replacement->second->definition;
}

bool CustomItemRegistry_IsFormAllowed(const char* key) {
    const S2HCustomItemDefinition* definition = CustomItemRegistry_Find(key);
    if (definition == nullptr) {
        return false;
    }
    return definition->formMask == 0 ||
           (definition->formMask & S2H_CUSTOM_ITEM_FORM_BIT(gSaveContext.save.playerForm)) != 0;
}

bool CustomItemRegistry_IsPauseItemFormAllowed(void) {
    const S2HCustomItemDefinition* definition = CustomItemRegistry_GetPauseItem();
    return definition != nullptr && CustomItemRegistry_IsFormAllowed(definition->key);
}

bool CustomItemRegistry_SetOwned(const char* key, bool owned) {
    if (key == nullptr || key[0] == '\0') {
        return false;
    }
    const S2HCustomItemDefinition* definition = CustomItemRegistry_Find(key);
    if (CustomItemRegistry_IsVanillaUpgrade(definition)) {
        SPDLOG_ERROR("[CustomItemRegistry] '{}' upgrades a vanilla item; its ownership is that item's", key);
        return false;
    }
    const bool wasOwned = sOwnedItems.contains(key);
    if (owned) {
        sOwnedItems.insert(key);
    } else {
        sOwnedItems.erase(key);
    }
    if (owned != wasOwned) GameInteractor::Instance->ExecuteHooks<GameInteractor::OnCustomItemOwnedChange>(key, owned);
    if (definition != nullptr && owned != wasOwned) {
        S2HCustomItemStateFunc callback = owned ? definition->onAcquire : definition->onRemove;
        if (callback != nullptr) {
            callback(key);
        }
    }
    return true;
}

void CustomItemRegistry_ReceiveGetItem(const char* key) {
    if (key == nullptr) return;
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnCustomItemReceive>(key);
    if (ProgressiveItems_ReceiveCustom(key)) return;
    CustomItemRegistry_SetOwned(key, true);

    const S2HCustomItemDefinition* definition = CustomItemRegistry_Find(key);
    if (definition != nullptr && definition->onReceive != nullptr) {
        definition->onReceive(key);
    }
}

bool CustomItemRegistry_SetWorn(const char* key, bool worn) {
    if (key == nullptr || CustomItemRegistry_Find(key) == nullptr) return false;
    if (worn) sWornItems.insert(key); else sWornItems.erase(key);
    return true;
}

bool CustomItemRegistry_IsWorn(const char* key) {
    if (key == nullptr) return false;
    if (sDrawingPlayer == nullptr) return sWornItems.contains(key);
    const char* const* keys = nullptr;
    uint32_t count = 0;
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerResolveWornCustomItems>(sDrawingPlayer, &keys, &count);
    for (uint32_t i = 0; keys != nullptr && i < count; ++i) {
        if (keys[i] != nullptr && std::strcmp(keys[i], key) == 0) return true;
    }
    return false;
}

std::vector<std::string> CustomItemRegistry_ListWorn() {
    return {sWornItems.begin(), sWornItems.end()};
}

void CustomItemRegistry_SetDrawingPlayer(Player* player) { sDrawingPlayer = player; }
Player* CustomItemRegistry_GetDrawingPlayer() { return sDrawingPlayer; }

bool CustomItemRegistry_IsOwned(const char* key) {
    const S2HCustomItemDefinition* definition = CustomItemRegistry_Find(key);
    if (definition == nullptr) {
        return false;
    }
    if (CustomItemRegistry_IsVanillaUpgrade(definition)) {
        return INV_CONTENT(definition->replacesItem) == definition->replacesItem;
    }
    return sOwnedItems.contains(key);
}

bool CustomItemRegistry_Equip(uint8_t button, const char* key) {
    const S2HCustomItemDefinition* definition = CustomItemRegistry_Find(key);
    if (!IsValidButton(button) || definition == nullptr || CustomItemRegistry_IsVanillaUpgrade(definition) ||
        !CustomItemRegistry_IsOwned(key) || !CustomItemRegistry_IsFormAllowed(key) ||
        (definition->flags & S2H_CUSTOM_ITEM_EQUIPPABLE) == 0) {
        return false;
    }
    const bool fitsButton = button == S2H_ITEM_BUTTON_B   ? (definition->flags & S2H_CUSTOM_ITEM_B_BUTTON) != 0
                            : IsDpadButton(button)        ? (definition->flags & S2H_CUSTOM_ITEM_DPAD) != 0
                                                          : (definition->flags & S2H_CUSTOM_ITEM_C_BUTTON) != 0;
    if (!fitsButton) {
        return false;
    }
    SwapOutOfOtherButtons(button, key);
    EquippedKey(button) = key;
    SyncButton(button);
    ReloadButtonIcon(button);
    return true;
}

void CustomItemRegistry_Unequip(uint8_t button) {
    if (!IsValidButton(button)) {
        return;
    }
    EquippedKey(button).clear();
    SyncButton(button);
    ReloadButtonIcon(button);
}

void CustomItemRegistry_CopyEquippedKey(uint8_t fromButton, uint8_t toButton) {
    if (IsValidButton(fromButton) && IsValidButton(toButton)) {
        EquippedKey(toButton) = EquippedKey(fromButton);
    }
}

const char* CustomItemRegistry_GetEquippedKey(uint8_t button) {
    if (!IsValidButton(button) || GetVanillaButtonItem(button) != ITEM_CUSTOM || EquippedKey(button).empty()) {
        return nullptr;
    }
    return EquippedKey(button).c_str();
}

uint16_t CustomItemRegistry_GetEquippedRuntimeId(uint8_t button) {
    return CustomItemRegistry_GetRuntimeId(CustomItemRegistry_GetEquippedKey(button));
}

// Returns false when vanilla must not go on to change the held item itself.
bool CustomItemRegistry_PressButtonItem(PlayState* play, Player* player, uint8_t button) {
    const S2HCustomItemDefinition* definition = GetEquippedDefinition(button);
    if (definition == nullptr || !CustomItemRegistry_IsFormAllowed(definition->key) ||
        (definition->canUse != nullptr && !definition->canUse(reinterpret_cast<struct Player*>(player),
                                                              reinterpret_cast<struct PlayState*>(play)))) {
        return false;
    }
    if ((definition->flags & S2H_CUSTOM_ITEM_WEARABLE) && button != S2H_ITEM_BUTTON_B && definition->init != nullptr) {
        const u8 previousButton=player->heldItemButton;
        player->heldItemButton=button;
        definition->init(play, player);
        player->heldItemButton=previousButton;
        return false;
    }
    if ((definition->flags & S2H_CUSTOM_ITEM_INSTANT) != 0) {
        if (definition->init != nullptr) {
            definition->init(reinterpret_cast<struct PlayState*>(play), reinterpret_cast<struct Player*>(player));
        }
        return false;
    }
    sActiveItem = definition->key;
    if (player->heldItemAction != PLAYER_IA_CUSTOM || sHeldItem.empty()) {
        return true;
    }
    if (sHeldItem == sActiveItem) {
        if (definition->use != nullptr) {
            definition->use(reinterpret_cast<struct Player*>(player), reinterpret_cast<struct PlayState*>(play));
        }
        return true;
    }
    Player_InitItemAction(play, player, PLAYER_IA_CUSTOM);
    return false;
}

bool CustomItemRegistry_SelectPauseItem(const char* key) {
    if (key == nullptr || !CustomItemRegistry_IsOwned(key)) {
        sPauseItem.clear();
        return false;
    }
    sPauseItem = key;
    return true;
}

const S2HCustomItemDefinition* CustomItemRegistry_GetPauseItem(void) {
    return FindDefinition(sPauseItem);
}

void CustomItemRegistry_PreparePauseMessage(void) {
    sMessageItem = sPauseItem;
}

const S2HCustomItemDefinition* CustomItemRegistry_GetMessageItem(void) {
    return FindDefinition(sMessageItem);
}

const S2HCustomItemDefinition* CustomItemRegistry_GetActive(void) {
    return sDrawingPlayer != nullptr ? CustomItemRegistry_Find(CustomItemRegistry_GetHeldKey()) : FindDefinition(sActiveItem);
}

const char* CustomItemRegistry_GetHeldKey(void) {
    if (sDrawingPlayer != nullptr) {
        const char* key = nullptr;
        GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerResolveHeldCustomItem>(sDrawingPlayer, &key);
        return key;
    }
    return sHeldItem.empty() ? nullptr : sHeldItem.c_str();
}

const char* CustomItemRegistry_ResolveTexture(const char* key, S2HItemIconSurface surface) {
    const S2HCustomItemDefinition* definition = CustomItemRegistry_Find(key);
    if (definition == nullptr || (definition->disabledIconSurfaces & surface) != 0) {
        return nullptr;
    }
    const char* path = surface == S2H_ITEM_NAME_TEXTURE ? definition->namePath : definition->iconPath;
    return path != nullptr && path[0] != '\0' ? path : nullptr;
}

bool CustomItemRegistry_IsEditorVisible(const char* key) {
    const S2HCustomItemDefinition* definition = CustomItemRegistry_Find(key);
    return definition != nullptr && (definition->presentationFlags & S2H_ITEM_HIDE_FROM_SAVE_EDITOR) == 0;
}

bool CustomItemRegistry_SetIconPath(const char* key, const char* path) {
    auto item = sCustomItems.find(key == nullptr ? "" : key);
    if (item == sCustomItems.end()) {
        return false;
    }
    SetString(item->second->definition.iconPath, item->second->iconPath, path);
    return true;
}

bool CustomItemRegistry_SetText(const char* key, S2HItemTextKind kind, const char* text) {
    auto item = sCustomItems.find(key == nullptr ? "" : key);
    if (item == sCustomItems.end()) {
        return false;
    }
    switch (kind) {
        case S2H_ITEM_TEXT_GET_ITEM:
            SetString(item->second->definition.getItemText, item->second->getItemText, text);
            return true;
        case S2H_ITEM_TEXT_PAUSE:
            SetString(item->second->definition.pauseText, item->second->pauseText, text);
            return true;
        default:
            SPDLOG_ERROR("[CustomItemRegistry] Custom item '{}' asked for unknown text {}", item->second->key,
                         (int)kind);
            return false;
    }
}

int32_t CustomItemRegistry_GetAmmo(const char* key) {
    const S2HCustomItemDefinition* definition = CustomItemRegistry_Find(key);
    return definition != nullptr && definition->getAmmo != nullptr ? definition->getAmmo(key) : -1;
}

uint8_t CustomItemRegistry_GetActiveMeleeWeapon(void) {
    const S2HCustomItemDefinition* definition = FindDefinition(sActiveItem);
    return definition == nullptr ? 0 : definition->meleeWeapon;
}

uint8_t CustomItemRegistry_GetHeldMeleeWeapon(void) {
    const S2HCustomItemDefinition* definition = FindDefinition(sHeldItem);
    return definition == nullptr ? 0 : definition->meleeWeapon;
}

void CustomItemRegistry_InitHeld(PlayState* play, Player* player) {
    if (sHeldItem != sActiveItem) {
        PutAwayHeldItem(play, player);
    }
    sHeldItem = sActiveItem;
    sHeldUpdateFrame = play->state.frames;
    const S2HCustomItemDefinition* definition = FindDefinition(sHeldItem);
    if (definition != nullptr && definition->init != nullptr) {
        definition->init(reinterpret_cast<struct PlayState*>(play), reinterpret_cast<struct Player*>(player));
    }
}

int32_t CustomItemRegistry_UpdateHeld(Player* player, PlayState* play) {
    const S2HCustomItemDefinition* definition = FindDefinition(sHeldItem);
    sHeldUpdateFrame = play->state.frames;
    if (definition == nullptr || definition->update == nullptr) {
        return 0;
    }
    return definition->update(reinterpret_cast<struct Player*>(player), reinterpret_cast<struct PlayState*>(play));
}

static const char* ResolveOtherPlayerHeldKey(Player* player) {
    const char* key = nullptr;
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerResolveHeldCustomItem>(player, &key);
    return key;
}

void CustomItemRegistry_DrawHeld(Player* player, PlayState* play) {
    const bool isOtherPlayer = player != GET_PLAYER(play);
    const char* key = isOtherPlayer ? ResolveOtherPlayerHeldKey(player) : (sHeldItem.empty() ? nullptr : sHeldItem.c_str());
    const S2HCustomItemDefinition* definition = CustomItemRegistry_Find(key);
    if (definition == nullptr) {
        return;
    }
    // Another player's item with an appearance is drawn at its own limb (DrawOtherPlayerAppearance).
    if (isOtherPlayer && definition->appearance.modelPath != nullptr) {
        return;
    }
    if (definition->drawHeld != nullptr) {
        definition->drawHeld(reinterpret_cast<struct Player*>(player), reinterpret_cast<struct PlayState*>(play));
        return;
    }
    if (definition->heldModelPath == nullptr) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)definition->heldModelPath);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawAppearance(PlayState* play, Player* player, const S2HCustomItemAppearance& appearance) {
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    if (appearance.scale > 0.0f) {
        // The limb matrix carries Link's 0.01 body scale: dividing it back out leaves the offsets in world units.
        const f32 unscale = player->actor.scale.x != 0.0f ? 1.0f / player->actor.scale.x : 1.0f;
        Matrix_Scale(unscale, unscale, unscale, MTXMODE_APPLY);
        Matrix_RotateYF(DEG_TO_RAD(appearance.rotation[1]), MTXMODE_APPLY);
        Matrix_RotateXFApply(DEG_TO_RAD(appearance.rotation[0]));
        Matrix_RotateZF(DEG_TO_RAD(appearance.rotation[2]), MTXMODE_APPLY);
        Matrix_Translate(appearance.offset[0], appearance.offset[1], appearance.offset[2], MTXMODE_APPLY);
        Matrix_Scale(appearance.scale, appearance.scale, appearance.scale, MTXMODE_APPLY);
    }
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)appearance.modelPath);
    Matrix_Pop();
    gDPPipeSync(POLY_OPA_DISP++);
    gSPLoadGeometryMode(POLY_OPA_DISP++, G_ZBUFFER | G_SHADE | G_CULL_BACK | G_LIGHTING | G_SHADING_SMOOTH);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawAppearanceAtLimb(PlayState* play, Player* player, s32 limbIndex, const char* key, u8 defaultLimb) {
    const S2HCustomItemDefinition* definition = CustomItemRegistry_Find(key);
    if (definition == nullptr || definition->appearance.modelPath == nullptr) {
        return;
    }
    const u8 limb = definition->appearance.limb != 0 ? definition->appearance.limb : defaultLimb;
    if (limbIndex == limb) {
        DrawAppearance(play, player, definition->appearance);
    }
}

static void DrawOtherPlayerAppearance(Player* player, s32 limbIndex) {
    if (gPlayState == nullptr || player == GET_PLAYER(gPlayState)) {
        return;
    }
    DrawAppearanceAtLimb(gPlayState, player, limbIndex, ResolveOtherPlayerHeldKey(player), PLAYER_LIMB_LEFT_HAND);

    const char* const* wornKeys = nullptr;
    uint32_t wornCount = 0;
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerResolveWornCustomItems>(player, &wornKeys,
                                                                                           &wornCount);
    for (uint32_t index = 0; wornKeys != nullptr && index < wornCount; index++) {
        DrawAppearanceAtLimb(gPlayState, player, limbIndex, wornKeys[index], PLAYER_LIMB_HEAD);
    }
}

void CustomItemRegistry_RegisterAppearanceDrawing(void) {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerPostLimbDraw>(DrawOtherPlayerAppearance);
}

const char* CustomItemRegistry_GetFirstPersonModel(void) {
    const S2HCustomItemDefinition* definition = FindDefinition(sHeldItem);
    return definition == nullptr ? nullptr : definition->firstPersonModelPath;
}

void CustomItemRegistry_HoldHeld(void) {
    if (!sHeldItem.empty() && gPlayState != nullptr) {
        sHeldUpdateFrame = gPlayState->state.frames;
    }
}

bool CustomItemRegistry_BindActor(Actor* actor, const char* key) {
    if (actor == nullptr || CustomItemRegistry_Find(key) == nullptr) {
        return false;
    }
    sActorItems[actor] = key;
    return true;
}

void CustomItemRegistry_UnbindActor(Actor* actor) {
    sActorItems.erase(actor);
}

const char* CustomItemRegistry_GetActorItem(Actor* actor) {
    auto item = sActorItems.find(actor);
    return item == sActorItems.end() ? nullptr : item->second.c_str();
}

// MM has no get-item table for these: the fanfare and the textbox are the whole acquisition.
bool CustomItemRegistry_Give(PlayState* play, const char* key) {
    if (const char* next = ProgressiveItems_ResolvePickup(key, UINT16_MAX)) key = next;
    key = GameInteractor_ExecuteOnResolveCustomGetItem(nullptr, play, key);
    const S2HCustomItemDefinition* definition = CustomItemRegistry_Find(key);
    if (play == nullptr || definition == nullptr) {
        return false;
    }
    sMessageItem = definition->key;
    CustomItemRegistry_ReceiveGetItem(key);
    Audio_PlayFanfare(NA_BGM_GET_ITEM);
    if (definition->getItemText != nullptr) {
        CustomMessage::Entry entry;
        if (definition->iconPath != nullptr && ResourceMgr_FileExists(definition->iconPath)) {
            entry.icon = ITEM_OCARINA_OF_TIME;
            entry.iconPath = definition->iconPath;
        }
        CustomMessage::StartTextbox(definition->getItemText, entry);
    }
    return true;
}

bool CustomItemRegistry_ShowTextbox(PlayState* play, const char* text, bool autoFormat) {
    if (play == nullptr || text == nullptr) {
        return false;
    }
    CustomMessage::Entry entry;
    entry.autoFormat = autoFormat;
    CustomMessage::StartTextbox(text, entry);
    return true;
}

void CustomItemRegistry_SetCurrentMod(const std::string& name) {
    sCurrentMod = name;
}

void CustomItemRegistry_ClearCurrentMod() {
    sCurrentMod.clear();
}

std::vector<std::string> CustomItemRegistry_ListOwned() { return {sOwnedItems.begin(), sOwnedItems.end()}; }
