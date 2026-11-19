
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "z64items.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"

#define SMALL_KEY_ICON "__OTR__icon_item_24_static_yar/gQuestIconSmallKeyTex"
#define MODE_OPTION "mm_keyrings.mode"
#define COUNT_OPTION "mm_keyrings.count"

typedef enum {
    KEY_RING_MODE_OFF,
    KEY_RING_MODE_RANDOM,
    KEY_RING_MODE_COUNT,
    KEY_RING_MODE_SELECTION,
} KeyRingMode;

typedef enum {
    KEY_RING_CHOICE_NO,
    KEY_RING_CHOICE_RANDOM,
    KEY_RING_CHOICE_YES,
} KeyRingChoice;

typedef struct {
    const char* key;
    const char* option;
    const char* optionLabel;
    const char* displayName;
    const char* getItemText;
    S2HKeyRing keyRing;
} KeyRingDefinition;

static bool IsKeyRingShuffled(const char* key, uint32_t seed);

static const KeyRingDefinition sKeyRings[] = {
    { "mm.key_ring.woodfall",
      "mm_keyrings.woodfall",
      "Woodfall Key Ring",
      "Woodfall Key Ring",
      "You got the %gWoodfall Key Ring%w!&Every small key of Woodfall Temple is yours.",
      { sizeof(S2HKeyRing), DUNGEON_SCENE_INDEX_WOODFALL_TEMPLE, 1, IsKeyRingShuffled } },
    { "mm.key_ring.snowhead",
      "mm_keyrings.snowhead",
      "Snowhead Key Ring",
      "Snowhead Key Ring",
      "You got the %bSnowhead Key Ring%w!&Every small key of Snowhead Temple is yours.",
      { sizeof(S2HKeyRing), DUNGEON_SCENE_INDEX_SNOWHEAD_TEMPLE, 3, IsKeyRingShuffled } },
    { "mm.key_ring.great_bay",
      "mm_keyrings.great_bay",
      "Great Bay Key Ring",
      "Great Bay Key Ring",
      "You got the %bGreat Bay Key Ring%w!&Every small key of Great Bay Temple is yours.",
      { sizeof(S2HKeyRing), DUNGEON_SCENE_INDEX_GREAT_BAY_TEMPLE, 1, IsKeyRingShuffled } },
    { "mm.key_ring.stone_tower",
      "mm_keyrings.stone_tower",
      "Stone Tower Key Ring",
      "Stone Tower Key Ring",
      "You got the %yStone Tower Key Ring%w!&Every small key of Stone Tower Temple is yours.",
      { sizeof(S2HKeyRing), DUNGEON_SCENE_INDEX_STONE_TOWER_TEMPLE, 4, IsKeyRingShuffled } },
};

static const char* const sModes[] = { "Off", "Random", "Count", "Selection" };
static const char* const sCounts[] = { "0", "1", "2", "3", "4" };
static const char* const sChoices[] = { "No", "Random", "Yes" };

static const S2HModApi* sApi;

static uint32_t HashSeed(uint32_t seed, uint32_t salt) {
    uint32_t hash = 2166136261u;
    const uint32_t words[] = { seed, salt };
    for (u32 i = 0; i < ARRAY_COUNT(words); i++) {
        for (u32 shift = 0; shift < 32; shift += 8) {
            hash = (hash ^ ((words[i] >> shift) & 0xFF)) * 16777619u;
        }
    }
    return hash;
}

static u32 GetShuffledRank(u32 ringIndex, uint32_t seed) {
    u32 order[ARRAY_COUNT(sKeyRings)];
    for (u32 i = 0; i < ARRAY_COUNT(order); i++) {
        order[i] = i;
    }
    for (u32 i = ARRAY_COUNT(order) - 1; i > 0; i--) {
        const u32 j = HashSeed(seed, i) % (i + 1);
        const u32 swapped = order[i];
        order[i] = order[j];
        order[j] = swapped;
    }
    for (u32 rank = 0; rank < ARRAY_COUNT(order); rank++) {
        if (order[rank] == ringIndex) {
            return rank;
        }
    }
    return ARRAY_COUNT(order);
}

static bool IsChosenForSeed(u32 ringIndex, uint32_t seed) {
    switch (sApi->GetRandoOption(sKeyRings[ringIndex].option)) {
        case KEY_RING_CHOICE_YES:
            return true;
        case KEY_RING_CHOICE_RANDOM:
            return (HashSeed(seed, 0x100 + ringIndex) & 1) == 0;
        case KEY_RING_CHOICE_NO:
            return false;
        default:
            fprintf(stderr, "[mm_keyrings] Unknown choice for %s\n", sKeyRings[ringIndex].option);
            return false;
    }
}

static bool IsKeyRingShuffled(const char* key, uint32_t seed) {
    u32 ringIndex = 0;
    while (ringIndex < ARRAY_COUNT(sKeyRings) && strcmp(sKeyRings[ringIndex].key, key) != 0) {
        ringIndex++;
    }
    if (ringIndex == ARRAY_COUNT(sKeyRings)) {
        return false;
    }
    switch (sApi->GetRandoOption(MODE_OPTION)) {
        case KEY_RING_MODE_OFF:
            return false;
        case KEY_RING_MODE_RANDOM:
            return GetShuffledRank(ringIndex, seed) < HashSeed(seed, 0x200) % ARRAY_COUNT(sKeyRings);
        case KEY_RING_MODE_COUNT:
            return GetShuffledRank(ringIndex, seed) < sApi->GetRandoOption(COUNT_OPTION);
        case KEY_RING_MODE_SELECTION:
            return IsChosenForSeed(ringIndex, seed);
        default:
            fprintf(stderr, "[mm_keyrings] Unknown key ring mode\n");
            return false;
    }
}

static void DrawKeyRingPickup(PlayState* play, S2HGetItemEntry* entry) {
    (void)entry;
    GetItem_Draw(play, GID_KEY_SMALL);
}

static void RegisterOptions(void) {
    const S2HModRandoOption mode = { sizeof(S2HModRandoOption),
                                     MODE_OPTION,
                                     "Key Rings",
                                     "Key rings hold every small key of one temple and replace them in the pool.\n\n"
                                     "Random - A random number of temples get one.\n"
                                     "Count - That many temples, chosen at random, get one.\n"
                                     "Selection - Each temple's own setting decides.",
                                     sModes,
                                     ARRAY_COUNT(sModes),
                                     KEY_RING_MODE_OFF,
                                     S2H_MOD_RANDO_WIDGET_COMBOBOX };
    const S2HModRandoOption count = { sizeof(S2HModRandoOption),
                                      COUNT_OPTION,
                                      "Key Ring Count",
                                      "How many temples get a key ring with Key Rings set to Count.",
                                      sCounts,
                                      ARRAY_COUNT(sCounts),
                                      ARRAY_COUNT(sCounts) - 1,
                                      S2H_MOD_RANDO_WIDGET_SLIDER };
    sApi->RegisterRandoOption(&mode);
    sApi->RegisterRandoOption(&count);
    for (u32 i = 0; i < ARRAY_COUNT(sKeyRings); i++) {
        const S2HModRandoOption choice = { sizeof(S2HModRandoOption),
                                           sKeyRings[i].option,
                                           sKeyRings[i].optionLabel,
                                           "Whether this temple gets a key ring with Key Rings set to Selection.",
                                           sChoices,
                                           ARRAY_COUNT(sChoices),
                                           KEY_RING_CHOICE_NO,
                                           S2H_MOD_RANDO_WIDGET_COMBOBOX };
        sApi->RegisterRandoOption(&choice);
    }
}

// The host copies the definition and its randomizer data, so both may live on the stack.
static void RegisterKeyRing(const KeyRingDefinition* ring) {
    S2HCustomItemRandomizer randomizer = { 0 };
    randomizer.structSize = sizeof(randomizer);
    randomizer.flags = S2H_CUSTOM_ITEM_RANDO_ADVANCEMENT;
    randomizer.type = S2H_CUSTOM_ITEM_TYPE_ITEM;
    randomizer.poolCount = 1;
    randomizer.logicKey = ring->key;
    randomizer.hintKey = ring->key;
    randomizer.keyRing = &ring->keyRing;

    S2HCustomItemDefinition definition = Z64Items_Define(ring->key, SMALL_KEY_ICON, NULL);
    definition.presentationFlags = S2H_ITEM_HIDE_FROM_KALEIDO | S2H_ITEM_HIDE_FROM_SAVE_EDITOR;
    definition.displayName = ring->displayName;
    definition.getItemEntry.drawFunc = DrawKeyRingPickup;
    Z64Items_SetTextbox(&definition, ring->getItemText);
    Z64Items_SetLogic(&definition, &randomizer);
    if (!Z64Items_Register(sApi, &definition)) {
        fprintf(stderr, "[mm_keyrings] Could not register %s\n", ring->key);
    }
}

S2H_MOD_EXPORT void ModSetApi(const S2HModApi* api) {
    sApi = api;
}

S2H_MOD_EXPORT void ModInit(void) {
    if (!S2H_MOD_API_HAS(sApi, RegisterRandoOption) || !S2H_MOD_API_HAS(sApi, RegisterCustomItem)) {
        fprintf(stderr, "[mm_keyrings] The host lacks the randomizer item API; the mod stays off\n");
        return;
    }
    RegisterOptions();
    for (u32 i = 0; i < ARRAY_COUNT(sKeyRings); i++) {
        RegisterKeyRing(&sKeyRings[i]);
    }
}
