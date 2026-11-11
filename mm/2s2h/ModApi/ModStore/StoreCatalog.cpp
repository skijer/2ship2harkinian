#include "StoreCatalog.h"

#include <regex>
#include <set>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace {

using Json = nlohmann::json;

constexpr int CatalogFormat = 2;
constexpr const char* StoreGame = "mm";
constexpr int ReleaseFormat = 1;
constexpr size_t MaxOptionsPerMod = 64;
constexpr size_t MaxOptionNameBytes = 160;
constexpr const char* ReservedPackId = "other";
const std::regex ModId("^[a-z0-9_-]{1,64}$");
const std::regex Hash("^[0-9a-f]{128}$");

std::string ReadString(const Json& object, const char* key) {
    const auto field = object.find(key);
    return field != object.end() && field->is_string() ? field->get<std::string>() : std::string();
}

std::vector<std::string> ReadStrings(const Json& object, const char* key) {
    std::vector<std::string> values;
    const auto field = object.find(key);
    if (field == object.end() || !field->is_array()) {
        return values;
    }
    for (const Json& value : *field) {
        if (value.is_string()) {
            values.push_back(value.get<std::string>());
        }
    }
    return values;
}

bool IsHttps(const std::string& url) {
    return url.rfind("https://", 0) == 0;
}

std::optional<StoreDownload> ReadDownload(const Json& object) {
    if (!object.is_object()) {
        return std::nullopt;
    }
    StoreDownload download;
    download.file = ReadString(object, "file");
    download.url = ReadString(object, "url");
    download.hash = ReadString(object, "hash");
    download.size = object.value("size", uint64_t(0));
    const bool safeFile = !download.file.empty() && download.file.find_first_of("/\\:") == std::string::npos;
    if (!safeFile || !IsHttps(download.url) || !std::regex_match(download.hash, Hash) || download.size == 0) {
        return std::nullopt;
    }
    return download;
}

bool ReadFlag(const Json& object, const char* key) {
    const auto field = object.find(key);
    return field != object.end() && field->is_boolean() && field->get<bool>();
}

std::vector<StoreOption> ReadOptions(const Json& object) {
    std::vector<StoreOption> options;
    const auto field = object.find("options");
    if (field == object.end() || !field->is_array() || field->size() > MaxOptionsPerMod) {
        return options;
    }
    std::set<std::string> seen;
    for (const Json& option : *field) {
        if (!option.is_object()) {
            continue;
        }
        const std::string id = ReadString(option, "id");
        const std::string name = ReadString(option, "name");
        const bool named = !name.empty() && name.size() <= MaxOptionNameBytes;
        if (std::regex_match(id, ModId) && named && seen.insert(id).second) {
            options.push_back({ id, name });
        }
    }
    return options;
}

// Keeps only the mods the catalog really lists, so a pack can be drawn without checking each id again.
std::optional<StorePack> ReadPack(const Json& object, const std::map<std::string, StoreMod>& mods) {
    if (!object.is_object() || object.value("game", std::string("oot")) != StoreGame) {
        return std::nullopt;
    }
    StorePack pack;
    pack.id = ReadString(object, "id");
    pack.name = ReadString(object, "name");
    pack.author = ReadString(object, "author");
    pack.description = ReadString(object, "description");
    pack.icon = ReadString(object, "icon");
    pack.version = ReadString(object, "version");
    if (!std::regex_match(pack.id, ModId) || pack.id == ReservedPackId || pack.name.empty()) {
        return std::nullopt;
    }
    std::set<std::string> seen;
    for (const std::string& id : ReadStrings(object, "mods")) {
        if (mods.count(id) != 0 && seen.insert(id).second) {
            pack.mods.push_back(id);
        }
    }
    if (pack.mods.empty()) {
        return std::nullopt;
    }
    return pack;
}

std::optional<StoreMod> ReadMod(const std::string& id, const Json& object) {
    if (!std::regex_match(id, ModId) || !object.is_object()) {
        return std::nullopt;
    }
    const auto variants = object.find("variants");
    const Json* metadata = &object;
    if (variants != object.end() && variants->is_object()) {
        const auto variant = variants->find(StoreGame);
        if (variant != variants->end() && variant->is_object()) {
            metadata = &*variant;
        }
    }
    if (!metadata->value("listed", true)) {
        return std::nullopt;
    }
    StoreMod mod;
    mod.id = id;
    mod.name = ReadString(*metadata, "name");
    mod.author = ReadString(*metadata, "author");
    mod.description = ReadString(*metadata, "description");
    mod.version = ReadString(*metadata, "version");
    mod.repository = ReadString(*metadata, "repository");
    mod.icon = ReadString(*metadata, "icon");
    mod.owner = ReadString(*metadata, "owner");
    mod.dependencyOnly = ReadFlag(*metadata, "dependency_only");
    mod.options = ReadOptions(*metadata);
    mod.permissions = ReadStrings(*metadata, "permissions");
    mod.dependencies = ReadStrings(*metadata, "dependencies");
    const auto builds = object.find("builds");
    if (builds == object.end() || !builds->is_object()) {
        return std::nullopt;
    }
    const auto build = ReadDownload(builds->value(StoreGame, Json()));
    if (!build) {
        return std::nullopt;
    }
    mod.build = *build;
    if (mod.name.empty()) {
        mod.name = id;
    }
    return mod;
}

} // namespace

std::optional<StoreCatalog> ModStore_ParseCatalog(const std::string& payload) {
    const Json root = Json::parse(payload, nullptr, false);
    if (!root.is_object() || (root.value("format", 0) != 1 && root.value("format", 0) != CatalogFormat)) {
        SPDLOG_ERROR("[ModStore] The catalog has an unknown format");
        return std::nullopt;
    }

    StoreCatalog catalog;
    catalog.sequence = root.value("sequence", uint64_t(0));
    const auto mods = root.find("mods");
    if (mods != root.end() && mods->is_object()) {
        for (const auto& [id, object] : mods->items()) {
            auto mod = ReadMod(id, object);
            if (mod) {
                catalog.mods.emplace(id, std::move(*mod));
            }
        }
    }
    const auto packs = root.find("packs");
    std::set<std::string> packIds;
    if (packs != root.end() && packs->is_array()) {
        for (const Json& object : *packs) {
            auto pack = ReadPack(object, catalog.mods);
            if (pack && packIds.insert(pack->id).second) {
                catalog.packs.push_back(std::move(*pack));
            }
        }
    }
    for (const Json& object : root.value("links", Json::array())) {
        const std::string url = object.is_object() ? ReadString(object, "url") : std::string();
        if (IsHttps(url)) {
            catalog.links.push_back({ ReadString(object, "name"), url, ReadString(object, "description") });
        }
    }
    return catalog;
}

std::optional<StoreRelease> ModStore_ParseRelease(const std::string& payload) {
    const Json root = Json::parse(payload, nullptr, false);
    if (!root.is_object() || root.value("format", 0) != ReleaseFormat) {
        SPDLOG_ERROR("[ModStore] The release manifest has an unknown format");
        return std::nullopt;
    }

    const auto game = root.find("game");
    if (game != root.end() ? (!game->is_string() || *game != StoreGame) : std::string(StoreGame) != "oot") {
        return std::nullopt;
    }
    StoreRelease release;
    release.version = ReadString(root, "version");
    release.pageUrl = ReadString(root, "page");
    const auto assets = root.find("assets");
    if (assets != root.end() && assets->is_object()) {
        for (const auto& [platform, object] : assets->items()) {
            auto download = ReadDownload(object);
            if (download) {
                release.assets.emplace(platform, std::move(*download));
            }
        }
    }
    if (release.version.empty() || !IsHttps(release.pageUrl)) {
        return std::nullopt;
    }
    return release;
}

int ModStore_CompareVersions(const std::string& left, const std::string& right) {
    const std::regex number("[0-9]+");
    auto leftNumber = std::sregex_iterator(left.begin(), left.end(), number);
    auto rightNumber = std::sregex_iterator(right.begin(), right.end(), number);
    const std::sregex_iterator end;
    for (; leftNumber != end && rightNumber != end; ++leftNumber, ++rightNumber) {
        const unsigned long leftValue = std::stoul(leftNumber->str());
        const unsigned long rightValue = std::stoul(rightNumber->str());
        if (leftValue != rightValue) {
            return leftValue < rightValue ? -1 : 1;
        }
    }
    return (leftNumber != end) - (rightNumber != end);
}
