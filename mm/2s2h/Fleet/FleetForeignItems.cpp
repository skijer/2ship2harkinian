#include "FleetForeignItems.h"
#include "FleetItemDraw.h"

#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "2s2h/ModApi/CustomItemRegistry/CustomItemRegistry.h"

extern "C" {
#include "functions.h"
}

namespace {

constexpr const char* kGame = "mm";

struct ForeignItem {
    std::string owner;
    std::string item;
};

std::vector<ForeignItem> sItems;
std::unordered_set<std::string> sKeys;
// Icon path to owner. Entries are never erased: the registry and the frame's display list point into them.
std::unordered_map<std::string, std::string> sIconOwners;

const char* RecordTextboxIcon(const ForeignItem& foreign) {
    const std::string path = FleetItemDraw_ForeignTextboxIconPath(foreign.owner.c_str(), foreign.item.c_str());
    if (path.empty()) {
        return nullptr;
    }
    return sIconOwners.emplace(path, foreign.owner).first->first.c_str();
}

std::string PresentedName(const nlohmann::json& foreign) {
    const std::string fake = foreign.value("fakeItemName", "");
    return fake.empty() ? foreign.at("itemName").get<std::string>() : fake;
}

void DrawForeignItem(PlayState* play, S2HGetItemEntry* entry) {
    if (entry->tableId < sItems.size()) {
        const ForeignItem& foreign = sItems[entry->tableId];
        if (FleetItemDraw_DrawForeign(play, foreign.owner.c_str(), foreign.item.c_str())) {
            return;
        }
    }
    GetItem_Draw(play, GID_RUPEE_BLUE);
}

void Register(const std::string& key, const ForeignItem& foreign, const nlohmann::json& reward) {
    const bool shared = reward.value("shared", false);
    const bool disguised = !reward.value("fakeItemName", std::string{}).empty();
    const std::string shown = disguised ? foreign.item : reward.value("label", foreign.item);
    const std::string text = "You found %y" + shown + (shared ? " (Shared)" : " (OoT)") + "%w!&" +
                             (shared ? "Both games share it." : "It belongs to Ocarina of Time.");
    S2HCustomItemRandomizer randomizer = {};
    randomizer.structSize = sizeof(randomizer);
    randomizer.flags = reward.value("advancement", false) ? S2H_CUSTOM_ITEM_RANDO_ADVANCEMENT : 0;
    S2HCustomItemDefinition definition = {};
    definition.structSize = sizeof(definition);
    definition.key = key.c_str();
    definition.randomizer = &randomizer;
    definition.presentationFlags = S2H_ITEM_HIDE_FROM_SAVE_EDITOR | S2H_ITEM_HIDE_FROM_KALEIDO;
    definition.getItemText = text.c_str();
    definition.displayName = foreign.item.c_str();
    definition.pauseText = foreign.item.c_str();
    definition.iconPath = RecordTextboxIcon(foreign);
    // Only the textbox reads the icon inside its owner's archives; any other surface would search 2Ship's.
    definition.disabledIconSurfaces = ~static_cast<uint32_t>(S2H_ITEM_ICON_TEXTBOX);
    definition.getItemEntry.tableId = static_cast<uint16_t>(sItems.size());
    definition.getItemEntry.drawFunc = DrawForeignItem;
    if (!CustomItemRegistry_Register(&definition)) {
        throw std::runtime_error("could not register the foreign item " + key);
    }
}

} // namespace

std::string FleetForeignItems_Key(const nlohmann::json& foreign) {
    return "fleet:" + foreign.at("itemGame").get<std::string>() + ":" + PresentedName(foreign);
}

std::string FleetForeignItems_KeyForCheck(const nlohmann::json& seed, const std::string& check) {
    for (const auto& foreign : seed.at("foreign")) {
        if (foreign.at("checkGame") == kGame && foreign.at("checkName") == check) {
            return FleetForeignItems_Key(foreign);
        }
    }
    return "";
}

void FleetForeignItems_Register(const nlohmann::json& seed) {
    for (const auto& foreign : seed.at("foreign")) {
        if (foreign.at("checkGame") != kGame) {
            continue;
        }
        const std::string key = FleetForeignItems_Key(foreign);
        if (sKeys.contains(key)) {
            continue;
        }
        const ForeignItem item = { foreign.at("itemGame").get<std::string>(), PresentedName(foreign) };
        Register(key, item, foreign);
        sItems.push_back(item);
        sKeys.insert(key);
    }
}

const char* FleetForeignItems_FindIconOwner(const char* iconPath) {
    if (iconPath == nullptr || sIconOwners.empty()) {
        return nullptr;
    }
    const auto found = sIconOwners.find(iconPath);
    return found != sIconOwners.end() ? found->second.c_str() : nullptr;
}
