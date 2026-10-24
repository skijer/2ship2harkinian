#ifndef OOT_COMPAT_SOH_RESOURCE_MANAGER_HELPERS_H
#define OOT_COMPAT_SOH_RESOURCE_MANAGER_HELPERS_H

// Ship's resource helpers that 2Ship exports with the same contract.
#include "z64.h"

#ifdef __cplusplus
extern "C" {
#endif
uint8_t ResourceMgr_FileExists(const char* resName);
AnimationHeaderCommon* ResourceMgr_LoadAnimByName(const char* path);
Gfx* ResourceMgr_LoadGfxByName(const char* path);
struct SkeletonHeader* ResourceMgr_LoadSkeletonByName(const char* path, SkelAnime* skelAnime);
char* ResourceMgr_LoadTexOrDListByName(const char* filePath);
Vtx* ResourceMgr_LoadVtxByName(char* path);
int ResourceMgr_OTRSigCheck(char* imgData);
void ResourceMgr_PatchGfxByName(const char* path, const char* patchName, int index, Gfx instruction);
void ResourceMgr_UnpatchGfxByName(const char* path, const char* patchName);
PlayerAnimationHeader* ResourceMgr_LoadPlayerAnimAsHeader(const char* animPath);
Sample* ResourceMgr_LoadAudioSample(const char* path);
uint8_t ResourceMgr_TexIsRaw(const char* texPath);
void ResourceMgr_UnregisterSkeleton(SkelAnime* skelAnime);
#ifdef __cplusplus
}
#endif

#endif
