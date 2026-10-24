#include "UnboundAudio.h"
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include "2s2h/BenPort.h"
#include "2s2h/Enhancements/Audio/AudioCollection.h"
extern "C" {
#include "z64.h"
#include "variables.h"
SequenceData* ResourceMgr_LoadSeqPtrByName(const char* path);
extern char** gSequenceMap;
extern size_t gSequenceMapSize;
extern char** gFontMap;
extern size_t gFontMapSize;
SoundFont* ResourceMgr_LoadAudioSoundFontByName(const char* path);
const char* ResourceGetNameByCrc(uint64_t crc);
bool OTRAudio_LockRegistry(void);
void OTRAudio_UnlockRegistry(void);
}
namespace {
const char* CanonicalPath(const char* path) { return path && std::strncmp(path, "__OTR__", 7) == 0 ? path + 7 : path; }
struct RegistryLock {
    bool locked = OTRAudio_LockRegistry();
    ~RegistryLock() { if (locked) OTRAudio_UnlockRegistry(); }
};
bool Grow(char*** map, size_t* size, u8** status, size_t needed, size_t limit) {
    if (*size && (!*map || !*status)) return false;
    if (needed <= *size) return *map != nullptr && *status != nullptr;
    if (needed > limit) return false;
    const size_t capacity = std::min(limit, std::max(needed, *size + 32));
    auto** next = static_cast<char**>(std::calloc(capacity, sizeof(char*)));
    auto* nextStatus = static_cast<u8*>(std::calloc(capacity, sizeof(u8)));
    if (next == nullptr || nextStatus == nullptr) { std::free(next); std::free(nextStatus); return false; }
    if (*size) { std::memcpy(next, *map, *size * sizeof(char*)); std::memcpy(nextStatus, *status, *size); }
    std::free(*map);
    std::free(*status);
    *map = next;
    *status = nextStatus;
    *size = capacity;
    return true;
}
char* CopyPath(const char* path) {
    const size_t size = std::strlen(path) + 1;
    auto* copy = static_cast<char*>(std::malloc(size));
    if (copy) std::memcpy(copy, path, size);
    return copy;
}
int32_t FindFont(const char* path) {
    for (size_t i = 0; i < gFontMapSize; ++i)
        if (gFontMap[i] != nullptr && std::strcmp(path, gFontMap[i]) == 0) return static_cast<int32_t>(i);
    return -1;
}
int32_t RegisterFont(const char* path) {
    path = CanonicalPath(path);
    if (!path || !path[0] || !gFontMap) return -1;
    const int32_t found = FindFont(path);
    if (found >= 0) return found;
    SoundFont* font = ResourceMgr_LoadAudioSoundFontByName(path);
    if (!font) return -1;
    uint32_t id = 0;
    while (id < gFontMapSize && gFontMap[id]) ++id;
    if (id >= 255 || !UnboundAudio_GrowFontMap(id + 1)) return -1;
    char* copy = CopyPath(path);
    if (!copy) return -1;
    gFontMap[id] = copy;
    gAudioCtx.fontLoadStatus[id] = 0;
    font->fntIndex = id;
    return static_cast<int32_t>(id);
}
uint16_t FindSequence(const char* path) {
    if (!path || !gSequenceMap) return 0;
    for (size_t i = 1; i < gSequenceMapSize && i < UINT16_MAX; ++i)
        if (gSequenceMap[i] && std::strcmp(path, gSequenceMap[i]) == 0) return static_cast<uint16_t>(i);
    return 0;
}
}
int UnboundAudio_GrowSequenceMap(uint32_t needed) {
    return Grow(&gSequenceMap, &gSequenceMapSize, &gAudioCtx.seqLoadStatus, needed, UINT16_MAX);
}
int UnboundAudio_GrowFontMap(uint32_t needed) {
    return Grow(&gFontMap, &gFontMapSize, &gAudioCtx.fontLoadStatus, needed, 255);
}
uint16_t Unbound_SequenceIdForPath(const char* path) {
    path = CanonicalPath(path);
    RegistryLock lock;
    return lock.locked ? FindSequence(path) : 0;
}
int32_t Unbound_RegisterSoundFont(const char* path) {
    RegistryLock lock;
    return lock.locked ? RegisterFont(path) : -1;
}
uint16_t Unbound_RegisterSequence(const char* path) {
    path = CanonicalPath(path);
    RegistryLock lock;
    if (!lock.locked || !path || !path[0] || !gSequenceMap || !AudioCollection::Instance) return 0;
    if (const uint16_t found = FindSequence(path)) return found;
    auto* sequence = ResourceMgr_LoadSeqPtrByName(path);
    if (!sequence) return 0;
    if (sequence->numFonts == -1) {
        uint64_t crc;
        std::memcpy(&crc, sequence->fonts, sizeof(crc));
        const char* fontPath = ResourceGetNameByCrc(crc);
        const int32_t font = fontPath ? RegisterFont(fontPath) : -1;
        if (font < 0) return 0;
        std::memset(sequence->fonts, 0, sizeof(sequence->fonts));
        sequence->fonts[0] = static_cast<u8>(font);
        sequence->numFonts = 1;
    }
    if (sequence->numFonts < 0 || sequence->numFonts > 16) return 0;
    uint32_t id = 1;
    while (id < UINT16_MAX && (AudioCollection::Instance->HasSequenceNum(static_cast<uint16_t>(id)) ||
           (id < gSequenceMapSize && gSequenceMap[id]))) ++id;
    if (id >= UINT16_MAX || !UnboundAudio_GrowSequenceMap(id + 1)) return 0;
    char* copy = CopyPath(path);
    if (!copy) return 0;
    gSequenceMap[id] = copy;
    gAudioCtx.seqLoadStatus[id] = 5;
    sequence->seqNumber = static_cast<uint16_t>(id);
    AudioCollection::Instance->AddToCollection(copy, static_cast<uint16_t>(id));
    return static_cast<uint16_t>(id);
}
