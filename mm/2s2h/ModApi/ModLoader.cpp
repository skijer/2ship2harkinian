#include "ModApi.h"
#include "PackageProtocol.h"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <SDL2/SDL_messagebox.h>

#include <ship/Context.h>
#include <ship/resource/File.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/Archive.h>
#include <ship/resource/archive/ArchiveManager.h>

#include "2s2h/BenPort.h"
#include "ModPermissions/ModPermissions.h"
#include "ModTrust/ModTrust.h"

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__) || defined(__linux__)
#include <dlfcn.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {

using Json = nlohmann::json;

constexpr uint32_t MaxRequirements = 4096;

class CurrentModScope {
  public:
    CurrentModScope(const std::string& name) {
        ModApi_SetCurrentMod(name);
    }

    ~CurrentModScope() {
        ModApi_ClearCurrentMod();
    }
};

const char* GetPlatformKey() {
#if defined(_WIN64)
    return "windows_x64";
#elif defined(_WIN32)
    return "windows_x86";
#elif defined(__APPLE__)
    return "darwin";
#elif defined(__linux__)
    return "linux_x64";
#else
    return nullptr;
#endif
}

uint64_t HashText(uint64_t hash, const std::string& text) {
    for (unsigned char value : text) {
        hash ^= value;
        hash *= 1099511628211ULL;
    }
    return hash;
}

uint64_t HashBytes(const std::vector<char>& bytes) {
    return HashText(1469598103934665603ULL, std::string(bytes.begin(), bytes.end()));
}

std::string Hex(uint64_t value) {
    constexpr char digits[] = "0123456789abcdef";
    std::string result(16, '0');
    for (size_t i = 0; i < result.size(); ++i) {
        result[result.size() - i - 1] = digits[value & 0xF];
        value >>= 4;
    }
    return result;
}

// The binary lives inside the .o2r; it is written out once so the OS loader can map it.
class ModLibrary {
  public:
    ModLibrary() = default;
    ModLibrary(const ModLibrary&) = delete;
    ModLibrary& operator=(const ModLibrary&) = delete;

    ~ModLibrary() {
        Unload();
    }

    bool Load(const std::vector<char>& libraryImage) {
        const std::string path = WriteToTemporaryFile(libraryImage);
        if (path.empty()) {
            return false;
        }
#if defined(_WIN32)
        mHandle = LoadLibraryA(path.c_str());
        mFileToDelete = path;
#elif defined(__APPLE__) || defined(__linux__)
        mHandle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
        unlink(path.c_str());
#else
        std::filesystem::remove(path);
#endif
        return mHandle != nullptr;
    }

    const void* GetBaseAddress() const {
#if defined(_WIN32)
        return mHandle;
#elif defined(__APPLE__) || defined(__linux__)
        Dl_info info;
        void* symbol = GetFunction("ModSetApi");
        return symbol != nullptr && dladdr(symbol, &info) != 0 ? info.dli_fbase : nullptr;
#else
        return nullptr;
#endif
    }

    void* GetFunction(const char* name) const {
        if (mHandle == nullptr) {
            return nullptr;
        }
#if defined(_WIN32)
        return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(mHandle), name));
#elif defined(__APPLE__) || defined(__linux__)
        return dlsym(mHandle, name);
#else
        return nullptr;
#endif
    }

  private:
    static std::string WriteToTemporaryFile(const std::vector<char>& libraryImage) {
        std::string path;
#if defined(_WIN32)
        char temporaryDirectory[MAX_PATH];
        char temporaryFile[MAX_PATH];
        if (GetTempPathA(MAX_PATH, temporaryDirectory) == 0 ||
            GetTempFileNameA(temporaryDirectory, "s2h", 0, temporaryFile) == 0) {
            return {};
        }
        path = temporaryFile;
#elif defined(__APPLE__) || defined(__linux__)
        char pathTemplate[] = "/tmp/2ship_mod_XXXXXX";
        int descriptor = mkstemp(pathTemplate);
        if (descriptor == -1) {
            return {};
        }
        fchmod(descriptor, 0755);
        close(descriptor);
        path = pathTemplate;
#else
        return {};
#endif
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output.write(libraryImage.data(), static_cast<std::streamsize>(libraryImage.size()));
        output.close();
        if (!output) {
            std::filesystem::remove(path);
            return {};
        }
        return path;
    }

    void Unload() {
        if (mHandle == nullptr) {
            return;
        }
#if defined(_WIN32)
        FreeLibrary(static_cast<HMODULE>(mHandle));
        if (!mFileToDelete.empty()) {
            DeleteFileA(mFileToDelete.c_str());
        }
#elif defined(__APPLE__) || defined(__linux__)
        dlclose(mHandle);
#endif
        mHandle = nullptr;
    }

#if defined(_WIN32)
    HMODULE mHandle = nullptr;
#else
    void* mHandle = nullptr;
#endif
    std::string mFileToDelete;
};

struct LoadedMod {
    std::string name;
    std::string archivePath;
    std::string binaryHash;
    ModLibrary library;
    S2HModGetRequirementsFunc getRequirements = nullptr;
    S2HModInitFunc init = nullptr;
    std::vector<std::string> requiredHooks;
    std::vector<std::string> requiredBehaviors;
    std::vector<std::string> requiredResources;
    bool compatible = false;
    bool compatibilityCached = false;
};

struct NativeModPackage {
    std::string name;
    std::string archivePath;
    std::shared_ptr<std::vector<char>> binary;
    std::vector<std::string> requiredResources;
    std::vector<std::string> permissions;
    ModProvenance provenance;
};

std::vector<std::unique_ptr<LoadedMod>> sLoadedMods;
std::unordered_map<std::string, std::string> sTrustLabels;
std::vector<std::string> sRefusedMods;

std::filesystem::path GetCachePath() {
    return ModLoader_GetModsDirectory() / ".2ship-modapi-cache.json";
}

Json LoadJsonObject(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        return Json::object();
    }
    try {
        Json object = Json::parse(input);
        return object.is_object() ? object : Json::object();
    } catch (const std::exception& exception) {
        SPDLOG_WARN("[ModApi] Ignoring '{}': {}", path.string(), exception.what());
        return Json::object();
    }
}

void SaveJsonObject(const std::filesystem::path& path, const Json& object) {
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    std::ofstream output(path, std::ios::trunc);
    if (!output) {
        SPDLOG_WARN("[ModApi] Could not write '{}'", path.string());
        return;
    }
    output << object.dump(2) << '\n';
}

#if defined(S2H_MOD_POLICY_ASK)
enum class UnverifiedModChoice : int {
    Refuse,
    LoadOnce,
    LoadAlways,
};

std::filesystem::path GetApprovalsPath() {
    return ModLoader_GetModsDirectory() / ".2ship-mod-approvals.json";
}

std::string DescribeUnverifiedMod(const NativeModPackage& package) {
    std::string message = "'" + package.name + "' wants to run native code on this computer, and 2Ship's " +
                          "sign-mod workflow did not sign it (" + package.provenance.problem + ").\n\n";
    const std::vector<std::string> imports = ModTrust_ListUnusualImports(*package.binary);
    if (!imports.empty()) {
        message += "It also calls into the system beyond what mods need:\n";
        for (const std::string& import : imports) {
            message += "  " + import + "\n";
        }
        message += "\n";
    }
    return message + "Content hash " + package.provenance.contentHash.substr(0, 16) +
           "\n\nOnly load it if you trust whoever gave it to you.";
}

UnverifiedModChoice AskAboutUnverifiedMod(const NativeModPackage& package) {
    const std::string message = DescribeUnverifiedMod(package);
    const SDL_MessageBoxButtonData buttons[] = {
        { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT | SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT,
          static_cast<int>(UnverifiedModChoice::Refuse), "Don't load" },
        { 0, static_cast<int>(UnverifiedModChoice::LoadOnce), "Load once" },
        { 0, static_cast<int>(UnverifiedModChoice::LoadAlways), "Always load this version" },
    };
    const SDL_MessageBoxData box = {
        SDL_MESSAGEBOX_WARNING, nullptr, "Unsigned mod", message.c_str(), SDL_arraysize(buttons), buttons, nullptr,
    };
    int choice = static_cast<int>(UnverifiedModChoice::Refuse);
    if (SDL_ShowMessageBox(&box, &choice) != 0 || choice < 0) {
        return UnverifiedModChoice::Refuse;
    }
    return static_cast<UnverifiedModChoice>(choice);
}

bool CanRunUnverifiedMod(const NativeModPackage& package) {
    static Json approvals = LoadJsonObject(GetApprovalsPath());
    if (approvals.contains(package.provenance.contentHash)) {
        return true;
    }
    const UnverifiedModChoice choice = AskAboutUnverifiedMod(package);
    if (choice == UnverifiedModChoice::LoadAlways) {
        approvals[package.provenance.contentHash] = package.name;
        SaveJsonObject(GetApprovalsPath(), approvals);
    }
    return choice != UnverifiedModChoice::Refuse;
}

bool CanRunMod(const NativeModPackage& package) {
    return package.provenance.state == ModTrustState::Verified || CanRunUnverifiedMod(package);
}
#elif defined(S2H_MOD_POLICY_SIGNED)
bool CanRunMod(const NativeModPackage& package) {
    if (package.provenance.state == ModTrustState::Verified) {
        return true;
    }
    sRefusedMods.push_back(package.name + ": " + ModTrust_Describe(package.provenance));
    return false;
}
#else
bool CanRunMod(const NativeModPackage&) {
    return true;
}
#endif

void ReportRefusedMods() {
    if (sRefusedMods.empty()) {
        return;
    }
    std::string message = "These native mods could not be loaded:\n\n";
    for (const std::string& refused : sRefusedMods) {
        message += "- " + refused + "\n";
    }
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Mods not loaded", message.c_str(), nullptr);
}

bool ReadManifest(const std::shared_ptr<Ship::Archive>& archive, Json& manifest) {
    auto file = archive->LoadFile("manifest.json");
    if (file == nullptr || !file->IsLoaded || file->Buffer == nullptr) {
        return false;
    }
    manifest = Json::parse(file->Buffer->begin(), file->Buffer->end());
    return manifest.is_object();
}

std::vector<std::string> ReadStringArray(const Json& manifest, const char* key) {
    std::vector<std::string> values;
    const auto array = manifest.find(key);
    if (array == manifest.end() || !array->is_array()) {
        return values;
    }
    for (const Json& value : *array) {
        if (value.is_string()) {
            values.push_back(value.get<std::string>());
        }
    }
    return values;
}

std::optional<NativeModPackage> ReadNativeModPackage(const std::shared_ptr<Ship::Archive>& archive,
                                                     const std::string& platform) {
    Json manifest;
    if (!ReadManifest(archive, manifest) || !manifest.contains("binaries") || !manifest["binaries"].is_object()) {
        return std::nullopt;
    }
    std::string protocolError;
    if (!ModPackage_IsCompatible(manifest, "mm", S2H_MOD_API_MAJOR, protocolError)) {
        const std::string message = archive->GetPath() + ": " + protocolError;
        sRefusedMods.push_back(message);
        sTrustLabels[archive->GetPath()] = protocolError + ", not loaded";
        SPDLOG_ERROR("[ModApi] {}", message);
        return std::nullopt;
    }
    auto binaryEntry = manifest["binaries"].find(platform);
    if (binaryEntry == manifest["binaries"].end() || !binaryEntry->is_string()) {
        return std::nullopt;
    }

    NativeModPackage package;
    const std::string binaryPath = binaryEntry->get<std::string>();
    const std::string fallbackName = std::filesystem::path(archive->GetPath()).stem().string();
    package.name = manifest.value("name", fallbackName);
    package.archivePath = archive->GetPath();
    auto file = archive->LoadFile(binaryPath);
    if (file == nullptr || !file->IsLoaded || file->Buffer == nullptr) {
        SPDLOG_ERROR("[ModApi] Could not read '{}' from mod '{}'", binaryPath, package.name);
        return std::nullopt;
    }
    package.binary = file->Buffer;
    package.requiredResources = ReadStringArray(manifest, "requires");
    package.permissions = ReadStringArray(manifest, "permissions");
    package.provenance = ModTrust_Verify(*archive);
    return package;
}

std::string GetTrustKey(const ModProvenance& provenance) {
    return provenance.state == ModTrustState::Verified ? "repo:" + provenance.repository
                                                       : "hash:" + provenance.contentHash;
}

std::unique_ptr<LoadedMod> LoadNativeMod(const NativeModPackage& package) {
    auto mod = std::make_unique<LoadedMod>();
    mod->name = package.name;
    mod->archivePath = package.archivePath;
    mod->requiredResources = package.requiredResources;
    mod->binaryHash = Hex(HashBytes(*package.binary));
    if (!mod->library.Load(*package.binary)) {
        SPDLOG_ERROR("[ModApi] Native library for mod '{}' could not be loaded", package.name);
        return nullptr;
    }
    ModPermissions_RegisterMod({ package.name, ModTrust_Describe(package.provenance), GetTrustKey(package.provenance),
                                 ModPermissions_Parse(package.permissions, package.name),
                                 mod->library.GetBaseAddress() });

    auto setApi = reinterpret_cast<S2HModSetApiFunc>(mod->library.GetFunction("ModSetApi"));
    if (setApi == nullptr) {
        SPDLOG_ERROR("[ModApi] Mod '{}' exports no ModSetApi", package.name);
        return nullptr;
    }
    mod->getRequirements = reinterpret_cast<S2HModGetRequirementsFunc>(mod->library.GetFunction("ModGetRequirements"));
    mod->init = reinterpret_cast<S2HModInitFunc>(mod->library.GetFunction("ModInit"));

    CurrentModScope currentMod(package.name);
    setApi(ModApi_Get());
    return mod;
}

bool ReadRequirements(LoadedMod& mod) {
    if (mod.getRequirements == nullptr) {
        return true;
    }
    const S2HModRequirements* requirements = mod.getRequirements();
    if (requirements == nullptr || requirements->structSize < offsetof(S2HModRequirements, requiredBehaviors) ||
        requirements->requiredHookCount > MaxRequirements ||
        (requirements->requiredHookCount != 0 && requirements->requiredHooks == nullptr)) {
        SPDLOG_ERROR("[ModApi] Mod '{}' returned invalid requirements", mod.name);
        return false;
    }
    for (uint32_t i = 0; i < requirements->requiredHookCount; ++i) {
        const char* name = requirements->requiredHooks[i];
        if (name == nullptr) {
            SPDLOG_ERROR("[ModApi] Mod '{}' returned a null hook requirement", mod.name);
            return false;
        }
        mod.requiredHooks.emplace_back(name);
    }
    if (requirements->structSize >= sizeof(S2HModRequirements)) {
        if (requirements->requiredBehaviorCount > MaxRequirements ||
            (requirements->requiredBehaviorCount && requirements->requiredBehaviors == nullptr)) return false;
        for (uint32_t i = 0; i < requirements->requiredBehaviorCount; ++i) {
            const char* name = requirements->requiredBehaviors[i];
            if (name == nullptr || name[0] == '\0') return false;
            mod.requiredBehaviors.emplace_back(name);
        }
    }
    return true;
}

bool HasRequiredHooks(const LoadedMod& mod) {
    bool compatible = true;
    for (const auto& name : mod.requiredHooks) {
        if (!ModApi_HasHook(name.c_str())) {
            SPDLOG_ERROR("[ModApi] Mod '{}' requires missing hook '{}'", mod.name, name);
            compatible = false;
        }
    }
    for (const auto& name : mod.requiredBehaviors) {
        if (!ModApi_Get()->HasVanillaBehavior(name.c_str())) {
            SPDLOG_ERROR("[ModApi] Mod '{}' requires missing vanilla behavior '{}'", mod.name, name);
            compatible = false;
        }
    }
    return compatible;
}

bool HasRequiredResources(const LoadedMod& mod) {
    auto archives = Ship::Context::GetRawInstance()->GetResourceManager()->GetArchiveManager();
    for (const auto& path : mod.requiredResources) {
        if (!archives->HasFile(path)) {
            SPDLOG_ERROR("[ModApi] Mod '{}' needs the resource '{}', which no loaded archive provides", mod.name, path);
            return false;
        }
    }
    return true;
}

std::string CacheKey(const LoadedMod& mod) {
    return mod.archivePath + "\n" + mod.name;
}

bool ReadCachedCompatibility(const Json& cache, const std::string& environment, LoadedMod& mod) {
    const std::string key = CacheKey(mod);
    if (!cache.contains(key) || !cache[key].is_object()) {
        return false;
    }
    const Json& entry = cache[key];
    const bool isCurrent = entry.value("binary", "") == mod.binaryHash &&
                           entry.value("environment", "") == environment && entry.contains("compatible") &&
                           entry["compatible"].is_boolean() && entry.contains("hooks") && entry["hooks"].is_array() && entry.contains("behaviors") && entry["behaviors"].is_array();
    if (!isCurrent || !std::all_of(entry["hooks"].begin(), entry["hooks"].end(),
                                   [](const Json& value) { return value.is_string(); })) {
        return false;
    }
    if (!std::all_of(entry["behaviors"].begin(), entry["behaviors"].end(), [](const Json& value) { return value.is_string(); })) return false;
    mod.requiredBehaviors = entry["behaviors"].get<std::vector<std::string>>();
    mod.requiredHooks = entry["hooks"].get<std::vector<std::string>>();
    mod.compatible = entry["compatible"].get<bool>();
    return true;
}

void CheckCompatibility(LoadedMod& mod, const Json& cache, const std::string& environment) {
    mod.compatibilityCached = ReadCachedCompatibility(cache, environment, mod);
    if (mod.compatibilityCached) {
        return;
    }
    try {
        CurrentModScope currentMod(mod.name);
        mod.compatible = ReadRequirements(mod) && HasRequiredHooks(mod);
    } catch (const std::exception& exception) {
        SPDLOG_ERROR("[ModApi] Mod '{}' failed compatibility verification: {}", mod.name, exception.what());
        mod.compatible = false;
    }
}

void InitMod(LoadedMod& mod) {
    try {
        if (mod.init != nullptr) {
            CurrentModScope currentMod(mod.name);
            mod.init();
        }
        SPDLOG_INFO("[ModApi] Loaded mod '{}'{}", mod.name, mod.compatibilityCached ? " (compatibility cached)" : "");
    } catch (const std::exception& exception) {
        SPDLOG_ERROR("[ModApi] Mod '{}' failed to initialize: {}", mod.name, exception.what());
    }
}

std::vector<NativeModPackage> FindNativeModPackages(const char* platform) {
    std::vector<NativeModPackage> packages;
    auto archives = Ship::Context::GetRawInstance()->GetResourceManager()->GetArchiveManager()->GetArchives();
    for (const auto& archive : *archives) {
        try {
            auto package = ReadNativeModPackage(archive, platform);
            if (package) {
                packages.push_back(std::move(*package));
            }
        } catch (const std::exception& exception) {
            SPDLOG_ERROR("[ModApi] Failed to inspect archive '{}': {}", archive->GetPath(), exception.what());
        }
    }
    return packages;
}

void LoadAllowedPackages(const std::vector<NativeModPackage>& packages) {
    std::unordered_set<std::string> modNames;
    for (const NativeModPackage& package : packages) {
        if (!modNames.insert(package.name).second) {
            SPDLOG_ERROR("[ModApi] Native mod name '{}' is duplicated; ignoring '{}'", package.name,
                         package.archivePath);
            continue;
        }
        const bool allowed = CanRunMod(package);
        const std::string trust = ModTrust_Describe(package.provenance);
        sTrustLabels[package.archivePath] = allowed ? trust : trust + ", not loaded";
        if (!allowed) {
            SPDLOG_WARN("[ModApi] Not loading mod '{}' ({})", package.name, trust);
            continue;
        }
        SPDLOG_INFO("[ModApi] Mod '{}' is {}", package.name, trust);
        try {
            auto mod = LoadNativeMod(package);
            if (mod != nullptr) {
                sLoadedMods.push_back(std::move(mod));
            }
        } catch (const std::exception& exception) {
            SPDLOG_ERROR("[ModApi] Mod '{}' failed to load: {}", package.name, exception.what());
        }
    }
}

} // namespace

void ModLoader_LoadMods() {
    const char* platform = GetPlatformKey();
    if (platform == nullptr) {
        SPDLOG_INFO("[ModApi] Native mods are not supported on this platform");
        return;
    }
    LoadAllowedPackages(FindNativeModPackages(platform));
    ReportRefusedMods();

    const std::string environment = Hex(HashText(1469598103934665603ULL, ModApi_GetCatalogFingerprint()));
    const Json previousCache = LoadJsonObject(GetCachePath());
    Json nextCache = Json::object();

    for (const auto& mod : sLoadedMods) {
        CheckCompatibility(*mod, previousCache, environment);
        nextCache[CacheKey(*mod)] = { { "binary", mod->binaryHash },
                                      { "environment", environment },
                                      { "compatible", mod->compatible },
                                      { "hooks", mod->requiredHooks }, { "behaviors", mod->requiredBehaviors } };
        // `requires` is never cached, so a package added between two boots is picked up.
        if (mod->compatible && HasRequiredResources(*mod)) {
            InitMod(*mod);
        }
    }
    SaveJsonObject(GetCachePath(), nextCache);
}

std::filesystem::path ModLoader_GetModsDirectory() {
    std::string modsPath = Ship::Context::LocateFileAcrossAppDirs("mods", appShortName);
    if (modsPath.empty()) {
        modsPath = Ship::Context::GetPathRelativeToAppDirectory("mods", appShortName);
    }
    return std::filesystem::path(modsPath);
}

std::string ModLoader_DescribeTrust(const std::string& archivePath) {
    auto label = sTrustLabels.find(archivePath);
    return label == sTrustLabels.end() ? std::string() : label->second;
}
