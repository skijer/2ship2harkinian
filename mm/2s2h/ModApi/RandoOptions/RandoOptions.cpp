#include "RandoOptions.h"

#include <algorithm>
#include <deque>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include <spdlog/spdlog.h>
#include <libultraship/bridge/consolevariablebridge.h>

#include "2s2h/BenGui/BenMenu.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/Rando/Rando.h"
#include "2s2h/SaveManager/SaveSections.h"

namespace BenGui {
extern std::shared_ptr<BenMenu> mBenMenu;
}

namespace {

constexpr const char* SaveSection = "randoModOptions";
constexpr const char* MenuSection = "Rando";
constexpr const char* MenuSidebar = "Mods";
constexpr const char* MenuSelectionCvar = "gSettings.Menu.RandoSidebarSection";
constexpr const char* CvarPrefix = "gRando.ModOptions.";
constexpr uint32_t MaxValueCount = 256;

struct RegisteredOption {
    std::string key;
    std::string label;
    std::string tooltip;
    std::string cvar;
    std::vector<std::string> values;
    std::unordered_map<int32_t, const char*> comboValues;
    uint8_t defaultValue = 0;
    bool isSlider = false;
};

struct SeedSnapshot {
    uint32_t seed = 0;
    std::map<std::string, uint8_t> values;
};

// Deque: the menu keeps the tooltip and combo pointers of every widget it was handed.
std::deque<RegisteredOption> sOptions;
SeedSnapshot sSnapshot;
bool sIsGenerating = false;

uint32_t GetLoadedSeed() {
    return gSaveContext.save.shipSaveInfo.rando.finalSeed;
}

RegisteredOption* Find(const char* key) {
    if (key == nullptr) {
        return nullptr;
    }
    for (RegisteredOption& option : sOptions) {
        if (option.key == key) {
            return &option;
        }
    }
    return nullptr;
}

bool IsLabelTaken(const std::string& label) {
    return std::any_of(sOptions.begin(), sOptions.end(),
                       [&label](const RegisteredOption& option) { return option.label == label; });
}

uint8_t ReadMenuChoice(const RegisteredOption& option) {
    const int32_t choice = CVarGetInteger(option.cvar.c_str(), option.defaultValue);
    return static_cast<uint8_t>(std::clamp<int32_t>(choice, 0, static_cast<int32_t>(option.values.size()) - 1));
}

uint8_t ReadCapturedChoice(const RegisteredOption& option) {
    const auto captured = sSnapshot.values.find(option.key);
    return captured != sSnapshot.values.end() ? captured->second : option.defaultValue;
}

bool IsMenuBuilt() {
    return BenGui::mBenMenu != nullptr && BenGui::mBenMenu->AreMenuElementsInitialized();
}

void AddWidget(const RegisteredOption& option) {
    WidgetPath path = { MenuSection, MenuSidebar, SECTION_COLUMN_1 };
    BenGui::mBenMenu->EnsureModSidebar(MenuSection, MenuSidebar, MenuSelectionCvar, 1);

    const WidgetType type = option.isSlider ? WIDGET_CVAR_SLIDER_INT : WIDGET_CVAR_COMBOBOX;
    WidgetInfo& widget = BenGui::mBenMenu->AddWidget(path, option.label, type);
    widget.CVar(option.cvar.c_str());
    if (option.isSlider) {
        widget.Options(UIWidgets::IntSliderOptions()
                           .Min(0)
                           .Max(static_cast<int32_t>(option.values.size()) - 1)
                           .DefaultValue(option.defaultValue)
                           .Tooltip(option.tooltip.c_str()));
        return;
    }
    widget.Options(UIWidgets::ComboboxOptions()
                       .ComboMap(&option.comboValues)
                       .DefaultIndex(option.defaultValue)
                       .Tooltip(option.tooltip.c_str()));
}

void AddAllWidgets() {
    for (const RegisteredOption& option : sOptions) {
        AddWidget(option);
    }
}

void CaptureSnapshot() {
    sSnapshot.seed = GetLoadedSeed();
    for (const RegisteredOption& option : sOptions) {
        sSnapshot.values[option.key] = ReadMenuChoice(option);
    }
}

nlohmann::json WriteSection() {
    nlohmann::json section;
    section["seed"] = sSnapshot.seed;
    section["values"] = sSnapshot.values;
    return section;
}

// Creating a file reads a null section after the seed was generated: the snapshot of that same seed must survive it.
void ReadSection(const nlohmann::json& section) {
    if (!section.is_object()) {
        const bool belongsToLoadedSave = sSnapshot.seed != 0 && sSnapshot.seed == GetLoadedSeed();
        if (!belongsToLoadedSave) {
            sSnapshot = {};
        }
        return;
    }
    sSnapshot = {};
    sSnapshot.seed = section.value("seed", 0u);
    for (const auto& [key, value] : section.value("values", nlohmann::json::object()).items()) {
        if (value.is_number_unsigned()) {
            sSnapshot.values[key] = value.get<uint8_t>();
        }
    }
}

bool IsValidDefinition(const S2HModRandoOption* option) {
    return option != nullptr && option->structSize >= S2H_MOD_RANDO_OPTION_MIN_SIZE && option->key != nullptr &&
           option->key[0] != '\0' && option->label != nullptr && option->label[0] != '\0' &&
           option->values != nullptr && option->valueCount >= 2 && option->valueCount <= MaxValueCount &&
           option->defaultValue < option->valueCount;
}

} // namespace

bool RandoOptions_Register(const S2HModRandoOption* option) {
    if (!IsValidDefinition(option)) {
        return false;
    }
    if (Find(option->key) != nullptr || IsLabelTaken(option->label)) {
        SPDLOG_ERROR("[RandoOptions] '{}' is already registered", option->key);
        return false;
    }

    RegisteredOption& registered = sOptions.emplace_back();
    registered.key = option->key;
    registered.label = option->label;
    registered.tooltip = option->tooltip == nullptr ? "" : option->tooltip;
    registered.cvar = std::string(CvarPrefix) + option->key;
    registered.defaultValue = option->defaultValue;
    const bool hasWidgetField = option->structSize >= offsetof(S2HModRandoOption, widget) + sizeof(option->widget);
    registered.isSlider = hasWidgetField && option->widget == S2H_MOD_RANDO_WIDGET_SLIDER;
    for (uint32_t index = 0; index < option->valueCount; index++) {
        registered.values.emplace_back(option->values[index] == nullptr ? "" : option->values[index]);
    }
    for (uint32_t index = 0; index < option->valueCount; index++) {
        registered.comboValues[static_cast<int32_t>(index)] = registered.values[index].c_str();
    }

    if (IsMenuBuilt()) {
        AddWidget(registered);
    }
    return true;
}

uint8_t RandoOptions_Get(const char* key) {
    const RegisteredOption* option = Find(key);
    if (option == nullptr) {
        return 0;
    }
    return IS_RANDO ? ReadCapturedChoice(*option) : ReadMenuChoice(*option);
}

uint8_t RandoOptions_GetForSeed(const char* key) {
    const RegisteredOption* option = Find(key);
    if (option == nullptr) {
        return 0;
    }
    return IS_RANDO && !sIsGenerating ? ReadCapturedChoice(*option) : ReadMenuChoice(*option);
}

void RandoOptions_Init() {
    SaveSections_Register(SaveSection, WriteSection, ReadSection);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnRandoSeedGeneration>(CaptureSnapshot);

    // Registered now rather than statically so it runs after the Rando menu created its own section.
    if (IsMenuBuilt()) {
        AddAllWidgets();
        return;
    }
    RegisterMenuInitFunc registration(AddAllWidgets);
}

void RandoOptions_BeginSeedGeneration() {
    sIsGenerating = true;
}

void RandoOptions_EndSeedGeneration() {
    sIsGenerating = false;
}
