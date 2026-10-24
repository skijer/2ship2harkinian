#pragma once
// Converts the mounted mm.o2r scenes into the 2Ship Unbound layout (unbound-docs/SPEC.md §4, §6).
#include <string>

namespace S2H::Unbound {

struct ExportReport {
    bool ok = false;
    size_t failures = 0;
    size_t scenes = 0;
    size_t rooms = 0;
    size_t copied = 0;
    std::string error;
};

inline constexpr const char* kBaseArchiveName = "mm-unbound.o2r";

// Writes scene/room, collision, path and text documents and copies the original ROM resources verbatim.
ExportReport ExportArchive(const std::string& outPath);

// The converter build and revision a base's manifest records; a base written by another one is stale.
std::string ConverterName();

// Mounts mm-unbound.o2r beside mm.o2r, converting it first when it is missing or stale. Call after the resource
// factories are registered and before mods are mounted, so mods layer over it.
bool EnsureBaseArchive();

} // namespace S2H::Unbound
