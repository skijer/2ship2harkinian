#pragma once

#include <string>
#include <nlohmann/json.hpp>

// Format 1 packages predate game identities and are local to their original host.
inline bool ModPackage_IsCompatible(const nlohmann::json& manifest, const char* game, int major,
                                    std::string& error) {
    const auto format = manifest.find("format");
    const auto target = manifest.find("game");
    const auto version = manifest.find("modapi");
    if (format != manifest.end() && (!format->is_number_integer() || (*format != 1 && *format != 2))) {
        error = "unsupported package format";
    } else if (target != manifest.end() && (!target->is_string() || *target != game)) {
        error = "package targets another game";
    } else if (format != manifest.end() && *format == 2 &&
               (target == manifest.end() || version == manifest.end())) {
        error = "format 2 requires game and modapi";
    } else if (version != manifest.end() && (!version->is_number_integer() || *version != major)) {
        error = "package targets another Mod API major";
    } else if (version == manifest.end() && major != 1) {
        error = "legacy package requires Mod API major 1";
    } else {
        return true;
    }
    return false;
}
