#ifndef S2H_MOD_API_O2R_EXTRACTOR_H
#define S2H_MOD_API_O2R_EXTRACTOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t structSize;
    const char* romPath;
    const char* assetsDir;
    const char* xmlDir;
    const char* configPath;
    const char* filelistDir;
    const char* outputPath;
} S2HO2rExtractRequest;

#define S2H_O2R_EXTRACT_REQUEST_MIN_SIZE \
    (offsetof(S2HO2rExtractRequest, outputPath) + sizeof(((S2HO2rExtractRequest*)0)->outputPath))

typedef bool (*S2HArchiveFileFunc)(const char* path, const uint8_t* data, uint64_t size, void* userData);
typedef bool (*S2HArchivePathFunc)(const char* path, void* userData);
uint32_t O2rExtractor_ReadFiles(const char* searchMask, S2HArchiveFileFunc visit, void* userData);
uint32_t O2rExtractor_ListFiles(const char* searchMask, S2HArchivePathFunc visit, void* userData);

uint32_t O2rExtractor_ExportFiles(const char* searchMask, const char* destinationDir);
bool O2rExtractor_Run(const S2HO2rExtractRequest* request);

#ifdef __cplusplus
}

#include <filesystem>
#include <string>

struct O2rZapdJob {
    std::filesystem::path romPath;
    std::filesystem::path assetsDir;
    std::string xmlDir;
    std::string configPath;
    std::string filelistDir;
    std::filesystem::path outputPath;
};

uint32_t O2rExtractor_ExportMatching(const std::string& searchMask, const std::filesystem::path& destination);
bool O2rExtractor_RunZapd(const O2rZapdJob& job);
#endif

#endif
