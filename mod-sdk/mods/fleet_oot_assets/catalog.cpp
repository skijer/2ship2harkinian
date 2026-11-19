#include "2s2h/ModApi/ModApi.h"
#include "FleetPresentationContext.h"
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <array>
#include <utility>

struct CatalogEntry {
    const char* key;
    const char* name;
    const char* nativeToken;
    int drawId;
};
#include "catalog.inc.h"
static const S2HModApi* sApi;
static const S2HModRequirements sRequirements = { sizeof(S2HModRequirements), nullptr, 0 };
static size_t sRegistered;
static const FleetPresentationContextV1* sContext;
#include "draw_bindings.inc.h"
#include "special_draws.inc.h"
template <size_t Index> static void DrawReward(PlayState* play, S2HGetItemEntry*) {
    if (!RequiresSpecialDraw(sCatalog[Index].nativeToken))
        FleetDrawNative(play, sCatalog[Index].drawId);
}
template <size_t... Index> static auto MakeDrawCallbacks(std::index_sequence<Index...>) {
    return std::array<S2HGetItemDrawFunc, sizeof...(Index)>{ DrawReward<Index>... };
}
static const auto sDrawCallbacks =
    MakeDrawCallbacks(std::make_index_sequence<sizeof(sCatalog) / sizeof(sCatalog[0])>{});

struct CatalogView {
    uint32_t structSize;
    uint32_t schema;
    uint32_t count;
    const char* owner;
    const char* (*keyAt)(uint32_t);
    const char* (*nameAt)(uint32_t);
    bool (*contains)(const char*);
    bool presentationReady;
};
static const char* KeyAt(uint32_t i) {
    return i < sRegistered ? sCatalog[i].key : nullptr;
}
static const char* NameAt(uint32_t i) {
    return i < sRegistered ? sCatalog[i].name : nullptr;
}
static bool Contains(const char* key) {
    if (!key)
        return false;
    for (size_t i = 0; i < sRegistered; ++i)
        if (!std::strcmp(sCatalog[i].key, key))
            return true;
    return false;
}
static CatalogView sView = { sizeof(CatalogView), 1, 0, "oot", KeyAt, NameAt, Contains, false };
static const CatalogView* GetCatalog() {
    return &sView;
}

extern "C" S2H_MOD_EXPORT void ModSetApi(const S2HModApi* api) {
    sApi = api;
}
extern "C" S2H_MOD_EXPORT const S2HModRequirements* ModGetRequirements() {
    return &sRequirements;
}
extern "C" S2H_MOD_EXPORT void ModInit() {
    if (!S2H_MOD_API_HAS(sApi, FindService) || !sApi->FindService)
        return;
    const auto get = reinterpret_cast<FleetGetPresentationContextV1>(sApi->FindService(FLEET_PRESENTATION_SERVICE));
    const auto context = get ? get() : nullptr;
    if (!context || context->structSize < sizeof(*context) || context->version != 1 || !context->receiver ||
        std::strcmp(context->receiver, "mm") || !context->isComboMode || !context->readOwnerLevel ||
        !context->isComboMode())
        return;
    sContext = context;
    if (!S2H_MOD_API_HAS(sApi, RegisterCustomItem) || !sApi->RegisterCustomItem ||
        !S2H_MOD_API_HAS(sApi, PublishService) || !sApi->PublishService)
        return;
    if (sRegistered)
        return;
    for (const auto& item : sCatalog) {
        S2HCustomItemRandomizer randomizer = {};
        randomizer.structSize = sizeof(randomizer);
        randomizer.poolCount = 0;
        randomizer.logicKey = item.key;
        randomizer.hintKey = item.key;
        S2HCustomItemDefinition definition = {};
        definition.structSize = sizeof(definition);
        definition.key = item.key;
        definition.displayName = item.name;
        definition.randomizer = &randomizer;
        definition.getItemEntry.drawFunc = sDrawCallbacks[&item - sCatalog];
        definition.presentationFlags = S2H_ITEM_HIDE_FROM_SAVE_EDITOR | S2H_ITEM_HIDE_FROM_KALEIDO;
        const std::string text = std::string("You found %y") + item.name + "%w!&Sent to Ocarina of Time.";
        definition.getItemText = text.c_str();
        definition.pauseText = item.name;
        if (!sApi->RegisterCustomItem(&definition)) {
            if (S2H_MOD_API_HAS(sApi, Log) && sApi->Log)
                sApi->Log("Fleet foreign catalog registration failed.");
            return;
        }
        ++sRegistered;
    }
    sView.count = static_cast<uint32_t>(sRegistered);
    sApi->PublishService("fleet.oot_assets.catalog.v1", reinterpret_cast<void*>(GetCatalog));
}
