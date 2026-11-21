#include <string.h>

#include "z64items.h"
#include "z64wheel.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "objects/object_link_child/object_link_child.h"
#include "objects/gameplay_keep/gameplay_keep.h"

#define SEASONS_KEY "nei.rod_of_seasons"
#define SEASONS_HELD_SCALE 0.4f
#define SEASONS_HELD_ROT_Y 72.414f
#define SEASONS_HELD_ROT_X 109.655f
#define SEASONS_HELD_OFFSET_Y 5.977f
#define SEASONS_HELD_OFFSET_Z (-3.218f)
#define SEASONS_GIVE_SCALE 0.4f
#define SEASONS_FLAME_SCALE 0.13333333f
#define SEASONS_RAIN_DROPS 30
#define SEASONS_SNOWFLAKES 64
#define SEASONS_PETAL_RADIUS 120.0f
#define SEASONS_PETAL_EVERY 2
#define SEASONS_SPARKLE_COUNT 12
#define SEASONS_SPARKLE_SPREAD 35.0f
#define SEASONS_WATER_GONE -32000
#define SEASONS_MAX_WATERBOXES 32
#define SEASONS_ICE_FOOTING 2.0f

typedef enum {
    SEASON_OFF,
    SEASON_SPRING,
    SEASON_SUMMER,
    SEASON_AUTUMN,
    SEASON_WINTER,
    SEASON_COUNT,
} Season;

typedef struct {
    s16 light[3];
    s16 ambient[3];
    u8 sparkle[3];
    u16 chime;
} SeasonEntry;

static const ALIGN_ASSET(2) char sIconTex[] = "__OTR__textures/icon_item_custom/gItemIconRodOfSeasonsTex";
static const ALIGN_ASSET(2) char sSpringIcon[] = "__OTR__textures/icon_item_custom/gItemIconSeasonSpringTex";
static const ALIGN_ASSET(2) char sSummerIcon[] = "__OTR__textures/icon_item_custom/gItemIconSeasonSummerTex";
static const ALIGN_ASSET(2) char sAutumnIcon[] = "__OTR__textures/icon_item_custom/gItemIconSeasonAutumnTex";
static const ALIGN_ASSET(2) char sWinterIcon[] = "__OTR__textures/icon_item_custom/gItemIconSeasonWinterTex";

static const ALIGN_ASSET(2) char sSpringRod[] = "__OTR__textures/icon_item_custom/gItemIconRodOfSeasonsSpringTex";
static const ALIGN_ASSET(2) char sSummerRod[] = "__OTR__textures/icon_item_custom/gItemIconRodOfSeasonsSummerTex";
static const ALIGN_ASSET(2) char sAutumnRod[] = "__OTR__textures/icon_item_custom/gItemIconRodOfSeasonsAutumnTex";
static const ALIGN_ASSET(2) char sWinterRod[] = "__OTR__textures/icon_item_custom/gItemIconRodOfSeasonsWinterTex";

static const SeasonEntry sSeasons[SEASON_COUNT] = {
    { { 0, 0, 0 }, { 0, 0, 0 }, { 255, 255, 255 }, NA_SE_SY_TRE_BOX_APPEAR },
    { { 0, 0, 0 }, { 0, 0, 0 }, { 190, 245, 200 }, NA_SE_EV_CHICKEN_CRY_A },
    { { 18, 14, -6 }, { 16, 12, -8 }, { 255, 230, 140 }, NA_SE_EV_FLAME_IGNITION },
    { { -34, -32, -26 }, { -42, -40, -34 }, { 235, 170, 90 }, NA_SE_EV_RAIN },
    { { -46, -34, -14 }, { -54, -42, -20 }, { 190, 220, 255 }, NA_SE_EV_ICE_FREEZE },
};

static const u8 sSeasonFlameColors[SEASON_COUNT][3] = {
    { 160, 230, 255 }, { 40, 185, 55 }, { 255, 60, 50 }, { 225, 165, 0 }, { 0, 80, 180 },
};

static const Z64WheelEntry sWheelEntries[SEASON_COUNT] = {
    { sIconTex, "No Season", sIconTex },   { sSpringIcon, "Spring", sSpringRod }, { sSummerIcon, "Summer", sSummerRod },
    { sAutumnIcon, "Autumn", sAutumnRod }, { sWinterIcon, "Winter", sWinterRod },
};

static const ALIGN_ASSET(2) char sNameTex[] = "__OTR__textures/item_name_custom/gRodOfSeasonsNameTex";
static const ALIGN_ASSET(2) char sRodDL[] = "__OTR__objects/object_nei_rod_of_seasons/gNeiRodOfSeasonsDL";

static const char* const sRequiredHooks[] = { "OnPlayerUpdate",          "OnActorDrawEnd",
                                              "OnBgCheckRaycastFloor",   "OnSceneInit",
                                              "OnPlayerPostLimbDraw",    "OnResolveEnvHazard",
                                              "OnPlayerResolveLimbDraw", "OnActorResolveBgCheckFlags",
                                              "OnInterfaceDrawEnd",      "OnSaveLoad" };
static const S2HModRequirements sRequirements = { sizeof(S2HModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const S2HCustomItemRandomizer sRodLogic = {
    sizeof(S2HCustomItemRandomizer),
    S2H_CUSTOM_ITEM_RANDO_ADVANCEMENT | S2H_CUSTOM_ITEM_RANDO_PROGRESSIVE,
    S2H_CUSTOM_ITEM_TYPE_ITEM,
    0,
    SEASON_COUNT - 1,
    SEASONS_KEY,
    SEASONS_KEY,
    "",
};

static const S2HModApi* sApi;
static bool sIsRodDrawn;
static bool sOwnsRain;
static bool sOwnsSnow;
static bool sOwnsSky;
static u8 sClearSkyboxConfig;
static u8 sAppliedSeason;
static MtxF sHandMatrix;
static bool sHasHandMatrix;

void Player_AnimSfx_PlayVoice(Player* player, u16 sfxId);

static u8 CurrentSeason(void) {
    u32 selection = Z64Wheel_GetSelection();

    return selection < SEASON_COUNT ? (u8)selection : SEASON_OFF;
}

static bool IsSeasonOwned(u8 season) {
    return season != SEASON_OFF && Z64Wheel_IsEntryUnlocked(season);
}

static void TakeRain(EnvironmentContext* env, u8 drops) {
    env->precipitation[PRECIP_RAIN_MAX] = drops;
    env->precipitation[PRECIP_RAIN_CUR] = drops;
    sOwnsRain = true;
}

static void ReleaseRain(EnvironmentContext* env) {
    if (!sOwnsRain) {
        return;
    }
    env->precipitation[PRECIP_RAIN_MAX] = 0;
    env->precipitation[PRECIP_RAIN_CUR] = 0;
    sOwnsRain = false;
}

static void TakeSnow(EnvironmentContext* env, u8 flakes) {
    env->precipitation[PRECIP_SNOW_MAX] = flakes;
    env->precipitation[PRECIP_SNOW_CUR] = flakes;
    sOwnsSnow = true;
}

static void ReleaseSnow(EnvironmentContext* env) {
    if (!sOwnsSnow) {
        return;
    }
    env->precipitation[PRECIP_SNOW_MAX] = 0;
    env->precipitation[PRECIP_SNOW_CUR] = 0;
    sOwnsSnow = false;
}

// MM has no gloomy sky; the open-air sky of days 1-3 (configs 0-2) is swapped for day 2's rain sky and lights.
static void TakeSky(EnvironmentContext* env) {
    if (sOwnsSky) {
        return;
    }
    if (env->skyboxConfig > SKYBOX_CONFIG_2) {
        return;
    }
    sClearSkyboxConfig = env->skyboxConfig;
    env->skyboxConfig = SKYBOX_CONFIG_1;
    env->changeSkyboxNextConfig = SKYBOX_CONFIG_1;
    sOwnsSky = true;
}

static void ReleaseSky(EnvironmentContext* env) {
    if (!sOwnsSky) {
        return;
    }
    env->skyboxConfig = sClearSkyboxConfig;
    env->changeSkyboxNextConfig = sClearSkyboxConfig;
    sOwnsSky = false;
}

static void ShadeWorld(EnvironmentContext* env, u8 season) {
    const SeasonEntry* entry = &sSeasons[season];

    for (u32 i = 0; i < 3; i++) {
        env->adjLightSettings.light1Color[i] = entry->light[i];
        env->adjLightSettings.ambientColor[i] = entry->ambient[i];
    }
}

static void ClearWeather(PlayState* play) {
    ReleaseRain(&play->envCtx);
    ReleaseSnow(&play->envCtx);
    ReleaseSky(&play->envCtx);
    ShadeWorld(&play->envCtx, SEASON_OFF);
}

static void SpawnPetals(PlayState* play, Player* player) {
    static Color_RGBA8 prim = { 255, 190, 215, 255 };
    static Color_RGBA8 env = { 255, 120, 170, 255 };
    s16 angle = (s16)(Rand_ZeroOne() * 0x10000);
    f32 radius = Rand_ZeroOne() * SEASONS_PETAL_RADIUS;
    Vec3f pos = { player->actor.world.pos.x + Math_SinS(angle) * radius, player->actor.world.pos.y + 90.0f,
                  player->actor.world.pos.z + Math_CosS(angle) * radius };
    Vec3f velocity = { Rand_CenteredFloat(1.0f), -1.0f, Rand_CenteredFloat(1.0f) };
    Vec3f accel = { 0.0f, 0.0f, 0.0f };

    EffectSsKirakira_SpawnDispersed(play, &pos, &velocity, &accel, &prim, &env, 200, 40);
}

static void ApplySeason(PlayState* play, Player* player) {
    EnvironmentContext* env = &play->envCtx;
    u8 season = CurrentSeason();

    if (!IsSeasonOwned(season) || env->skyboxDisabled) {
        ClearWeather(play);
        return;
    }
    ShadeWorld(env, season);
    switch (season) {
        case SEASON_SPRING:
            ReleaseRain(env);
            ReleaseSnow(env);
            ReleaseSky(env);
            if ((play->gameplayFrames % SEASONS_PETAL_EVERY) == 0) {
                SpawnPetals(play, player);
            }
            break;
        case SEASON_SUMMER:
            ReleaseRain(env);
            ReleaseSnow(env);
            ReleaseSky(env);
            break;
        case SEASON_AUTUMN:
            ReleaseSnow(env);
            TakeRain(env, SEASONS_RAIN_DROPS);
            TakeSky(env);
            Audio_PlaySfx(NA_SE_EV_RAIN - SFX_FLAG);
            break;
        case SEASON_WINTER:
            ReleaseRain(env);
            TakeSnow(env, SEASONS_SNOWFLAKES);
            TakeSky(env);
            break;
        default:
            ClearWeather(play);
            break;
    }
}

// The scene header is shared cached memory, so drained surfaces are restored the moment summer lets go.
static WaterBox* sDriedHeader;
static s32 sDriedSurfaces[SEASONS_MAX_WATERBOXES];
static u16 sDriedCount;

static void RestoreWaterBoxes(void) {
    if (sDriedHeader == NULL) {
        return;
    }
    for (u16 i = 0; i < sDriedCount; i++) {
        sDriedHeader[i].minPos.y = sDriedSurfaces[i];
    }
    sDriedHeader = NULL;
    sDriedCount = 0;
}

static void DryWaterBoxes(PlayState* play) {
    CollisionHeader* header = play->colCtx.colHeader;

    if (header == NULL || header->waterBoxes == NULL || header->numWaterBoxes == 0) {
        return;
    }
    if (sDriedHeader != header->waterBoxes) {
        RestoreWaterBoxes();
        sDriedCount = MIN(header->numWaterBoxes, SEASONS_MAX_WATERBOXES);
        for (u16 i = 0; i < sDriedCount; i++) {
            sDriedSurfaces[i] = header->waterBoxes[i].minPos.y;
        }
        sDriedHeader = header->waterBoxes;
    }
    for (u16 i = 0; i < sDriedCount; i++) {
        sDriedHeader[i].minPos.y = SEASONS_WATER_GONE;
    }
}

static void ThawHotRooms(PlayState* play, s16* hazard) {
    if (*hazard != PLAYER_ENV_HAZARD_HOTROOM || CurrentSeason() != SEASON_WINTER || !IsSeasonOwned(SEASON_WINTER)) {
        return;
    }
    *hazard = PLAYER_ENV_HAZARD_NONE;
}

static void SpawnSeasonSparkles(Player* player, PlayState* play, u8 season) {
    const SeasonEntry* entry = &sSeasons[season];
    Color_RGBA8 prim = { entry->sparkle[0], entry->sparkle[1], entry->sparkle[2], 255 };
    Color_RGBA8 env = { entry->sparkle[0] / 2, entry->sparkle[1] / 2, entry->sparkle[2] / 2, 255 };
    Vec3f accel = { 0.0f, -0.2f, 0.0f };

    for (u32 i = 0; i < SEASONS_SPARKLE_COUNT; i++) {
        Vec3f pos = { player->actor.world.pos.x + Rand_CenteredFloat(SEASONS_SPARKLE_SPREAD),
                      player->actor.world.pos.y + 20.0f + Rand_ZeroFloat(50.0f),
                      player->actor.world.pos.z + Rand_CenteredFloat(SEASONS_SPARKLE_SPREAD) };
        Vec3f velocity = { Rand_CenteredFloat(2.0f), 1.5f, Rand_CenteredFloat(2.0f) };

        EffectSsKirakira_SpawnFocused(play, &pos, &velocity, &accel, &prim, &env, 400, 20);
    }
}

static u8 NextAvailableSeason(u8 from) {
    for (u8 step = 1; step <= SEASON_COUNT; step++) {
        u8 candidate = (from + step) % SEASON_COUNT;

        if (candidate == SEASON_OFF || IsSeasonOwned(candidate)) {
            return candidate;
        }
    }
    return SEASON_OFF;
}

static void TurnWorldToSeason(Player* player, PlayState* play, u8 season) {
    RestoreWaterBoxes();
    ClearWeather(play);
    ApplySeason(play, player);
    SpawnSeasonSparkles(player, play, season);
    Audio_PlaySfx_AtPos(&player->actor.world.pos, sSeasons[season].chime);
    Player_AnimSfx_PlayVoice(player, NA_SE_VO_LI_MAGIC_ATTACK);
    sAppliedSeason = season;
}

static void TurnSeason(Player* player, PlayState* play) {
    if (!Z64Wheel_Open()) {
        Z64Wheel_SetSelection(NextAvailableSeason(CurrentSeason()));
    }
}

// A new scene rebuilds the environment, so the sky saved from the last one must not be restored into it.
static void SyncSeasonOnSceneInit(s16 sceneId, s8 spawnNum) {
    sOwnsSky = false;
    sAppliedSeason = CurrentSeason();
}

static bool IsStandingOnIce(PlayState* play, Player* player) {
    f32 surface;
    WaterBox* waterBox;

    if (play == NULL || player == NULL || CurrentSeason() != SEASON_WINTER || !IsSeasonOwned(SEASON_WINTER)) {
        return false;
    }
    if ((player->stateFlags1 & PLAYER_STATE1_8000000) || player->actor.velocity.y > 0.0f) {
        return false;
    }
    if (!WaterBox_GetSurface1(play, &play->colCtx, player->actor.world.pos.x, player->actor.world.pos.z, &surface,
                              &waterBox)) {
        return false;
    }
    return player->actor.world.pos.y >= surface - SEASONS_ICE_FOOTING;
}

static void FreezeWaterUnderfoot(CollisionContext* colCtx, Vec3f* pos, Actor* actor, CollisionPoly** poly, s32* bgId,
                                 f32* floorY) {
    PlayState* play = gPlayState;
    Player* player = play == NULL ? NULL : GET_PLAYER(play);
    f32 surface;
    WaterBox* waterBox;

    if (player == NULL || actor != &player->actor || CurrentSeason() != SEASON_WINTER ||
        !IsSeasonOwned(SEASON_WINTER)) {
        return;
    }
    if ((player->stateFlags1 & PLAYER_STATE1_8000000) || player->actor.velocity.y > 0.0f) {
        return;
    }
    if (!WaterBox_GetSurface1(play, colCtx, pos->x, pos->z, &surface, &waterBox) || surface <= *floorY) {
        return;
    }
    *floorY = surface;
}

// The engine's own "no ripples this frame" bit.
static void SkipRipplesOnIce(Actor* actor, s32* flags) {
    PlayState* play = gPlayState;

    if (play == NULL || actor != &GET_PLAYER(play)->actor) {
        return;
    }
    if (IsStandingOnIce(play, GET_PLAYER(play))) {
        *flags |= UPDBGCHECKINFO_FLAG_40;
    }
}

static void TakeOutRod(PlayState* play, Player* player) {
    sIsRodDrawn = true;
    Audio_PlaySfx_AtPos(&player->actor.world.pos, NA_SE_PL_CHANGE_ARMS);
}

static void StowRod(Player* player, PlayState* play) {
    sIsRodDrawn = false;
    Audio_PlaySfx_AtPos(&player->actor.world.pos, NA_SE_PL_CHANGE_ARMS);
}

static void CloseRodHand(Player* player, int32_t limbIndex, Gfx** dList, Gfx* limbDList, Vec3f* pos) {
    if (!sIsRodDrawn || limbIndex != PLAYER_LIMB_RIGHT_HAND) {
        return;
    }
    *dList = (Gfx*)gLinkHumanRightHandClosedDL;
}

static const char* const sPickupKeys[SEASON_COUNT] = { "", "nei.seasons.spring", "nei.seasons.summer",
                                                       "nei.seasons.autumn", "nei.seasons.winter" };
static const char* const sVariantNames[SEASON_COUNT] = { "", "spring", "summer", "autumn", "winter" };

static void ReceiveSeason(const char* itemKey, const char* variantKey) {
    for (u8 season = SEASON_SPRING; season < SEASON_COUNT; season++) {
        if (strcmp(variantKey, sVariantNames[season]) == 0) {
            Z64Wheel_SetEntryUnlocked(season, true);
            Z64Wheel_SetSelection(season);
            return;
        }
    }
}

static void WakeAllSeasons(const char* key) {
    for (u8 season = SEASON_SPRING; season < SEASON_COUNT; season++) {
        Z64Wheel_SetEntryUnlocked(season, true);
    }
}

static void SyncReceivedSeasons(void) {
    for (u8 season = SEASON_SPRING; season < SEASON_COUNT; season++) {
        if (sApi->IsCustomItemOwned(sPickupKeys[season])) {
            Z64Wheel_SetEntryUnlocked(season, true);
        }
    }
}

static const char* WakeMessage(u8 season) {
    switch (season) {
        case SEASON_SPRING:
            return "The rod wakes to %gSpring%w! Flowers&open, and what sleeps through&winter is awake.";
        case SEASON_SUMMER:
            return "The rod wakes to %rSummer%w! The&heat dries shallow water and&thaws what the cold held.";
        case SEASON_AUTUMN:
            return "The rod wakes to %yAutumn%w! Rain&falls, and the wind carries leaves&across the land.";
        case SEASON_WINTER:
            return "The rod wakes to %bWinter%w! Snow&falls, and water freezes hard&enough to walk on.";
        default:
            return "The rod wakes to a new season!";
    }
}

static void KeepSeasonRunning(Player* player) {
    PlayState* play = gPlayState;

    if (play == NULL || player != GET_PLAYER(play)) {
        return;
    }
    SyncReceivedSeasons();
    if (!sApi->IsCustomItemOwned(SEASONS_KEY)) {
        return;
    }
    u8 season = CurrentSeason();

    if (season != sAppliedSeason && !Z64Wheel_IsOpen()) {
        TurnWorldToSeason(player, play, season);
        return;
    }
    // Clearing it after the scene checks is what keeps the splash, the swim and the wading sfx off the ice.
    if (IsStandingOnIce(play, player)) {
        player->actor.bgCheckFlags &= ~(BGCHECKFLAG_WATER | BGCHECKFLAG_WATER_TOUCH);
    }
    ApplySeason(play, player);
    if (IsSeasonOwned(season) && season == SEASON_SUMMER) {
        DryWaterBoxes(play);
    } else {
        RestoreWaterBoxes();
    }
}

static int32_t HoldRod(Player* player, PlayState* play) {
    return 0;
}

static void CaptureHandMatrix(Player* player, int32_t limbIndex) {
    if (limbIndex != PLAYER_LIMB_RIGHT_HAND) {
        return;
    }
    Matrix_Get(&sHandMatrix);
    sHasHandMatrix = true;
}

static void DrawRodInHand(Actor* actor, PlayState* play) {
    Player* player = (Player*)actor;
    f32 unscale = player->actor.scale.x != 0.0f ? 1.0f / player->actor.scale.x : 1.0f;

    if (!sIsRodDrawn || !sHasHandMatrix || player->actor.scale.y < 0.0f ||
        (player->stateFlags2 & PLAYER_STATE2_20000000)) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    Matrix_Put(&sHandMatrix);
    Matrix_Scale(unscale, unscale, unscale, MTXMODE_APPLY);
    Matrix_RotateYF(DEG_TO_RAD(SEASONS_HELD_ROT_Y), MTXMODE_APPLY);
    Matrix_RotateXF(DEG_TO_RAD(SEASONS_HELD_ROT_X), MTXMODE_APPLY);
    Matrix_Translate(0.0f, SEASONS_HELD_OFFSET_Y, SEASONS_HELD_OFFSET_Z, MTXMODE_APPLY);
    Matrix_Scale(SEASONS_HELD_SCALE, SEASONS_HELD_SCALE, SEASONS_HELD_SCALE, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sRodDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

// The soul flame of MM's En_Light, the same one the host draws for pickups, tinted by the season.
static void DrawSeasonFlame(PlayState* play, const u8* color) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               (uintptr_t)Gfx_TwoTexScroll(play->state.gfxCtx, 0, 0, 0, 0x10, 0x20, 1, play->state.frames & 0x3F,
                                           (-6 * play->state.frames) & 0x7F, 0x10, 0x20));
    Matrix_Push();
    Matrix_ReplaceRotation(&play->billboardMtxF);
    Matrix_Scale(SEASONS_FLAME_SCALE, SEASONS_FLAME_SCALE, SEASONS_FLAME_SCALE, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gDPSetPrimColor(POLY_XLU_DISP++, 0xC0, 0xC0, color[0], color[1], color[2], 0);
    gDPSetEnvColor(POLY_XLU_DISP++, color[0], color[1], color[2], 0);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gameplay_keep_DL_01ACF0);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawSpinningRod(PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    Matrix_Scale(SEASONS_GIVE_SCALE, SEASONS_GIVE_SCALE, SEASONS_GIVE_SCALE, MTXMODE_APPLY);
    Matrix_RotateYF((s16)(play->gameplayFrames * 2) * 0.01f, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sRodDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawRodForSeason(PlayState* play, u8 season) {
    DrawSeasonFlame(play, sSeasonFlameColors[season]);
    DrawSpinningRod(play);
}

static void DrawRodGetItem(PlayState* play, S2HGetItemEntry* getItemEntry) {
    DrawRodForSeason(play, SEASON_OFF);
}

static void DrawSpringGetItem(PlayState* play, S2HGetItemEntry* getItemEntry) {
    DrawRodForSeason(play, SEASON_SPRING);
}

static void DrawSummerGetItem(PlayState* play, S2HGetItemEntry* getItemEntry) {
    DrawRodForSeason(play, SEASON_SUMMER);
}

static void DrawAutumnGetItem(PlayState* play, S2HGetItemEntry* getItemEntry) {
    DrawRodForSeason(play, SEASON_AUTUMN);
}

static void DrawWinterGetItem(PlayState* play, S2HGetItemEntry* getItemEntry) {
    DrawRodForSeason(play, SEASON_WINTER);
}

static const S2HGetItemDrawFunc sSeasonGetItemDraws[SEASON_COUNT] = {
    DrawRodGetItem, DrawSpringGetItem, DrawSummerGetItem, DrawAutumnGetItem, DrawWinterGetItem,
};

static bool RegisterSeasonPickups(void) {
    for (u8 season = SEASON_SPRING; season < SEASON_COUNT; season++) {
        S2HCustomPickupVariant pickup = Z64Items_DefinePickup(SEASONS_KEY, sVariantNames[season], sPickupKeys[season]);
        S2HCustomItemRandomizer logic = sRodLogic;

        logic.flags = S2H_CUSTOM_ITEM_RANDO_ADVANCEMENT;
        logic.poolCount = 1;
        logic.logicKey = sPickupKeys[season];
        logic.hintKey = sPickupKeys[season];
        pickup.iconPath = sWheelEntries[season].itemIconPath;
        pickup.getItemModelPath = sRodDL;
        pickup.getItemScale = SEASONS_GIVE_SCALE;
        pickup.getItemText = WakeMessage(season);
        pickup.onReceive = ReceiveSeason;
        pickup.randomizer = &logic;
        pickup.drawSoulFlame = false;
        pickup.flameRed = sSeasonFlameColors[season][0];
        pickup.flameGreen = sSeasonFlameColors[season][1];
        pickup.flameBlue = sSeasonFlameColors[season][2];
        pickup.drawFunc = sSeasonGetItemDraws[season];
        if (!Z64Items_RegisterPickup(sApi, &pickup)) {
            return false;
        }
    }
    return true;
}

S2H_MOD_EXPORT void ModSetApi(const S2HModApi* api) {
    sApi = api;
}

S2H_MOD_EXPORT const S2HModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

S2H_MOD_EXPORT void ModInit(void) {
    if (!S2H_MOD_API_HAS(sApi, StorageSet)) {
        return;
    }

    S2HCustomItemDefinition rod = Z64Items_Define(SEASONS_KEY, sIconTex, sNameTex);

    Z64Items_SetButtons(&rod, S2H_CUSTOM_ITEM_C_BUTTON | S2H_CUSTOM_ITEM_DPAD);
    Z64Items_SetForms(&rod, S2H_CUSTOM_ITEM_FORM_BIT(PLAYER_FORM_HUMAN));
    Z64Items_SetPlacement(&rod, 0, 23, 0);
    Z64Items_SetTextbox(&rod, "You got the %rRod of Seasons%w!&A staff the old spirits&answer to.^"
                              "Press %y\xB2%w to turn the world&through the seasons it has&"
                              "woken to, or hold %y\xB3%w to pick one.");
    Z64Items_SetPauseText(&rod, "%rRod of Seasons&%wPress %y\xB2%w for the next season, %y\xB3%w to&pick one.");
    Z64Items_SetAction(&rod, TakeOutRod, HoldRod);
    Z64Items_SetHeldCallbacks(&rod, TurnSeason, StowRod, NULL);
    rod.modelGroup = PLAYER_MODELGROUP_DEFAULT;
    rod.getItemEntry.drawFunc = DrawRodGetItem;
    rod.onReceive = WakeAllSeasons;
    S2HCustomItemRandomizer rodLogic = sRodLogic;
    rodLogic.poolCount = 0;
    Z64Items_SetLogic(&rod, &rodLogic);

    if (!Z64Items_Register(sApi, &rod)) {
        return;
    }
    Z64Wheel_Register(sApi, SEASONS_KEY, SEASONS_KEY, sWheelEntries, ARRAY_COUNT(sWheelEntries), true);
    if (!RegisterSeasonPickups()) {
        return;
    }
    S2H_REGISTER_HOOK(sApi, OnPlayerUpdate, KeepSeasonRunning);
    S2H_REGISTER_HOOK(sApi, OnSceneInit, SyncSeasonOnSceneInit);
    S2H_REGISTER_HOOK(sApi, OnResolveEnvHazard, ThawHotRooms);
    S2H_REGISTER_HOOK(sApi, OnPlayerResolveLimbDraw, CloseRodHand);
    S2H_REGISTER_HOOK(sApi, OnActorResolveBgCheckFlags, SkipRipplesOnIce);
    S2H_REGISTER_HOOK(sApi, OnPlayerPostLimbDraw, CaptureHandMatrix);
    S2H_REGISTER_HOOK_FOR_ID(sApi, OnActorDrawEnd, ACTOR_PLAYER, DrawRodInHand);
    S2H_REGISTER_HOOK(sApi, OnBgCheckRaycastFloor, FreezeWaterUnderfoot);
}
