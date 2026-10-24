#include "2s2h/ModApi/ProgressiveItems/ProgressiveItems.h"
#include "2s2h/Network/Anchor/Anchor.h"
#include "2s2h/ModApi/CustomItemRegistry/CustomItemRegistry.h"
#include "2s2h/ModApi/CustomEquipRegistry/CustomEquipRegistry.h"
#include "2s2h/ModApi/RandoInfCustom/RandoInfCustom.h"
void Anchor::SendPacket_CustomProgress(const std::string& kind, const char* key, bool state) {
    if (!IsSaveLoaded() || isProcessingIncomingPacket || !roomState.syncItemsAndFlags || key == nullptr) {
        return;
    }

    nlohmann::json payload;
    payload["type"] = CUSTOM_PROGRESS;
    payload["targetTeamId"] = CVarGetString("gNetwork.Anchor.TeamId", "default");
    payload["addToQueue"] = true;
    payload["kind"] = kind;
    payload["key"] = key;
    payload["state"] = state;

    QueueOutgoingPacket(payload);
}

void Anchor::HandlePacket_CustomProgress(nlohmann::json payload) {
    if (!IsSaveLoaded() || !roomState.syncItemsAndFlags) {
        return;
    }

    const std::string kind = payload.at("kind").get<std::string>();
    const std::string key = payload.at("key").get<std::string>();
    const bool state = payload.at("state").get<bool>();

    if (kind == "item") {
        CustomItemRegistry_SetOwned(key.c_str(), state);
    } else if (kind == "receive") {
        CustomItemRegistry_ReceiveGetItem(key.c_str());
    } else if (kind == "equipment") {
        CustomEquipRegistry_SetOwned(key.c_str(), state);
    } else if (kind == "flag") {
        RandoInfCustom_SetByKey(key.c_str(), state);
    } else {
        SPDLOG_WARN("[Anchor] CUSTOM_PROGRESS: unknown kind '{}' for '{}'", kind, key);
    }
}

nlohmann::json Anchor::PrepCustomProgressState() {
    return nlohmann::json{
        { "item", CustomItemRegistry_ListOwned() },
        { "equipment", CustomEquipRegistry_ListOwned() },
        { "flag", RandoInfCustom_ListSet() },
        { "progressive", ProgressiveItems_NetworkState() },
    };
}

// Merged like the vanilla flags: a team save only ever adds progress.
void Anchor::ApplyCustomProgressState(const nlohmann::json& state) {
    ProgressiveItems_MergeNetworkState(state.value("progressive", nlohmann::json::array()));
    for (const std::string& key : state.value("item", std::vector<std::string>{})) {
        CustomItemRegistry_SetOwned(key.c_str(), true);
    }
    for (const std::string& key : state.value("equipment", std::vector<std::string>{})) {
        CustomEquipRegistry_SetOwned(key.c_str(), true);
    }
    for (const std::string& key : state.value("flag", std::vector<std::string>{})) {
        RandoInfCustom_SetByKey(key.c_str(), true);
    }
}
