#include "2s2h/ModApi/ModApi.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

static const S2HModApi* sApi;
static const S2HModRequirements sRequirements = { sizeof(S2HModRequirements), nullptr, 0 };
static bool HasExtractedAssets() {
    if (!S2H_MOD_API_HAS(sApi, HasModArchiveFile) || !sApi->HasModArchiveFile("oot.o2r")) return false;
    const char* markers[] = {
        "textures/oot_icon_item_static/gItemIconSlingshotTex",
        "textures/oot_icon_item_nes_static/gPauseQuestStatus00ENGTex",
        "textures/oot_icon_item_nes_static/gPauseQuestStatus10ENGTex",
        "textures/oot_icon_item_nes_static/gPauseQuestStatus20ENGTex",
        "textures/oot_icon_item_24_static/gQuestIconMedallionForestTex",
        "textures/oot_icon_item_24_static/gQuestIconKokiriEmeraldTex",
        "objects/oot_object_gi_boomerang/gGiBoomerangDL",
        "objects/oot_object_gi_hammer/gGiHammerDL",
        "objects/oot_object_niw/gCuccoSkel",
        "objects/oot_object_gi_niwatori/gGiChickenDL",
        "objects/oot_object_ma1/gMalonChildSkel",
        "objects/oot_object_ma2/gMalonAdultSkel",
        "objects/oot_object_gi_bracelet/gGiGoronBraceletDL",
        "objects/oot_object_ganon/gGanondorfSkel",
        "objects/oot_object_toki_objects/object_toki_objects_DL_001BD0",
        "overlays/oot_ovl_Magic_Fire/sModelDL",
        "overlays/oot_ovl_Magic_Dark/sDiamondModelDL",
        "overlays/oot_ovl_Magic_Wind/sSkel",
    };
    for (const char* marker : markers) if (!sApi->HasResource(marker)) return false;
    return true;
}

static uint32_t ReadBE(const uint8_t* data) {
    return uint32_t(data[0]) << 24 | uint32_t(data[1]) << 16 | uint32_t(data[2]) << 8 | data[3];
}

static void ExtractAssets() {
    if (!sApi->AskPlayer("OoT Asset Loader", "Extract Ocarina of Time item assets from your own ROM?")) return;
    S2HUserFile file = {};
    file.structSize = sizeof(file);
    if (!sApi->PickUserFile("Select your Ocarina of Time ROM", "Nintendo 64 ROM", "*.z64 *.n64 *.v64", &file)) return;
    if (file.data == nullptr || file.size < 0x40 || file.size > 64 * 1024 * 1024) {
        sApi->FreeUserFile(&file);
        sApi->TellPlayer("OoT Asset Loader", "That file is not a supported OoT ROM.");
        return;
    }
    std::vector<uint8_t> rom(file.data, file.data + file.size);
    sApi->FreeUserFile(&file);
    const char* version = nullptr;
    if (rom.size() >= 0x40 && rom.size() % 4 == 0) {
        uint32_t magic = ReadBE(rom.data());
        if (magic == 0x37804012) {
            for (size_t i = 0; i < rom.size(); i += 2) std::swap(rom[i], rom[i + 1]);
        } else if (magic == 0x40123780) {
            for (size_t i = 0; i < rom.size(); i += 4) {
                std::swap(rom[i], rom[i + 3]);
                std::swap(rom[i + 1], rom[i + 2]);
            }
        }
        if (ReadBE(rom.data()) == 0x80371240) {
            switch (ReadBE(rom.data() + 0x10)) {
                case 0xEC7011B7: version = "N64_NTSC_10"; break;
                case 0xD43DA81F: version = "N64_NTSC_11"; break;
                case 0x693BA2AE: version = "N64_NTSC_12"; break;
            }
        }
    }
    if (version == nullptr) {
        sApi->TellPlayer("OoT Asset Loader", "Use an original OoT NTSC 1.0, 1.1 or 1.2 ROM.");
        return;
    }
    std::string xml = std::string("assets/xml/") + version;
    std::string config = std::string("assets/Config_") + version + ".xml";
    S2HRomExtractRequest request = {};
    request.structSize = sizeof(request);
    request.rom = rom.data();
    request.romSize = rom.size();
    request.zapdAssetMask = "oot_zapd/*";
    request.zapdAssetRoot = "oot_zapd";
    request.xmlDir = xml.c_str();
    request.configPath = config.c_str();
    request.filelistDir = "assets/filelists";
    request.outputFileName = "oot.o2r";
    if (sApi->ExtractRomToModsFolder(&request)) {
        sApi->RequestRestart("OoT item assets are ready. Restart to load them.");
    } else sApi->TellPlayer("OoT Asset Loader", "Asset extraction failed. See the game log.");
}

static void DrawLoaderMenu() {
    bool extract = false;
    ModUI_Text("OoT Asset Loader: use your own NTSC 1.0, 1.1 or 1.2 ROM.");
    if (ModUI_Checkbox("Extract OoT assets now", &extract) && extract) ExtractAssets();
}

extern "C" S2H_MOD_EXPORT void ModSetApi(const S2HModApi* api) { sApi = api; }
extern "C" S2H_MOD_EXPORT const S2HModRequirements* ModGetRequirements() { return &sRequirements; }
extern "C" S2H_MOD_EXPORT void ModInit() {
    if (!S2H_MOD_API_HAS(sApi, RequestRestart)) return;
    S2HModMenuWidget button = {};
    button.structSize = sizeof(button);
    button.section = "Enhancements";
    button.sidebar = "OoT Asset Loader";
    button.type = S2H_MOD_MENU_CUSTOM;
    button.label = "Extract OoT assets";
    button.onChange = DrawLoaderMenu;
    sApi->RegisterMenuSidebar("Enhancements", "OoT Asset Loader", "gMods.OotAssets.Menu", 1);
    sApi->RegisterMenuWidget(&button);
    if (!HasExtractedAssets()) ExtractAssets();
}
