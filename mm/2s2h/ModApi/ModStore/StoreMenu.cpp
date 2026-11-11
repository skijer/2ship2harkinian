#include "ModStore.h"
#include "StoreHttp.h"
#include "StoreInternal.h"
#include "StorePackCard.h"
#include "StorePackDefaults.h"

#include <algorithm>
#include <filesystem>
#include <functional>
#include <map>
#include <set>
#include <sstream>

#include <SDL2/SDL_misc.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <fast/Fast3dGui.h>
#include <ship/Context.h>
#include <ship/resource/File.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/Archive.h>
#include <ship/resource/archive/ArchiveManager.h>

#include <libultraship/bridge/consolevariablebridge.h>
#define CVAR_SETTING(name) "gSettings." name
#include "2s2h/BenGui/BenGui.hpp"
#include "2s2h/BenGui/UIWidgets.hpp"
#include "variables.h"
extern "C" {
#include "build.h"
}

namespace {

#define CVAR_STORE_CHECK_AT_STARTUP CVAR_SETTING("ModStore.CheckAtStartup")
constexpr int StartupCheckUnasked = 0;
constexpr int StartupCheckOn = 1;
constexpr int StartupCheckOff = 2;
constexpr size_t PackGridColumns = 3;
constexpr size_t MaxManifestBytes = 1024 * 1024;
constexpr size_t MaxOptionsPerMod = 64;
constexpr size_t MaxOptionIdBytes = 64;
constexpr size_t MaxOptionNameBytes = 160;
constexpr ItemId DefaultPackIconItem = ITEM_OCARINA_OF_TIME;
constexpr const char* AuthorGroupIcon = "ITEM_BOMB";

using Actions = std::vector<std::function<void()>>;
using Json = nlohmann::json;

std::map<std::string, std::set<std::string>> sUncheckedByPack;
bool sRequestedCatalog = false;
std::string sExpandedPack;
ImGuiTextFilter sPackFilter;
bool sOptionsChanged = false;

std::shared_ptr<Ship::Gui> GetGui() {
    return Ship::Context::GetRawInstance()->GetWindow()->GetGui();
}

// The GUI caches MM item icons under their resource path, so an icon name goes through the item table.
ImTextureID PackIcon(const std::string& icon) {
    static const std::map<std::string, ItemId> itemsByIconName = {
        { "ITEM_OCARINA_OF_TIME", ITEM_OCARINA_OF_TIME },
        { "ITEM_BOW", ITEM_BOW },
        { "ITEM_BOMB", ITEM_BOMB },
        { "ITEM_LENS_OF_TRUTH", ITEM_LENS_OF_TRUTH },
        { "ITEM_SHIELD_HERO", ITEM_SHIELD_HERO },
        { "ITEM_MASK_KEATON", ITEM_MASK_KEATON },
        { "ITEM_MASK_CAPTAIN", ITEM_MASK_CAPTAIN },
        { "ITEM_MASK_TRUTH", ITEM_MASK_TRUTH },
        { "ITEM_MASK_BREMEN", ITEM_MASK_BREMEN },
    };
    const auto gui = std::dynamic_pointer_cast<Fast::Fast3dGui>(GetGui());
    const auto item = itemsByIconName.find(icon);
    const ItemId itemId = item != itemsByIconName.end() ? item->second : DefaultPackIconItem;
    const char* texture = static_cast<const char*>(gItemIcons[itemId]);
    if (gui == nullptr || texture == nullptr || !gui->HasTextureByName(texture)) {
        return 0;
    }
    return gui->GetTextureByName(texture);
}

UIWidgets::ButtonOptions InlineButton() {
    return UIWidgets::ButtonOptions().Size(UIWidgets::Sizes::Inline).Color(THEME_COLOR);
}

void SetStartupCheck(int value) {
    CVarSetInteger(CVAR_STORE_CHECK_AT_STARTUP, value);
    GetGui()->SaveConsoleVariablesNextFrame();
}

const char* DescribeState(StoreModState state) {
    switch (state) {
        case StoreModState::NotInstalled:
            return "not installed";
        case StoreModState::Installed:
            return "installed";
        case StoreModState::Outdated:
            return "update available";
        case StoreModState::Disabled:
            return "disabled";
        case StoreModState::InstallsOnRestart:
            return "installs on restart";
        case StoreModState::DisabledOnRestart:
            return "disabled on restart";
        case StoreModState::RemovedOnRestart:
            return "removed on restart";
        default:
            return "unknown";
    }
}

ImVec4 StateColor(StoreModState state) {
    switch (state) {
        case StoreModState::Installed:
            return ImVec4(0.4f, 0.85f, 0.4f, 1.0f);
        case StoreModState::Outdated:
            return UIWidgets::ColorValues.at(UIWidgets::Colors::Yellow);
        case StoreModState::InstallsOnRestart:
        case StoreModState::DisabledOnRestart:
        case StoreModState::RemovedOnRestart:
            return UIWidgets::ColorValues.at(UIWidgets::Colors::Cyan);
        default:
            return UIWidgets::ColorValues.at(UIWidgets::Colors::Gray);
    }
}

StoreModState StateOf(const StoreView& view, const std::string& id) {
    const auto state = view.states.find(id);
    return state != view.states.end() ? state->second : StoreModState::NotInstalled;
}

std::string Join(const std::vector<std::string>& values, const char* separator = ", ") {
    std::string text;
    for (const std::string& value : values) {
        text += (text.empty() ? "" : separator) + value;
    }
    return text;
}

void DrawModTooltip(const StoreMod& mod) {
    std::string text = mod.description.empty() ? mod.name : mod.description;
    text += "\n\nBy " + mod.author + " (" + mod.repository + ")";
    if (!mod.permissions.empty()) {
        text += "\nPermissions: " + Join(mod.permissions);
    }
    if (!mod.dependencies.empty()) {
        text += "\nAlso installs: " + Join(mod.dependencies);
    }
    UIWidgets::Tooltip(text.c_str());
}

void DrawModLabel(const StoreView& view, const StoreMod& mod) {
    ImGui::SameLine();
    ImGui::TextDisabled("v%s", mod.version.c_str());
    ImGui::SameLine();
    const StoreModState state = StateOf(view, mod.id);
    ImGui::TextColored(StateColor(state), "%s", DescribeState(state));
}

// Warns before a change that would leave other installed mods without something they need.
void ConfirmIfNeeded(const StoreView& view, const StoreMod& mod, const char* verb, Actions& actions,
                     std::function<void()> change) {
    const std::vector<std::string> dependents = ModStore_FindDependents(view, mod.id);
    if (dependents.empty()) {
        actions.push_back(std::move(change));
        return;
    }
    const std::string title = std::string(verb) + " " + mod.name;
    actions.push_back([title, dependents, change] {
        BenGui::RegisterPopup(title, Join(dependents) + " need it and will stop working.", "Continue", "Cancel",
                              change);
    });
}

std::set<std::string> CuratedModIds() {
    std::set<std::string> ids;
    for (const StorePack& pack : ModStore_DefaultPacks()) {
        ids.insert(pack.mods.begin(), pack.mods.end());
    }
    return ids;
}

std::vector<StorePack> PublishedCuratedPacks(const StoreCatalog& catalog) {
    std::vector<StorePack> packs;
    for (StorePack pack : ModStore_DefaultPacks()) {
        std::erase_if(pack.mods, [&](const std::string& id) { return catalog.mods.count(id) == 0; });
        if (!pack.mods.empty()) {
            packs.push_back(std::move(pack));
        }
    }
    return packs;
}

// The catalog still groups the curated mods its own way; only unrelated community packs are kept.
bool IsCommunityPack(const StorePack& pack, const std::set<std::string>& curatedIds) {
    return std::none_of(pack.mods.begin(), pack.mods.end(), [&](const std::string& id) {
        return curatedIds.count(id) != 0 || id == ModStore_SharedAssetsMod;
    });
}

std::string CreditedAuthor(const StoreMod& mod) {
    if (!mod.owner.empty()) {
        return mod.owner;
    }
    return mod.author.empty() ? "Unknown author" : mod.author;
}

StorePack MakeAuthorGroup(const std::string& author) {
    constexpr char HexDigits[] = "0123456789abcdef";
    StorePack group;
    // The id becomes part of a CVar path, so the author is hex-encoded instead of used as written.
    group.id = "other-";
    for (unsigned char c : author) {
        group.id += HexDigits[c >> 4];
        group.id += HexDigits[c & 15];
    }
    group.name = "Other mods - " + author;
    group.author = author;
    group.description = "Mods by this author outside a curated pack.";
    group.icon = AuthorGroupIcon;
    return group;
}

std::vector<StorePack> GroupUnpackedModsByAuthor(const StoreCatalog& catalog, const std::vector<StorePack>& packs) {
    std::set<std::string> packed = { ModStore_SharedAssetsMod };
    for (const StorePack& pack : packs) {
        packed.insert(pack.mods.begin(), pack.mods.end());
    }
    std::map<std::string, StorePack> groupsByAuthor;
    for (const auto& [id, mod] : catalog.mods) {
        if (packed.count(id) != 0 || mod.dependencyOnly) {
            continue;
        }
        const std::string author = CreditedAuthor(mod);
        const auto [group, created] = groupsByAuthor.try_emplace(author);
        if (created) {
            group->second = MakeAuthorGroup(author);
        }
        group->second.mods.push_back(id);
    }
    std::vector<StorePack> groups;
    for (auto& [author, group] : groupsByAuthor) {
        groups.push_back(std::move(group));
    }
    return groups;
}

std::vector<StorePack> PacksWithOtherMods(const StoreCatalog& catalog) {
    std::vector<StorePack> packs = PublishedCuratedPacks(catalog);
    const std::set<std::string> curatedIds = CuratedModIds();
    for (const StorePack& pack : catalog.packs) {
        if (IsCommunityPack(pack, curatedIds)) {
            packs.push_back(pack);
        }
    }
    for (StorePack& group : GroupUnpackedModsByAuthor(catalog, packs)) {
        packs.push_back(std::move(group));
    }
    return packs;
}

std::string PackSelectionKey(const std::string& packId) {
    return CVAR_SETTING("ModStore.PackSelection.") + packId;
}

std::set<std::string>& PackSelection(const std::string& packId) {
    const auto [entry, inserted] = sUncheckedByPack.try_emplace(packId);
    if (!inserted) {
        return entry->second;
    }
    std::istringstream saved(CVarGetString(PackSelectionKey(packId).c_str(), ""));
    std::string modId;
    while (std::getline(saved, modId, ',')) {
        entry->second.insert(modId);
    }
    return entry->second;
}

void SavePackSelection(const std::string& packId, const std::set<std::string>& unchecked) {
    const std::vector<std::string> modIds(unchecked.begin(), unchecked.end());
    const std::string saved = Join(modIds, ",");
    CVarSetString(PackSelectionKey(packId).c_str(), saved.c_str());
    GetGui()->SaveConsoleVariablesNextFrame();
}

bool IsOptionId(const std::string& id) {
    const bool sized = !id.empty() && id.size() <= MaxOptionIdBytes;
    return sized && std::all_of(id.begin(), id.end(), [](unsigned char c) {
               return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
           });
}

std::string ReadOptionText(const Json& option, const char* key) {
    if (!option.is_object()) {
        return std::string();
    }
    const auto field = option.find(key);
    return field != option.end() && field->is_string() ? field->get<std::string>() : std::string();
}

std::vector<StoreOption> ParseManifestOptions(const Json& manifest) {
    std::vector<StoreOption> options;
    if (!manifest.is_object()) {
        return options;
    }
    const auto declared = manifest.find("options");
    if (declared == manifest.end() || !declared->is_array() || declared->size() > MaxOptionsPerMod) {
        return options;
    }
    std::set<std::string> seen;
    for (const Json& option : *declared) {
        const std::string id = ReadOptionText(option, "id");
        const std::string name = ReadOptionText(option, "name");
        const bool named = !name.empty() && name.size() <= MaxOptionNameBytes;
        if (IsOptionId(id) && named && seen.insert(id).second) {
            options.push_back({ id, name });
        }
    }
    return options;
}

std::vector<StoreOption> ReadArchiveOptions(Ship::Archive& archive) {
    const auto file = archive.LoadFile("manifest.json");
    const bool loaded = file != nullptr && file->IsLoaded && file->Buffer != nullptr;
    if (!loaded || file->Buffer->size() > MaxManifestBytes) {
        return {};
    }
    return ParseManifestOptions(Json::parse(file->Buffer->begin(), file->Buffer->end(), nullptr, false));
}

std::map<std::string, std::vector<StoreOption>> ReadInstalledOptions() {
    std::map<std::string, std::vector<StoreOption>> optionsByMod;
    const auto archives = Ship::Context::GetRawInstance()->GetResourceManager()->GetArchiveManager()->GetArchives();
    if (archives == nullptr) {
        return optionsByMod;
    }
    for (const auto& archive : *archives) {
        const std::string modId = std::filesystem::path(archive->GetPath()).stem().string();
        try {
            optionsByMod[modId] = ReadArchiveOptions(*archive);
        } catch (const std::exception& error) {
            // Optional presentation metadata must never keep the store from opening.
            SPDLOG_WARN("[ModStore] Could not read the options of {}: {}", modId, error.what());
        }
    }
    return optionsByMod;
}

// An installed manifest can list options before a new catalog is published. Presentation only:
// download URLs, hashes and trust decisions always come from the signed catalog.
const std::vector<StoreOption>& InstalledOptions(const std::string& modId) {
    static const std::map<std::string, std::vector<StoreOption>> optionsByMod = ReadInstalledOptions();
    static const std::vector<StoreOption> none;
    const auto found = optionsByMod.find(modId);
    return found == optionsByMod.end() ? none : found->second;
}

void DrawModOption(const StoreMod& mod, const StoreOption& option) {
    const std::string cvar = "gMods.Store." + mod.id + "." + option.id;
    bool enabled = CVarGetInteger(cvar.c_str(), 1) != 0;
    ImGui::PushID(option.id.c_str());
    if (UIWidgets::Checkbox(option.name.c_str(), &enabled, UIWidgets::CheckboxOptions().Color(THEME_COLOR))) {
        CVarSetInteger(cvar.c_str(), enabled ? 1 : 0);
        GetGui()->SaveConsoleVariablesNextFrame();
        sOptionsChanged = true;
    }
    ImGui::PopID();
}

void DrawModOptions(const StoreMod& mod) {
    const std::vector<StoreOption>& installed = InstalledOptions(mod.id);
    const std::vector<StoreOption>& options = installed.empty() ? mod.options : installed;
    if (options.empty()) {
        return;
    }
    ImGui::PushID(mod.id.c_str());
    if (ImGui::TreeNodeEx("Items and features", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextWrapped("Enable individual features. Changes apply after restarting Unbound.");
        for (const StoreOption& option : options) {
            DrawModOption(mod, option);
        }
        ImGui::TreePop();
    }
    ImGui::PopID();
}

void DrawPackTile(const StoreView& view, const StorePack& pack, Actions& actions) {
    ImGui::PushID(pack.id.c_str());
    const bool expanded = sExpandedPack == pack.id;
    StorePackCard::Content card;
    card.name = pack.name;
    card.author = pack.author.empty() ? "Community" : pack.author;
    card.version = pack.version.empty() ? "Versions: per mod" : "Version " + pack.version;
    card.icon = PackIcon(pack.icon);
    if (StorePackCard::Draw(card, expanded, ImGui::GetContentRegionAvail().x)) {
        sExpandedPack = expanded ? "" : pack.id;
    }
    const auto installed = std::count_if(pack.mods.begin(), pack.mods.end(), [&](const std::string& id) {
        return ModStore_IsInstalled(StateOf(view, id));
    });
    ImGui::BeginDisabled(view.busy || pack.mods.empty());
    if (UIWidgets::Button(
            "Install pack",
            InlineButton().Tooltip("Install every mod in this pack and its dependencies. Applies after restart."))) {
        const std::vector<std::string> ids = pack.mods;
        actions.push_back([ids] { ModStore_Install(ids); });
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextDisabled("%d/%zu installed", static_cast<int>(installed), pack.mods.size());
    ImGui::PopID();
}

void DrawSelectionShortcuts(const StorePack& pack, std::set<std::string>& unchecked) {
    if (ImGui::SmallButton("Select all")) {
        unchecked.clear();
        SavePackSelection(pack.id, unchecked);
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Select none")) {
        unchecked = std::set<std::string>(pack.mods.begin(), pack.mods.end());
        SavePackSelection(pack.id, unchecked);
    }
}

// Returns whether the mod is selected for the pack's "Install selection".
bool DrawSelectableMod(const StoreView& view, const StorePack& pack, const StoreMod& mod) {
    std::set<std::string>& unchecked = PackSelection(pack.id);
    bool checked = unchecked.count(mod.id) == 0;
    ImGui::PushID(mod.id.c_str());
    if (UIWidgets::Checkbox("##selected", &checked, UIWidgets::CheckboxOptions().Color(THEME_COLOR))) {
        if (checked) {
            unchecked.erase(mod.id);
        } else {
            unchecked.insert(mod.id);
        }
        SavePackSelection(pack.id, unchecked);
    }
    ImGui::SameLine();
    ImGui::TextUnformatted(ModStore_DisplayName(mod).c_str());
    DrawModTooltip(mod);
    DrawModLabel(view, mod);
    DrawModOptions(mod);
    ImGui::PopID();
    return checked;
}

void DrawPackSelection(const StoreView& view, const StorePack& pack, Actions& actions) {
    ImGui::TextWrapped("Choose which mods to install. Required dependencies are included automatically. This does "
                       "not remove installed mods.");
    DrawSelectionShortcuts(pack, PackSelection(pack.id));
    std::vector<std::string> selected;
    for (const std::string& id : pack.mods) {
        const auto mod = view.catalog.mods.find(id);
        if (mod != view.catalog.mods.end() && DrawSelectableMod(view, pack, mod->second)) {
            selected.push_back(id);
        }
    }
    ImGui::TextDisabled("%zu selected", selected.size());
    ImGui::BeginDisabled(view.busy || selected.empty());
    if (UIWidgets::Button("Install selection", InlineButton())) {
        actions.push_back([selected] { ModStore_Install(selected); });
    }
    ImGui::EndDisabled();
}

void DrawPackDetails(const StoreView& view, const StorePack& pack, Actions& actions) {
    ImGui::PushID(pack.id.c_str());
    ImGui::Spacing();
    ImGui::SeparatorText(pack.name.c_str());
    ImGui::TextWrapped("%s", pack.description.empty() ? "No description provided." : pack.description.c_str());
    if (ImGui::SmallButton("Collapse details")) {
        sExpandedPack.clear();
    }
    ImGui::Indent();
    DrawPackSelection(view, pack, actions);
    ImGui::Unindent();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::PopID();
}

void DrawPackRow(const StoreView& view, const std::vector<const StorePack*>& row, float width, Actions& actions) {
    const ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoPadOuterX;
    if (ImGui::BeginTable("##PackGrid", PackGridColumns, flags, ImVec2(width, 0))) {
        for (const StorePack* pack : row) {
            ImGui::TableNextColumn();
            DrawPackTile(view, *pack, actions);
        }
        ImGui::EndTable();
    }
    const auto expanded =
        std::find_if(row.begin(), row.end(), [](const StorePack* pack) { return pack->id == sExpandedPack; });
    if (expanded != row.end()) {
        DrawPackDetails(view, **expanded, actions);
    }
    ImGui::Spacing();
}

std::vector<const StorePack*> FilterPacks(const std::vector<StorePack>& packs) {
    std::vector<const StorePack*> visible;
    for (const StorePack& pack : packs) {
        const std::string searchable = pack.name + " " + pack.author + " " + pack.description;
        if (sPackFilter.PassFilter(searchable.c_str())) {
            visible.push_back(&pack);
        }
    }
    return visible;
}

void DrawPacks(const StoreView& view, Actions& actions) {
    sPackFilter.Draw("Search packs", 240.0f);
    const std::vector<StorePack> packs = PacksWithOtherMods(view.catalog);
    const std::vector<const StorePack*> visible = FilterPacks(packs);
    if (visible.empty()) {
        ImGui::TextDisabled("No packs found.");
        return;
    }
    // Keeps three readable columns on narrow menus; the child supplies the horizontal scrolling.
    const float width = std::max(ImGui::GetContentRegionAvail().x, ImGui::GetFontSize() * 60.0f);
    ImGui::SetNextWindowContentSize(ImVec2(width, 0));
    if (ImGui::BeginChild("##PackGridScroll", ImVec2(0, 0), ImGuiChildFlags_None,
                          ImGuiWindowFlags_HorizontalScrollbar)) {
        for (size_t first = 0; first < visible.size(); first += PackGridColumns) {
            const size_t last = std::min(first + PackGridColumns, visible.size());
            const std::vector<const StorePack*> row(visible.begin() + first, visible.begin() + last);
            ImGui::PushID(static_cast<int>(first));
            DrawPackRow(view, row, width, actions);
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
}

void DrawInstalledRow(const StoreView& view, const StoreMod& mod, StoreModState state, Actions& actions) {
    ImGui::TextUnformatted(ModStore_DisplayName(mod).c_str());
    DrawModTooltip(mod);
    DrawModLabel(view, mod);
    const std::string id = mod.id;
    ImGui::BeginDisabled(view.busy);
    if (state == StoreModState::Outdated) {
        ImGui::SameLine();
        if (UIWidgets::Button(("Update##" + id).c_str(), InlineButton())) {
            actions.push_back([id] { ModStore_Install({ id }); });
        }
    }
    const bool disabled = state == StoreModState::Disabled || state == StoreModState::DisabledOnRestart;
    ImGui::SameLine();
    if (UIWidgets::Button(((disabled ? "Enable##" : "Disable##") + id).c_str(), InlineButton())) {
        if (disabled) {
            actions.push_back([id] { ModStore_Enable(id); });
        } else {
            ConfirmIfNeeded(view, mod, "Disable", actions, [id] { ModStore_Disable(id); });
        }
    }
    if (state != StoreModState::RemovedOnRestart) {
        ImGui::SameLine();
        if (UIWidgets::Button(("Remove##" + id).c_str(), InlineButton())) {
            ConfirmIfNeeded(view, mod, "Remove", actions, [id] { ModStore_Remove(id); });
        }
    }
    ImGui::EndDisabled();
    DrawModOptions(mod);
}

void DrawInstalled(const StoreView& view, Actions& actions) {
    std::vector<std::string> outdated;
    for (const auto& [id, state] : view.states) {
        if (state == StoreModState::Outdated) {
            outdated.push_back(id);
        }
    }
    if (!outdated.empty()) {
        ImGui::BeginDisabled(view.busy);
        if (UIWidgets::Button(("Update all (" + std::to_string(outdated.size()) + ")").c_str(), InlineButton())) {
            actions.push_back([outdated] { ModStore_Install(outdated); });
        }
        ImGui::EndDisabled();
    }

    bool any = false;
    for (const auto& [id, state] : view.states) {
        if (state == StoreModState::NotInstalled) {
            continue;
        }
        any = true;
        DrawInstalledRow(view, view.catalog.mods.at(id), state, actions);
    }
    if (!any) {
        ImGui::TextDisabled("No mod from the store is installed yet.");
    }
}

void DrawLinks(const StoreView& view) {
    if (view.catalog.links.empty()) {
        ImGui::TextDisabled("No links yet.");
    }
    for (const StoreLink& link : view.catalog.links) {
        ImGui::TextUnformatted(link.name.c_str());
        if (!link.description.empty()) {
            ImGui::TextWrapped("%s", link.description.c_str());
        }
        if (UIWidgets::Button(("Open##" + link.url).c_str(), InlineButton().Tooltip(link.url.c_str()))) {
            SDL_OpenURL(link.url.c_str());
        }
        ImGui::Separator();
    }
}

void DrawReleaseBanner(const StoreView& view, Actions& actions) {
    if (!view.newerRelease) {
        return;
    }
    const StoreRelease& release = *view.newerRelease;
    ImGui::TextColored(UIWidgets::ColorValues.at(UIWidgets::Colors::Yellow), "Unbound %s is available (you have %s).",
                       release.version.c_str(), gBuildForkVersion);

    ImGui::BeginDisabled(view.busy);
    if (ModStore_CanInstallRelease(release)) {
        if (UIWidgets::Button("Update Unbound", InlineButton())) {
            actions.push_back(ModStore_InstallRelease);
        }
    } else if (UIWidgets::Button("Open the release page", InlineButton())) {
        SDL_OpenURL(release.pageUrl.c_str());
    }
    ImGui::SameLine();
    if (UIWidgets::Button("Skip this version", InlineButton())) {
        const std::string version = release.version;
        actions.push_back([version] { ModStore_SkipRelease(version); });
    }
    ImGui::EndDisabled();
    ImGui::Separator();
}

void DrawRestartBanner(const StoreView& view) {
    if (!view.restartNeeded && !sOptionsChanged) {
        return;
    }
    ImGui::TextColored(UIWidgets::ColorValues.at(UIWidgets::Colors::Cyan),
                       "Changes apply the next time Unbound starts.");
    ImGui::SameLine();
    if (UIWidgets::Button("Close Unbound now", InlineButton())) {
        Ship::Context::GetRawInstance()->GetWindow()->Close();
    }
    ImGui::Separator();
}

void DrawStartupChoice() {
    const int choice = CVarGetInteger(CVAR_STORE_CHECK_AT_STARTUP, StartupCheckUnasked);
    if (choice == StartupCheckUnasked) {
        ImGui::TextWrapped("Check for new versions of Unbound and of your mods every time the game starts?");
        if (UIWidgets::Button("Yes##StoreStartup", InlineButton())) {
            SetStartupCheck(StartupCheckOn);
        }
        ImGui::SameLine();
        if (UIWidgets::Button("No##StoreStartup", InlineButton())) {
            SetStartupCheck(StartupCheckOff);
        }
        ImGui::Separator();
        return;
    }
    bool enabled = choice == StartupCheckOn;
    if (UIWidgets::Checkbox("Check for updates at startup", &enabled,
                            UIWidgets::CheckboxOptions().Color(THEME_COLOR))) {
        SetStartupCheck(enabled ? StartupCheckOn : StartupCheckOff);
    }
}

void DrawToolbar(const StoreView& view, Actions& actions) {
    ImGui::BeginDisabled(view.busy);
    if (UIWidgets::Button("Refresh##Store", InlineButton())) {
        actions.push_back(ModStore_Refresh);
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (view.busy) {
        ImGui::Text("%s", view.status.empty() ? "Working..." : view.status.c_str());
    } else {
        ImGui::TextDisabled("%s", view.status.c_str());
    }
}

} // namespace

void ModStore_DrawMenu() {
    if (!ModStore_HasHttps()) {
        ImGui::TextWrapped("This build of Unbound has no HTTPS client, so the store is not available.");
        return;
    }
    DrawStartupChoice();

    Actions actions;
    {
        auto lock = ModStore_LockView();
        const StoreView& view = ModStore_View();
        if (!view.hasCatalog && !view.busy && !sRequestedCatalog) {
            sRequestedCatalog = true;
            actions.push_back(ModStore_Refresh);
        }
        DrawToolbar(view, actions);
        DrawReleaseBanner(view, actions);
        DrawRestartBanner(view);
        if (view.hasCatalog && ImGui::BeginTabBar("##NeiStoreTabs")) {
            if (ImGui::BeginTabItem("Packs")) {
                DrawPacks(view, actions);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Installed")) {
                DrawInstalled(view, actions);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Links")) {
                DrawLinks(view);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    // Actions take the view lock themselves, so they run once it is released.
    for (const auto& action : actions) {
        action();
    }
}
