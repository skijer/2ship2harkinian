#include "FleetFieldTest.h"
#include "FleetForeignItems.h"
#include "FleetPortal.h"
#include "FleetRuntime.h"

#include <algorithm>
#include <cstring>
#include <set>
#include <vector>

#include <libultraship/bridge/consolevariablebridge.h>

#include "2s2h/CustomItem/CustomItem.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ModApi/CustomItemRegistry/CustomItemRegistry.h"
#include "2s2h/ModApi/RandoItems/RandoItems.h"
#include "2s2h/Rando/Rando.h"
#include "2s2h/SaveManager/SaveSections.h"
#include "2s2h/ShipInit.hpp"

extern "C" {
#include "z64.h"
#include "functions.h"
#include "variables.h"
#include "overlays/kaleido_scope/ovl_kaleido_scope/z_kaleido_scope.h"
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
constexpr size_t kShowcaseBatch = 24;
constexpr int kShowcaseDrawFrames = 45;
constexpr int kShowcaseTextFrames = 30;
constexpr u16 kNoShownSlot = 0xFF;
constexpr const char* kItemUnequipCvar = "gEnhancements.Equipment.ItemUnequip";

enum class ShowcasePhase { NotStarted, Drawing, Texts, Done };

// Draws every foreign item in a real frame next to Link, then opens each one's get-item text until its icon draws.
struct Showcase {
    ShowcasePhase phase = ShowcasePhase::NotStarted;
    std::vector<std::string> keys;
    std::set<std::string> shownIcons;
    size_t next = 0;
    size_t texts = 0;
    int frames = 0;
};

std::optional<Travel> sTravel;
Showcase sShowcase;
bool sIsButtonCheckQueued = false;
nlohmann::json sButtonCheck;
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

void DrawShowcaseDrop(Actor* actor, PlayState* play) {
    const size_t index = static_cast<size_t>(CUSTOM_ITEM_PARAM);
    if (index >= sShowcase.keys.size()) {
        return;
    }
    Matrix_Scale(30.0f, 30.0f, 30.0f, MTXMODE_APPLY);
    CustomItemRegistry_DrawAcquisition(sShowcase.keys[index].c_str(), play);
}

void DrawNextShowcaseBatch(PlayState* play) {
    ClearRain(play);
    if (sShowcase.next == sShowcase.keys.size()) {
        sShowcase.phase = ShowcasePhase::Texts;
        sShowcase.next = 0;
        return;
    }
    const Vec3f feet = GET_PLAYER(play)->actor.world.pos;
    const size_t end = std::min(sShowcase.next + kShowcaseBatch, sShowcase.keys.size());
    for (size_t index = sShowcase.next; index < end; index++) {
        const s16 angle = static_cast<s16>((index - sShowcase.next) * 0x10000 / kShowcaseBatch);
        EnItem00* drop = CustomItem::Spawn(feet.x + Math_SinS(angle) * kRainRadius, feet.y + kRainHeight,
                                           feet.z + Math_CosS(angle) * kRainRadius, 0, CustomItem::TOSS_ON_SPAWN,
                                           static_cast<s16>(index), nullptr, DrawShowcaseDrop);
        if (drop != nullptr) {
            sRainDrops.push_back(&drop->actor);
        }
    }
    sShowcase.next = end;
    sShowcase.frames = kShowcaseDrawFrames;
}

void ShowNextShowcaseText(PlayState* play) {
    if (play->msgCtx.msgMode != MSGMODE_NONE) {
        if (Message_GetState(&play->msgCtx) != TEXT_STATE_CLOSING) {
            Message_CloseTextbox(play);
        }
        return;
    }
    while (sShowcase.next < sShowcase.keys.size()) {
        const std::string& key = sShowcase.keys[sShowcase.next++];
        const char* icon = CustomItemRegistry_ResolveTexture(key.c_str(), S2H_ITEM_ICON_TEXTBOX);
        if (icon == nullptr || !sShowcase.shownIcons.insert(icon).second) {
            continue;
        }
        if (CustomItemRegistry_ShowGetItemText(play, key.c_str())) {
            ++sShowcase.texts;
            sShowcase.frames = kShowcaseTextFrames;
            return;
        }
    }
    sShowcase.phase = ShowcasePhase::Done;
}

void AdvanceShowcase(PlayState* play) {
    if (sShowcase.frames > 0) {
        --sShowcase.frames;
        return;
    }
    switch (sShowcase.phase) {
        case ShowcasePhase::Drawing:
            DrawNextShowcaseBatch(play);
            break;
        case ShowcasePhase::Texts:
            ShowNextShowcaseText(play);
            break;
        default:
            break;
    }
}

const char* ShowcasePhaseName() {
    switch (sShowcase.phase) {
        case ShowcasePhase::NotStarted:
            return "notStarted";
        case ShowcasePhase::Drawing:
            return "drawing";
        case ShowcasePhase::Texts:
            return "texts";
        case ShowcasePhase::Done:
            return "done";
        default:
            return "unknown";
    }
}

bool IsOnAnyButton(const std::string& key) {
    for (uint8_t button = 0; button < S2H_ITEM_BUTTON_COUNT; button++) {
        const char* equipped = CustomItemRegistry_GetEquippedKey(button);
        if (equipped != nullptr && key == equipped) {
            return true;
        }
    }
    return false;
}

// Owns the first C-button custom item that is free and usable now; the caller restores ownership.
std::string BorrowCButtonItem(bool* wasOwned) {
    for (const std::string& key : CustomItemRegistry_ListKeys()) {
        const S2HCustomItemDefinition* definition = CustomItemRegistry_Find(key.c_str());
        const uint32_t needed = S2H_CUSTOM_ITEM_EQUIPPABLE | S2H_CUSTOM_ITEM_C_BUTTON;
        if (definition == nullptr || (definition->flags & needed) != needed || IsOnAnyButton(key)) {
            continue;
        }
        *wasOwned = CustomItemRegistry_IsOwned(key.c_str());
        CustomItemRegistry_SetOwned(key.c_str(), true);
        if (CustomItemRegistry_IsUsableNow(key.c_str())) {
            return key;
        }
        CustomItemRegistry_SetOwned(key.c_str(), *wasOwned);
    }
    return "";
}

bool IsKeySavedOnButton(const nlohmann::json& saveFile, const std::string& key, uint8_t button) {
    const nlohmann::json equipped = saveFile.value(nlohmann::json::json_pointer("/sections/owl/customItems/equipped"),
                                                   nlohmann::json::array());
    return std::any_of(equipped.begin(), equipped.end(), [&](const nlohmann::json& entry) {
        return entry.value("button", -1) == button && entry.value("key", "") == key;
    });
}

void SetItemUnequip(int value) {
    CVarSetInteger(kItemUnequipCvar, value);
    // Fleet's settings sync re-registers the hook after the ModApi's, which is the order that cleared the button.
    ShipInit::Init(kItemUnequipCvar);
}

// Item Unequip must not undo a custom equip, and a key whose button was overwritten must not reach the save.
nlohmann::json CheckCustomButtons(PlayState* play) {
    constexpr uint8_t button = S2H_ITEM_BUTTON_C_FIRST;
    bool wasOwned = false;
    const std::string key = BorrowCButtonItem(&wasOwned);
    if (key.empty()) {
        return { { "passed", true }, { "skipped", "no free C-button custom item" } };
    }
    const char* previous = CustomItemRegistry_GetEquippedKey(button);
    const std::string previousKey = previous != nullptr ? previous : "";
    const u8 previousItem = BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_C_LEFT);
    const u8 previousSlot = C_SLOT_EQUIP(0, EQUIP_SLOT_C_LEFT);
    const int previousUnequip = CVarGetInteger(kItemUnequipCvar, 0);

    SetItemUnequip(1);
    const bool isEquipped = CustomItemRegistry_Equip(button, key.c_str());
    play->pauseCtx.equipTargetCBtn = PAUSE_EQUIP_C_LEFT;
    GameInteractor_Should(VB_KALEIDO_EQUIP_ITEM_TO_BUTTON, true, kNoShownSlot, ITEM_CUSTOM);
    const char* kept = CustomItemRegistry_GetEquippedKey(button);
    const bool isKept = kept != nullptr && key == kept;

    BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_C_LEFT) = ITEM_NONE;
    nlohmann::json saveFile;
    SaveSections_Write(saveFile, true);
    const bool isGhostSaved = IsKeySavedOnButton(saveFile, key, button);

    CustomItemRegistry_Unequip(button);
    if (previousKey.empty()) {
        BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_C_LEFT) = previousItem;
        C_SLOT_EQUIP(0, EQUIP_SLOT_C_LEFT) = previousSlot;
        Interface_LoadItemIconImpl(play, EQUIP_SLOT_C_LEFT);
    } else {
        CustomItemRegistry_Equip(button, previousKey.c_str());
    }
    CustomItemRegistry_SetOwned(key.c_str(), wasOwned);
    SetItemUnequip(previousUnequip);
    return { { "passed", isEquipped && isKept && !isGhostSaved },
             { "key", key },
             { "equipped", isEquipped },
             { "keptAfterItemUnequip", isKept },
             { "ghostSaved", isGhostSaved } };
}

nlohmann::json Status() {
    const bool isPlaying = IsPlaying();
    nlohmann::json status = { { "playing", isPlaying },
                              { "scene", isPlaying ? static_cast<int>(gPlayState->sceneId) : -1 },
                              { "transitioning", isPlaying && IsTransitioning(gPlayState) },
                              { "travel", sTravel ? "queued" : sTravelResult },
                              { "rained", sRained.size() },
                              { "collected", CountCollected() },
                              { "showcase",
                                { { "phase", ShowcasePhaseName() },
                                  { "keys", sShowcase.keys.size() },
                                  { "texts", sShowcase.texts } } },
                              { "slowestSaveMs", FleetRuntime_SlowestSaveMilliseconds() } };
    if (!sButtonCheck.is_null()) {
        status["buttons"] = sButtonCheck;
    }
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
    if (request == "TestOwns") {
        return nlohmann::json{ { "owned", FleetRuntime_ReadOwnership(input.value("items", nlohmann::json::array())) } }
            .dump();
    }
    if (request == "TestTravel") {
        sTravel = Travel{ input.value("mode", ""), input.value("entrance", -1), input.value("passage", "") };
        sTravelResult = "queued";
    } else if (request == "TestItemRain") {
        sRainCount = input.value("count", 24);
    } else if (request == "TestNudge") {
        Nudge(input);
    } else if (request == "TestForeignShowcase") {
        sShowcase = { ShowcasePhase::Drawing, FleetForeignItems_ListKeys() };
    } else if (request == "TestCustomButtons") {
        sButtonCheck = nullptr;
        sIsButtonCheckQueued = true;
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
    if (IsTransitioning(play) || play->pauseCtx.state != PAUSE_STATE_OFF) {
        return;
    }
    if (sShowcase.phase == ShowcasePhase::Drawing || sShowcase.phase == ShowcasePhase::Texts) {
        AdvanceShowcase(play);
        return;
    }
    if (play->msgCtx.msgMode != MSGMODE_NONE) {
        return;
    }
    if (sIsButtonCheckQueued) {
        sButtonCheck = CheckCustomButtons(play);
        sIsButtonCheckQueued = false;
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
