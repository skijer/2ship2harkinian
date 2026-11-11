/**
 * Golden hearts past MM's twenty: each golden heart is drawn over a red one and takes damage first. They live
 * in the save's mod storage, so the vanilla health fields and their twenty-heart cap stay untouched.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "z64items.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "2s2h/ModApi/RandoInfCustom/RandoInfCustom.h"

#define MOD_NAME "golden_hearts"
#define POOL_OPTION "oot.golden_hearts"
#define STARTING_OPTION "oot.starting_golden_hearts"
// Fleet's shared Golden Hearts family names MM's copy by this key.
#define GOLDEN_HEART_KEY "nei.heart.golden"
#define CAPACITY_KEY "capacity"
#define HEALTH_KEY "health"
#define MAX_GOLDEN_HEARTS 20
#define HEART_HEALTH 16

static const char* const sRequiredHooks[] = { "OnPlayerHealthChange", "OnCustomItemReceive", "OnLoadFile",
                                              "OnInterfaceDrawEnd" };
static const S2HModRequirements sRequirements = { sizeof(S2HModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const char sHeartFullTex[] = "__OTR__parameter_static/gHeartFullTex";
static const char* const sHeartFractionTex[] = {
    NULL,
    "__OTR__parameter_static/gHeartQuarterTex",
    "__OTR__parameter_static/gHeartHalfTex",
    "__OTR__parameter_static/gHeartThreeQuarterTex",
};

static const S2HModApi* sApi;
static u8 sCapacity;
static s16 sHealth;

static void Store(void) {
    sApi->StorageSet(MOD_NAME, CAPACITY_KEY, &sCapacity, sizeof(sCapacity));
    sApi->StorageSet(MOD_NAME, HEALTH_KEY, &sHealth, sizeof(sHealth));
}

static s16 GetMaxHealth(void) {
    return sCapacity * HEART_HEALTH;
}

// A new randomizer file starts with the seed's golden hearts full; an older file keeps what it stored.
static void LoadGoldenHearts(s32 fileNum) {
    sCapacity = 0;
    sHealth = 0;
    if (sApi->StorageHas(MOD_NAME, CAPACITY_KEY)) {
        sApi->StorageGet(MOD_NAME, CAPACITY_KEY, &sCapacity, sizeof(sCapacity));
        sApi->StorageGet(MOD_NAME, HEALTH_KEY, &sHealth, sizeof(sHealth));
        return;
    }
    if (gSaveContext.save.shipSaveInfo.saveType == SAVETYPE_RANDO) {
        sCapacity = MIN(sApi->GetRandoOption(STARTING_OPTION), MAX_GOLDEN_HEARTS);
        sHealth = GetMaxHealth();
        Store();
    }
}

static void ReceiveGoldenHeart(const char* key) {
    if (RandoInfCustom_IsLogicSimulation() || strcmp(key, GOLDEN_HEART_KEY) != 0 ||
        sCapacity >= MAX_GOLDEN_HEARTS) {
        return;
    }
    sCapacity++;
    sHealth = GetMaxHealth();
    Store();
}

// Health_ChangeBy runs this before its death check, so restored red health can still save Link.
static void ShieldWithGoldenHearts(s16 amount) {
    s16* health = &gSaveContext.save.saveInfo.playerData.health;
    const s16 capacity = gSaveContext.save.saveInfo.playerData.healthCapacity;

    if (amount < 0 && sHealth > 0) {
        const s16 absorbed = MIN(-amount, sHealth);
        sHealth -= absorbed;
        *health = MIN(*health + absorbed, capacity);
        Store();
    } else if (amount > 0 && *health >= capacity && sHealth < GetMaxHealth()) {
        sHealth = MIN(sHealth + amount, GetMaxHealth());
        Store();
    }
}

static void DrawHeart(PlayState* play, s32 index, const char* texture) {
    const f32 posX = 30.0f + 10.0f * (index % 10);
    const f32 posY = 26.0f + 10.0f * (index / 10);
    const f32 halfSize = 8.0f * 0.68f;
    const s32 step = (s32)((1.0f / 0.68f) * (1 << 10));

    OPEN_DISPS(play->state.gfxCtx);
    gDPLoadTextureBlock(OVERLAY_DISP++, texture, G_IM_FMT_IA, G_IM_SIZ_8b, 16, 16, 0, G_TX_NOMIRROR | G_TX_WRAP,
                        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gSPTextureRectangle(OVERLAY_DISP++, (s32)((posX - halfSize) * 4), (s32)((posY - halfSize) * 4),
                        (s32)((posX + halfSize) * 4), (s32)((posY + halfSize) * 4), G_TX_RENDERTILE, 0, 0, step,
                        step);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawGoldenHearts(PlayState* play) {
    if (sHealth <= 0 || play->interfaceCtx.healthAlpha == 0) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL39_Overlay(play->state.gfxCtx);
    gDPSetCombineLERP(OVERLAY_DISP++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0, PRIMITIVE,
                      ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0);
    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 215, 40, play->interfaceCtx.healthAlpha);
    gDPSetEnvColor(OVERLAY_DISP++, 140, 90, 0, 255);
    CLOSE_DISPS(play->state.gfxCtx);

    const s32 fullHearts = sHealth / HEART_HEALTH;
    const s32 quarters = (sHealth % HEART_HEALTH) / (HEART_HEALTH / 4);
    for (s32 i = 0; i < fullHearts; i++) {
        DrawHeart(play, i, sHeartFullTex);
    }
    if (quarters > 0) {
        DrawHeart(play, fullHearts, sHeartFractionTex[quarters]);
    }
}

static const char* const sCounts[] = { "0",  "1",  "2",  "3",  "4",  "5",  "6",  "7",  "8",  "9", "10",
                                       "11", "12", "13", "14", "15", "16", "17", "18", "19", "20" };

static const S2HModRandoOption sOptions[] = {
    { sizeof(S2HModRandoOption), POOL_OPTION, "Golden Heart Containers",
      "Golden Heart Containers added to the item pool. Each adds a golden heart over the red ones.", sCounts,
      ARRAY_COUNT(sCounts), 0, S2H_MOD_RANDO_WIDGET_SLIDER },
    { sizeof(S2HModRandoOption), STARTING_OPTION, "Starting Golden Hearts",
      "Golden hearts Link starts with, past the twenty red ones.", sCounts, ARRAY_COUNT(sCounts), 0,
      S2H_MOD_RANDO_WIDGET_SLIDER },
};

static void RegisterGoldenHeartContainer(void) {
    static const S2HCustomItemRandomizer pool = {
        .structSize = sizeof(S2HCustomItemRandomizer),
        .type = S2H_CUSTOM_ITEM_TYPE_ITEM,
        .poolCount = 0,
        .logicKey = GOLDEN_HEART_KEY,
        .hintKey = GOLDEN_HEART_KEY,
        .poolCountOption = POOL_OPTION,
    };
    S2HCustomItemDefinition definition =
        Z64Items_Define(GOLDEN_HEART_KEY, "__OTR__textures/icon_item_custom/gItemIconGoldenHeartTex", NULL);
    definition.presentationFlags = S2H_ITEM_HIDE_FROM_KALEIDO;
    definition.displayName = "Golden Heart Container";
    definition.getItemScale = 1.0f;
    Z64Items_SetTextbox(&definition, "You got a %yGolden Heart Container%w!&A golden heart now guards a red one.");
    Z64Items_SetGetItemModel(&definition, "__OTR__objects/object_gi_hearts/gGiHeartContainerDL");
    Z64Items_SetLogic(&definition, &pool);
    Z64Items_Register(sApi, &definition);
}

S2H_MOD_EXPORT void ModSetApi(const S2HModApi* api) {
    sApi = api;
}

S2H_MOD_EXPORT const S2HModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

S2H_MOD_EXPORT void ModInit(void) {
    if (!S2H_MOD_API_HAS(sApi, RegisterRandoOption) || !S2H_MOD_API_HAS(sApi, StorageSet)) {
        fprintf(stderr, "[golden_hearts] The host lacks the randomizer or storage API; the mod stays off\n");
        return;
    }
    for (u32 i = 0; i < ARRAY_COUNT(sOptions); i++) {
        sApi->RegisterRandoOption(&sOptions[i]);
    }
    RegisterGoldenHeartContainer();
    S2H_REGISTER_HOOK(sApi, OnLoadFile, LoadGoldenHearts);
    S2H_REGISTER_HOOK(sApi, OnCustomItemReceive, ReceiveGoldenHeart);
    S2H_REGISTER_HOOK(sApi, OnPlayerHealthChange, ShieldWithGoldenHearts);
    S2H_REGISTER_HOOK(sApi, OnInterfaceDrawEnd, DrawGoldenHearts);
}
