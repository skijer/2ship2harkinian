#pragma once
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

ExportReport ExportArchive(const std::string& outPath);

std::string ConverterName();

// Mounts mm-unbound.o2r beside mm.o2r, converting it first when it is missing or stale. Call after the resource
// factories are registered and before mods are mounted, so mods layer over it.
bool EnsureBaseArchive();

} // namespace S2H::Unbound
