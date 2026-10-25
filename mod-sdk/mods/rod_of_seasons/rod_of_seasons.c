#include "z64items.h"
#include "z64wheel.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "objects/object_link_child/object_link_child.h"

#define SEASONS_KEY "nei.rod_of_seasons"
// The pose NEI dialled in and baked: the staff is 96 units tall, so the scale is the share of Link's own
// height it takes up.
#define SEASONS_HELD_SCALE 0.4f
#define SEASONS_HELD_ROT_Y 72.414f
#define SEASONS_HELD_ROT_X 109.655f
#define SEASONS_HELD_OFFSET_Y 5.977f
#define SEASONS_HELD_OFFSET_Z (-3.218f)
#define SEASONS_RAIN_DROPS 30
#define SEASONS_SNOWFLAKES 64
#define SEASONS_PETAL_RADIUS 120.0f
#define SEASONS_PETAL_EVERY 2
#define SEASONS_SPARKLE_COUNT 12
#define SEASONS_SPARKLE_SPREAD 35.0f
#define SEASONS_WATER_GONE -32000
#define SEASONS_MAX_WATERBOXES 32
// How far below his feet the frozen surface may sit and still count as the floor he is standing on.
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

// The rod itself never changes on the pause screen or its button: the season rides along as a coin in the corner.
static const ALIGN_ASSET(2) char sSpringRod[] = "__OTR__textures/icon_item_custom/gItemIconRodOfSeasonsSpringTex";
static const ALIGN_ASSET(2) char sSummerRod[] = "__OTR__textures/icon_item_custom/gItemIconRodOfSeasonsSummerTex";
static const ALIGN_ASSET(2) char sAutumnRod[] = "__OTR__textures/icon_item_custom/gItemIconRodOfSeasonsAutumnTex";
static const ALIGN_ASSET(2) char sWinterRod[] = "__OTR__textures/icon_item_custom/gItemIconRodOfSeasonsWinterTex";

// Offsets onto whatever light the scene already has, so they shade every room without knowing anything
// about it. Winter pulls red down hardest and barely touches blue, which reads as cold rather than dark.
static const SeasonEntry sSeasons[SEASON_COUNT] = {
    { { 0, 0, 0 }, { 0, 0, 0 }, { 255, 255, 255 }, NA_SE_SY_TRE_BOX_APPEAR },
    { { 0, 0, 0 }, { 0, 0, 0 }, { 190, 245, 200 }, NA_SE_EV_CHICKEN_CRY_A },
    { { 18, 14, -6 }, { 16, 12, -8 }, { 255, 230, 140 }, NA_SE_EV_FLAME_IGNITION },
    { { -34, -32, -26 }, { -42, -40, -34 }, { 235, 170, 90 }, NA_SE_EV_RAIN },
    { { -46, -34, -14 }, { -54, -42, -20 }, { 190, 220, 255 }, NA_SE_EV_ICE_FREEZE },
};

static const Z64WheelEntry sWheelEntries[SEASON_COUNT] = {
    { sIconTex, "No Season", sIconTex },   { sSpringIcon, "Spring", sSpringRod }, { sSummerIcon, "Summer", sSummerRod },
    { sAutumnIcon, "Autumn", sAutumnRod }, { sWinterIcon, "Winter", sWinterRod },
};

static const ALIGN_ASSET(2) char sNameTex[] = "__OTR__textures/item_name_custom/gRodOfSeasonsNameTex";
static const ALIGN_ASSET(2) char sRodDL[] = "__OTR__objects/object_nei_rod_of_seasons/gNeiRodOfSeasonsDL";

static const char* const sRequiredHooks[] = { "OnPlayerUpdate",
                                              "OnActorDrawEnd",
                                              "OnBgCheckRaycastFloor",
                                              "OnSceneInit",
                                              "OnPlayerPostLimbDraw",
                                              "OnResolveEnvHazard",
                                              "OnPlayerResolveLimbDraw",
                                              "OnActorResolveBgCheckFlags",
                                              "OnInterfaceDrawEnd",
                                              "OnSaveLoad" };
static const S2HModRequirements sRequirements = { sizeof(S2HModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

// One item and one slot, found four times over: the seed spreads the copies, and each one wakes a season.
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
static u8 sWokenSeason;
static u8 sAppliedSeason;
static MtxF sHandMatrix;
static bool sHasHandMatrix;

void Player_AnimSfx_PlayVoice(Player* player, u16 sfxId);

// Which seasons Link carries, and which one the world is wearing, both live in the wheel, and the wheel
// lives in the save file: one file's spring is not another's.
static u8 CurrentSeason(void) {
    u32 selection = Z64Wheel_GetSelection();

    return selection < SEASON_COUNT ? (u8)selection : SEASON_OFF;
}

static bool IsSeasonOwned(u8 season) {
    return season != SEASON_OFF && Z64Wheel_IsEntryUnlocked(season);
}

// The scene's own weather writes these same fields, so the rod may only undo what the rod set.
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
    ShadeWorld(&play->envCtx, SEASON_OFF);
}

// Majora's Mask has no petal effect, so spring is a drift of pink sparkles around Link.
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

// Environment_Update rebuilds all of this every pass, which is also what carries the season through a scene
// load: every write here is idempotent, so it simply happens again.
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
            if ((play->gameplayFrames % SEASONS_PETAL_EVERY) == 0) {
                SpawnPetals(play, player);
            }
            break;
        case SEASON_SUMMER:
            ReleaseRain(env);
            ReleaseSnow(env);
            break;
        case SEASON_AUTUMN:
            // Particles on screen suppress the engine's whole rain pass, so the flakes drain out first.
            ReleaseSnow(env);
            TakeRain(env, SEASONS_RAIN_DROPS);
            Audio_PlaySfx(NA_SE_EV_RAIN - SFX_FLAG);
            break;
        case SEASON_WINTER:
            ReleaseRain(env);
            TakeSnow(env, SEASONS_SNOWFLAKES);
            break;
        default:
            ClearWeather(play);
            break;
    }
}

// Summer drains every basin. The scene's own header is shared cached memory, so the surfaces are put back
// the moment the season lets go of them, and re-applied each frame because the actors that own their water
// rewrite the same fields.
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

// Winter puts out the hot rooms, and only those: an underwater hazard is not heat, and waiving every one of
// them would hand Link endless air.
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

// Locked seasons are not offered: the button turns through what Link carries, plus the blank setting that
// hands the weather back to the scene. The wheel itself only ever shows the unlocked ones.
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
    if (player == NULL) {
        return;
    }
    RestoreWaterBoxes();
    ClearWeather(play);
    ApplySeason(play, player);
    SpawnSeasonSparkles(player, play, season);
    Audio_PlaySfx_AtPos(&player->actor.world.pos, sSeasons[season].chime);
    Player_AnimSfx_PlayVoice(player, NA_SE_VO_LI_MAGIC_ATTACK);
    sAppliedSeason = season;
}

// The wheel is the picker NEI uses for this rod: it stops the world and waits for an answer, so the season
// is chosen rather than cycled past. Whoever writes the selection, the world follows it a frame later.
static void TurnSeason(Player* player, PlayState* play) {
    if (!Z64Wheel_Open()) {
        Z64Wheel_SetSelection(NextAvailableSeason(CurrentSeason()));
    }
}

// A scene that has just loaded already stands in the season the save holds.
static void SyncSeasonOnSceneInit(s16 sceneId, s8 spawnNum) {
    sAppliedSeason = CurrentSeason();
}

// Winter freezes every surface: the floor raycast answers with the water's own surface, so Link stands on it
// instead of sinking. Only for Link, only while he is above the water and not already swimming in it.
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

static void FreezeWaterUnderfoot(CollisionContext* colCtx, Vec3f* pos, Actor* actor, CollisionPoly** poly,
                                 s32* bgId, f32* floorY) {
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

// The engine's own "no ripples this frame" bit: walking on ice is walking on a solid, so the water it stands on
// never notices.
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

// The staff is gripped, not palmed: the same closed fist the Hookshot puts on that hand.
static void CloseRodHand(Player* player, int32_t limbIndex, Gfx** dList, Gfx* limbDList, Vec3f* pos) {
    if (!sIsRodDrawn || limbIndex != PLAYER_LIMB_RIGHT_HAND) {
        return;
    }
    *dList = (Gfx*)gLinkHumanRightHandClosedDL;
}

static u8 CountLockedSeasons(void) {
    u8 locked = 0;

    for (u8 season = SEASON_SPRING; season < SEASON_COUNT; season++) {
        locked += IsSeasonOwned(season) ? 0 : 1;
    }
    return locked;
}

// Whichever one turns up: each pickup hands over a season Link is missing, so the four of them add up to
// the whole year however the seed spreads them out.
static u8 PickMissingSeason(void) {
    u8 locked = CountLockedSeasons();

    if (locked == 0) {
        return SEASON_OFF;
    }
    u8 wanted = (u8)(Rand_ZeroOne() * locked);

    for (u8 season = SEASON_SPRING; season < SEASON_COUNT; season++) {
        if (IsSeasonOwned(season)) {
            continue;
        }
        if (wanted == 0) {
            return season;
        }
        wanted--;
    }
    return SEASON_OFF;
}

static void GrantSeason(void) {
    u8 season = PickMissingSeason();

    if (season == SEASON_OFF) {
        return;
    }
    Z64Wheel_SetEntryUnlocked(season, true);
    sWokenSeason = season;
}

// Every copy of the rod in the pool is this same item: what a copy hands over is a season, not a second rod.
static const char* const sPickupKeys[SEASON_COUNT] = { "", "nei.seasons.spring", "nei.seasons.summer", "nei.seasons.autumn", "nei.seasons.winter" };
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

static void WakeSeason(const char* key) {
    for (u8 season = SEASON_SPRING; season < SEASON_COUNT; season++) Z64Wheel_SetEntryUnlocked(season, true);
}

static const char* WakeMessage(u8 season) {
    switch (season) {
        case SEASON_SPRING:
            return "The rod wakes to %gSpring%w!\nFlowers open, and what sleeps\nthrough winter is awake.";
        case SEASON_SUMMER:
            return "The rod wakes to %rSummer%w!\nThe heat dries shallow water and\nthaws what the cold held.";
        case SEASON_AUTUMN:
            return "The rod wakes to %yAutumn%w!\nRain falls, and the wind carries\nleaves across the land.";
        case SEASON_WINTER:
            return "The rod wakes to %bWinter%w!\nSnow falls, and water freezes hard\nenough to walk on.";
        default:
            return "The rod wakes to a new season!";
    }
}

// The pickup has its own textbox running, so the season it woke waits until Link has the screen back.
static void AnnounceWokenSeason(PlayState* play, Player* player) {
    if (Message_GetState(&play->msgCtx) != TEXT_STATE_NONE || Player_InCsMode(play)) {
        return;
    }
    sApi->ShowTextbox(play, WakeMessage(sWokenSeason), true);
    sWokenSeason = SEASON_OFF;
}

// The season is world state, not a spell: it keeps running whether or not the rod sits on a button.
static void KeepSeasonRunning(Player* player) {
    PlayState* play = gPlayState;
    u8 season;

    if (play == NULL || player == NULL || !sApi->IsCustomItemOwned(SEASONS_KEY)) {
        return;
    }
    // Carrying the rod means carrying at least one season, whichever way it arrived.
    for (u8 season = SEASON_SPRING; season < SEASON_COUNT; season++) {
        if (sApi->IsCustomItemOwned(sPickupKeys[season])) Z64Wheel_SetEntryUnlocked(season, true);
    }
    if (sWokenSeason != SEASON_OFF) {
        AnnounceWokenSeason(play, player);
    }
    season = CurrentSeason();
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

// Nothing touches the matrix between the hand limb and the end of the post-limb pass while the rod is out, so
// this is the hand bone's own matrix: the staff follows the arm instead of floating beside Link.
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

    if (!sIsRodDrawn || !sHasHandMatrix || player->actor.scale.y < 0.0f) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    Matrix_Put(&sHandMatrix);
    // That matrix carries Link's own 0.01 body scale: dividing it back out leaves the offsets in world units.
    Matrix_Scale(unscale, unscale, unscale, MTXMODE_APPLY);
    Matrix_RotateYF(DEG_TO_RAD(SEASONS_HELD_ROT_Y), MTXMODE_APPLY);
    Matrix_RotateXF(DEG_TO_RAD(SEASONS_HELD_ROT_X), MTXMODE_APPLY);
    // The offsets come after the rotations, so each slides the staff along its OWN shaft.
    Matrix_Translate(0.0f, SEASONS_HELD_OFFSET_Y, SEASONS_HELD_OFFSET_Z, MTXMODE_APPLY);
    Matrix_Scale(SEASONS_HELD_SCALE, SEASONS_HELD_SCALE, SEASONS_HELD_SCALE, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sRodDL);
    CLOSE_DISPS(play->state.gfxCtx);
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
    Z64Items_SetTextbox(&rod, "You got the %rRod of Seasons%w!\nA staff the old spirits answer to.\x10Press a "
                              "%yC button%w to turn the world through\nthe seasons it has woken to, or hold\n%yL%w "
                              "to pick one.\x10It wakes to one more season each\ntime you find it again.");
    Z64Items_SetPauseText(&rod, "%rRod of Seasons\n%wPress a %yC button%w for the next season,\n%yL%w to pick one.");
    Z64Items_SetAction(&rod, TakeOutRod, HoldRod);
    Z64Items_SetHeldCallbacks(&rod, TurnSeason, StowRod, NULL);
    Z64Items_SetGetItemModel(&rod, sRodDL);
    rod.modelGroup = PLAYER_MODELGROUP_DEFAULT;
    rod.onReceive = WakeSeason;
    S2HCustomItemRandomizer rodLogic = sRodLogic;
    rodLogic.poolCount = 0;
    Z64Items_SetLogic(&rod, &rodLogic);

    if (!Z64Items_Register(sApi, &rod)) {
        return;
    }
    Z64Wheel_Register(sApi, SEASONS_KEY, SEASONS_KEY, sWheelEntries, ARRAY_COUNT(sWheelEntries), true);
    for (u8 season = SEASON_SPRING; season < SEASON_COUNT; season++) {
        S2HCustomPickupVariant pickup = Z64Items_DefinePickup(SEASONS_KEY, sVariantNames[season], sPickupKeys[season]);
        pickup.iconPath = sWheelEntries[season].itemIconPath;
        pickup.getItemModelPath = sRodDL;
        static const u8 flameColors[][3] = { { 0, 0, 0 }, { 40, 185, 55 }, { 255, 60, 50 }, { 225, 165, 0 }, { 0, 80, 180 } };
        pickup.drawSoulFlame = true;
        pickup.flameRed = flameColors[season][0];
        pickup.flameGreen = flameColors[season][1];
        pickup.flameBlue = flameColors[season][2];
        pickup.getItemText = WakeMessage(season);
        pickup.onReceive = ReceiveSeason;
        S2HCustomItemRandomizer randomizer = sRodLogic;
        randomizer.poolCount = 1;
        randomizer.flags = S2H_CUSTOM_ITEM_RANDO_ADVANCEMENT;
        randomizer.logicKey = randomizer.hintKey = pickup.key;
        pickup.randomizer = &randomizer;
        if (!Z64Items_RegisterPickup(sApi, &pickup)) return;
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
