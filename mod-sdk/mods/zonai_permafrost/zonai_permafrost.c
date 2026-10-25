#include <string.h>

#include "z64items.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "z64rumble.h"

#define ZONAI_KEY "nei.zonai_permafrost"
#define ZONAI_TIME_PRIORITY 100
#define ZONAI_MAGIC_TO_START 4
#define ZONAI_MAGIC_PER_DRAIN 1
#define ZONAI_DRAIN_INTERVAL 10
#define ZONAI_LOW_MAGIC 8
#define ZONAI_TINT_ALPHA 26
#define ZONAI_TINT_PULSE 8
#define ZONAI_RELEASE_PITCH_START 1.45f
#define ZONAI_RELEASE_PITCH_END 0.55f
#define ZONAI_RELEASE_PITCH_STEP 0.11f

// A player in any of these is busy with something that owns them; the time stop does not answer meanwhile.
#define ZONAI_BLOCKING_STATES     (PLAYER_STATE1_DEAD | PLAYER_STATE1_TALKING | PLAYER_STATE1_20 | PLAYER_STATE1_20000000)

static const ALIGN_ASSET(2) char sIconTex[] = "__OTR__textures/icon_item_custom/gItemIconZonaiPermafrostTex";
static const ALIGN_ASSET(2) char sNameTex[] = "__OTR__textures/item_name_custom/gZonaiPermafrostNameTex";
static const ALIGN_ASSET(2) char sGiveDL[] = "__OTR__objects/object_nei_magic_spell/gZonaiPermafrostGiveDL";

static const u32 sItemButtons[S2H_ITEM_BUTTON_COUNT] = { BTN_B,   BTN_CLEFT, BTN_CDOWN, BTN_CRIGHT, 0,
                                                         BTN_DRIGHT, BTN_DLEFT, BTN_DDOWN, BTN_DUP };

static const char* const sRequiredHooks[] = { "OnPlayerUpdate", "OnSceneInit" };
static const S2HModRequirements sRequirements = { sizeof(S2HModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const S2HModApi* sApi;
static bool sIsTimeStopped;
static s16 sStoppedFrames;
static u32 sToggleFrame;
static Vec3f sSfxPos;
static f32 sReleasePitch = ZONAI_RELEASE_PITCH_END;

// The ice samples carry SFX_FLAG, which parks a one-shot in PLAYING until the same position stops it.
static void PlayOwnSfx(Player* player, u16 sfxId) {
    sSfxPos = player->actor.world.pos;
    AudioSfx_PlaySfx(sfxId - SFX_FLAG, &sSfxPos, 4, &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                     &gSfxDefaultReverb);
}

// The sound bank rereads the pitch pointer every audio frame, so walking it down bends a cue already ringing.
static void PlayReleaseChime(void) {
    sReleasePitch = ZONAI_RELEASE_PITCH_START;
    AudioSfx_PlaySfx(NA_SE_SY_SET_ICE_ARROW, &gSfxDefaultPos, 4, &sReleasePitch, &gSfxDefaultFreqAndVolScale,
                     &gSfxDefaultReverb);
}

static void BendReleaseChimeDown(void) {
    sReleasePitch = MAX(sReleasePitch - ZONAI_RELEASE_PITCH_STEP, ZONAI_RELEASE_PITCH_END);
}

// The register a Deku Nut flash writes: the transition fade runs the white-out back down by itself.
static void FlashScreen(void) {
    R_TRANS_FADE_FLASH_ALPHA_STEP = -1;
}

static void TintScreen(PlayState* play, u8 alpha) {
    play->envCtx.fillScreen = alpha != 0;
    play->envCtx.screenFillColor[0] = 120;
    play->envCtx.screenFillColor[1] = 230;
    play->envCtx.screenFillColor[2] = 210;
    play->envCtx.screenFillColor[3] = alpha;
}

static void SpawnRuneRing(Player* player, PlayState* play, f32 radius) {
    Vec3f accel = { 0.0f, 0.0f, 0.0f };
    Color_RGBA8 prim = { 100, 255, 150, 255 };
    Color_RGBA8 env = { 0, 200, 80, 255 };

    for (u8 i = 0; i < 8; i++) {
        s16 angle = (s16)(i * (0x10000 / 8));
        Vec3f pos = { player->actor.world.pos.x + Math_SinS(angle) * radius,
                      player->actor.world.pos.y + 30.0f + Rand_CenteredFloat(20.0f),
                      player->actor.world.pos.z + Math_CosS(angle) * radius };
        Vec3f velocity = { Math_SinS(angle) * 3.0f, Rand_ZeroFloat(1.5f), Math_CosS(angle) * 3.0f };

        EffectSsKirakira_SpawnFocused(play, &pos, &velocity, &accel, &prim, &env, 600, 20);
    }
}

static void SpawnRuneBurst(Player* player, PlayState* play) {
    SpawnRuneRing(player, play, 40.0f);
    SpawnRuneRing(player, play, 110.0f);
    SpawnRuneRing(player, play, 180.0f);
}

static void SpawnStillAir(Player* player, PlayState* play) {
    Vec3f zero = { 0.0f, 0.0f, 0.0f };
    Color_RGBA8 prim = { 120, 255, 160, 200 };
    Color_RGBA8 env = { 0, 180, 60, 150 };

    for (u8 i = 0; i < 3; i++) {
        Vec3f pos = { player->actor.world.pos.x + Rand_CenteredFloat(360.0f),
                      player->actor.world.pos.y + 20.0f + Rand_ZeroFloat(130.0f),
                      player->actor.world.pos.z + Rand_CenteredFloat(360.0f) };

        EffectSsKirakira_SpawnFocused(play, &pos, &zero, &zero, &prim, &env, 460, 15);
    }
}

static bool HasMagic(s16 amount) {
    return gSaveContext.save.saveInfo.playerData.isMagicAcquired &&
           gSaveContext.save.saveInfo.playerData.magic >= amount;
}

static void StartTime(Player* player, PlayState* play) {
    sApi->ReleaseTimeControl(ZONAI_KEY);
    sIsTimeStopped = false;
    SpawnRuneBurst(player, play);
    FlashScreen();
    TintScreen(play, 0);
    Rumble_Request(200.0f, 100, 15, 40);
    AudioSfx_StopByPos(&sSfxPos);
    PlayOwnSfx(player, NA_SE_EV_ICE_MELT);
    PlayReleaseChime();
}

static void StopTime(Player* player, PlayState* play) {
    if (!HasMagic(ZONAI_MAGIC_TO_START)) {
        Audio_PlaySfx(NA_SE_SY_ERROR);
        return;
    }
    gSaveContext.save.saveInfo.playerData.magic -= ZONAI_MAGIC_TO_START;
    sApi->RequestTimeControl(ZONAI_KEY, ZONAI_TIME_PRIORITY, 0.0f, true);
    sIsTimeStopped = true;
    sStoppedFrames = 0;
    SpawnRuneBurst(player, play);
    FlashScreen();
    TintScreen(play, ZONAI_TINT_ALPHA);
    Rumble_Request(300.0f, 150, 20, 60);
    AudioSfx_StopByPos(&sSfxPos);
    PlayOwnSfx(player, NA_SE_EV_ICE_FREEZE);
    Audio_PlaySfx(NA_SE_SY_SET_ICE_ARROW);
}

static void ToggleTime(PlayState* play, Player* player) {
    sToggleFrame = play->state.frames;
    if (sIsTimeStopped) {
        StartTime(player, play);
    } else {
        StopTime(player, play);
    }
}

static s32 FindEquippedButton(void) {
    for (u8 button = 0; button < S2H_ITEM_BUTTON_COUNT; button++) {
        const char* equipped = sApi->GetEquippedCustomItem(button);
        if (equipped != NULL && strcmp(equipped, ZONAI_KEY) == 0) {
            return button;
        }
    }
    return -1;
}

static void KeepTimeStopped(Player* player, PlayState* play) {
    if (sStoppedFrames < 0x7000) {
        sStoppedFrames++;
    }
    if ((sStoppedFrames % ZONAI_DRAIN_INTERVAL) == 0) {
        if (!HasMagic(ZONAI_MAGIC_PER_DRAIN)) {
            StartTime(player, play);
            return;
        }
        gSaveContext.save.saveInfo.playerData.magic -= ZONAI_MAGIC_PER_DRAIN;
    }
    bool isFlickering = !HasMagic(ZONAI_LOW_MAGIC) && (play->gameplayFrames % 4) < 2;
    if (isFlickering) {
        TintScreen(play, 0);
    } else {
        SpawnStillAir(player, play);
        TintScreen(play, (u8)(ZONAI_TINT_ALPHA + (s32)(Math_SinS((s16)(sStoppedFrames * 1200)) * ZONAI_TINT_PULSE)));
    }
    if ((play->gameplayFrames % 40) == 0) {
        PlayOwnSfx(player, NA_SE_EV_ICE_MELT);
    }
}

// Vanilla stops reading item buttons in midair or while swimming; the time stop keeps answering there.
static bool WasToggledOutsideItemButtons(Player* player, PlayState* play, s32 button) {
    return button >= 0 && (play->state.input[0].press.button & sItemButtons[button]) &&
           play->state.frames != sToggleFrame && !(player->stateFlags1 & ZONAI_BLOCKING_STATES) &&
           (!(player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) || (player->stateFlags1 & PLAYER_STATE1_8000000));
}

static void UpdateZonai(Player* player) {
    PlayState* play = gPlayState;

    BendReleaseChimeDown();
    if (play == NULL) {
        return;
    }
    s32 button = FindEquippedButton();
    if (sIsTimeStopped && button < 0) {
        StartTime(player, play);
        return;
    }
    if (WasToggledOutsideItemButtons(player, play, button)) {
        ToggleTime(play, player);
        return;
    }
    if (sIsTimeStopped) {
        KeepTimeStopped(player, play);
    }
}

static void ForgetStopOnSceneChange(s16 sceneId, s8 spawnNum) {
    sIsTimeStopped = false;
}

static bool CanToggleTime(Player* player, PlayState* play) {
    return !(player->stateFlags1 & ZONAI_BLOCKING_STATES);
}

// Shuffled once by the randomizer; no vanilla logic needs it.
static const S2HCustomItemRandomizer sRandomizerItem = {
    sizeof(S2HCustomItemRandomizer), 0, S2H_CUSTOM_ITEM_TYPE_ITEM, 0, 1, ZONAI_KEY, ZONAI_KEY, "",
};

S2H_MOD_EXPORT void ModSetApi(const S2HModApi* api) {
    sApi = api;
}

S2H_MOD_EXPORT const S2HModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

S2H_MOD_EXPORT void ModInit(void) {
    if (!S2H_MOD_API_HAS(sApi, RequestTimeControl)) {
        return;
    }

    S2HCustomItemDefinition zonai = Z64Items_Define(ZONAI_KEY, sIconTex, sNameTex);

    Z64Items_SetButtons(&zonai, S2H_CUSTOM_ITEM_C_BUTTON | S2H_CUSTOM_ITEM_DPAD);
    Z64Items_SetForms(&zonai, S2H_CUSTOM_ITEM_FORM_BIT(PLAYER_FORM_HUMAN));
    Z64Items_SetPlacement(&zonai, 0, 11, 0);
    Z64Items_SetTextbox(&zonai, "You got the %rZonai Permafrost%w!&A Zonai device that holds the&flow of time "
                                "perfectly still.^Press a %yC button%w to %cstop time%w. Only you&keep moving, and "
                                "enemies stay open&to your blows.^It spends %gmagic%w for as long as the&world "
                                "is frozen.");
    Z64Items_SetPauseText(&zonai, "%rZonai Permafrost&%wPress a %yC button%w to stop time, again to&let it run. "
                                  "Drains magic.");
    Z64Items_SetCanUse(&zonai, CanToggleTime);
    Z64Items_SetAction(&zonai, ToggleTime, NULL);
    zonai.flags |= S2H_CUSTOM_ITEM_INSTANT;
    Z64Items_SetGetItemModel(&zonai, sGiveDL);

    Z64Items_SetLogic(&zonai, &sRandomizerItem);
    if (!Z64Items_Register(sApi, &zonai)) {
        return;
    }
    S2H_REGISTER_HOOK(sApi, OnPlayerUpdate, UpdateZonai);
    S2H_REGISTER_HOOK(sApi, OnSceneInit, ForgetStopOnSceneChange);
}
