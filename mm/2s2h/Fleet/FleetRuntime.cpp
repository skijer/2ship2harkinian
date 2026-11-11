#include "FleetRuntime.h"
#include "FleetRuntimeBridge.h"
#include "FleetGame.h"
#include "FleetForeignItems.h"

#include <filesystem>
#include <algorithm>
#include <fstream>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <memory>
#include <fleet/rando/CrossDungeonRuntime.h>
#include <spdlog/spdlog.h>

#include "2s2h/ModApi/CustomItemRegistry/CustomItemRegistry.h"
#include "2s2h/ModApi/RandoItems/RandoItems.h"
#include "2s2h/ModApi/RandoInfCustom/RandoInfCustom.h"
#include "2s2h/SaveManager/SaveManager.h"
#include "2s2h/SaveManager/SaveSections.h"
#include "2s2h/BenJsonConversions.hpp"
#include "2s2h/Rando/Rando.h"
#include "2s2h/Rando/Logic/Logic.h"

extern "C" {
#include "functions.h"
#include "variables.h"
extern PlayState* gPlayState;
}

#include "CrossEntranceMmGrottos.h"

namespace {

using namespace ComboRando::CrossDungeon;
constexpr const char* kGame = "mm";
nlohmann::json sSession;
std::set<std::string> sReported;
RuntimeState sRoutes(1);
std::optional<RuntimeState::Transition> sDeparture;
bool sTransitionComplete = false;
int32_t sCheckpoint = -1;

bool IsKnownCheckpoint(int32_t entrance) {
    if (sRoutes.IsRecoveryEntrance(entrance)) {
        return true;
    }
    const auto cross = sSession.at("seed").value("crossDungeonEntrances", nlohmann::json{});
    if (!cross.is_object()) {
        return false;
    }
    const auto dungeons = cross.value("dungeons", nlohmann::json::array());
    for (const auto& entry : dungeons) {
        const auto dungeon = ParseDungeon(entry);
        if (dungeon.returnTo == Endpoint{ 1, entrance }) {
            return true;
        }
    }
    return false;
}
bool sSkipTransition = false;

template <class Function> Function Host(const char* name) {
    return reinterpret_cast<Function>(FleetGame_GetHostFunction(name));
}

bool IsPlaying() {
    return gPlayState != nullptr && gGameState == &gPlayState->state && gGameState->running &&
           gSaveContext.gameMode == GAMEMODE_NORMAL && IS_RANDO && gSaveContext.fileNum == FleetRuntime_Slot();
}

// A scene title card is not a textbox: rewards may arrive under it.
bool IsTextboxOpen() {
    const uint8_t mode = gPlayState->msgCtx.msgMode;
    return mode != MSGMODE_NONE &&
           (mode < MSGMODE_SCENE_TITLE_CARD_FADE_IN_BACKGROUND || mode > MSGMODE_SCENE_TITLE_CARD_FADE_OUT_BACKGROUND);
}

void Grant(const nlohmann::json& delivery) {
    const std::string item = delivery.value("trap", false) ? "Knockoff Item" : delivery.at("item").get<std::string>();
    RandoItemId itemId = RI_UNKNOWN;
    for (const auto& [id, definition] : Rando::StaticData::Items) {
        if (Rando::StaticData::GetItemDisplayName(id) == item ||
            (definition.spoilerName != nullptr && item == definition.spoilerName)) {
            itemId = id;
            break;
        }
    }
    if (itemId == RI_UNKNOWN) {
        throw std::runtime_error("unknown MM delivery: " + item);
    }
    itemId = Rando::ConvertItem(itemId);
    if (itemId == RI_JUNK) {
        uint32_t hash = FleetRuntime_Seed().at("masterSeed").get<uint32_t>();
        for (unsigned char character : delivery.value("id", item)) {
            hash = (hash ^ character) * 16777619u;
        }
        constexpr RandoItemId junk[] = { RI_RUPEE_GREEN, RI_RUPEE_BLUE, RI_RUPEE_RED };
        itemId = junk[hash % 3];
    }
    Rando::GiveItem(itemId);
}

void GrantSharedTier(const std::string& key, int current, const nlohmann::json& item) {
    RandoItemId grant = RI_UNKNOWN;
    if (key == "wallets") {
        grant = current == 0 ? RI_WALLET_ADULT : current == 1 ? RI_WALLET_GIANT : RI_WALLET_TYCOON;
    } else if (key == "bows") {
        grant = current == 0 ? RI_BOW : current == 1 ? RI_QUIVER_40 : RI_QUIVER_50;
    } else if (key == "bombBags") {
        grant = current == 0 ? RI_BOMB_BAG_20 : current == 1 ? RI_BOMB_BAG_30 : RI_BOMB_BAG_40;
    } else if (key == "magic") {
        grant = current == 0 ? RI_SINGLE_MAGIC : RI_DOUBLE_MAGIC;
    }
    if (grant == RI_UNKNOWN) {
        Grant({ { "item", item } });
        return;
    }
    Rando::GiveItem(grant);
}

nlohmann::json ReadSharedTiers() {
    return { { "bows", static_cast<int>(CUR_UPG_VALUE(UPG_QUIVER)) },
             { "bombBags", static_cast<int>(CUR_UPG_VALUE(UPG_BOMB_BAG)) },
             { "bombchuBags", static_cast<int>(INV_CONTENT(ITEM_BOMBCHU) != ITEM_NONE) },
             { "magic", static_cast<int>(gSaveContext.save.saveInfo.playerData.isMagicAcquired +
                                         gSaveContext.save.saveInfo.playerData.isDoubleMagicAcquired) },
             { "wallets", static_cast<int>(CUR_UPG_VALUE(UPG_WALLET)) },
             { "fireArrows", static_cast<int>(INV_CONTENT(ITEM_ARROW_FIRE) == ITEM_ARROW_FIRE) },
             { "iceArrows", static_cast<int>(INV_CONTENT(ITEM_ARROW_ICE) == ITEM_ARROW_ICE) },
             { "lightArrows", static_cast<int>(INV_CONTENT(ITEM_ARROW_LIGHT) == ITEM_ARROW_LIGHT) },
             { "eponasSong", static_cast<int>(CHECK_QUEST_ITEM(QUEST_SONG_EPONA) != 0) },
             { "songOfStorms", static_cast<int>(CHECK_QUEST_ITEM(QUEST_SONG_STORMS) != 0) },
             { "hookshot", static_cast<int>(INV_CONTENT(ITEM_HOOKSHOT) != ITEM_NONE) },
             { "lens", static_cast<int>(INV_CONTENT(ITEM_LENS_OF_TRUTH) != ITEM_NONE) },
             { "goronMask", static_cast<int>(INV_CONTENT(ITEM_MASK_GORON) != ITEM_NONE) },
             { "zoraMask", static_cast<int>(INV_CONTENT(ITEM_MASK_ZORA) != ITEM_NONE) },
             { "keatonMask", static_cast<int>(INV_CONTENT(ITEM_MASK_KEATON) != ITEM_NONE) },
             { "bunnyHood", static_cast<int>(INV_CONTENT(ITEM_MASK_BUNNY) != ITEM_NONE) },
             { "maskOfTruth", static_cast<int>(INV_CONTENT(ITEM_MASK_TRUTH) != ITEM_NONE) } };
}

void SynchronizeSharedTiers() {
    const auto report = Host<bool (*)(const char*, const char*)>("ReportSharedTiers");
    const auto get = Host<const char* (*)(const char*)>("GetSharedTiers");
    if (report == nullptr || get == nullptr) {
        return;
    }
    const std::string native = ReadSharedTiers().dump();
    if (!report(kGame, native.c_str())) {
        return;
    }
    const char* raw = get(kGame);
    if (raw == nullptr) {
        return;
    }
    const auto shared = nlohmann::json::parse(raw);
    bool changed = false;
    for (const auto& entry : shared) {
        const std::string key = entry.at("key");
        const int desired = entry.at("tier");
        for (int attempt = 0; attempt < 16; attempt++) {
            const int previous = ReadSharedTiers().value(key, 0);
            if (previous >= desired) {
                break;
            }
            if (key == "bombchuBags") {
                INV_CONTENT(ITEM_BOMBCHU) = ITEM_BOMBCHU;
                AMMO(ITEM_BOMBCHU) = CUR_CAPACITY(UPG_BOMB_BAG);
            } else {
                GrantSharedTier(key, previous, entry.at("item"));
            }
            changed = true;
            if (ReadSharedTiers().value(key, 0) <= previous) {
                break;
            }
        }
    }
    if (changed) {
        FleetRuntime_Save();
    }
}

void ReceiveItems() {
    const auto get = Host<const char* (*)(const char*)>("GetDeliveries");
    const char* raw = get != nullptr ? get(kGame) : nullptr;
    if (raw == nullptr) {
        return;
    }
    const auto deliveries = nlohmann::json::parse(raw);
    bool changed = false;
    for (const auto& delivery : deliveries) {
        const std::string key = "fleet:received:" + delivery.at("id").get<std::string>();
        const uint16_t flag = RandoInfCustom_Register(key.c_str());
        if (flag == 0) {
            throw std::runtime_error("could not register Fleet receipt");
        }
        if (RandoInfCustom_Get(S2H_RANDO_INF_SAVE, flag)) {
            continue;
        }
        // Logged before the grant: after a crash, the last line names the item.
        SPDLOG_INFO("[Fleet] Receiving {} from {}", delivery.at("item").get<std::string>(),
                    delivery.at("id").get<std::string>());
        Grant(delivery);
        RandoInfCustom_Set(S2H_RANDO_INF_SAVE, flag, true);
        changed = true;
    }
    if (changed) {
        FleetRuntime_Save();
    }
}

void ReportChecks() {
    const auto report = Host<bool (*)(const char*, const char*)>("ReportObtained");
    if (report == nullptr) {
        return;
    }
    const auto& placements = FleetRuntime_Seed().at(kGame).at("placements");
    static std::unordered_map<std::string, RandoCheckId> checkIds;
    if (checkIds.empty()) {
        for (const auto& [id, definition] : Rando::StaticData::Checks) {
            checkIds[Rando::StaticData::GetCheckDisplayName(id)] = id;
        }
    }
    bool saved = false;
    for (const auto& [check, item] : placements.items()) {
        if (sReported.contains(check)) {
            continue;
        }
        const auto found = checkIds.find(check);
        const RandoCheckId checkId = found != checkIds.end() ? found->second : RC_UNKNOWN;
        const bool obtained = checkId != RC_UNKNOWN && RANDO_SAVE_CHECKS[checkId].obtained;
        if (!obtained) {
            continue;
        }
        if (!saved) {
            FleetRuntime_Save();
            saved = true;
        }
        if (report(kGame, check.c_str())) {
            sReported.insert(check);
        }
    }
}

} // namespace

void FleetRuntime_Initialize() {
    if (!sSession.is_null()) {
        return;
    }
    const auto get = Host<const char* (*)()>("GetSession");
    const char* raw = get != nullptr ? get() : nullptr;
    if (raw == nullptr) {
        return;
    }
    sSession = nlohmann::json::parse(raw);
    S2HCustomItemRandomizer randomizer = {};
    randomizer.structSize = sizeof(randomizer);
    S2HCustomItemDefinition definition = {};
    definition.structSize = sizeof(definition);
    definition.key = "fleet:foreign";
    definition.randomizer = &randomizer;
    definition.presentationFlags = S2H_ITEM_HIDE_FROM_SAVE_EDITOR | S2H_ITEM_HIDE_FROM_KALEIDO;
    definition.getItemText = "You found an item for the other game!";
    definition.displayName = "Cross-game item";

    if (!CustomItemRegistry_Register(&definition)) {
        throw std::runtime_error("could not register Fleet foreign item");
    }
    FleetForeignItems_Register(FleetRuntime_Seed());
    FleetRuntime_InstallRoutes();
    SaveSections_Register(
        "fleet", [] { return nlohmann::json{ { "checkpoint", sCheckpoint } }; },
        [](const nlohmann::json& section) {
            sCheckpoint = section.is_object() ? section.value("checkpoint", -1) : -1;
            if (!IsKnownCheckpoint(sCheckpoint)) {
                sCheckpoint = -1;
            }
        });
}

bool FleetRuntime_HasSession() {
    FleetRuntime_Initialize();
    return sSession.is_object();
}

const nlohmann::json& FleetRuntime_Seed() {
    return sSession.at("seed");
}

int FleetRuntime_Slot() {
    return sSession.is_object() ? sSession.at("slot").get<int>() : -1;
}

void FleetRuntime_Save() {
    if (IsPlaying()) {
        auto& rupees = gSaveContext.save.saveInfo.playerData.rupees;
        rupees =
            static_cast<int16_t>(std::clamp<int>(rupees + gSaveContext.rupeeAccumulator, 0, CUR_CAPACITY(UPG_WALLET)));
        gSaveContext.rupeeAccumulator = 0;
    }
    nlohmann::json saveFile;
    const std::string fileName = SaveManager_GetFileName(gSaveContext.fileNum + 1);
    if (SaveManager_ReadSaveFile(fileName, saveFile) == -2) {
        throw std::runtime_error("cannot replace corrupt MM Fleet save");
    }
    saveFile["version"] = 7;
    saveFile["type"] = "2S2H_SAVE";
    SaveContext owlContext = gSaveContext;
    owlContext.save.isOwlSave = true;
    owlContext.save.shipSaveInfo.pauseSaveEntrance = owlContext.save.entrance;
    saveFile["owlSave"] = owlContext;
    SaveSections_Write(saveFile, true);
    if (!saveFile.contains("newCycleSave")) {
        saveFile["newCycleSave"]["save"] = gSaveContext.save;
        SaveSections_Write(saveFile, false);
    }
    SaveManager_WriteFleetSave(fileName, saveFile);
}

void FleetRuntime_FrameEnd() {
    if (!FleetRuntime_HasSession() || !IsPlaying() || gPlayState->transitionTrigger != TRANS_TRIGGER_OFF) {
        return;
    }
    ReportChecks();
    if (!IsTextboxOpen()) {
        ReceiveItems();
        SynchronizeSharedTiers();
    }
}

void FleetRuntime_InstallRoutes() {
    sRoutes.Install(FleetRuntime_Seed().value("crossDungeonEntrances", nlohmann::json{}));
}

nlohmann::json FleetRuntime_Status() {
    auto received = nlohmann::json::array();
    if (sSession.is_object() && IsPlaying()) {
        const auto get = Host<const char* (*)(const char*)>("GetDeliveries");
        const char* raw = get != nullptr ? get(kGame) : nullptr;
        if (raw != nullptr) {
            for (const auto& delivery : nlohmann::json::parse(raw)) {
                const std::string id = delivery.at("id");
                const std::string key = "fleet:received:" + id;
                if (RandoInfCustom_Get(S2H_RANDO_INF_SAVE, RandoInfCustom_Find(key.c_str()))) {
                    received.push_back(id);
                }
            }
        }
    }
    return { { "received", received },
             { "sharedTiers", sSession.is_object() && IsPlaying() ? ReadSharedTiers() : nlohmann::json{} },
             { "playing", sSession.is_object() && IsPlaying() },
             { "slot", gSaveContext.fileNum },
             { "scene", sSession.is_object() && IsPlaying() ? static_cast<int>(gPlayState->sceneId) : -1 } };
}

bool FleetRuntime_CollectForSmoke(const std::string& check) {
    if (!sSession.is_object() || !sSession.value("runtimeTests", false) || !IsPlaying() ||
        !FleetRuntime_Seed().at(kGame).at("placements").contains(check)) {
        return false;
    }
    const RandoCheckId id = Rando::StaticData::GetCheckIdFromDisplayName(check);
    const std::string expected = FleetForeignItems_KeyForCheck(FleetRuntime_Seed(), check);
    if (id == RC_UNKNOWN || RANDO_SAVE_CHECKS[id].randoItemId != RandoItems_GetRandoGet(expected.c_str())) {
        return false;
    }
    RANDO_SAVE_CHECKS[id].obtained = true;
    RANDO_SAVE_CHECKS[id].cycleObtained = true;
    RANDO_SAVE_CHECKS[id].eligible = false;
    ReportChecks();
    return true;
}

nlohmann::json FleetRuntime_CycleSmoke() {
    if (!sSession.is_object() || !sSession.value("runtimeTests", false) || !IsPlaying()) {
        return { { "preserved", false } };
    }
    const auto original = std::make_unique<SaveContext>(gSaveContext);
    const auto scene = gPlayState->actorCtx.sceneFlags;
    const auto id = RC_STARTING_ITEM_DEKU_MASK;
    const auto oldCheck = RANDO_SAVE_CHECKS[id];
    gSaveContext.save.saveInfo.weekEventReg[0] |= 0x80;
    gPlayState->actorCtx.sceneFlags.chest |= 0x80000000;
    gPlayState->actorCtx.sceneFlags.switches[0] |= 0x40000000;
    RANDO_SAVE_CHECKS[id].cycleObtained = true;
    gSaveContext.save.day = 3;
    Sram_SaveEndOfCycle(gPlayState);
    const bool preserved = (gSaveContext.save.saveInfo.weekEventReg[0] & 0x80) != 0 &&
                           (gPlayState->actorCtx.sceneFlags.chest & 0x80000000) != 0 &&
                           (gPlayState->actorCtx.sceneFlags.switches[0] & 0x40000000) != 0 &&
                           RANDO_SAVE_CHECKS[id].cycleObtained;
    const bool clockReset = gSaveContext.save.day == 0;
    gSaveContext = *original;
    gPlayState->actorCtx.sceneFlags = scene;
    RANDO_SAVE_CHECKS[id] = oldCheck;
    return { { "preserved", preserved }, { "clockReset", clockReset } };
}

int32_t FleetRuntime_Route(int32_t entrance, bool beforeNative) {
    if (!sSession.is_object() || !IsPlaying()) {
        return -1;
    }
    const int32_t canonicalExit = ComboMmCanonicalGrottoExit(entrance, gPlayState->sceneId);
    const bool grottoExit = canonicalExit != entrance;
    if (gSaveContext.respawnFlag != 0 && !(gSaveContext.respawnFlag == 4 && grottoExit)) {
        return -1;
    }
    const int32_t canonical = grottoExit ? canonicalExit : ComboMmCanonicalGrottoLoad(entrance);
    const auto transition = sRoutes.Lookup(beforeNative ? Phase::BeforeNative : Phase::AfterNative, canonical);
    if (!transition) {
        return -1;
    }
    if (transition->to.game == 1) {
        sCheckpoint = sRoutes.RecoveryForArrival(transition->to.index);
        return FleetRuntime_Materialize(transition->to.index);
    }
    sDeparture = transition;
    sTransitionComplete = false;
    return entrance;
}

bool FleetRuntime_TakeDeparture(FleetDestination& destination, std::string& payload) {
    if (!sDeparture || !sTransitionComplete) {
        return false;
    }
    const auto transition = *sDeparture;
    sCheckpoint = transition.checkpoint.index;
    sDeparture.reset();
    sTransitionComplete = false;
    gPlayState->transitionTrigger = TRANS_TRIGGER_OFF;
    gPlayState->transitionMode = TRANS_MODE_OFF;
    gSaveContext.save.entrance = FleetRuntime_Materialize(transition.checkpoint.index);
    FleetRuntime_Save();
    payload = nlohmann::json{
        { "cross", true }, { "slot", FleetRuntime_Slot() }, { "entrance", transition.to.index }
    }.dump();
    destination = { .game = "oot",
                    .entrance = static_cast<uint32_t>(transition.to.index),
                    .payloadJson = payload.c_str() };
    return true;
}

bool FleetRuntime_QueueArrival(int32_t entrance) {
    const bool queued = sRoutes.QueueArrival(entrance);
    if (queued) {
        sSkipTransition = IsPlaying();
        sCheckpoint = sRoutes.RecoveryForArrival(entrance);
    }
    return queued;
}

int32_t FleetRuntime_Materialize(int32_t entrance) {
    return ComboMmMaterializeGrotto(entrance);
}

int32_t FleetRuntime_ConsumeArrival() {
    const int32_t entrance = sRoutes.ConsumeArrival();
    sSkipTransition = false;
    if (entrance < 0) {
        return -1;
    }
    sCheckpoint = sRoutes.RecoveryForArrival(entrance);
    return FleetRuntime_Materialize(entrance);
}

void FleetRuntime_ObserveScene(int32_t scene) {
    const auto* owner = sRoutes.FindEntrance(sCheckpoint);
    if (owner == nullptr) {
        sCheckpoint = -1;
        return;
    }
    bool inside = false;
    if (owner->pool == Pool::Dungeon) {
        inside = scene == SCENE_MITURIN || scene == SCENE_HAKUGIN || scene == SCENE_SEA || scene == SCENE_INISIE_N ||
                 scene == SCENE_INISIE_R || scene == SCENE_MITURIN_BS || scene == SCENE_HAKUGIN_BS ||
                 scene == SCENE_SEA_BS || scene == SCENE_INISIE_BS || scene == SCENE_KYOJINNOMA;
        for (const auto& dungeon : ComboCrossMmCatalogue::ExpandedDungeons()) {
            if (dungeon.entrance == sCheckpoint) {
                inside = inside || Rando::Logic::Regions.at(dungeon.region).sceneId == scene;
            }
        }
    } else if (owner->pool == Pool::Grotto) {
        inside = scene == SCENE_KAKUSIANA;
    } else {
        for (const auto& interior : ComboCrossMmCatalogue::Interiors()) {
            if (interior.entrance == sCheckpoint) {
                inside = Rando::Logic::Regions.at(interior.region).sceneId == scene;
            }
        }
    }
    if (!inside) {
        sCheckpoint = -1;
    }
}

extern "C" int32_t FleetRuntime_RecoveryEntrance(void) {
    return sCheckpoint >= 0 && IsKnownCheckpoint(sCheckpoint) ? FleetRuntime_Materialize(sCheckpoint) : -1;
}

extern "C" int32_t FleetRuntime_ResolveExit(int32_t entrance, int beforeNative) {
    return FleetRuntime_Route(entrance, beforeNative != 0);
}

extern "C" int FleetRuntime_CompleteTransition(void) {
    if (!sDeparture) {
        return 0;
    }
    sTransitionComplete = true;
    return 1;
}

extern "C" void FleetRuntime_CancelTransition(void) {
    sDeparture.reset();
    sTransitionComplete = false;
}

extern "C" int FleetRuntime_EntrancesEnabled(void) {
    return sSession.is_object() && sRoutes.Enabled();
}

extern "C" int32_t FleetRuntime_ResolveTransition(int32_t entrance) {
    if (sSkipTransition) {
        sSkipTransition = false;
        return entrance;
    }
    int32_t resolved = FleetRuntime_Route(entrance, true);
    if (resolved < 0) {
        resolved = FleetRuntime_Route(entrance, false);
    }
    return resolved >= 0 ? resolved : entrance;
}
