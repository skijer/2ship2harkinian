#include "FormModel.h"

#include <array>
#include <cstring>
#include <deque>
#include <string>

#include <spdlog/spdlog.h>

#include "2s2h/GameInteractor/GameInteractor.h"

extern "C" {
#include "macros.h"
#include "z64.h"
#include "functions.h"
#include "variables.h"
extern TexturePtr sPlayerEyesTextures[PLAYER_FORM_MAX][PLAYER_EYES_MAX];
extern TexturePtr sPlayerMouthTextures[PLAYER_FORM_MAX][PLAYER_MOUTH_MAX];
uint8_t ResourceMgr_FileExists(const char* path);
Gfx* ResourceMgr_LoadGfxByName(const char* path);
SkeletonHeader* ResourceMgr_LoadSkeletonByName(const char* path, SkelAnime* skelAnime);
}

namespace {

constexpr const char* OtrPrefix = "__OTR__";
constexpr const char* VanillaObjectPrefix = "objects/object_link_";
constexpr const char* AdultRigDirectory = "/object_link_boy/";
constexpr const char* ChildRigDirectory = "/object_link_child/";
constexpr const char* AdultSkeleton = "gLinkAdultSkel";
constexpr const char* ChildSkeleton = "gLinkChildSkel";
constexpr s32 MaxLinkLimbCount = 32;
constexpr std::array<const char*, PLAYER_EYES_MAX> EyeNames = { "Open",      "Half",  "Closedf", "RollLeft",
                                                                 "RollRight", "Shock", "Unk1",    "Unk2" };

std::string sModelPath;
float sRootScaleAdult = 1.0f;
float sRootScaleChild = 1.0f;
float sRootDrop = 0.0f;
FlexSkeletonHeader* sSkeletonAdult = nullptr;
FlexSkeletonHeader* sSkeletonChild = nullptr;
bool sTriedRigs = false;
bool sUsesAdultRig = true;
Player* sSwappedPlayer = nullptr;
void** sSavedSkeleton = nullptr;
s32 sSavedDListCount = 0;
OverrideLimbDrawFlex sVanillaOverride = nullptr;

std::deque<std::string> sFaceTexturePaths;
std::array<TexturePtr, PLAYER_EYES_MAX> sSavedEyes{};
std::array<TexturePtr, PLAYER_MOUTH_MAX> sSavedMouths{};
bool sFaceTexturesSwapped = false;

bool IsLinkSkeleton(const SkeletonHeader* header) {
    return header != nullptr && header->limbCount > 0 && header->limbCount <= MaxLinkLimbCount &&
           header->segment != nullptr;
}

FlexSkeletonHeader* LoadSkeleton(const char* rigDirectory, const char* skeleton) {
    const std::string path = sModelPath + rigDirectory + skeleton;
    SkeletonHeader* header = ResourceMgr_LoadSkeletonByName(path.c_str(), nullptr);
    if (!IsLinkSkeleton(header)) {
        return nullptr;
    }
    return reinterpret_cast<FlexSkeletonHeader*>(header);
}

// MM has no ages: Human Link is the child, so a form's child rig is the natural fit and the adult one the fallback.
FlexSkeletonHeader* ActiveSkeleton() {
    if (!sTriedRigs) {
        sTriedRigs = true;
        sSkeletonChild = LoadSkeleton(ChildRigDirectory, ChildSkeleton);
        sSkeletonAdult = LoadSkeleton(AdultRigDirectory, AdultSkeleton);
        if (sSkeletonChild == nullptr && sSkeletonAdult == nullptr) {
            SPDLOG_WARN("[FormModel] '{}' ships no Link skeleton; keeping Link's body", sModelPath);
        }
    }
    sUsesAdultRig = sSkeletonChild == nullptr;
    return sUsesAdultRig ? sSkeletonAdult : sSkeletonChild;
}

const char* VanillaPlayerLeaf(const char* path) {
    if (path == nullptr) {
        return nullptr;
    }
    const char* leaf = path;
    if (std::strncmp(leaf, OtrPrefix, std::strlen(OtrPrefix)) == 0) {
        leaf += std::strlen(OtrPrefix);
    }
    if (std::strncmp(leaf, VanillaObjectPrefix, std::strlen(VanillaObjectPrefix)) != 0) {
        return nullptr;
    }
    return leaf + std::strlen("objects/");
}

Gfx* ResolveMirrored(const char* vanillaPath) {
    const char* leaf = VanillaPlayerLeaf(vanillaPath);
    if (leaf == nullptr) {
        return nullptr;
    }
    const std::string path = sModelPath + "/" + leaf;
    if (!ResourceMgr_FileExists(path.c_str())) {
        return nullptr;
    }
    return ResourceMgr_LoadGfxByName(path.c_str());
}

bool IsModelPath(const Gfx* dList) {
    return dList != nullptr && std::strstr(reinterpret_cast<const char*>(dList), sModelPath.c_str()) != nullptr;
}

void RestoreSkeleton() {
    if (sSwappedPlayer == nullptr) {
        return;
    }
    sSwappedPlayer->skelAnime.skeleton = sSavedSkeleton;
    sSwappedPlayer->skelAnime.dListCount = sSavedDListCount;
    sSwappedPlayer = nullptr;
}

std::string FaceTexturePath(const char* rigDirectory, const std::string& textureName) {
    return sModelPath + rigDirectory + textureName;
}

// A mirror is authored either with MM's texture names or with OoT's gLink{Adult,Child}Eyes*/Mouth* ones.
const char* FindFaceTexture(const char* vanillaPath, const std::string& ootName) {
    const char* leaf = VanillaPlayerLeaf(vanillaPath);
    const std::string mirrored = leaf != nullptr ? sModelPath + "/" + leaf : std::string();
    const std::string ootNamed = FaceTexturePath(sUsesAdultRig ? AdultRigDirectory : ChildRigDirectory, ootName);

    for (const std::string& candidate : { mirrored, ootNamed }) {
        if (!candidate.empty() && ResourceMgr_FileExists(candidate.c_str())) {
            return sFaceTexturePaths.emplace_back(std::string(OtrPrefix) + candidate).c_str();
        }
    }
    return nullptr;
}

void SwapFaceTextures() {
    const char* rigPrefix = sUsesAdultRig ? "gLinkAdult" : "gLinkChild";

    for (s32 i = 0; i < PLAYER_EYES_MAX; i++) {
        sSavedEyes[i] = sPlayerEyesTextures[PLAYER_FORM_HUMAN][i];
        const std::string ootName = std::string(rigPrefix) + "Eyes" + EyeNames[i] + "Tex";
        const char* path = FindFaceTexture(static_cast<const char*>(sSavedEyes[i]), ootName);
        if (path != nullptr) {
            sPlayerEyesTextures[PLAYER_FORM_HUMAN][i] = const_cast<char*>(path);
        }
    }
    for (s32 i = 0; i < PLAYER_MOUTH_MAX; i++) {
        sSavedMouths[i] = sPlayerMouthTextures[PLAYER_FORM_HUMAN][i];
        const std::string ootName = std::string(rigPrefix) + "Mouth" + std::to_string(i + 1) + "Tex";
        const char* path = FindFaceTexture(static_cast<const char*>(sSavedMouths[i]), ootName);
        if (path != nullptr) {
            sPlayerMouthTextures[PLAYER_FORM_HUMAN][i] = const_cast<char*>(path);
        }
    }
    sFaceTexturesSwapped = true;
}

void RestoreFaceTextures() {
    if (!sFaceTexturesSwapped) {
        return;
    }
    for (s32 i = 0; i < PLAYER_EYES_MAX; i++) {
        sPlayerEyesTextures[PLAYER_FORM_HUMAN][i] = sSavedEyes[i];
    }
    for (s32 i = 0; i < PLAYER_MOUTH_MAX; i++) {
        sPlayerMouthTextures[PLAYER_FORM_HUMAN][i] = sSavedMouths[i];
    }
    sFaceTexturePaths.clear();
    sFaceTexturesSwapped = false;
}

void SwapSkeleton(Actor* actor, bool* should) {
    RestoreSkeleton();
    Player* player = reinterpret_cast<Player*>(actor);
    if (sModelPath.empty() || !*should || player->transformation != PLAYER_FORM_HUMAN) {
        return;
    }
    FlexSkeletonHeader* skeleton = ActiveSkeleton();
    if (skeleton == nullptr) {
        return;
    }
    if (!sFaceTexturesSwapped) {
        SwapFaceTextures();
    }
    sSwappedPlayer = player;
    sSavedSkeleton = player->skelAnime.skeleton;
    sSavedDListCount = player->skelAnime.dListCount;
    player->skelAnime.skeleton = skeleton->sh.segment;
    player->skelAnime.dListCount = skeleton->dListCount;
}

void ScaleRootLimb(s32 limbIndex, Vec3f* pos) {
    if (limbIndex != PLAYER_LIMB_ROOT) {
        return;
    }
    const float scale = sUsesAdultRig ? sRootScaleAdult : sRootScaleChild;
    pos->x *= scale;
    pos->y *= scale;
    pos->z *= scale;
    pos->y -= sRootDrop;
}

bool HideEmptySheath(const Player* player, s32 limbIndex, Gfx** dList) {
    if (gPlayState != nullptr && player != GET_PLAYER(gPlayState)) return false;
    if (limbIndex != PLAYER_LIMB_SHEATH || GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD) != EQUIP_VALUE_SWORD_NONE ||
        GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD) != EQUIP_VALUE_SHIELD_NONE) {
        return false;
    }
    *dList = nullptr;
    return true;
}

// MM's override swaps the waist and the empty hands for its own child meshes, which the form's skeleton already replaces.
bool IsBareLimb(const Player* player, s32 limbIndex) {
    switch (limbIndex) {
        case PLAYER_LIMB_WAIST:
            return true;
        case PLAYER_LIMB_LEFT_HAND:
            return player->leftHandType == PLAYER_MODELTYPE_LH_OPEN || player->leftHandType == PLAYER_MODELTYPE_LH_CLOSED;
        case PLAYER_LIMB_RIGHT_HAND:
            return player->rightHandType == PLAYER_MODELTYPE_RH_OPEN ||
                   player->rightHandType == PLAYER_MODELTYPE_RH_CLOSED;
        default:
            return false;
    }
}

void ResolveLimbDraw(const Player* player, s32 limbIndex, Gfx** dList, Gfx* limbDList, Vec3f* pos) {
    ScaleRootLimb(limbIndex, pos);
    if (HideEmptySheath(player, limbIndex, dList) || *dList == nullptr) {
        return;
    }
    if (IsBareLimb(player, limbIndex) && IsModelPath(limbDList)) {
        *dList = limbDList;
        return;
    }
    Gfx* mirrored = ResolveMirrored(reinterpret_cast<const char*>(*dList));
    if (mirrored != nullptr) {
        *dList = mirrored;
        return;
    }
    const bool isUnresolvedEquipment = VanillaPlayerLeaf(reinterpret_cast<const char*>(*dList)) != nullptr;
    if (!isUnresolvedEquipment && IsModelPath(limbDList)) {
        *dList = limbDList;
    }
}

s32 OverrideLimbDrawMirrored(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, Actor* actor) {
    Gfx* limbDList = *dList;
    const s32 result = sVanillaOverride(play, limbIndex, dList, pos, rot, actor);
    ResolveLimbDraw(reinterpret_cast<Player*>(actor), limbIndex, dList, limbDList, pos);
    return result;
}

} // namespace

void FormModel_Init() {
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::ShouldActorDraw>(ACTOR_PLAYER, SwapSkeleton);
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnActorDraw>(
        ACTOR_PLAYER, [](Actor*) { RestoreSkeleton(); });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDestroy>([]() { sSwappedPlayer = nullptr; });
}

bool FormModel_IsActive(void) {
    return !sModelPath.empty();
}

void FormModel_BeginPlayerDraw(Player* player, s32* lod, OverrideLimbDrawFlex* overrideLimbDraw) {
    if (sSwappedPlayer != player) {
        return;
    }
    *lod = 0;
    sVanillaOverride = *overrideLimbDraw;
    *overrideLimbDraw = OverrideLimbDrawMirrored;
}

void FormModel_SetActive(const S2HFormDefinition* form) {
    const char* modelPath = form == nullptr ? nullptr : form->modelPath;
    const bool hasModel = modelPath != nullptr && modelPath[0] != '\0';

    RestoreSkeleton();
    RestoreFaceTextures();
    sTriedRigs = false;
    sSkeletonAdult = nullptr;
    sSkeletonChild = nullptr;
    sModelPath = hasModel ? modelPath : "";
    sRootScaleAdult = hasModel && form->rootScaleAdult > 0.0f ? form->rootScaleAdult : 1.0f;
    sRootScaleChild = hasModel && form->rootScaleChild > 0.0f ? form->rootScaleChild : 1.0f;
    sRootDrop = hasModel ? form->rootDrop : 0.0f;
}

void FormModel_BeginOtherPlayerDraw(Player* player, const char* key) {
    FormModel_SetActive(FormRegistry_Find(key));
    bool should = true;
    SwapSkeleton(&player->actor, &should);
}

void FormModel_EndOtherPlayerDraw() {
    RestoreSkeleton();
    RestoreFaceTextures();
    FormModel_SetActive(FormRegistry_Find(FormRegistry_GetActiveKey()));
}
