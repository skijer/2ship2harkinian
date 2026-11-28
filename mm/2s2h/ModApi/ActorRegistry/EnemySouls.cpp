#include "EnemySouls.h"

#include <algorithm>
#include <cassert>
#include <deque>
#include <map>
#include <string_view>
#include <unordered_map>

#include <spdlog/spdlog.h>

#include "2s2h/Enhancements/Modes/EnemyRandomizer.h"
#include "2s2h/ModApi/CustomItemRegistry/CustomItemRegistry.h"
#include "2s2h/ModApi/ModDungeons/DungeonRegistry.h"
#include "2s2h/ModApi/RandoItems/RandoItems.h"
#include "2s2h/Rando/ActorBehavior/ActorBehavior.h"
#include "2s2h/Rando/ActorBehavior/Souls.h"
#include "2s2h/Rando/DrawFuncs.h"
#include "2s2h/Rando/Rando.h"

extern "C" {
#include "z64.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
uint8_t ResourceMgr_FileExists(const char* path);
int ResourceMgr_OTRSigCheck(char* imgData);
SkeletonHeader* ResourceMgr_LoadSkeletonByName(const char* path, SkelAnime* skelAnime);
// OPEN_DISPS redeclares these inside its body, which would give them C++ linkage here.
void FrameInterpolation_RecordOpenChild(const void* a, int b);
void FrameInterpolation_RecordCloseChild(void);
}

namespace {

constexpr std::string_view ResourcePrefix = "__OTR__";
constexpr Color_RGB8 DefaultFlameColor = { 120, 110, 230 };
constexpr float DefaultModelScale = 0.01f;
// The acquisition caller applies a 30x scale; this matches the soul flame's 0.06 world scale.
constexpr float FlameScale = 0.13333333f;

struct Soul {
    std::string itemKey;
    std::string name;
    uint8_t category;
    uint8_t origin;
    // Prefixed resource names: SkelAnime keeps the animation's and reads it back every update.
    std::string skeletonResource;
    std::string animationResource;
    S2HSoulModel model;
    Color_RGB8 flameColor;
    SkelAnime skelAnime;
    std::vector<Vec3s> jointTable;
    std::vector<Vec3s> morphTable;
    bool isPosed;
    bool hasModel;
    u32 lastUpdatedFrame;
};

std::deque<Soul> sSouls;
std::map<std::string, size_t> sSoulsBySoulKey;
std::map<std::string, std::vector<std::string>> sNativeSoulSharers;
std::unordered_map<int16_t, bool> sBossTargetability;

std::string ToResourceName(const char* path) {
    const std::string_view view = path != nullptr ? path : "";
    return view.starts_with(ResourcePrefix) ? std::string(view) : std::string(ResourcePrefix) + std::string(view);
}

bool HasResource(const std::string& resourceName) {
    return ResourceMgr_FileExists(resourceName.c_str() + ResourcePrefix.size());
}

bool IsNativeSoulKey(const std::string& soulKey) {
    const std::vector<NativeEnemySoul>& souls = GetNativeEnemySouls();
    return std::any_of(souls.begin(), souls.end(),
                       [&soulKey](const NativeEnemySoul& soul) { return soulKey == soul.key; });
}

bool HasGenericFields(const S2HActorDefinition* definition) {
    return definition->structSize >= S2H_ACTOR_DEFINITION_GENERIC_SOUL_SIZE;
}

uint8_t ReadCategory(const S2HActorDefinition* definition) {
    const bool declaresCategory = HasGenericFields(definition) &&
                                  definition->enemyCategory > S2H_ENEMY_CATEGORY_REGULAR &&
                                  definition->enemyCategory <= S2H_ENEMY_CATEGORY_BOSS;
    if (declaresCategory) {
        return definition->enemyCategory;
    }
    return (definition->enemyFlags & S2H_ACTOR_ENEMY_MINIBOSS) ? S2H_ENEMY_CATEGORY_MINIBOSS
                                                               : S2H_ENEMY_CATEGORY_REGULAR;
}

uint8_t ResolveOrigin(const S2HActorDefinition* definition, const std::string& soulKey) {
    const uint8_t declared = HasGenericFields(definition) ? definition->enemyOrigin : S2H_ENEMY_ORIGIN_AUTO;
    if (declared != S2H_ENEMY_ORIGIN_AUTO) {
        return declared;
    }
    if (soulKey.starts_with("oot.")) {
        return S2H_ENEMY_ORIGIN_OOT;
    }
    return soulKey.starts_with("mm.") ? S2H_ENEMY_ORIGIN_MM : S2H_ENEMY_ORIGIN_CUSTOM;
}

Color_RGB8 ReadFlameColor(const S2HSoulModel& model) {
    if (model.flameRed == 0 && model.flameGreen == 0 && model.flameBlue == 0) {
        return DefaultFlameColor;
    }
    return { model.flameRed, model.flameGreen, model.flameBlue };
}

bool IsSoulModelInMemory(const S2HActorDefinition* definition) {
    return definition->structSize >= S2H_ACTOR_DEFINITION_SOUL_MEMORY_SIZE && definition->isSoulModelInMemory != 0;
}

// Assumes headers built into a mod outlive its souls: mods stay loaded for the whole session.
bool StoreModelSource(Soul& soul, const S2HSoulModel& model, bool isInMemory) {
    if (isInMemory) {
        return model.idleAnimation != nullptr;
    }
    soul.skeletonResource = ToResourceName(model.skeleton);
    soul.animationResource = ToResourceName(model.idleAnimation);
    soul.model.skeleton = soul.skeletonResource.c_str();
    soul.model.idleAnimation = soul.animationResource.c_str();
    return model.idleAnimation != nullptr && HasResource(soul.skeletonResource) && HasResource(soul.animationResource);
}

void StoreModel(Soul& soul, const S2HSoulModel& model, bool isInMemory) {
    soul.model = model;
    soul.flameColor = ReadFlameColor(model);
    soul.hasModel = model.limbCount > 0 && StoreModelSource(soul, model, isInMemory);
    if (!soul.hasModel) {
        SPDLOG_WARN("[EnemySouls] '{}' draws only its flame: its soul model is missing", soul.itemKey);
    }
}

const SkeletonHeader* LoadSoulSkeleton(const Soul& soul) {
    char* skeleton = (char*)soul.model.skeleton;
    if (!ResourceMgr_OTRSigCheck(skeleton)) {
        return (const SkeletonHeader*)skeleton;
    }
    return ResourceMgr_LoadSkeletonByName(skeleton, NULL);
}

// SkelAnime writes skeleton limbCount + 1 frames into the tables, so they are sized from the skeleton, not the mod.
bool PoseSoul(Soul& soul, PlayState* play) {
    const SkeletonHeader* header = LoadSoulSkeleton(soul);
    if (header == nullptr) {
        SPDLOG_WARN("[EnemySouls] '{}' draws only its flame: its soul skeleton did not load", soul.itemKey);
        return false;
    }
    const s32 limbCount = header->limbCount + 1;
    void* skeleton = (void*)soul.model.skeleton;
    AnimationHeader* animation = (AnimationHeader*)soul.model.idleAnimation;
    soul.jointTable.assign(limbCount, {});
    soul.morphTable.assign(limbCount, {});
    if (soul.model.isFlex) {
        SkelAnime_InitFlex(play, &soul.skelAnime, (FlexSkeletonHeader*)skeleton, animation, soul.jointTable.data(),
                           soul.morphTable.data(), limbCount);
    } else {
        SkelAnime_Init(play, &soul.skelAnime, (SkeletonHeader*)skeleton, animation, soul.jointTable.data(),
                       soul.morphTable.data(), limbCount);
    }
    soul.isPosed = true;
    return true;
}

} // namespace

// Outside the anonymous namespace: OPEN_DISPS redeclares the interpolation hooks in its body, and inside one they
// get C++ linkage and fail to link.
static void DrawSoulModel(Soul& soul, PlayState* play) {
    if (!soul.isPosed && !PoseSoul(soul, play)) {
        soul.hasModel = false;
        return;
    }
    if (soul.lastUpdatedFrame != play->state.frames) {
        soul.lastUpdatedFrame = play->state.frames;
        SkelAnime_Update(&soul.skelAnime);
    }
    const float scale = soul.model.scale > 0.0f ? soul.model.scale : DefaultModelScale;
    const Vec3s& root = soul.jointTable[0];
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    // Animations place the root far from the origin; the soul floats on its root instead.
    Matrix_Translate(-root.x, -root.y + soul.model.yOffset, -root.z, MTXMODE_APPLY);
    // No actor binds segments for a pickup: one the model calls would run the last actor's leftover as opcodes.
    for (u8 segment = 0x08; segment <= 0x0C; segment++) {
        gSPSegment(POLY_OPA_DISP++, segment, (uintptr_t)gEmptyDL);
    }
    if (soul.model.isFlex) {
        SkelAnime_DrawFlexOpa(play, soul.skelAnime.skeleton, soul.skelAnime.jointTable, soul.skelAnime.dListCount,
                              NULL, NULL, NULL);
    } else {
        SkelAnime_DrawOpa(play, soul.skelAnime.skeleton, soul.skelAnime.jointTable, NULL, NULL, NULL);
    }
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

// The soul's index rides in drawItemId, which MM leaves to the item's owner.
static void DrawSoul(PlayState* play, S2HGetItemEntry* entry) {
    assert(entry->drawItemId < sSouls.size());
    Soul& soul = sSouls[entry->drawItemId];
    if (soul.hasModel) {
        DrawSoulModel(soul, play);
    }
    // The flame billboards the matrix in place, which later draws must not inherit.
    Matrix_Push();
    DrawEnLight(soul.flameColor, { FlameScale, FlameScale, FlameScale });
    Matrix_Pop();
}

namespace {

void RegisterSoulItem(size_t index) {
    const Soul& soul = sSouls[index];
    const std::string text = "You got the %r" + soul.name + "%w!&They can be defeated now.";
    S2HCustomItemRandomizer randomizer = {};
    randomizer.structSize = sizeof(randomizer);
    randomizer.flags = S2H_CUSTOM_ITEM_RANDO_ADVANCEMENT;
    randomizer.type = S2H_CUSTOM_ITEM_TYPE_ITEM;
    randomizer.logicKey = soul.itemKey.c_str();
    randomizer.hintKey = soul.itemKey.c_str();
    S2HCustomItemDefinition definition = {};
    definition.structSize = sizeof(definition);
    definition.key = soul.itemKey.c_str();
    definition.presentationFlags = S2H_ITEM_HIDE_FROM_KALEIDO;
    definition.getItemText = text.c_str();
    definition.displayName = soul.name.c_str();
    definition.getItemEntry.drawFunc = DrawSoul;
    definition.getItemEntry.drawItemId = (uint16_t)index;
    definition.randomizer = &randomizer;
    if (!CustomItemRegistry_Register(&definition)) {
        SPDLOG_ERROR("[EnemySouls] Could not register '{}'", soul.itemKey);
    }
}

size_t AddSoul(const S2HActorDefinition* definition, const S2HCustomEnemy& enemy, uint8_t category) {
    Soul& soul = sSouls.emplace_back();
    soul.itemKey = std::string("soul.") + enemy.soulKey;
    soul.name = std::string("Soul of ") + enemy.name;
    soul.category = category;
    soul.origin = ResolveOrigin(definition, enemy.soulKey);
    sSoulsBySoulKey.emplace(enemy.soulKey, sSouls.size() - 1);
    return sSouls.size() - 1;
}

// A later definition may bring the model the first one sharing the soul left out.
void AttachSoulModel(const S2HActorDefinition* definition, size_t index) {
    Soul& soul = sSouls[index];
    const bool declaresModel = HasGenericFields(definition) && definition->soulModel.skeleton != nullptr;
    if (!declaresModel || soul.model.skeleton != nullptr || CustomItemRegistry_Find(soul.itemKey.c_str()) != nullptr) {
        return;
    }
    StoreModel(soul, definition->soulModel, IsSoulModelInMemory(definition));
    RegisterSoulItem(index);
}

uint8_t RegisterCustomSoul(const S2HActorDefinition* definition, const S2HCustomEnemy& enemy, uint8_t category) {
    const auto known = sSoulsBySoulKey.find(enemy.soulKey);
    const size_t index = known != sSoulsBySoulKey.end() ? known->second : AddSoul(definition, enemy, category);
    AttachSoulModel(definition, index);
    return sSouls[index].category;
}

int32_t RegisterDrop(const S2HCustomEnemy& enemy) {
    const std::string key = std::string("drop.") + enemy.key;
    const std::string name = std::string("Enemy Drop ") + enemy.name;
    const int32_t check = DungeonRegistry_RegisterEnemyDropCheck(key.c_str(), name.c_str());
    if (check < 0) {
        SPDLOG_ERROR("[EnemySouls] Could not register drop check '{}'", key);
        return -1;
    }
    Rando::ActorBehavior::RegisterEnemyDrop(enemy.actorId, (RandoCheckId)check);
    return check;
}

const char* DescribeOrigin(uint8_t origin) {
    switch (origin) {
        case S2H_ENEMY_ORIGIN_OOT:
            return "custom; vanilla in Ocarina of Time";
        case S2H_ENEMY_ORIGIN_MM:
            return "custom; vanilla in Majora's Mask";
        case S2H_ENEMY_ORIGIN_CUSTOM:
            return "custom";
        default:
            SPDLOG_WARN("[EnemySouls] Unknown enemy origin {}", origin);
            return "custom";
    }
}

} // namespace

void EnemySouls_OnEnemyRegistered(const S2HActorDefinition* definition, S2HCustomEnemy& enemy) {
    const uint8_t category = ReadCategory(definition);
    const bool dropOnKill = HasGenericFields(definition) && definition->dropOnKill != 0;
    enemy.soulCategory = S2H_ENEMY_CATEGORY_REGULAR;
    enemy.dropCheck = dropOnKill ? RegisterDrop(enemy) : -1;
    if (category != S2H_ENEMY_CATEGORY_REGULAR) {
        enemy.flags |= S2H_ACTOR_ENEMY_MINIBOSS;
    }
    if (enemy.soulKey == nullptr) {
        return;
    }
    if (IsNativeSoulKey(enemy.soulKey)) {
        sNativeSoulSharers[enemy.soulKey].push_back(enemy.name);
        return;
    }
    enemy.soulCategory = RegisterCustomSoul(definition, enemy, category);
    if (enemy.soulCategory == S2H_ENEMY_CATEGORY_BOSS) {
        sBossTargetability[enemy.actorId] = (definition->actorFlags & ACTOR_FLAG_ATTENTION_ENABLED) != 0;
    }
}

// Souls a mod registered itself join the same way: their own poolCount is not needed.
void EnemySouls_AddToPool(const RandoSaveInfo& save, std::vector<RandoItemId>& itemPool) {
    for (const auto& [soulKey, index] : sSoulsBySoulKey) {
        const bool isBoss = sSouls[index].category == S2H_ENEMY_CATEGORY_BOSS;
        const RandoOptionId option = isBoss ? RO_SHUFFLE_BOSS_SOULS : RO_SHUFFLE_ENEMY_SOULS;
        const RandoItemId soulItem = (RandoItemId)GetCustomEnemySoulItem(soulKey.c_str());
        const bool isShuffled = save.randoSaveOptions[option] == RO_GENERIC_YES && soulItem != 0 &&
                                EnemyRandomizer::IsSoulInPool(soulKey.c_str());
        if (isShuffled && std::find(itemPool.begin(), itemPool.end(), soulItem) == itemPool.end()) {
            itemPool.push_back(soulItem);
        }
    }
}

bool EnemySouls_HasBosses() {
    return !sBossTargetability.empty();
}

bool EnemySouls_IsBossWithoutSoul(int16_t actorId) {
    if (!sBossTargetability.contains(actorId)) {
        return false;
    }
    const S2HCustomEnemy* boss = ActorRegistry_FindEnemyById(actorId);
    return boss != nullptr && !HaveCustomEnemySoul(boss->soulKey);
}

bool EnemySouls_HoldBossWithoutSoul(Actor* actor) {
    const auto boss = sBossTargetability.find(actor->id);
    if (boss == sBossTargetability.end()) {
        return false;
    }
    if (EnemySouls_IsBossWithoutSoul(actor->id)) {
        actor->flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
        return true;
    }
    if (boss->second) {
        actor->flags |= ACTOR_FLAG_ATTENTION_ENABLED;
    }
    return false;
}

std::string EnemySouls_DescribeNativeSoul(const char* soulKey, const std::string& name) {
    const auto sharers = sNativeSoulSharers.find(soulKey);
    if (sharers == sNativeSoulSharers.end()) {
        return name + " (vanilla)";
    }
    std::string names;
    for (const std::string& sharer : sharers->second) {
        names += names.empty() ? sharer : ", " + sharer;
    }
    return name + " (vanilla; shared with custom " + names + ")";
}

std::string EnemySouls_DescribeCustomSoul(const char* soulKey, const std::string& fallbackName) {
    const auto known = sSoulsBySoulKey.find(soulKey);
    if (known == sSoulsBySoulKey.end()) {
        return fallbackName + " (custom)";
    }
    const Soul& soul = sSouls[known->second];
    return soul.name + " (" + DescribeOrigin(soul.origin) + ")";
}

void EnemySouls_WriteSpoiler(nlohmann::json& spoiler) {
    nlohmann::json souls = nlohmann::json::object();
    for (const auto& [soulKey, index] : sSoulsBySoulKey) {
        souls[sSouls[index].itemKey] = EnemySouls_DescribeCustomSoul(soulKey.c_str(), sSouls[index].name);
    }
    for (const NativeEnemySoul& soul : GetNativeEnemySouls()) {
        if (sNativeSoulSharers.contains(soul.key)) {
            const std::string name = Rando::StaticData::Items[soul.randoItemId].name;
            souls[Rando::StaticData::Items[soul.randoItemId].spoilerName] =
                EnemySouls_DescribeNativeSoul(soul.key, name);
        }
    }
    spoiler["enemySouls"] = souls;
}
