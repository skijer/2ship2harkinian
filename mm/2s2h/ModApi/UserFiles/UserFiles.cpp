#include "UserFiles.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/ArchiveManager.h>
#include <spdlog/spdlog.h>

#include "2s2h/BenPort.h"
#include "2s2h/ModApi/ModApi.h"
#include "2s2h/ModApi/ModPermissions/ModPermissions.h"
#include "2s2h/ModApi/O2rExtractor/O2rExtractor.h"
#include "2s2h/ModApi/O2rExtractor/ModArchive.h"

#if !defined(__SWITCH__) && !defined(__WIIU__)
#define USER_FILES_HAS_DIALOGS 1
#include "2s2h/Extractor/portable-file-dialogs.h"
#endif

namespace {

constexpr uint64_t kMaxPickedFileSize = 1ULL << 30;
constexpr size_t kMaxFileNameLength = 128;
constexpr const char* kPendingSuffix = ".pending";
constexpr const char* kExtractWorkspace = ".mod-extract";
constexpr const char* kExtractRomName = "rom.z64";

// Letters, digits, '.', '_' and '-', not starting with '.': no folders, and none of the game's own dotfiles.
bool IsPlainFileName(const char* name) {
    if (name == nullptr || name[0] == '\0' || name[0] == '.' || std::strlen(name) > kMaxFileNameLength) {
        return false;
    }
    const std::string text(name);
    const bool plain = std::all_of(text.begin(), text.end(), [](unsigned char character) {
        return std::isalnum(character) || character == '.' || character == '_' || character == '-';
    });
    return plain && std::filesystem::path(text).extension() != kPendingSuffix;
}

bool IsRelativeInside(const char* path) {
    if (path == nullptr || path[0] == '\0') {
        return false;
    }
    const std::filesystem::path relative(path);
    if (relative.is_absolute() || relative.has_root_name()) {
        return false;
    }
    return std::none_of(relative.begin(), relative.end(),
                        [](const std::filesystem::path& part) { return part == ".."; });
}

std::string DescribeSize(uint64_t bytes) {
    char text[32];
    std::snprintf(text, sizeof(text), "%.1f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
    return text;
}

bool CanUseFiles(const ModIdentity* mod) {
    if (mod == nullptr) {
        SPDLOG_ERROR("[UserFiles] Only a mod can ask for files");
        return false;
    }
    return ModPermissions_HasDeclared(*mod, ModPermission::Files);
}

// A mounted archive cannot be replaced while the game runs, so a busy name waits beside it until the next start.
bool PlaceInModsFolder(const std::filesystem::path& source, const std::string& fileName) {
    const std::filesystem::path mods = ModLoader_GetModsDirectory();
    std::error_code error;
    std::filesystem::create_directories(mods, error);
    const bool isTaken = std::filesystem::exists(mods / fileName);
    const std::filesystem::path target = isTaken ? mods / (fileName + kPendingSuffix) : mods / fileName;
    std::filesystem::remove(target, error);
    std::filesystem::rename(source, target, error);
    if (error) {
        std::filesystem::copy_file(source, target, std::filesystem::copy_options::overwrite_existing, error);
        std::filesystem::remove(source);
    }
    if (error) {
        SPDLOG_ERROR("[UserFiles] Could not place '{}' in the mods folder: {}", fileName, error.message());
        return false;
    }
    SPDLOG_INFO("[UserFiles] Wrote '{}'{}", target.string(), isTaken ? ", installed at the next start" : "");
    return true;
}

bool WriteBytes(const std::filesystem::path& path, const void* data, uint64_t size) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
    return output.good();
}

bool ReadPickedFile(const std::filesystem::path& path, S2HUserFile* file) {
    std::error_code error;
    const uint64_t size = std::filesystem::file_size(path, error);
    if (error || size > kMaxPickedFileSize) {
        SPDLOG_ERROR("[UserFiles] '{}' cannot be read or is larger than 1 GB", path.string());
        return false;
    }
    auto* data = static_cast<uint8_t*>(std::malloc(size > 0 ? size : 1));
    std::ifstream input(path, std::ios::binary);
    if (data == nullptr || !input.read(reinterpret_cast<char*>(data), static_cast<std::streamsize>(size))) {
        std::free(data);
        return false;
    }
    file->data = data;
    file->size = size;
    std::snprintf(file->name, sizeof(file->name), "%s", path.filename().string().c_str());
    return true;
}

// The player picking the archive in the dialog is the consent; it stays mounted until the game closes.
bool MountFor(const ModIdentity* mod, const char* title) {
    if (!CanUseFiles(mod)) {
        return false;
    }
#ifdef USER_FILES_HAS_DIALOGS
    const std::string dialogTitle = mod->name + ": " + (title != nullptr ? title : "Choose an archive to load");
    const std::vector<std::string> chosen =
        pfd::open_file(dialogTitle, ".", { "Ship archives", "*.o2r *.otr *.zip" }).result();
    if (chosen.empty()) {
        return false;
    }
    auto archive = Ship::Context::GetRawInstance()->GetResourceManager()->GetArchiveManager()->AddArchive(chosen[0]);
    if (archive == nullptr) {
        SPDLOG_ERROR("[UserFiles] '{}' could not mount '{}'", mod->name, chosen[0]);
        return false;
    }
    SPDLOG_INFO("[UserFiles] The player mounted '{}' for '{}'", chosen[0], mod->name);
    return true;
#else
    return false;
#endif
}

bool PickFor(const ModIdentity* mod, const char* title, const char* filterName, const char* filterPatterns,
             S2HUserFile* file) {
    if (file == nullptr || file->structSize < S2H_USER_FILE_MIN_SIZE || !CanUseFiles(mod)) {
        return false;
    }
#ifdef USER_FILES_HAS_DIALOGS
    const std::string dialogTitle = mod->name + ": " + (title != nullptr ? title : "Choose a file to give it");
    const std::vector<std::string> filters = { filterName != nullptr ? filterName : "Files",
                                               filterPatterns != nullptr ? filterPatterns : "*", "All files", "*" };
    const std::vector<std::string> chosen = pfd::open_file(dialogTitle, ".", filters).result();
    if (chosen.empty() || !ReadPickedFile(chosen[0], file)) {
        return false;
    }
    SPDLOG_INFO("[UserFiles] The player gave '{}' the file '{}'", mod->name, file->name);
    return true;
#else
    return false;
#endif
}

bool SaveFor(const ModIdentity* mod, const char* fileName, const void* data, uint64_t size) {
    if (!CanUseFiles(mod) || data == nullptr || !IsPlainFileName(fileName)) {
        return false;
    }
    if (!ModPermissions_ConfirmNow(*mod, "Save a file",
                                   std::string("wants to save '") + fileName + "' (" + DescribeSize(size) +
                                       ") in your mods folder.")) {
        return false;
    }
    std::error_code error;
    std::filesystem::create_directories(ModLoader_GetModsDirectory(), error);
    const std::filesystem::path temporary = ModLoader_GetModsDirectory() / ("." + std::string(fileName) + ".writing");
    return WriteBytes(temporary, data, size) && PlaceInModsFolder(temporary, fileName);
}

bool IsValidExtractRequest(const S2HRomExtractRequest* request) {
    return request != nullptr && request->structSize >= S2H_ROM_EXTRACT_REQUEST_MIN_SIZE && request->rom != nullptr &&
           request->romSize > 0 && request->zapdAssetMask != nullptr && IsRelativeInside(request->zapdAssetRoot) &&
           IsRelativeInside(request->xmlDir) && IsRelativeInside(request->configPath) &&
           IsRelativeInside(request->filelistDir) && IsPlainFileName(request->outputFileName);
}

bool ExtractFor(const ModIdentity* mod, const S2HRomExtractRequest* request) {
    if (!CanUseFiles(mod)) {
        return false;
    }
    if (!IsValidExtractRequest(request)) {
        SPDLOG_ERROR("[UserFiles] '{}' sent an invalid extraction request", mod->name);
        return false;
    }
    if (!ModPermissions_ConfirmNow(*mod, "Extract assets",
                                   std::string("wants to create '") + request->outputFileName +
                                       "' in your mods folder out of the ROM you chose. It takes a few minutes, "
                                       "and the game looks frozen while it runs.")) {
        return false;
    }

    const std::filesystem::path workspace =
        std::filesystem::path(Ship::Context::GetAppDirectoryPath(appShortName)) / kExtractWorkspace;
    std::error_code error;
    std::filesystem::remove_all(workspace, error);
    std::filesystem::create_directories(workspace, error);
    const std::filesystem::path romPath = workspace / kExtractRomName;
    const std::filesystem::path outputPath = workspace / request->outputFileName;
    const bool prepared = O2rExtractor_ExportMatching(request->zapdAssetMask, workspace) > 0 &&
                          WriteBytes(romPath, request->rom, request->romSize);
    std::string archiveError;
    const bool extracted = prepared &&
                           O2rExtractor_RunZapd({ romPath, workspace / request->zapdAssetRoot, request->xmlDir,
                                                  request->configPath, request->filelistDir, outputPath }) &&
                           ModArchive_Prepare(outputPath, archiveError) &&
                           PlaceInModsFolder(outputPath, request->outputFileName);
    if (!archiveError.empty()) SPDLOG_ERROR("[UserFiles] {}", archiveError);
    std::filesystem::remove_all(workspace, error);
    return extracted;
}

} // namespace

extern "C" bool UserFiles_Pick(const char* title, const char* filterName, const char* filterPatterns,
                               S2HUserFile* file) {
    return PickFor(ModPermissions_FindCaller(MOD_CALLER_ADDRESS()), title, filterName, filterPatterns, file);
}

extern "C" bool UserFiles_HasModArchiveFile(const char* fileName) {
    if (fileName == nullptr || !IsPlainFileName(fileName) || std::filesystem::path(fileName).extension() != ".o2r") return false;
    std::error_code error;
    return std::filesystem::is_regular_file(ModLoader_GetModsDirectory() / fileName, error);
}

extern "C" void UserFiles_Free(S2HUserFile* file) {
    if (file == nullptr) {
        return;
    }
    std::free(file->data);
    file->data = nullptr;
    file->size = 0;
}

extern "C" bool UserFiles_SaveToMods(const char* fileName, const void* data, uint64_t size) {
    return SaveFor(ModPermissions_FindCaller(MOD_CALLER_ADDRESS()), fileName, data, size);
}

extern "C" bool UserFiles_ExtractRomToMods(const S2HRomExtractRequest* request) {
    return ExtractFor(ModPermissions_FindCaller(MOD_CALLER_ADDRESS()), request);
}

void UserFiles_ApplyPending(const std::filesystem::path& modsDirectory) {
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(modsDirectory, error)) {
        const std::filesystem::path pending = entry.path();
        if (!entry.is_regular_file() || pending.extension() != kPendingSuffix) {
            continue;
        }
        const std::filesystem::path target = pending.parent_path() / pending.stem();
        std::filesystem::remove(target, error);
        std::filesystem::rename(pending, target, error);
        if (error) {
            SPDLOG_ERROR("[UserFiles] Could not install '{}': {}", target.string(), error.message());
            continue;
        }
        SPDLOG_INFO("[UserFiles] Installed '{}'", target.string());
    }
}

extern "C" bool UserFiles_MountArchive(const char* title) {
    return MountFor(ModPermissions_FindCaller(MOD_CALLER_ADDRESS()), title);
}
