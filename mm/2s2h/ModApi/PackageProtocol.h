#pragma once

#include <string>
#include <nlohmann/json.hpp>

// Package metadata identifies the game. Native loading and the mod's declared
// requirements determine whether its binary can run on this host.
inline bool ModPackage_IsCompatible(const nlohmann::json& manifest, const char* game, std::string& error) {
    const auto format = manifest.find("format");
    const auto target = manifest.find("game");
    if (format != manifest.end() && (!format->is_number_integer() || (*format != 1 && *format != 2))) {
        error = "unsupported package format";
    } else if (target != manifest.end() && (!target->is_string() || *target != game)) {
        error = "package targets another game";
    } else if (format != manifest.end() && *format == 2 && target == manifest.end()) {
        error = "format 2 requires game";
    } else {
        return true;
    }
    return false;
}
