#include "EnemyRandomizer.h"

#include <algorithm>
#include <cfloat>
#include <cstring>
#include <map>
#include <optional>
#include <random>
#include <set>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

#include <libultraship/bridge/consolevariablebridge.h>
#include <spdlog/spdlog.h>

#include "2s2h/ActorTable/CustomActorTable.h"
#include "2s2h/BenGui/BenGui.hpp"
#include "2s2h/BenGui/BenMenu.h"
#include "2s2h/BenGui/MenuTypes.h"
#include "2s2h/BenGui/UIWidgets.hpp"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ModApi/ActorRegistry/ActorRegistry.h"
#include "2s2h/ModApi/ActorRegistry/EnemySouls.h"
#include "2s2h/ModApi/ModApi.h"
#include "2s2h/ModApi/RandoItems/RandoItems.h"
#include "2s2h/Rando/ActorBehavior/Souls.h"
#include "2s2h/Rando/Logic/EnemyDropLogic.h"
#include "2s2h/Rando/Rando.h"
#include "2s2h/SaveManager/SaveSections.h"
#include "2s2h/ShipInit.hpp"
#include "2s2h/ShipUtils.h"

extern "C" {
#include "functions.h"
#include "variables.h"
#include "overlays/actors/ovl_En_Encount1/z_en_encount1.h"
#include "overlays/actors/ovl_En_Encount3/z_en_encount3.h"
#include "overlays/actors/ovl_En_Encount4/z_en_encount4.h"
#include "overlays/actors/ovl_En_Peehat/z_en_peehat.h"
extern PlayState* gPlayState;
}

namespace BenGui {
extern std::shared_ptr<BenMenu> mBenMenu;
}

#define CVAR_MODE "gModes.RandomizedEnemies"
#define CVAR_LIST_PREFIX "gModes.RandomizedEnemyList."

namespace {

enum EnhancementMode {
    ENHANCEMENT_OFF,
    ENHANCEMENT_RANDOM,
    ENHANCEMENT_RANDOM_SEEDED,
};

enum RoomFlag {
    ROOM_CLEAR_TRIGGER = 1 << 0,
    ROW_SUBMERGED = 1 << 3,
    ROW_SURFACE = 1 << 4,
    ROOM_TIMED = 1 << 7,
};

enum WaterClass : u8 {
    WATER_CLASS_DRY = 1 << 0,
    WATER_CLASS_SURFACE = 1 << 1,
    WATER_CLASS_SUBMERGED = 1 << 2,
};

struct EnemySpawnRow {
    s16 sceneId;
    s16 setupIndex;
    s16 roomIndex;
    s16 actorIndex;
    s16 actorId;
    s16 params;
    s16 posX;
    s16 posY;
    s16 posZ;
    s16 rotY;
    s16 roomFlags;
};

const EnemySpawnRow sEnemySpawns[] = {
#include "2s2h/Rando/StaticData/EnemySpawns.inc"
};

// Stands on the water surface as well as on land (blue Tektites).
constexpr u32 ENEMY_SURFACE_WALKER = 1u << 16;
constexpr u32 ENEMY_ORIGINAL_ONLY = 1u << 17;
// Eyegores break Stone Tower blocks and the Woodfall Gekko frees a frog: their home spawns stay.
constexpr u32 ENEMY_NEVER_REPLACED = 1u << 18;

struct NativeEnemy {
    const char* key;
    const char* name;
    s16 actorId;
    s32 paramsMask;
    s32 paramsValue;
    s16 spawnParams;
    RandoItemId soul;
    u32 flags;
    s16 spawnHeight;
    s16 rotX;
    s16 rotZ;
};

constexpr u32 MINIBOSS = S2H_ACTOR_ENEMY_MINIBOSS;
constexpr u32 NOT_CLEAR = S2H_ACTOR_ENEMY_NOT_IN_CLEAR_ROOMS;
constexpr u32 NOT_TIMED = S2H_ACTOR_ENEMY_NOT_IN_TIMED_ROOMS;
constexpr u32 SURFACE = S2H_ACTOR_ENEMY_NEEDS_WATER;
constexpr u32 UNDERWATER = S2H_ACTOR_ENEMY_UNDERWATER;
constexpr u32 AQUATIC = S2H_ACTOR_ENEMY_NEEDS_WATER | S2H_ACTOR_ENEMY_UNDERWATER;
constexpr u32 CEILING = S2H_ACTOR_ENEMY_NEEDS_CEILING;

// First match wins, so a variant row precedes its family's catch-all row (mask 0).
// clang-format off
const NativeEnemy sNativeEnemies[] = {
    { "mm.armos",          "Armos",          ACTOR_EN_AM,          0,      0,     -1,     RI_SOUL_ENEMY_ARMOS,         0,                   0,   0,  0 },
    { "mm.bad_bat",        "Bad Bat",        ACTOR_EN_BAT,         0,      0,     -255,   RI_SOUL_ENEMY_BAD_BAT,       0,                   50,  0,  0 },
    { "mm.beamos",         "Beamos",         ACTOR_EN_VM,          0,      0,     -256,   RI_SOUL_ENEMY_BEAMOS,        NOT_CLEAR,           0,   0,  0 },
    { "mm.big_octo",       "Big Octo",       ACTOR_EN_BIGOKUTA,    0,      0,     0,      RI_SOUL_ENEMY_OCTOROK,       MINIBOSS | SURFACE | ENEMY_ORIGINAL_ONLY, 0, 0, 0 },
    { "mm.big_poe",        "Big Poe",        ACTOR_EN_BIGPO,       0xFF,   0,     -256,   RI_SOUL_ENEMY_POE,           MINIBOSS | NOT_CLEAR, 50,  0,  0 },
    { "mm.bio_baba",       "Bio Deku Baba",  ACTOR_BOSS_05,        0,      0,     0,      RI_SOUL_ENEMY_DEKU_BABA,     AQUATIC | ENEMY_ORIGINAL_ONLY, 0, 0, 0 },
    { "mm.blue_bubble",    "Blue Bubble",    ACTOR_EN_BB,          0,      0,     -1,     RI_SOUL_ENEMY_BUBBLE,        0,                   50,  0,  0 },
    { "mm.boe",            "Boe",            ACTOR_EN_MKK,         0,      0,     -256,   RI_SOUL_ENEMY_BOE,           0,                   0,   0,  0 },
    { "mm.captain_keeta",  "Captain Keeta",  ACTOR_EN_BSB,         0,      0,     0x007F, RI_SOUL_ENEMY_CAPTAIN_KEETA, MINIBOSS | NOT_CLEAR, 0,   0,  0 },
    { "mm.chuchu",         "Chuchu",         ACTOR_EN_SLIME,       0,      0,     -256,   RI_SOUL_ENEMY_CHUCHU,        0,                   0,   0,  0 },
    { "mm.death_armos",    "Death Armos",    ACTOR_EN_FAMOS,       0,      0,     0,      RI_SOUL_ENEMY_DEATH_ARMOS,   ENEMY_ORIGINAL_ONLY, 0,   0,  0 },
    { "mm.deku_baba",      "Deku Baba",      ACTOR_EN_DEKUBABA,    0,      0,     0,      RI_SOUL_ENEMY_DEKU_BABA,     0,                   0,   0,  0 },
    { "mm.dexihand",       "Dexihand",       ACTOR_EN_WDHAND,      0,      0,     0,      RI_SOUL_ENEMY_DEXIHAND,      AQUATIC | ENEMY_ORIGINAL_ONLY, 0, 0, 0 },
    { "mm.dinolfos",       "Dinolfos",       ACTOR_EN_DINOFOS,     0,      0,     0,      RI_SOUL_ENEMY_DINOLFOS,      MINIBOSS,            0,   0,  0 },
    { "mm.dodongo",        "Dodongo",        ACTOR_EN_DODONGO,     0,      0,     0,      RI_SOUL_ENEMY_DODONGO,       0,                   0,   0,  0 },
    { "mm.dragonfly",      "Dragonfly",      ACTOR_EN_GRASSHOPPER, 0,      0,     0,      RI_SOUL_ENEMY_DRAGONFLY,     0,                   75,  0,  0 },
    { "mm.eeno",           "Eeno",           ACTOR_EN_SNOWMAN,     0,      0,     -256,   RI_SOUL_ENEMY_EENO,          0,                   0,   0,  0 },
    { "mm.eyegore",        "Eyegore",        ACTOR_EN_EGOL,        0,      0,     0x1FC0, RI_SOUL_ENEMY_EYEGORE,       MINIBOSS | NOT_CLEAR | ENEMY_NEVER_REPLACED, 0, 12, 10 },
    { "mm.fire_keese",     "Fire Keese",     ACTOR_EN_FIREFLY,     0x7FFF, 0,     0,      RI_SOUL_ENEMY_KEESE,         0,                   75,  0,  0 },
    { "mm.ice_keese",      "Ice Keese",      ACTOR_EN_FIREFLY,     0x7FFF, 4,     4,      RI_SOUL_ENEMY_KEESE,         0,                   75,  0,  0 },
    { "mm.keese",          "Keese",          ACTOR_EN_FIREFLY,     0,      0,     2,      RI_SOUL_ENEMY_KEESE,         0,                   75,  0,  0 },
    { "mm.floormaster",    "Floormaster",    ACTOR_EN_FLOORMAS,    0,      0,     0,      RI_SOUL_ENEMY_WALLMASTER,    0,                   0,   0,  0 },
    { "mm.freezard",       "Freezard",       ACTOR_EN_FZ,          0,      0,     2,      RI_SOUL_ENEMY_FREEZARD,      NOT_CLEAR,           0,   0,  0 },
    { "mm.garo",           "Garo",           ACTOR_EN_JSO,         0,      0,     0,      RI_SOUL_ENEMY_GARO,          ENEMY_ORIGINAL_ONLY, 0,   0,  0 },
    { "mm.garo_master",    "Garo Master",    ACTOR_EN_JSO2,        0,      0,     1,      RI_SOUL_ENEMY_GARO,          MINIBOSS,            0,   0,  0 },
    { "mm.gekko",          "Gekko",          ACTOR_EN_PAMETFROG,   0,      0,     1,      RI_SOUL_ENEMY_GEKKO,         MINIBOSS | NOT_CLEAR | ENEMY_NEVER_REPLACED, 0, 0, 0 },
    { "mm.gomess",         "Gomess",         ACTOR_EN_DEATH,       0,      0,     0,      RI_SOUL_ENEMY_GOMESS,        MINIBOSS | NOT_CLEAR, 0,   0,  0 },
    { "mm.guay",           "Guay",           ACTOR_EN_CROW,        0,      0,     0,      RI_SOUL_ENEMY_GUAY,          0,                   75,  0,  0 },
    { "mm.hiploop",        "Hiploop",        ACTOR_EN_PP,          0,      0,     0,      RI_SOUL_ENEMY_HIPLOOP,       0,                   0,   0,  0 },
    { "mm.iron_knuckle",   "Iron Knuckle",   ACTOR_EN_IK,          0,      0,     -254,   RI_SOUL_ENEMY_IRON_KNUCKLE,  MINIBOSS | NOT_CLEAR, 0,   0,  0 },
    { "mm.leever",         "Leever",         ACTOR_EN_NEO_REEBA,   0,      0,     0,      RI_SOUL_ENEMY_LEEVER,        ENEMY_ORIGINAL_ONLY, 0,   0,  0 },
    { "mm.like_like",      "Like Like",      ACTOR_EN_RR,          0,      0,     3,      RI_SOUL_ENEMY_LIKE_LIKE,     UNDERWATER,          0,   0,  0 },
    { "mm.mad_scrub",      "Mad Scrub",      ACTOR_EN_DEKUNUTS,    0,      0,     -255,   RI_SOUL_ENEMY_MAD_SCRUB,     0,                   0,   0,  0 },
    { "mm.mini_baba",      "Mini Baba",      ACTOR_EN_KAREBABA,    0,      0,     1,      RI_SOUL_ENEMY_DEKU_BABA,     NOT_CLEAR,           0,   0,  0 },
    { "mm.nejiron",        "Nejiron",        ACTOR_EN_BAGUO,       0,      0,     0,      RI_SOUL_ENEMY_NEJIRON,       0,                   0,   0,  0 },
    { "mm.octorok",        "Octorok",        ACTOR_EN_OKUTA,       0,      0,     -256,   RI_SOUL_ENEMY_OCTOROK,       SURFACE,             0,   0,  0 },
    { "mm.peahat_larva",   "Peahat Larva",   ACTOR_EN_PEEHAT,      0xFFFF, 1,     1,      RI_SOUL_ENEMY_PEAHAT,        0,                   100, 0,  0 },
    { "mm.peahat",         "Peahat",         ACTOR_EN_PEEHAT,      0,      0,     0,      RI_SOUL_ENEMY_PEAHAT,        0,                   0,   0,  0 },
    { "mm.poe",            "Poe",            ACTOR_EN_POH,         0,      0,     0xFF,   RI_SOUL_ENEMY_POE,           0,                   50,  0,  0 },
    { "mm.red_bubble",     "Red Bubble",     ACTOR_EN_BBFALL,      0,      0,     0,      RI_SOUL_ENEMY_BUBBLE,        0,                   50,  0,  0 },
    { "mm.redead",         "Redead",         ACTOR_EN_RD,          0,      0,     0x7F01, RI_SOUL_ENEMY_REDEAD,        0,                   0,   0,  0 },
    { "mm.shellblade",     "Shellblade",     ACTOR_EN_SB,          0,      0,     0,      RI_SOUL_ENEMY_SHELLBLADE,    AQUATIC,             0,   0,  0 },
    { "mm.skullfish",      "Skullfish",      ACTOR_EN_PR2,         0,      0,     0,      RI_SOUL_ENEMY_SKULLFISH,     AQUATIC,             0,   0,  0 },
    { "mm.desbreko",       "Desbreko",       ACTOR_EN_PR,          0,      0,     0,      RI_SOUL_ENEMY_SKULLFISH,     AQUATIC | ENEMY_ORIGINAL_ONLY, 0, 0, 0 },
    { "mm.skulltula",      "Skulltula",      ACTOR_EN_ST,          0,      0,     0x3F,   RI_SOUL_ENEMY_SKULLTULA,     CEILING,             0,   0,  0 },
    { "mm.snapper",        "Snapper",        ACTOR_EN_KAME,        0,      0,     0,      RI_SOUL_ENEMY_SNAPPER,       NOT_CLEAR,           0,   0,  0 },
    { "mm.stalchild",      "Stalchild",      ACTOR_EN_SKB,         0,      0,     0,      RI_SOUL_ENEMY_STALCHILD,     0,                   0,   0,  0 },
    { "mm.takkuri",        "Takkuri",        ACTOR_EN_THIEFBIRD,   0,      0,     0,      RI_SOUL_ENEMY_TAKKURI,       ENEMY_ORIGINAL_ONLY, 0,   0,  0 },
    { "mm.tektite",        "Tektite",        ACTOR_EN_TITE,        0,      0,     -2,     RI_SOUL_ENEMY_TEKTITE,       ENEMY_SURFACE_WALKER, 0,  0,  0 },
    { "mm.wallmaster",     "Wallmaster",     ACTOR_EN_WALLMAS,     0,      0,     1,      RI_SOUL_ENEMY_WALLMASTER,    NOT_CLEAR,           0,   0,  0 },
    { "mm.wart",           "Wart",           ACTOR_BOSS_04,        0,      0,     0,      RI_SOUL_ENEMY_WART,          MINIBOSS | NOT_CLEAR, 0,   0,  0 },
    { "mm.wizrobe",        "Wizrobe",        ACTOR_EN_WIZ,         0,      0,     0x027F, RI_SOUL_ENEMY_WIZROBE,       MINIBOSS | NOT_TIMED, 0,   0,  0 },
    { "mm.wolfos",         "Wolfos",         ACTOR_EN_WF,          0,      0,     0,      RI_SOUL_ENEMY_WOLFOS,        0,                   0,   0,  0 },
};
// clang-format on

// Actors a placed enemy spawns on its own, whose objects must count as loaded too.
const std::pair<s16, s16> sCompanionActors[] = {
    { ACTOR_EN_WIZ, ACTOR_EN_WIZ_BROCK },   { ACTOR_EN_WIZ, ACTOR_EN_WIZ_FIRE },
    { ACTOR_EN_PAMETFROG, ACTOR_EN_BIGPAMET }, { ACTOR_BOSS_04, ACTOR_EN_TANRON2 },
    { ACTOR_EN_DEATH, ACTOR_EN_MINIDEATH },   { ACTOR_EN_OKUTA, ACTOR_OBJ_ICEBLOCK },
};

struct EnemyChoice {
    std::string key;
    std::string name;
    s16 actorId;
    s16 params;
    u32 flags;
    s16 spawnHeight;
    s16 rotX;
    s16 rotZ;
};

struct PlacementConstraints {
    u32 roomFlags = 0;
    u8 waterClasses = 0;
    bool ceiling = false;
    bool miniboss = false;
};

using ScenePlacement = std::map<std::string, std::string>;

std::map<s16, ScenePlacement> sPlacement;
std::string sSeedEnemyPoolExcluded;
std::string sSeedSoulPoolExcluded;
std::unordered_set<const Actor*> sAwayActors;

constexpr s16 ACTOR_ID_NONE = -1;
// Actor_Spawn runs init (and the miniboss VB) before it returns, so placement is announced ahead of the spawn.
s16 sPendingAwayActorId = ACTOR_ID_NONE;

void MarkNextSpawnAway(s16 actorId) {
    sPendingAwayActorId = actorId;
}

bool IsAwayActor(const Actor* actor) {
    return sAwayActors.contains(actor) || actor->id == sPendingAwayActorId;
}

struct ReplacedChild {
    Actor* spawner;
    void (*release)(Actor* spawner, Actor* child);
};
std::unordered_map<const Actor*, ReplacedChild> sReplacedChildren;

std::set<std::string> ParseKeyList(const std::string& list) {
    std::set<std::string> keys;
    std::stringstream stream(list);
    std::string key;
    while (std::getline(stream, key, ',')) {
        if (!key.empty()) {
            keys.insert(key);
        }
    }
    return keys;
}

bool IsNativeSoulKey(const char* soulKey) {
    const std::vector<NativeEnemySoul>& souls = GetNativeEnemySouls();
    return std::any_of(souls.begin(), souls.end(),
                       [soulKey](const NativeEnemySoul& soul) { return strcmp(soul.key, soulKey) == 0; });
}

std::string JoinKeyList(const std::set<std::string>& keys) {
    std::string list;
    for (const std::string& key : keys) {
        list += list.empty() ? key : "," + key;
    }
    return list;
}

bool IsRandoModeActive() {
    return IS_RANDO && RANDO_SAVE_OPTIONS[RO_ENEMY_RANDOMIZER] != RO_GENERIC_OFF;
}

s32 GetEnhancementMode() {
    return CVarGetInteger(CVAR_MODE, ENHANCEMENT_OFF);
}

bool IsRandomizerActive() {
    return IsRandoModeActive() || GetEnhancementMode() != ENHANCEMENT_OFF;
}

const NativeEnemy* FindNativeEnemy(s16 actorId, s16 params) {
    for (const NativeEnemy& enemy : sNativeEnemies) {
        if (enemy.actorId == actorId && (params & enemy.paramsMask) == enemy.paramsValue) {
            return &enemy;
        }
    }
    return nullptr;
}

const NativeEnemy* FindNativeEnemyByKey(const std::string& key) {
    for (const NativeEnemy& enemy : sNativeEnemies) {
        if (key == enemy.key) {
            return &enemy;
        }
    }
    return nullptr;
}

const S2HCustomEnemy* FindCustomEnemy(const std::string& key) {
    const int16_t actorId = ActorRegistry_FindCustomId(key.c_str());
    return actorId >= 0 ? ActorRegistry_FindEnemyById(actorId) : nullptr;
}

EnemyChoice ToChoice(const NativeEnemy& enemy) {
    return { enemy.key,         enemy.name, enemy.actorId, enemy.spawnParams, enemy.flags,
             enemy.spawnHeight, enemy.rotX, enemy.rotZ };
}

EnemyChoice ToChoice(const S2HCustomEnemy& enemy) {
    return { enemy.key, enemy.name, enemy.actorId, enemy.params, enemy.flags, enemy.spawnHeight, 0, 0 };
}

std::vector<EnemyChoice> ListInstalledChoices() {
    std::vector<EnemyChoice> choices;
    for (const NativeEnemy& enemy : sNativeEnemies) {
        if (!(enemy.flags & ENEMY_ORIGINAL_ONLY)) {
            choices.push_back(ToChoice(enemy));
        }
    }
    // A custom boss registers as an enemy only to carry its soul; it belongs to its arena, not to enemy slots.
    for (uint32_t i = 0; i < ActorRegistry_GetEnemyCount(); i++) {
        const S2HCustomEnemy* enemy = ActorRegistry_GetEnemyAt(i);
        if (enemy->soulCategory != S2H_ENEMY_CATEGORY_BOSS) {
            choices.push_back(ToChoice(*enemy));
        }
    }
    return choices;
}

u8 GetFittingWaterClasses(u32 flags) {
    const bool needsWater = flags & S2H_ACTOR_ENEMY_NEEDS_WATER;
    const bool underwater = flags & S2H_ACTOR_ENEMY_UNDERWATER;
    u8 classes = 0;
    if (!needsWater) {
        classes |= WATER_CLASS_DRY;
    }
    if ((needsWater && !underwater) || (flags & ENEMY_SURFACE_WALKER)) {
        classes |= WATER_CLASS_SURFACE;
    }
    if (underwater) {
        classes |= WATER_CLASS_SUBMERGED;
    }
    return classes;
}

u8 GetRowWaterClass(u32 rowFlags) {
    if (rowFlags & ROW_SUBMERGED) {
        return WATER_CLASS_SUBMERGED;
    }
    return (rowFlags & ROW_SURFACE) ? WATER_CLASS_SURFACE : WATER_CLASS_DRY;
}

// Aquatic natives move to their own depth wherever their row lies in water (Octorok to the surface).
u8 GetSpawnWaterClass(const NativeEnemy& original, u32 rowFlags) {
    const u8 rowClass = GetRowWaterClass(rowFlags);
    if (rowClass == WATER_CLASS_DRY) {
        return rowClass;
    }
    const bool needsWater = original.flags & S2H_ACTOR_ENEMY_NEEDS_WATER;
    if (needsWater && (original.flags & S2H_ACTOR_ENEMY_UNDERWATER)) {
        return WATER_CLASS_SUBMERGED;
    }
    if (needsWater || (original.flags & ENEMY_SURFACE_WALKER)) {
        return WATER_CLASS_SURFACE;
    }
    return rowClass;
}

bool IsChoiceAllowed(const EnemyChoice& choice, const PlacementConstraints& constraints, s32 minibossMode) {
    const bool isClearRoom = constraints.roomFlags & ROOM_CLEAR_TRIGGER;
    const bool isTimedRoom = constraints.roomFlags & ROOM_TIMED;
    const bool isMiniboss = choice.flags & S2H_ACTOR_ENEMY_MINIBOSS;

    if ((isClearRoom || isTimedRoom) && (choice.flags & S2H_ACTOR_ENEMY_NOT_IN_CLEAR_ROOMS)) {
        return false;
    }
    if (isTimedRoom && (choice.flags & S2H_ACTOR_ENEMY_NOT_IN_TIMED_ROOMS)) {
        return false;
    }
    if ((constraints.waterClasses & ~GetFittingWaterClasses(choice.flags)) != 0) {
        return false;
    }
    if ((choice.flags & S2H_ACTOR_ENEMY_NEEDS_CEILING) && !constraints.ceiling) {
        return false;
    }
    switch (minibossMode) {
        case RO_ENEMY_MINIBOSSES_VANILLA:
            return !isMiniboss;
        case RO_ENEMY_MINIBOSSES_AMONG_THEMSELVES:
            return isMiniboss == constraints.miniboss;
        case RO_ENEMY_MINIBOSSES_ANYWHERE:
            return true;
        default:
            SPDLOG_WARN("[EnemyRandomizer] Unknown miniboss mode {}", minibossMode);
            return !isMiniboss;
    }
}

PlacementConstraints ConstraintsOf(const NativeEnemy& original, u32 roomFlags, u8 waterClass) {
    PlacementConstraints constraints;
    constraints.roomFlags = roomFlags;
    constraints.waterClasses = waterClass;
    constraints.ceiling = original.flags & S2H_ACTOR_ENEMY_NEEDS_CEILING;
    constraints.miniboss = original.flags & S2H_ACTOR_ENEMY_MINIBOSS;
    return constraints;
}

u32 GetRoomFlags(s16 sceneId, s16 roomIndex) {
    u32 flags = 0;
    for (const EnemySpawnRow& row : sEnemySpawns) {
        if (row.sceneId != sceneId || row.roomIndex != roomIndex) {
            continue;
        }
        flags |= row.roomFlags;
        if (row.actorId == ACTOR_OBJ_ROOMTIMER) {
            flags |= ROOM_TIMED;
        }
    }
    return flags;
}

// Spawner rows stand for the children they spawn: the child is what gets replaced.
std::vector<const NativeEnemy*> GetOriginalsOfRow(const EnemySpawnRow& row) {
    static const std::pair<s16, s16> encount1Children[] = {
        { ACTOR_EN_GRASSHOPPER, 1 }, { ACTOR_EN_WALLMAS, 0 }, { ACTOR_EN_PR2, 0 }, { ACTOR_EN_PR2, 0 }
    };
    switch (row.actorId) {
        case ACTOR_EN_ENCOUNT1: {
            const s32 type = (row.params >> 11) & 0x1F;
            if (type >= ARRAY_COUNT(encount1Children)) {
                return {};
            }
            return { FindNativeEnemy(encount1Children[type].first, encount1Children[type].second) };
        }
        case ACTOR_EN_ENCOUNT3:
            return { FindNativeEnemy(ACTOR_EN_JSO, 0) };
        case ACTOR_EN_ENCOUNT4:
            return { FindNativeEnemy(ACTOR_EN_SKB, 0) };
        case ACTOR_EN_PEEHAT:
            return { FindNativeEnemy(row.actorId, row.params), FindNativeEnemy(ACTOR_EN_PEEHAT, 1) };
        default:
            return { FindNativeEnemy(row.actorId, row.params) };
    }
}

void AddConstraints(std::map<std::string, PlacementConstraints>& sceneKeys, const NativeEnemy* original,
                    u32 roomFlags, u32 rowFlags) {
    if (original == nullptr || (original->flags & ENEMY_NEVER_REPLACED)) {
        return;
    }
    const u8 waterClass = GetSpawnWaterClass(*original, rowFlags);
    auto found = sceneKeys.find(original->key);
    if (found == sceneKeys.end()) {
        sceneKeys.emplace(original->key, ConstraintsOf(*original, roomFlags, waterClass));
        return;
    }
    found->second.roomFlags |= roomFlags;
    found->second.waterClasses |= waterClass;
}

std::map<s16, std::map<std::string, PlacementConstraints>> CollectSceneKeys() {
    std::map<s16, std::map<std::string, PlacementConstraints>> scenes;
    for (const EnemySpawnRow& row : sEnemySpawns) {
        const u32 roomFlags = GetRoomFlags(row.sceneId, row.roomIndex);
        for (const NativeEnemy* original : GetOriginalsOfRow(row)) {
            AddConstraints(scenes[row.sceneId], original, roomFlags, (u16)row.roomFlags);
        }
    }
    return scenes;
}

// The row a runtime spawn came from: its own entry, or the spawner nearest to a spawned child.
const EnemySpawnRow* FindNearestRow(s16 sceneId, s16 roomIndex, const Vec3f& pos) {
    const EnemySpawnRow* nearest = nullptr;
    f32 nearestDistSq = FLT_MAX;
    for (const EnemySpawnRow& row : sEnemySpawns) {
        if (row.sceneId != sceneId || row.roomIndex != roomIndex) {
            continue;
        }
        const f32 dx = row.posX - pos.x;
        const f32 dy = row.posY - pos.y;
        const f32 dz = row.posZ - pos.z;
        const f32 distSq = SQ(dx) + SQ(dy) + SQ(dz);
        if (distSq < nearestDistSq) {
            nearestDistSq = distSq;
            nearest = &row;
        }
    }
    return nearest;
}

std::set<std::pair<s16, std::string>> CollectSubmergedGroups() {
    std::set<std::pair<s16, std::string>> submerged;
    for (const auto& [sceneId, sceneKeys] : CollectSceneKeys()) {
        for (const auto& [originalKey, constraints] : sceneKeys) {
            if (constraints.waterClasses == WATER_CLASS_SUBMERGED) {
                submerged.insert({ sceneId, originalKey });
            }
        }
    }
    return submerged;
}

bool IsGroupOnlySubmerged(s16 sceneId, const std::string& originalKey) {
    static const std::set<std::pair<s16, std::string>> sSubmergedGroups = CollectSubmergedGroups();
    return sSubmergedGroups.contains({ sceneId, originalKey });
}

std::vector<EnemyChoice> FilterChoices(const std::vector<EnemyChoice>& pool, const PlacementConstraints& constraints,
                                       s32 minibossMode) {
    std::vector<EnemyChoice> allowed;
    for (const EnemyChoice& choice : pool) {
        if (IsChoiceAllowed(choice, constraints, minibossMode)) {
            allowed.push_back(choice);
        }
    }
    return allowed;
}

std::string PickReplacement(const std::string& originalKey, const PlacementConstraints& constraints,
                            const std::vector<EnemyChoice>& pool, s32 minibossMode, std::mt19937& random) {
    if (minibossMode == RO_ENEMY_MINIBOSSES_VANILLA && constraints.miniboss) {
        return originalKey;
    }
    const std::vector<EnemyChoice> allowed = FilterChoices(pool, constraints, minibossMode);
    if (allowed.empty()) {
        return originalKey;
    }
    return allowed[random() % allowed.size()].key;
}

std::vector<EnemyChoice> GetRandoPool() {
    const std::set<std::string> excluded = ParseKeyList(sSeedEnemyPoolExcluded);
    std::vector<EnemyChoice> pool;
    for (const EnemyChoice& choice : ListInstalledChoices()) {
        if (!excluded.contains(choice.key)) {
            pool.push_back(choice);
        }
    }
    return pool;
}

std::vector<EnemyChoice> GetEnhancementPool() {
    std::vector<EnemyChoice> pool;
    for (const EnemyChoice& choice : ListInstalledChoices()) {
        if (CVarGetInteger((CVAR_LIST_PREFIX + choice.key).c_str(), 1)) {
            pool.push_back(choice);
        }
    }
    return pool;
}

nlohmann::json WritePlacement() {
    nlohmann::json placement = nlohmann::json::object();
    for (const auto& [sceneId, scenePlacement] : sPlacement) {
        nlohmann::json& scene = placement[std::to_string(sceneId)];
        scene = nlohmann::json::object();
        for (const auto& [original, replacement] : scenePlacement) {
            scene[original] = replacement;
        }
    }
    return placement;
}

void ReadPlacement(const nlohmann::json& placement) {
    sPlacement.clear();
    if (!placement.is_object()) {
        return;
    }
    for (const auto& [sceneText, scene] : placement.items()) {
        if (!scene.is_object()) {
            continue;
        }
        ScenePlacement& scenePlacement = sPlacement[(s16)std::stoi(sceneText)];
        for (const auto& [original, replacement] : scene.items()) {
            if (replacement.is_string()) {
                scenePlacement[original] = replacement.get<std::string>();
            }
        }
    }
}

nlohmann::json WriteSeedData() {
    return { { "enemyPlacement", WritePlacement() },
             { "enemyPoolExcluded", sSeedEnemyPoolExcluded },
             { "soulPoolExcluded", sSeedSoulPoolExcluded } };
}

void ReadSeedData(const nlohmann::json& source) {
    const bool isObject = source.is_object();
    ReadPlacement(isObject ? source.value("enemyPlacement", nlohmann::json()) : nlohmann::json());
    sSeedEnemyPoolExcluded = isObject ? source.value("enemyPoolExcluded", std::string()) : std::string();
    sSeedSoulPoolExcluded = isObject ? source.value("soulPoolExcluded", std::string()) : std::string();
    Rando::Logic::RefreshEnemyDropLogic();
}

std::optional<EnemyChoice> FindChoice(const std::string& key) {
    if (const NativeEnemy* native = FindNativeEnemyByKey(key)) {
        return ToChoice(*native);
    }
    if (const S2HCustomEnemy* custom = FindCustomEnemy(key)) {
        return ToChoice(*custom);
    }
    return std::nullopt;
}

u64 GetEnhancementSeed() {
    return IS_RANDO ? gSaveContext.save.shipSaveInfo.rando.finalSeed : gSaveContext.save.shipSaveInfo.fileCreatedAt;
}

std::optional<EnemyChoice> ChooseRandomReplacement(PlayState* play, const NativeEnemy& original, const Vec3f& pos,
                                                   u32 salt) {
    const s16 roomIndex = play->roomCtx.curRoom.num;
    const EnemySpawnRow* row = FindNearestRow(play->sceneId, roomIndex, pos);
    const u8 waterClass = row != nullptr ? GetSpawnWaterClass(original, (u16)row->roomFlags) : WATER_CLASS_DRY;
    const PlacementConstraints constraints =
        ConstraintsOf(original, GetRoomFlags(play->sceneId, roomIndex), waterClass);
    const std::vector<EnemyChoice> allowed =
        FilterChoices(GetEnhancementPool(), constraints, RO_ENEMY_MINIBOSSES_VANILLA);
    if (constraints.miniboss || allowed.empty()) {
        return std::nullopt;
    }
    if (GetEnhancementMode() != ENHANCEMENT_RANDOM_SEEDED) {
        return allowed[Ship_Random(0, (s32)allowed.size())];
    }
    const u64 spawnSeed = GetEnhancementSeed() + play->sceneId * 7919 + (s32)pos.x * 31 + (s32)pos.y * 17 +
                          (s32)pos.z + salt;
    std::mt19937 random((u32)(spawnSeed ^ (spawnSeed >> 32)));
    return allowed[random() % allowed.size()];
}

std::optional<EnemyChoice> ChooseReplacement(PlayState* play, s16 actorId, s16 params, const Vec3f& pos,
                                             u32 salt) {
    const NativeEnemy* original = FindNativeEnemy(actorId, params);
    if (original == nullptr || (original->flags & ENEMY_NEVER_REPLACED)) {
        return std::nullopt;
    }
    std::optional<EnemyChoice> replacement;
    if (IsRandoModeActive()) {
        const auto scene = sPlacement.find(play->sceneId);
        const bool isPlaced = scene != sPlacement.end() && scene->second.contains(original->key);
        replacement = isPlaced ? FindChoice(scene->second.at(original->key)) : std::nullopt;
    } else {
        replacement = ChooseRandomReplacement(play, *original, pos, salt);
    }
    if (!replacement.has_value() || replacement->key == original->key) {
        return std::nullopt;
    }
    return replacement;
}

bool IsOriginalBlockedByClearedRoom(PlayState* play, s16 actorId) {
    const ActorOverlay* overlay = ActorOverlayTable_Get(actorId);
    const bool isEnemyCategory = overlay != nullptr && overlay->profile != nullptr &&
                                 overlay->profile->type == ACTORCAT_ENEMY;
    return isEnemyCategory && Flags_GetClear(play, play->roomCtx.curRoom.num);
}

Vec3f GetGroundedPosition(PlayState* play, const EnemyChoice& choice, Vec3f pos) {
    if (choice.flags & S2H_ACTOR_ENEMY_NEEDS_CEILING) {
        return pos;
    }
    CollisionPoly* poly = nullptr;
    Vec3f probe = { pos.x, pos.y + 50.0f, pos.z };
    const f32 floorY = BgCheck_EntityRaycastFloor1(&play->colCtx, &poly, &probe);
    if (floorY > BGCHECK_Y_MIN) {
        pos.y = floorY;
    }
    pos.y += choice.spawnHeight;
    return pos;
}

Actor* SpawnReplacement(PlayState* play, const EnemyChoice& choice, const Vec3f& pos, s16 rotY, u32 halfDaysBits) {
    const Vec3f spawnPos = GetGroundedPosition(play, choice, pos);
    MarkNextSpawnAway(choice.actorId);
    Actor* actor = Actor_SpawnAsChildAndCutscene(&play->actorCtx, play, choice.actorId, spawnPos.x, spawnPos.y,
                                                 spawnPos.z, choice.rotX, rotY, choice.rotZ, choice.params,
                                                 CS_ID_NONE, halfDaysBits, nullptr);
    if (actor != nullptr && actor->init != nullptr) {
        sAwayActors.insert(actor);
    }
    sPendingAwayActorId = ACTOR_ID_NONE;
    return actor;
}

s16 DecodeEntryRotY(const ActorEntry* entry) {
    s16 rotY = (entry->rot.y >> 7) & 0x1FF;
    if (!(entry->id & 0x8000)) {
        return DEG_TO_BINANG(rotY);
    }
    return rotY > 180 ? rotY - 360 : rotY;
}

u32 DecodeEntryHalfDays(const ActorEntry* entry) {
    return ((entry->rot.x & 7) << 7) | (entry->rot.z & 0x7F);
}

void ReleaseEncount1Child(Actor* spawner, Actor* child) {
    EnEncount1* encount1 = (EnEncount1*)spawner;
    if (encount1->spawnActiveCount > 0) {
        encount1->spawnActiveCount--;
    }
}

void ReleaseEncount3Child(Actor* spawner, Actor* child) {
    EnEncount3* encount3 = (EnEncount3*)spawner;
    encount3->child = nullptr;
    if (encount3->unk14E > 0) {
        encount3->unk14E--;
    }
}

void ReleaseEncount4Child(Actor* spawner, Actor* child) {
    EnEncount4* encount4 = (EnEncount4*)spawner;
    if (encount4->unk_14C > 0) {
        encount4->unk_14C--;
    }
    if (child->colChkInfo.health == 0) {
        encount4->unk_14E++;
        return;
    }
    encount4->unk_14C = 0;
    encount4->unk_14E = 0;
}

void ReleasePeahatLarva(Actor* spawner, Actor* child) {
    EnPeehat* peahat = (EnPeehat*)spawner;
    if (peahat->unk_2AC > 0) {
        peahat->unk_2AC--;
    }
}

void HandleSpawnerChild(va_list args, bool* should, void (*release)(Actor*, Actor*), u32 salt) {
    Actor* spawner = va_arg(args, Actor*);
    const s16 actorId = (s16)va_arg(args, int);
    const Vec3f* pos = va_arg(args, Vec3f*);
    const s16 params = (s16)va_arg(args, s32);
    Actor** child = va_arg(args, Actor**);

    const std::optional<EnemyChoice> replacement = ChooseReplacement(gPlayState, actorId, params, *pos, salt);
    if (!replacement.has_value()) {
        return;
    }
    *should = false;
    *child = SpawnReplacement(gPlayState, *replacement, *pos, spawner->shape.rot.y, HALFDAYBIT_ALL);
    if (*child != nullptr) {
        sReplacedChildren[*child] = { spawner, release };
    }
}

void HandleSpawnEntry(va_list args, bool* should) {
    va_arg(args, ActorContext*);
    ActorEntry* entry = va_arg(args, ActorEntry*);
    PlayState* play = va_arg(args, PlayState*);
    Actor** replacement = va_arg(args, Actor**);

    const s16 actorId = entry->id & 0x1FFF;
    const std::optional<EnemyChoice> choice = ChooseReplacement(play, actorId, entry->params, entry->pos, 0);
    if (!choice.has_value()) {
        return;
    }
    *should = false;
    if (IsOriginalBlockedByClearedRoom(play, actorId)) {
        return;
    }
    *replacement = SpawnReplacement(play, *choice, entry->pos, DecodeEntryRotY(entry), DecodeEntryHalfDays(entry));
}

void ReleaseReplacedChild(Actor* actor) {
    for (auto& [child, link] : sReplacedChildren) {
        if (link.spawner == actor) {
            link.spawner = nullptr;
        }
    }
    auto found = sReplacedChildren.find(actor);
    if (found == sReplacedChildren.end()) {
        return;
    }
    if (found->second.spawner != nullptr && found->second.spawner->update != nullptr) {
        found->second.release(found->second.spawner, actor);
    }
    sReplacedChildren.erase(found);
}

bool HasWizrobePlatform(PlayState* play) {
    for (Actor* prop = play->actorCtx.actorLists[ACTORCAT_PROP].first; prop != nullptr; prop = prop->next) {
        if (prop->id == ACTOR_EN_WIZ_BROCK) {
            return true;
        }
    }
    return false;
}

// The Wizrobe teleports between En_Wiz_Brock platforms and loops forever choosing one when fewer than two exist.
void SpawnWizrobePlatforms(Actor* wizrobe) {
    if (HasWizrobePlatform(gPlayState)) {
        return;
    }
    constexpr s32 PLATFORM_COUNT = 3;
    constexpr f32 PLATFORM_RADIUS = 160.0f;
    for (s32 i = 0; i < PLATFORM_COUNT; i++) {
        const s16 angle = (s16)(i * (0x10000 / PLATFORM_COUNT));
        CollisionPoly* poly = nullptr;
        Vec3f pos = { wizrobe->home.pos.x + Math_SinS(angle) * PLATFORM_RADIUS, wizrobe->home.pos.y + 50.0f,
                      wizrobe->home.pos.z + Math_CosS(angle) * PLATFORM_RADIUS };
        const f32 floorY = BgCheck_EntityRaycastFloor1(&gPlayState->colCtx, &poly, &pos);
        pos.y = floorY > BGCHECK_Y_MIN ? floorY : wizrobe->home.pos.y;
        Actor_Spawn(&gPlayState->actorCtx, gPlayState, ACTOR_EN_WIZ_BROCK, pos.x, pos.y, pos.z, 0, 0, 0, 0);
    }
}

// Placed minibosses must count for the room-clear flag; Keeta starts as a prop and Wart/Gekko as bosses.
void PrepareAwayActor(Actor* actor) {
    if (actor->id == sPendingAwayActorId) {
        sPendingAwayActorId = ACTOR_ID_NONE;
        sAwayActors.insert(actor);
    }
    if (!sAwayActors.contains(actor)) {
        return;
    }
    if (actor->category != ACTORCAT_ENEMY && actor->update != nullptr) {
        Actor_ChangeCategory(gPlayState, &gPlayState->actorCtx, actor, ACTORCAT_ENEMY);
    }
    if (actor->id == ACTOR_EN_WIZ) {
        SpawnWizrobePlatforms(actor);
    }
}

// Gomess indexes Stone Tower Temple light settings that other scenes do not have.
void ClampSceneLightOverride(Actor* actor) {
    if (!sAwayActors.contains(actor)) {
        return;
    }
    EnvironmentContext* envCtx = &gPlayState->envCtx;
    const u8 lightOverride = envCtx->lightSettingOverride;
    const bool isIndex =
        lightOverride != LIGHT_SETTING_OVERRIDE_NONE && lightOverride != LIGHT_SETTING_OVERRIDE_FULL_CONTROL;
    if (isIndex && lightOverride >= envCtx->numLightSettings) {
        envCtx->lightSettingOverride = LIGHT_SETTING_OVERRIDE_NONE;
    }
}

std::unordered_set<s16> CollectPlacedObjectIds() {
    std::unordered_set<s16> actorIds;
    for (const EnemyChoice& choice : ListInstalledChoices()) {
        actorIds.insert(choice.actorId);
    }
    for (const auto& [owner, companion] : sCompanionActors) {
        actorIds.insert(companion);
    }
    std::unordered_set<s16> objectIds;
    for (s16 actorId : actorIds) {
        const ActorOverlay* overlay = ActorOverlayTable_Get(actorId);
        if (overlay != nullptr && overlay->profile != nullptr && overlay->profile->objectId != GAMEPLAY_KEEP) {
            objectIds.insert(overlay->profile->objectId);
        }
    }
    return objectIds;
}

void RegisterEnemyRandomizer() {
    const bool isActive = IsRandomizerActive();

    COND_VB_SHOULD(VB_SPAWN_ACTOR_ENTRY, isActive, { HandleSpawnEntry(args, should); });
    COND_VB_SHOULD(VB_ENCOUNT1_SPAWN_ENEMY, isActive, { HandleSpawnerChild(args, should, ReleaseEncount1Child, 1); });
    COND_VB_SHOULD(VB_ENCOUNT3_SPAWN_GARO, isActive, { HandleSpawnerChild(args, should, ReleaseEncount3Child, 3); });
    COND_VB_SHOULD(VB_ENCOUNT4_SPAWN_STALCHILD, isActive,
                   { HandleSpawnerChild(args, should, ReleaseEncount4Child, 4); });
    COND_VB_SHOULD(VB_PEAHAT_SPAWN_LARVA, isActive, { HandleSpawnerChild(args, should, ReleasePeahatLarva, 5); });

    // The port loads assets by path, so a placed enemy whose object the room lacks can borrow gameplay_keep's slot.
    static std::unordered_set<s16> sPlacedObjectIds;
    sPlacedObjectIds = isActive ? CollectPlacedObjectIds() : std::unordered_set<s16>();
    COND_VB_SHOULD(VB_ENABLE_OBJECT_DEPENDENCY, isActive, {
        const s16 objectId = (s16)va_arg(args, int);
        if (sPlacedObjectIds.contains(objectId)) {
            *should = false;
        }
    });

    // Children replaced before the mode is turned off still die after it, and must leave sReplacedChildren then.
    COND_HOOK(OnActorDestroy, true, ReleaseReplacedChild);
}

// Always on: mods (e.g. the spawn tester) place minibosses away from home without the randomizer running.
void RegisterAwayActorHooks() {
    COND_VB_SHOULD(VB_MINIBOSS_USE_HOME_SCENE, true, {
        const Actor* actor = va_arg(args, Actor*);
        *should = !IsAwayActor(actor);
    });
    COND_HOOK(OnActorInit, true, PrepareAwayActor);
    COND_ID_HOOK(OnActorUpdate, ACTOR_EN_DEATH, true, ClampSceneLightOverride);
    COND_HOOK(OnActorDestroy, true, [](Actor* actor) { sAwayActors.erase(actor); });
    ModApi_PublishHostService("enemy_randomizer.mark_next_spawn_away", reinterpret_cast<void*>(MarkNextSpawnAway));
}

void RegisterSeedSaveSection() {
    SaveSections_Register("enemyRandomizer", WriteSeedData, ReadSeedData);
}

const std::unordered_map<int32_t, const char*> sEnhancementModes = {
    { ENHANCEMENT_OFF, "Off" },
    { ENHANCEMENT_RANDOM, "Random" },
    { ENHANCEMENT_RANDOM_SEEDED, "Random (Seeded)" },
};

void DrawEnhancementWidget() {
    UIWidgets::CVarCombobox(
        "Enemy Randomizer", CVAR_MODE, &sEnhancementModes,
        UIWidgets::ComboboxOptions()
            .Color(THEME_COLOR)
            .Tooltip("Replaces enemies with random ones outside of a randomizer seed that sets its own enemies.\n\n"
                     "- Random: enemies change every time a room loads.\n"
                     "- Random (Seeded): enemies depend on the file or seed.\n\n"
                     "Requires a scene reload."));
    if (GetEnhancementMode() == ENHANCEMENT_OFF) {
        return;
    }
    for (const EnemyChoice& choice : ListInstalledChoices()) {
        UIWidgets::CVarCheckbox(choice.name.c_str(), (CVAR_LIST_PREFIX + choice.key).c_str(),
                                UIWidgets::CheckboxOptions().DefaultValue(true).Color(THEME_COLOR));
    }
}

void RegisterEnhancementWidgets() {
    WidgetPath path = { "Enhancements", "Gameplay", SECTION_COLUMN_2 };
    BenGui::mBenMenu->AddWidget(path, "Enemy Randomizer", WIDGET_CUSTOM).CustomFunction([](WidgetInfo& info) {
        DrawEnhancementWidget();
    });
}

} // namespace

namespace EnemyRandomizer {

void AssignForSeed(uint32_t seed) {
    sSeedEnemyPoolExcluded = CVarGetString(ENEMY_POOL_EXCLUDED_CVAR, "");
    sSeedSoulPoolExcluded = CVarGetString(SOUL_POOL_EXCLUDED_CVAR, "");
    sPlacement.clear();
    if (RANDO_SAVE_OPTIONS[RO_ENEMY_RANDOMIZER] != RO_GENERIC_OFF) {
        const s32 minibossMode = RANDO_SAVE_OPTIONS[RO_ENEMY_RANDOMIZER_MINIBOSSES];
        const std::vector<EnemyChoice> pool = GetRandoPool();
        std::mt19937 random(seed);
        for (const auto& [sceneId, sceneKeys] : CollectSceneKeys()) {
            for (const auto& [originalKey, constraints] : sceneKeys) {
                sPlacement[sceneId][originalKey] =
                    PickReplacement(originalKey, constraints, pool, minibossMode, random);
            }
        }
    }
    Rando::Logic::RefreshEnemyDropLogic();
}

void WriteSpoiler(nlohmann::json& spoiler) {
    spoiler["enemyPlacement"] = WritePlacement();
    spoiler["enemyPoolExcluded"] = sSeedEnemyPoolExcluded;
    spoiler["soulPoolExcluded"] = sSeedSoulPoolExcluded;
}

void ReadSpoiler(const nlohmann::json& spoiler) {
    ReadSeedData(spoiler);
}

static std::optional<LogicReplacement> ResolveReplacement(int16_t sceneId, const std::string& originalKey,
                                                          const std::string& replacementKey) {
    const bool onlySubmerged = IsGroupOnlySubmerged(sceneId, originalKey);
    if (const NativeEnemy* native = FindNativeEnemyByKey(replacementKey)) {
        return LogicReplacement{ native->actorId, nullptr, onlySubmerged };
    }
    if (const S2HCustomEnemy* custom = FindCustomEnemy(replacementKey)) {
        return LogicReplacement{ custom->actorId, custom, onlySubmerged };
    }
    return std::nullopt;
}

LogicReplacement FindLogicReplacement(int16_t sceneId, int16_t originalActorId) {
    const LogicReplacement unchanged = { originalActorId, nullptr, false };
    const auto scene = sPlacement.find(sceneId);
    if (scene == sPlacement.end()) {
        return unchanged;
    }
    for (const auto& [originalKey, replacementKey] : scene->second) {
        const NativeEnemy* original = FindNativeEnemyByKey(originalKey);
        if (original == nullptr || original->actorId != originalActorId) {
            continue;
        }
        return ResolveReplacement(sceneId, originalKey, replacementKey).value_or(unchanged);
    }
    return unchanged;
}

bool IsPlacementActive() {
    return !sPlacement.empty();
}

static bool IsRowReplaced(const EnemySpawnRow& row, const ScenePlacement& placement) {
    for (const NativeEnemy* original : GetOriginalsOfRow(row)) {
        if (original != nullptr && placement.contains(original->key)) {
            return true;
        }
    }
    return false;
}

std::vector<LogicReplacement> FindSpawnsOfActor(int16_t sceneId, int16_t actorId) {
    static const ScenePlacement sNoPlacement;
    const auto scene = sPlacement.find(sceneId);
    const ScenePlacement& placement = scene != sPlacement.end() ? scene->second : sNoPlacement;
    std::vector<LogicReplacement> spawns;
    for (const auto& [originalKey, replacementKey] : placement) {
        const std::optional<LogicReplacement> spawn = ResolveReplacement(sceneId, originalKey, replacementKey);
        if (spawn.has_value() && spawn->actorId == actorId) {
            spawns.push_back(*spawn);
        }
    }
    for (const EnemySpawnRow& row : sEnemySpawns) {
        if (row.sceneId == sceneId && row.actorId == actorId && !IsRowReplaced(row, placement)) {
            spawns.push_back({ actorId, nullptr, false });
            break;
        }
    }
    return spawns;
}

std::vector<SceneReplacement> ListSceneReplacements(int16_t sceneId) {
    std::vector<SceneReplacement> replacements;
    const auto scene = sPlacement.find(sceneId);
    if (scene == sPlacement.end()) {
        return replacements;
    }
    for (const auto& [originalKey, replacementKey] : scene->second) {
        const NativeEnemy* original = FindNativeEnemyByKey(originalKey);
        const std::optional<LogicReplacement> replacement = ResolveReplacement(sceneId, originalKey, replacementKey);
        if (original != nullptr && replacement.has_value()) {
            replacements.push_back({ original->actorId, *replacement });
        }
    }
    return replacements;
}

bool IsSoulInPool(const char* soulKey) {
    const std::string excluded = IS_RANDO ? sSeedSoulPoolExcluded : CVarGetString(SOUL_POOL_EXCLUDED_CVAR, "");
    return !ParseKeyList(excluded).contains(soulKey);
}

bool IsCustomSoulItemExcluded(int32_t randoItem) {
    const char* itemKey = RandoItems_IsModItem(randoItem) ? RandoItems_GetKey(randoItem) : nullptr;
    const std::string_view prefix = "soul.";
    if (itemKey == nullptr || !std::string_view(itemKey).starts_with(prefix)) {
        return false;
    }
    return !IsSoulInPool(itemKey + prefix.size());
}

std::vector<PoolEntry> ListEnemyPool(bool minibosses) {
    std::vector<PoolEntry> entries;
    for (const EnemyChoice& choice : ListInstalledChoices()) {
        const bool isMiniboss = choice.flags & S2H_ACTOR_ENEMY_MINIBOSS;
        if (isMiniboss == minibosses) {
            entries.push_back({ choice.key, choice.name });
        }
    }
    return entries;
}

std::vector<PoolEntry> ListSoulPool() {
    std::vector<PoolEntry> entries;
    for (const NativeEnemySoul& soul : GetNativeEnemySouls()) {
        const std::string name = Rando::StaticData::Items[soul.randoItemId].name;
        entries.push_back({ soul.key, EnemySouls_DescribeNativeSoul(soul.key, name) });
    }
    std::set<std::string> customSouls;
    for (uint32_t i = 0; i < ActorRegistry_GetEnemyCount(); i++) {
        const S2HCustomEnemy* enemy = ActorRegistry_GetEnemyAt(i);
        if (enemy->soulKey != nullptr && !IsNativeSoulKey(enemy->soulKey) && customSouls.insert(enemy->soulKey).second) {
            const std::string fallbackName = std::string("Soul of ") + enemy->name;
            entries.push_back({ enemy->soulKey, EnemySouls_DescribeCustomSoul(enemy->soulKey, fallbackName) });
        }
    }
    return entries;
}

bool IsKeyExcluded(const char* cvar, const std::string& key) {
    return ParseKeyList(CVarGetString(cvar, "")).contains(key);
}

void SetKeyExcluded(const char* cvar, const std::string& key, bool excluded) {
    std::set<std::string> keys = ParseKeyList(CVarGetString(cvar, ""));
    if (excluded) {
        keys.insert(key);
    } else {
        keys.erase(key);
    }
    CVarSetString(cvar, JoinKeyList(keys).c_str());
    Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
}

} // namespace EnemyRandomizer

static RegisterShipInitFunc sInitFunc(RegisterEnemyRandomizer, { CVAR_MODE, "IS_RANDO" });
static RegisterShipInitFunc sSaveSectionInitFunc(RegisterSeedSaveSection);
static RegisterShipInitFunc sAwayActorInitFunc(RegisterAwayActorHooks);
static RegisterMenuInitFunc sMenuInitFunc(RegisterEnhancementWidgets);
