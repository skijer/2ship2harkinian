#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "2s2h/BenPort.h"
#include "2s2h/ModApi/ModApi.h"
#include "2s2h/Rando/Types.h"
#include "overlays/actors/ovl_En_Kanban/z_en_kanban.h"

#define SHUFFLE_SIGNS_OPTION "nei.shuffle_signs"
#define PLACEMENT_RADIUS 50.0f
#define SPARKLE_INTERVAL 4

typedef enum {
    SIGNS_OFF,
    SIGNS_OVERWORLD,
    SIGNS_DUNGEONS,
    SIGNS_ALL,
} SignShuffleMode;

#define OVERWORLD_SIGN ((1u << SIGNS_OVERWORLD) | (1u << SIGNS_ALL))

typedef struct {
    const char* key;
    const char* name;
    int16_t scene;
    const char* region;
} SignCheck;

// Scene variants (spring, cleared swamp, setups) place the same sign again, sometimes a few units off.
typedef struct {
    const char* key;
    int16_t scene;
    Vec3f pos;
} SignPlacement;

static const char* const sRequiredHooks[] = { "OnActorUpdate" };
static const S2HModRequirements sRequirements = { sizeof(S2HModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const S2HModApi* sApi;

static const SignCheck sChecks[] = {
    { "nei.sign.termina_field", "Termina Field Sign", SCENE_00KEIKOKU, "RR_TERMINA_FIELD" },
    { "nei.sign.clock_town_east", "East Clock Town Sign", SCENE_TOWN, "RR_CLOCK_TOWN_EAST" },
    { "nei.sign.clock_town_north", "North Clock Town Sign", SCENE_BACKTOWN, "RR_CLOCK_TOWN_NORTH" },
    { "nei.sign.doggy_racetrack", "Doggy Racetrack Sign", SCENE_F01_B, "RR_DOGGY_RACETRACK" },
    { "nei.sign.gorman_track", "Gorman Track Sign", SCENE_KOEPONARACE, "RR_GORMAN_TRACK" },
    { "nei.sign.road_to_swamp_1", "Road to Southern Swamp Sign 1", SCENE_24KEMONOMITI, "RR_ROAD_TO_SOUTHERN_SWAMP" },
    { "nei.sign.road_to_swamp_2", "Road to Southern Swamp Sign 2", SCENE_24KEMONOMITI, "RR_ROAD_TO_SOUTHERN_SWAMP" },
    { "nei.sign.swamp_north_1", "Southern Swamp North Sign 1", SCENE_20SICHITAI, "RR_SOUTHERN_SWAMP_NORTH" },
    { "nei.sign.swamp_north_2", "Southern Swamp North Sign 2", SCENE_20SICHITAI, "RR_SOUTHERN_SWAMP_NORTH" },
    { "nei.sign.swamp_woods", "Southern Swamp Woods Sign", SCENE_20SICHITAI, "RR_SOUTHERN_SWAMP_NEAR_WOODS" },
    { "nei.sign.swamp_south_1", "Southern Swamp South Sign 1", SCENE_20SICHITAI, "RR_SOUTHERN_SWAMP_SOUTH" },
    { "nei.sign.swamp_south_2", "Southern Swamp South Sign 2", SCENE_20SICHITAI, "RR_SOUTHERN_SWAMP_SOUTH" },
    { "nei.sign.swamp_spider_house", "Swamp Spider House Sign", SCENE_KINSTA1,
      "RR_SWAMP_SPIDER_HOUSE_CENTER_ROOM_LOWER" },
    { "nei.sign.deku_palace_1", "Deku Palace Sign 1", SCENE_22DEKUCITY, "RR_DEKU_PALACE_OUTSIDE" },
    { "nei.sign.deku_palace_2", "Deku Palace Sign 2", SCENE_22DEKUCITY, "RR_DEKU_PALACE_OUTSIDE" },
    { "nei.sign.deku_palace_3", "Deku Palace Sign 3", SCENE_22DEKUCITY, "RR_DEKU_PALACE_OUTSIDE" },
    { "nei.sign.woods_of_mystery_1", "Woods of Mystery Sign 1", SCENE_26SARUNOMORI, "RR_WOODS_OF_MYSTERY" },
    { "nei.sign.woods_of_mystery_2", "Woods of Mystery Sign 2", SCENE_26SARUNOMORI, "RR_WOODS_OF_MYSTERY" },
    { "nei.sign.woods_of_mystery_3", "Woods of Mystery Sign 3", SCENE_26SARUNOMORI, "RR_WOODS_OF_MYSTERY" },
    { "nei.sign.woods_of_mystery_4", "Woods of Mystery Sign 4", SCENE_26SARUNOMORI, "RR_WOODS_OF_MYSTERY" },
    { "nei.sign.woods_of_mystery_5", "Woods of Mystery Sign 5", SCENE_26SARUNOMORI, "RR_WOODS_OF_MYSTERY" },
    { "nei.sign.woods_of_mystery_6", "Woods of Mystery Sign 6", SCENE_26SARUNOMORI, "RR_WOODS_OF_MYSTERY" },
    { "nei.sign.woods_of_mystery_7", "Woods of Mystery Sign 7", SCENE_26SARUNOMORI, "RR_WOODS_OF_MYSTERY" },
    { "nei.sign.woods_of_mystery_8", "Woods of Mystery Sign 8", SCENE_26SARUNOMORI, "RR_WOODS_OF_MYSTERY" },
    { "nei.sign.woods_of_mystery_9", "Woods of Mystery Sign 9", SCENE_26SARUNOMORI, "RR_WOODS_OF_MYSTERY" },
    { "nei.sign.path_to_mountain_village", "Path to Mountain Village Sign", SCENE_13HUBUKINOMITI,
      "RR_PATH_TO_MOUNTAIN_VILLAGE_LOWER" },
    { "nei.sign.mountain_village_1", "Mountain Village Sign 1", SCENE_10YUKIYAMANOMURA, "RR_MOUNTAIN_VILLAGE" },
    { "nei.sign.mountain_village_2", "Mountain Village Sign 2", SCENE_10YUKIYAMANOMURA, "RR_MOUNTAIN_VILLAGE" },
    { "nei.sign.mountain_village_3", "Mountain Village Sign 3", SCENE_10YUKIYAMANOMURA, "RR_MOUNTAIN_VILLAGE" },
    { "nei.sign.mountain_village_4", "Mountain Village Sign 4", SCENE_10YUKIYAMANOMURA, "RR_MOUNTAIN_VILLAGE" },
    { "nei.sign.mountain_village_5", "Mountain Village Sign 5", SCENE_10YUKIYAMANOMURA, "RR_MOUNTAIN_VILLAGE" },
    { "nei.sign.path_to_goron_village", "Path to Goron Village Sign", SCENE_17SETUGEN, "RR_PATH_TO_GORON_VILLAGE" },
    { "nei.sign.goron_village_1", "Goron Village Sign 1", SCENE_11GORONNOSATO, "RR_GORON_VILLAGE" },
    { "nei.sign.goron_village_2", "Goron Village Sign 2", SCENE_11GORONNOSATO, "RR_GORON_VILLAGE" },
    { "nei.sign.goron_village_3", "Goron Village Sign 3", SCENE_11GORONNOSATO, "RR_GORON_VILLAGE" },
    { "nei.sign.path_to_snowhead_lower", "Path to Snowhead Lower Sign", SCENE_14YUKIDAMANOMITI,
      "RR_PATH_TO_SNOWHEAD_LOWER" },
    { "nei.sign.path_to_snowhead_upper", "Path to Snowhead Upper Sign", SCENE_14YUKIDAMANOMITI,
      "RR_PATH_TO_SNOWHEAD_UPPER" },
    { "nei.sign.snowhead", "Snowhead Sign", SCENE_12HAKUGINMAE, "RR_SNOWHEAD_NEAR_PATH" },
    { "nei.sign.great_bay_coast_cliff", "Great Bay Coast Cliff Sign", SCENE_30GYOSON, "RR_GREAT_BAY_COAST_CLIFFSIDE" },
    { "nei.sign.great_bay_coast_1", "Great Bay Coast Sign 1", SCENE_30GYOSON, "RR_GREAT_BAY_COAST" },
    { "nei.sign.great_bay_coast_2", "Great Bay Coast Sign 2", SCENE_30GYOSON, "RR_GREAT_BAY_COAST" },
    { "nei.sign.great_bay_coast_3", "Great Bay Coast Sign 3", SCENE_30GYOSON, "RR_GREAT_BAY_COAST" },
    { "nei.sign.great_bay_coast_4", "Great Bay Coast Sign 4", SCENE_30GYOSON, "RR_GREAT_BAY_COAST" },
    { "nei.sign.great_bay_coast_5", "Great Bay Coast Sign 5", SCENE_30GYOSON, "RR_GREAT_BAY_COAST" },
    { "nei.sign.great_bay_coast_6", "Great Bay Coast Sign 6", SCENE_30GYOSON, "RR_GREAT_BAY_COAST" },
    { "nei.sign.zora_cape_1", "Zora Cape Sign 1", SCENE_31MISAKI, "RR_ZORA_CAPE" },
    { "nei.sign.zora_cape_2", "Zora Cape Sign 2", SCENE_31MISAKI, "RR_ZORA_CAPE" },
    { "nei.sign.zora_cape_3", "Zora Cape Sign 3", SCENE_31MISAKI, "RR_ZORA_CAPE" },
    { "nei.sign.ikana_canyon_upper_1", "Ikana Canyon Upper Sign 1", SCENE_IKANA, "RR_IKANA_CANYON_UPPER" },
    { "nei.sign.ikana_canyon_upper_2", "Ikana Canyon Upper Sign 2", SCENE_IKANA, "RR_IKANA_CANYON_UPPER" },
    { "nei.sign.ikana_canyon_upper_3", "Ikana Canyon Upper Sign 3", SCENE_IKANA, "RR_IKANA_CANYON_UPPER" },
    { "nei.sign.ikana_canyon_upper_4", "Ikana Canyon Upper Sign 4", SCENE_IKANA, "RR_IKANA_CANYON_UPPER" },
    { "nei.sign.ikana_canyon_lower_1", "Ikana Canyon Lower Sign 1", SCENE_IKANA, "RR_IKANA_CANYON_LOWER" },
    { "nei.sign.ikana_canyon_lower_2", "Ikana Canyon Lower Sign 2", SCENE_IKANA, "RR_IKANA_CANYON_LOWER" },
    { "nei.sign.ikana_canyon_lower_3", "Ikana Canyon Lower Sign 3", SCENE_IKANA, "RR_IKANA_CANYON_LOWER" },
};

static const SignPlacement sPlacements[] = {
    { "nei.sign.termina_field", SCENE_00KEIKOKU, { -3250, -222, 4226 } },
    { "nei.sign.clock_town_east", SCENE_TOWN, { 1130, 372, -1163 } },
    { "nei.sign.clock_town_north", SCENE_BACKTOWN, { -917, 292, -1651 } },
    { "nei.sign.doggy_racetrack", SCENE_F01_B, { -4004, 132, 1132 } },
    { "nei.sign.gorman_track", SCENE_KOEPONARACE, { -1576, -116, 4244 } },
    { "nei.sign.road_to_swamp_1", SCENE_24KEMONOMITI, { 980, -182, 2136 } },
    { "nei.sign.road_to_swamp_2", SCENE_24KEMONOMITI, { 2941, -182, 2603 } },
    { "nei.sign.swamp_north_1", SCENE_20SICHITAI, { -174, 8, -570 } },
    { "nei.sign.swamp_north_1", SCENE_20SICHITAI2, { -174, 8, -570 } },
    { "nei.sign.swamp_north_2", SCENE_20SICHITAI, { 2813, 57, 2414 } },
    { "nei.sign.swamp_woods", SCENE_20SICHITAI, { -1103, 12, 1972 } },
    { "nei.sign.swamp_woods", SCENE_20SICHITAI2, { -1103, 12, 1972 } },
    { "nei.sign.swamp_south_1", SCENE_20SICHITAI, { 3853, 19, -1895 } },
    { "nei.sign.swamp_south_1", SCENE_20SICHITAI2, { 3853, 19, -1895 } },
    { "nei.sign.swamp_south_2", SCENE_20SICHITAI, { 3989, 15, -1103 } },
    { "nei.sign.swamp_south_2", SCENE_20SICHITAI2, { 3989, 15, -1103 } },
    { "nei.sign.swamp_spider_house", SCENE_KINSTA1, { 0, -20, -965 } },
    { "nei.sign.deku_palace_1", SCENE_22DEKUCITY, { -94, 0, 1764 } },
    { "nei.sign.deku_palace_2", SCENE_22DEKUCITY, { 58, 0, 1445 } },
    { "nei.sign.deku_palace_3", SCENE_22DEKUCITY, { 98, 0, 1766 } },
    { "nei.sign.woods_of_mystery_1", SCENE_26SARUNOMORI, { -1090, 0, 877 } },
    { "nei.sign.woods_of_mystery_2", SCENE_26SARUNOMORI, { -880, 0, 1090 } },
    { "nei.sign.woods_of_mystery_3", SCENE_26SARUNOMORI, { -662, 0, 877 } },
    { "nei.sign.woods_of_mystery_4", SCENE_26SARUNOMORI, { -880, 0, -670 } },
    { "nei.sign.woods_of_mystery_5", SCENE_26SARUNOMORI, { -878, 0, -1092 } },
    { "nei.sign.woods_of_mystery_6", SCENE_26SARUNOMORI, { -665, 0, -880 } },
    { "nei.sign.woods_of_mystery_7", SCENE_26SARUNOMORI, { -1976, 0, 1 } },
    { "nei.sign.woods_of_mystery_8", SCENE_26SARUNOMORI, { -1758, 0, 216 } },
    { "nei.sign.woods_of_mystery_9", SCENE_26SARUNOMORI, { -1543, 0, -1 } },
    { "nei.sign.path_to_mountain_village", SCENE_13HUBUKINOMITI, { 406, -395, 4619 } },
    { "nei.sign.mountain_village_1", SCENE_10YUKIYAMANOMURA, { -1064, 8, -79 } },
    { "nei.sign.mountain_village_1", SCENE_10YUKIYAMANOMURA2, { -1064, 8, -79 } },
    { "nei.sign.mountain_village_2", SCENE_10YUKIYAMANOMURA, { -421, 8, 336 } },
    { "nei.sign.mountain_village_2", SCENE_10YUKIYAMANOMURA2, { -421, 8, 336 } },
    { "nei.sign.mountain_village_3", SCENE_10YUKIYAMANOMURA, { -46, -9, 723 } },
    { "nei.sign.mountain_village_3", SCENE_10YUKIYAMANOMURA2, { -46, -25, 723 } },
    { "nei.sign.mountain_village_4", SCENE_10YUKIYAMANOMURA, { 345, 8, -305 } },
    { "nei.sign.mountain_village_4", SCENE_10YUKIYAMANOMURA2, { 345, 8, -305 } },
    { "nei.sign.mountain_village_5", SCENE_10YUKIYAMANOMURA, { 1761, 15, 986 } },
    { "nei.sign.mountain_village_5", SCENE_10YUKIYAMANOMURA2, { 1761, 15, 986 } },
    { "nei.sign.path_to_goron_village", SCENE_17SETUGEN, { -1209, 440, -700 } },
    { "nei.sign.path_to_goron_village", SCENE_17SETUGEN2, { -1209, 440, -700 } },
    { "nei.sign.goron_village_1", SCENE_11GORONNOSATO, { -987, -200, 137 } },
    { "nei.sign.goron_village_1", SCENE_11GORONNOSATO2, { -982, -200, 171 } },
    { "nei.sign.goron_village_2", SCENE_11GORONNOSATO, { -943, -200, -1866 } },
    { "nei.sign.goron_village_2", SCENE_11GORONNOSATO2, { -942, -200, -1866 } },
    { "nei.sign.goron_village_3", SCENE_11GORONNOSATO, { 1153, -200, -736 } },
    { "nei.sign.goron_village_3", SCENE_11GORONNOSATO2, { 1241, -200, -741 } },
    { "nei.sign.path_to_snowhead_lower", SCENE_14YUKIDAMANOMITI, { 1212, -14, 1273 } },
    { "nei.sign.path_to_snowhead_upper", SCENE_14YUKIDAMANOMITI, { -1042, 360, -2527 } },
    { "nei.sign.snowhead", SCENE_12HAKUGINMAE, { -757, 211, 6196 } },
    { "nei.sign.great_bay_coast_cliff", SCENE_30GYOSON, { -3088, 240, 4011 } },
    { "nei.sign.great_bay_coast_1", SCENE_30GYOSON, { -2398, 15, 649 } },
    { "nei.sign.great_bay_coast_2", SCENE_30GYOSON, { -1329, 62, 2347 } },
    { "nei.sign.great_bay_coast_3", SCENE_30GYOSON, { -651, 13, 3981 } },
    { "nei.sign.great_bay_coast_4", SCENE_30GYOSON, { -490, 75, 6012 } },
    { "nei.sign.great_bay_coast_5", SCENE_30GYOSON, { 1070, 80, 4649 } },
    { "nei.sign.great_bay_coast_6", SCENE_30GYOSON, { 1718, 80, 2959 } },
    { "nei.sign.zora_cape_1", SCENE_31MISAKI, { -5691, 14, 1424 } },
    { "nei.sign.zora_cape_2", SCENE_31MISAKI, { 1093, 12, 757 } },
    { "nei.sign.zora_cape_3", SCENE_31MISAKI, { 4037, 40, 3967 } },
    { "nei.sign.ikana_canyon_upper_1", SCENE_IKANA, { -1755, 794, 1452 } },
    { "nei.sign.ikana_canyon_upper_2", SCENE_IKANA, { -1094, 600, 178 } },
    { "nei.sign.ikana_canyon_upper_3", SCENE_IKANA, { -145, 592, -529 } },
    { "nei.sign.ikana_canyon_upper_4", SCENE_IKANA, { 486, 200, 2084 } },
    { "nei.sign.ikana_canyon_lower_1", SCENE_IKANA, { -157, -432, 3924 } },
    { "nei.sign.ikana_canyon_lower_2", SCENE_IKANA, { -2980, -505, 3087 } },
    { "nei.sign.ikana_canyon_lower_3", SCENE_IKANA, { 2265, -160, 4554 } },
};

static int32_t sPlacementChecks[ARRAY_COUNT(sPlacements)];

typedef struct {
    Color_RGBA8 prim;
    Color_RGBA8 env;
} SparkleColor;

static const SparkleColor sSparkleColors[RITYPE_MAX] = {
    [RITYPE_BOSS_KEY] = { { 255, 80, 40, 255 }, { 200, 0, 0, 255 } },
    [RITYPE_HEALTH] = { { 255, 120, 160, 255 }, { 255, 0, 60, 255 } },
    [RITYPE_JUNK] = { { 230, 230, 230, 255 }, { 120, 120, 120, 255 } },
    [RITYPE_LESSER] = { { 150, 220, 255, 255 }, { 0, 100, 255, 255 } },
    [RITYPE_MAJOR] = { { 255, 240, 120, 255 }, { 255, 180, 0, 255 } },
    [RITYPE_MASK] = { { 230, 150, 255, 255 }, { 140, 0, 220, 255 } },
    [RITYPE_SKULLTULA_TOKEN] = { { 255, 255, 200, 255 }, { 200, 160, 0, 255 } },
    [RITYPE_SMALL_KEY] = { { 220, 220, 255, 255 }, { 120, 120, 200, 255 } },
    [RITYPE_STRAY_FAIRY] = { { 255, 200, 255, 255 }, { 255, 80, 200, 255 } },
};

static const char* const sModes[] = { "Off", "Overworld", "Dungeons", "All" };

static const S2HModRandoOption sOption = {
    sizeof(S2HModRandoOption),
    SHUFFLE_SIGNS_OPTION,
    "Shuffle Signs",
    "The first read of a signpost gives an item. Overworld: field, town and spider house signs. Dungeons: signs "
    "inside temples (Majora's Mask has none yet). All: both.",
    sModes,
    ARRAY_COUNT(sModes),
    SIGNS_OFF,
    S2H_MOD_RANDO_WIDGET_COMBOBOX,
};

static void RegisterSignChecks(void) {
    for (uint32_t i = 0; i < ARRAY_COUNT(sChecks); i++) {
        const SignCheck* sign = &sChecks[i];
        const S2HRandoCheckDefinition definition = { sizeof(S2HRandoCheckDefinition),
                                                     sign->key,
                                                     sign->name,
                                                     sign->scene,
                                                     sign->region,
                                                     NULL,
                                                     SHUFFLE_SIGNS_OPTION,
                                                     OVERWORLD_SIGN };
        if (sApi->RegisterRandoCheck(&definition) < 0) {
            fprintf(stderr, "[shuffle_signs] %s was rejected\n", sign->key);
        }
    }
    for (uint32_t i = 0; i < ARRAY_COUNT(sPlacements); i++) {
        sPlacementChecks[i] = sApi->GetRandoCheckId(sPlacements[i].key);
    }
}

static int32_t FindSignCheck(int16_t scene, Vec3f* home) {
    for (uint32_t i = 0; i < ARRAY_COUNT(sPlacements); i++) {
        Vec3f placement = sPlacements[i].pos;
        if (sPlacements[i].scene == scene && Math_Vec3f_DistXYZ(&placement, home) < PLACEMENT_RADIUS) {
            return sPlacementChecks[i];
        }
    }
    return -1;
}

static void SpawnSparkle(Actor* actor, uint8_t category) {
    const SparkleColor* color = &sSparkleColors[category];
    Color_RGBA8 prim = color->prim;
    Color_RGBA8 env = color->env;
    Vec3f pos = { actor->world.pos.x + Rand_CenteredFloat(40.0f), actor->world.pos.y + 20.0f + Rand_ZeroFloat(40.0f),
                  actor->world.pos.z + Rand_CenteredFloat(40.0f) };
    Vec3f velocity = { 0.0f, 0.4f, 0.0f };
    Vec3f accel = { 0.0f, 0.0f, 0.0f };
    EffectSsKirakira_SpawnFocused(gPlayState, &pos, &velocity, &accel, &prim, &env, 2000, 16);
}

// Opening the sign's textbox is the read; the randomizer's queue gives the item once the text closes.
static void UpdateSign(Actor* actor) {
    EnKanban* sign = (EnKanban*)actor;
    if (gPlayState == NULL || actor->params == ENKANBAN_PIECE || sign->actionState != ENKANBAN_SIGN) {
        return;
    }
    const int32_t check = FindSignCheck(gPlayState->sceneId, &actor->home.pos);
    if (check < 0 || sApi->IsRandoCheckObtained(check)) {
        return;
    }
    const uint8_t category = sApi->GetRandoCheckItemCategory(check);
    if (category >= RITYPE_MAX) {
        return;
    }
    if (sign->msgFlag) {
        sApi->AwardRandoCheck(check);
        return;
    }
    if (gPlayState->gameplayFrames % SPARKLE_INTERVAL == 0) {
        SpawnSparkle(actor, category);
    }
}

S2H_MOD_EXPORT void ModSetApi(const S2HModApi* api) {
    sApi = api;
}

S2H_MOD_EXPORT const S2HModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

S2H_MOD_EXPORT void ModInit(void) {
    if (!S2H_MOD_API_HAS(sApi, GetRandoCheckItemCategory)) {
        fprintf(stderr, "[shuffle_signs] The host lacks the rando check API; the mod stays off\n");
        return;
    }
    sApi->RegisterRandoOption(&sOption);
    RegisterSignChecks();
    S2H_REGISTER_HOOK_FOR_ID(sApi, OnActorUpdate, ACTOR_EN_KANBAN, UpdateSign);
}
