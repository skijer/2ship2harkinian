#include "SaveSections.h"

#include <map>
#include <unordered_map>

#include "SaveManager.h"
#include "2s2h/GameInteractor/GameInteractor.h"

extern "C" {
#include "z64save.h"
#include "macros.h"
#include "src/overlays/gamestates/ovl_file_choose/z_file_select.h"
extern FileSelectState* gFileSelectState;
}

namespace {

struct Section {
    SaveSectionWrite write;
    SaveSectionRead read;
};

std::map<std::string, Section> sSections;
std::unordered_map<std::string, nlohmann::json> sLastReadByFile;

const char* VariantKey(bool isOwlSave) {
    return isOwlSave ? "owl" : "newCycle";
}

void ReadAll(const nlohmann::json& sections) {
    for (const auto& [name, section] : sSections) {
        const bool present = sections.is_object() && sections.contains(name);
        section.read(present ? sections[name] : nlohmann::json());
    }
}

void RegisterHooksOnce() {
    static bool registered = false;
    if (registered) {
        return;
    }
    registered = true;

    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSaveInit>(
        [](s16 fileNum) { ReadAll(nlohmann::json()); });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSaveLoad>([](s16 fileNum) {
        auto lastRead = sLastReadByFile.find(SaveManager_GetFileName(fileNum));
        ReadAll(lastRead == sLastReadByFile.end() ? nlohmann::json() : lastRead->second);
    });
}

} // namespace

void SaveSections_Register(const std::string& name, SaveSectionWrite write, SaveSectionRead read) {
    RegisterHooksOnce();
    sSections[name] = { std::move(write), std::move(read) };
}

void SaveSections_Write(nlohmann::json& saveFile, bool isOwlSave) {
    // File select copies and erases files through the same writer; only gameplay owns the live sections.
    if (gFileSelectState != NULL || sSections.empty()) {
        return;
    }
    nlohmann::json& variant = saveFile["sections"][VariantKey(isOwlSave)];
    for (const auto& [name, section] : sSections) {
        variant[name] = section.write();
    }
}

void SaveSections_RememberRead(const std::string& fileName, const nlohmann::json& saveFile, bool isOwlSave) {
    const char* key = VariantKey(isOwlSave);
    const bool present = saveFile.contains("sections") && saveFile["sections"].contains(key);
    sLastReadByFile[fileName] = present ? saveFile["sections"][key] : nlohmann::json();
}
