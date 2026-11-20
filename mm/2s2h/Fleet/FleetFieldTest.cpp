#include "FleetFieldTest.h"
#include "FleetPortal.h"
#include "FleetRuntime.h"

#include <algorithm>
#include <cstring>
#include <set>
#include <vector>

#include "2s2h/CustomItem/CustomItem.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ModApi/RandoItems/RandoItems.h"
#include "2s2h/Rando/Rando.h"

extern "C" {
#include "z64.h"
#include "functions.h"
#include "variables.h"
extern PlayState* gPlayState;
}

#include "CrossEntranceMmGrottos.h"

namespace {

struct Travel {
    std::string mode;
    int32_t entrance;
    std::string passage;
};

constexpr int32_t kGrottoHoleExit = 0xFFFF;
constexpr int kRainOnLinkEvery = 3;
constexpr float kRainRadius = 220.0f;
constexpr float kRainHeight = 160.0f;
constexpr float kOnLinkHeight = 20.0f;
constexpr int kMessageCloseFrames = 600;

std::optional<Travel> sTravel;
std::string sTravelResult = "idle";
std::optional<int> sRainCount;
std::vector<Actor*> sRainDrops;
std::set<RandoCheckId> sRained;
int sMessageCloseFrames = 0;
int sNudgeFrames = 0;
s8 sNudgeStickX = 0;
s8 sNudgeStickY = 0;
bool sIsDriving = false;

bool IsPlaying() {
    return gPlayState != nullptr && gGameState == &gPlayState->state && gSaveContext.gameMode == GAMEMODE_NORMAL;
}

void HoldStick() {
    if (sNudgeFrames <= 0) {
        return;
    }
    --sNudgeFrames;
    Input& input = gPlayState->state.input[0];
    input.cur.stick_x = sNudgeStickX;
    input.cur.stick_y = sNudgeStickY;
    input.rel.stick_x = sNudgeStickX;
    input.rel.stick_y = sNudgeStickY;
}

// Runs after the pad is read and before Play. Walks drive Link into water and voids on purpose, so he keeps a full
// heart bar every frame instead of reaching a game over.
void DriveTestFrame() {
    if (!IsPlaying()) {
        return;
    }
    gSaveContext.save.saveInfo.playerData.health = gSaveContext.save.saveInfo.playerData.healthCapacity;
    HoldStick();
}

void StartDriving() {
    if (sIsDriving) {
        return;
    }
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameStateMainStart>(DriveTestFrame);
    sIsDriving = true;
}

void Nudge(const nlohmann::json& input) {
    sNudgeStickX = static_cast<s8>(input.value("x", 0));
    sNudgeStickY = static_cast<s8>(input.value("y", 0));
    sNudgeFrames = input.value("frames", 0);
}

bool IsTransitioning(const PlayState* play) {
    return play->transitionTrigger != TRANS_TRIGGER_OFF || play->transitionMode != TRANS_MODE_OFF;
}

// Mirrors the player's scene-exit code: an exit of 0xFFFF climbs out of a grotto through its hole.
void TakeSceneExit(PlayState* play, int32_t exit) {
    if (exit == kGrottoHoleExit) {
        gSaveContext.respawnFlag = 4;
        play->nextEntrance = gSaveContext.respawn[RESPAWN_MODE_UNK_3].entrance;
        play->transitionType = TRANS_TYPE_FADE_WHITE;
        gSaveContext.nextTransitionType = TRANS_TYPE_FADE_WHITE;
    } else {
        play->nextEntrance = static_cast<u16>(exit);
        gSaveContext.retainWeatherMode = true;
        Scene_SetExitFade(play);
    }
    play->transitionTrigger = TRANS_TRIGGER_START;
}

int32_t SceneExitFor(int32_t logicalExit) {
    const auto* grotto = ComboCrossMmCatalogue::FindGrottoByExit(logicalExit);
    if (grotto == nullptr) {
        return logicalExit;
    }
    return grotto->holeReturn ? kGrottoHoleExit : ComboCrossMmCatalogue::GrottoSeasonEntrance(grotto->nativeReturn);
}

void Warp(PlayState* play, int32_t entrance) {
    gSaveContext.respawnFlag = 0;
    play->transitionType = TRANS_TYPE_FADE_BLACK;
    gSaveContext.nextTransitionType = TRANS_TYPE_FADE_BLACK;
    FleetRuntime_SkipNextRoute();
    play->nextEntrance = static_cast<u16>(FleetRuntime_Materialize(entrance));
    play->transitionTrigger = TRANS_TRIGGER_START;
}

// Falling into a hole stores the hole as the way back and loads the grotto, as Door_Ana does.
void Enter(PlayState* play, int32_t entrance) {
    if (ComboCrossMmCatalogue::FindGrottoByEntry(entrance) == nullptr) {
        TakeSceneExit(play, entrance);
        return;
    }
    play->nextEntrance = static_cast<u16>(FleetRuntime_Materialize(entrance));
    play->transitionTrigger = TRANS_TRIGGER_START;
}

bool RunTravel(PlayState* play, const Travel& travel) {
    if (travel.mode == "field") {
        Warp(play, ENTRANCE(TERMINA_FIELD, 0));
        return true;
    }
    if (travel.mode == "warp") {
        Warp(play, travel.entrance);
        return true;
    }
    if (travel.mode == "enter") {
        Enter(play, travel.entrance);
        return true;
    }
    if (travel.mode == "exit") {
        TakeSceneExit(play, SceneExitFor(travel.entrance));
        return true;
    }
    if (travel.mode == "passage") {
        FleetPortal_RequestDeparture(travel.passage.c_str());
        return true;
    }
    return false;
}

bool IsForeignItem(RandoItemId item) {
    const char* key = RandoItems_GetKey(item);
    return key != nullptr && std::strncmp(key, "fleet:", 6) == 0;
}

std::vector<RandoCheckId> ListRainChecks() {
    std::vector<RandoCheckId> foreign;
    std::vector<RandoCheckId> native;
    for (int32_t id = RC_UNKNOWN + 1; id < RC_MAX; id++) {
        const auto check = static_cast<RandoCheckId>(id);
        const RandoSaveCheck& saved = RANDO_SAVE_CHECKS[check];
        if (!saved.shuffled || saved.obtained || saved.eligible || sRained.contains(check)) {
            continue;
        }
        (IsForeignItem(saved.randoItemId) ? foreign : native).push_back(check);
    }
    foreign.insert(foreign.end(), native.begin(), native.end());
    return foreign;
}

void CollectRainDrop(Actor* actor, PlayState* play) {
    RANDO_SAVE_CHECKS[CUSTOM_ITEM_PARAM].eligible = true;
}

void DrawRainDrop(Actor* actor, PlayState* play) {
    const auto check = static_cast<RandoCheckId>(CUSTOM_ITEM_PARAM);
    Matrix_Scale(30.0f, 30.0f, 30.0f, MTXMODE_APPLY);
    Rando::DrawItem(Rando::ConvertItem(RANDO_SAVE_CHECKS[check].randoItemId, check), check, actor);
}

void ClearRain(PlayState* play) {
    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_MISC].first; actor != nullptr; actor = actor->next) {
        if (std::find(sRainDrops.begin(), sRainDrops.end(), actor) != sRainDrops.end()) {
            Actor_Kill(actor);
        }
    }
    sRainDrops.clear();
}

// Drops seed checks the way shuffled grass does: every third lands on Link and is collected, the rest only draw.
void RainItems(PlayState* play, int count) {
    ClearRain(play);
    const Vec3f feet = GET_PLAYER(play)->actor.world.pos;
    const std::vector<RandoCheckId> checks = ListRainChecks();
    const int total = std::min<int>(count, static_cast<int>(checks.size()));
    for (int i = 0; i < total; i++) {
        const bool isOnLink = i % kRainOnLinkEvery == 0;
        const s16 angle = static_cast<s16>(i * 0x10000 / std::max(total, 1));
        const float radius = isOnLink ? 0.0f : kRainRadius;
        const s16 flags = CustomItem::KILL_ON_TOUCH | (isOnLink ? 0 : CustomItem::TOSS_ON_SPAWN);
        EnItem00* drop = CustomItem::Spawn(feet.x + Math_SinS(angle) * radius,
                                           feet.y + (isOnLink ? kOnLinkHeight : kRainHeight),
                                           feet.z + Math_CosS(angle) * radius, 0, flags, static_cast<s16>(checks[i]),
                                           CollectRainDrop, DrawRainDrop);
        if (drop != nullptr) {
            sRainDrops.push_back(&drop->actor);
            sRained.insert(checks[i]);
        }
    }
    sMessageCloseFrames = kMessageCloseFrames;
}

void CloseRainMessages(PlayState* play) {
    if (sMessageCloseFrames <= 0) {
        return;
    }
    --sMessageCloseFrames;
    const u8 state = Message_GetState(&play->msgCtx);
    if (play->msgCtx.msgMode != MSGMODE_NONE && state != TEXT_STATE_CLOSING && state != TEXT_STATE_NONE) {
        Message_CloseTextbox(play);
    }
}

int CountCollected() {
    return static_cast<int>(std::count_if(sRained.begin(), sRained.end(), [](RandoCheckId check) {
        return RANDO_SAVE_CHECKS[check].obtained;
    }));
}

nlohmann::json Status() {
    const bool isPlaying = IsPlaying();
    nlohmann::json status = { { "playing", isPlaying },
                              { "scene", isPlaying ? static_cast<int>(gPlayState->sceneId) : -1 },
                              { "transitioning", isPlaying && IsTransitioning(gPlayState) },
                              { "travel", sTravel ? "queued" : sTravelResult },
                              { "rained", sRained.size() },
                              { "collected", CountCollected() } };
    if (isPlaying) {
        const Player* player = GET_PLAYER(gPlayState);
        const Vec3f& pos = player->actor.world.pos;
        status["player"] = { pos.x, pos.y, pos.z };
        status["grounded"] = (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) != 0;
        status["free"] = !Player_InCsMode(gPlayState) && gPlayState->csCtx.state == CS_STATE_IDLE;
        status["nudging"] = sNudgeFrames > 0;
        status["msgMode"] = gPlayState->msgCtx.msgMode;
        status["pause"] = gPlayState->pauseCtx.state;
    }
    return status;
}

} // namespace

std::optional<std::string> FleetFieldTest_Request(const std::string& request, const nlohmann::json& input) {
    if (request == "TestTravel") {
        sTravel = Travel{ input.value("mode", ""), input.value("entrance", -1), input.value("passage", "") };
        sTravelResult = "queued";
    } else if (request == "TestItemRain") {
        sRainCount = input.value("count", 24);
    } else if (request == "TestNudge") {
        Nudge(input);
    } else if (request != "TestStatus") {
        return std::nullopt;
    }
    if (request != "TestStatus") {
        StartDriving();
    }
    return Status().dump();
}

void FleetFieldTest_FrameEnd() {
    if (!IsPlaying()) {
        return;
    }
    PlayState* play = gPlayState;
    CloseRainMessages(play);
    if (IsTransitioning(play) || play->msgCtx.msgMode != MSGMODE_NONE || play->pauseCtx.state != PAUSE_STATE_OFF) {
        return;
    }
    if (sTravel) {
        sTravelResult = RunTravel(play, *sTravel) ? "started" : "unknown mode " + sTravel->mode;
        sTravel.reset();
    }
    if (sRainCount) {
        RainItems(play, *sRainCount);
        sRainCount.reset();
    }
}
