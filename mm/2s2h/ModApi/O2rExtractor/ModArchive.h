#pragma once
#include <filesystem>
#include <string>
#include <zip.h>

// Secondary asset packs inherit the running game's version. A foreign ROM's
// version entry would make Archive::Load reject the entire mod archive.
inline bool ModArchive_Prepare(const std::filesystem::path& path, std::string& error) {
    int code = 0;
    zip_t* archive = zip_open(path.string().c_str(), ZIP_CHECKCONS, &code);
    if (archive == nullptr) { error = "Cannot open extracted ZIP (" + std::to_string(code) + ")"; return false; }
    bool hasResources = false;
    const zip_int64_t count = zip_get_num_entries(archive, 0);
    for (zip_uint64_t index = 0; index < static_cast<zip_uint64_t>(count); ++index) {
        const char* name = zip_get_name(archive, index, 0);
        if (name == nullptr) { error = zip_strerror(archive); zip_discard(archive); return false; }
        if (std::string(name) == "version" || std::string(name) == "portVersion") {
            if (zip_delete(archive, index) != 0) { error = zip_strerror(archive); zip_discard(archive); return false; }
        } else if (*name != '\0' && name[std::char_traits<char>::length(name)-1] != '/') hasResources = true;
    }
    if (!hasResources) { error = "The extractor produced no asset resources"; zip_discard(archive); return false; }
    if (zip_close(archive) != 0) { error = zip_strerror(archive); zip_discard(archive); return false; }
    return true;
}
