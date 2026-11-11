#include "FleetPassages.h"
#include "FleetPortal.h"
#include "FleetRuntime.h"

#include <algorithm>
#include <cmath>
#include <optional>

#include <fleet/rando/CrossPassages.h>

#include "2s2h/ModApi/ActorRegistry/ActorRegistry.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

extern "C" {
#include "z64.h"
#include "functions.h"
#include "variables.h"
void DoorAna_Draw(Actor* thisx, PlayState* play);
}

namespace {

using ComboRando::CrossDungeon::kPassages;

static_assert(kPassages[0].mm == ENTRANCE(ROAD_TO_IKANA, 0) && kPassages[1].mm == ENTRANCE(GREAT_BAY_COAST, 0) &&
              kPassages[2].mm == ENTRANCE(MOUNTAIN_VILLAGE_WINTER, 0) &&
              kPassages[3].mm == ENTRANCE(WOODS_OF_MYSTERY, 0));

// Mountain Village thaws into its own scene; both seasons keep the passage beside the same spawn.
bool IsPassageScene(size_t passage, s16 scene) {
    switch (passage) {
        case 0:
            return scene == SCENE_IKANAMAE;
        case 1:
            return scene == SCENE_30GYOSON;
        case 2:
            return scene == SCENE_10YUKIYAMANOMURA || scene == SCENE_10YUKIYAMANOMURA2;
        case 3:
            return scene == SCENE_26SARUNOMORI;
        default:
            return false;
    }
}

constexpr float kTriggerRadius = 20.0f;
// A player who arrives beside the hole must step this far away before it can take them back.
constexpr float kArmRadius = 70.0f;

struct PassageHole {
    Actor actor;
    bool armed;
};

s16 sHoleId = -1;

bool IsPassageEnabled(const char* key) {
    if (!FleetRuntime_HasSession()) {
        return false;
    }
    const auto cross = FleetRuntime_Seed().value("crossDungeonEntrances", nlohmann::json::object());
    const auto passages =
        cross.is_object() ? cross.value("passages", nlohmann::json::array()) : nlohmann::json::array();
    return std::find(passages.begin(), passages.end(), key) != passages.end();
}

void InitHole(Actor* actor, PlayState* play) {
    Actor_SetScale(actor, 0.01f);
    actor->room = -1;
    auto* hole = reinterpret_cast<PassageHole*>(actor);
    hole->armed = Actor_WorldDistXZToActor(actor, &GET_PLAYER(play)->actor) > kArmRadius;
}

bool CanDepart(PlayState* play, const Player* player) {
    return play->transitionTrigger == TRANS_TRIGGER_OFF && play->transitionMode == TRANS_MODE_OFF &&
           play->pauseCtx.state == 0 && !Player_InCsMode(play) &&
           !(player->stateFlags1 & (PLAYER_STATE1_800000 | PLAYER_STATE1_8000000));
}

void UpdateHole(Actor* actor, PlayState* play) {
    auto* hole = reinterpret_cast<PassageHole*>(actor);
    Player* player = GET_PLAYER(play);
    const float distance = Actor_WorldDistXZToActor(actor, &player->actor);
    if (!hole->armed) {
        hole->armed = distance > kArmRadius;
        return;
    }
    if (distance < kTriggerRadius && std::fabs(player->actor.world.pos.y - actor->world.pos.y) < 30.0f &&
        CanDepart(play, player)) {
        FleetPortal_RequestDeparture(kPassages[actor->params].key);
        hole->armed = false;
    }
}

// Dry floor at the spawn's own height, beside its approach rather than on it.
std::optional<Vec3f> FindHoleSpot(PlayState* play, const ActorEntry& spawn) {
    const float forwardX = Math_SinS(spawn.rot.y);
    const float forwardZ = Math_CosS(spawn.rot.y);
    for (float side : { 150.0f, -150.0f, 220.0f, -220.0f }) {
        for (float ahead : { 80.0f, 160.0f }) {
            Vec3f probe = { spawn.pos.x + forwardX * ahead + forwardZ * side, spawn.pos.y + 50.0f,
                            spawn.pos.z + forwardZ * ahead - forwardX * side };
            CollisionPoly* floor = nullptr;
            const float floorY = BgCheck_EntityRaycastFloor1(&play->colCtx, &floor, &probe);
            if (floor == nullptr || std::fabs(floorY - spawn.pos.y) > 40.0f) {
                continue;
            }
            float surface = 0.0f;
            WaterBox* water = nullptr;
            if (WaterBox_GetSurface1(play, &play->colCtx, probe.x, probe.z, &surface, &water) &&
                surface > floorY - 10.0f) {
                continue;
            }
            return Vec3f{ probe.x, floorY, probe.z };
        }
    }
    return std::nullopt;
}

// The spawn list is indexed through the scene's entrance list; linkActorEntry points into it.
const ActorEntry* SpawnFor(PlayState* play, s32 entrance) {
    const ActorEntry* spawns = play->linkActorEntry - play->setupEntranceList[play->curSpawn].spawn;
    return &spawns[play->setupEntranceList[(entrance >> 4) & 0x1F].spawn];
}

void SpawnHoles() {
    if (gPlayState == nullptr || sHoleId < 0 || gSaveContext.gameMode != GAMEMODE_NORMAL) {
        return;
    }
    for (size_t index = 0; index < std::size(kPassages); index++) {
        const auto& passage = kPassages[index];
        if (!IsPassageScene(index, gPlayState->sceneId) || !IsPassageEnabled(passage.key)) {
            continue;
        }
        const auto spot = FindHoleSpot(gPlayState, *SpawnFor(gPlayState, passage.mm));
        if (spot) {
            Actor_Spawn(&gPlayState->actorCtx, gPlayState, sHoleId, spot->x, spot->y, spot->z, 0, 0, 0,
                        static_cast<s32>(index));
        }
    }
}

void RegisterPassages() {
    S2HActorDefinition hole = {};
    hole.structSize = sizeof(hole);
    hole.key = "fleet_passage_hole";
    hole.description = "Fleet passage to the other game";
    hole.category = ACTORCAT_ITEMACTION;
    hole.actorFlags = ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
    hole.objectId = GAMEPLAY_KEEP;
    hole.instanceSize = sizeof(PassageHole);
    hole.init = InitHole;
    hole.update = UpdateHole;
    hole.draw = DoorAna_Draw;
    sHoleId = ActorRegistry_Register(&hole);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneSpawnActors>(SpawnHoles);
}

static RegisterShipInitFunc sPassagesInit(RegisterPassages, {});

} // namespace

std::optional<s32> FleetPassages_Entrance(const std::string& key) {
    const auto* passage = ComboRando::CrossDungeon::FindPassage(key);
    return passage != nullptr ? std::optional<s32>(passage->mm) : std::nullopt;
}
