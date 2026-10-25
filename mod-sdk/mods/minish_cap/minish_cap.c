/**
 * The Minish Cap: shrinks Link to Minish size and back. The pod-soil warp map of the Ocarina of Time version has
 * no counterpart in Majora's Mask, so only the size change is ported.
 */

#include "z64items.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "regs.h"
#include "variables.h"

#define MINISH_CAP_KEY "nei.minish_cap"

#define PLAYER_SCALE_NORMAL 0.01f
#define MINISH_SCALE 0.001f
#define MINISH_SIZE_FACTOR (MINISH_SCALE / PLAYER_SCALE_NORMAL)
#define MINISH_SPEED_FACTOR 0.2f
#define SCALE_STEP 0.0005f
#define MINISH_CAMERA_FLOOR 0.14f
#define MINISH_BODY_EXTENT 2

typedef enum {
    MINISH_SIZE_NORMAL,
    MINISH_SIZE_SHRINKING,
    MINISH_SIZE_TINY,
    MINISH_SIZE_GROWING,
} MinishSizePhase;

static const ALIGN_ASSET(2) char sIconTex[] = "__OTR__textures/icon_item_custom/gItemIconMinishCapTex";
static const ALIGN_ASSET(2) char sNameTex[] = "__OTR__textures/item_name_custom/gMinishCapNameTex";
static const ALIGN_ASSET(2) char sCapDL[] = "__OTR__objects/object_nei_minish_cap/Cylinder_opaque_dl";

static const char* const sRequiredHooks[] = {
    "OnPlayerUpdate",
    "OnCameraResolveView",
    "OnPlayerResolveMotionScale",
};
static const S2HModRequirements sRequirements = { sizeof(S2HModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const S2HModApi* sApi;

static MinishSizePhase sSizePhase;
static f32 sPlayerScale = PLAYER_SCALE_NORMAL;
static bool sIsUsePending;
static s16 sLastScene = -1;
static u32 sLastFrames;

// Link's own properties with a Minish-sized wall radius; the vanilla ones are only resolved when the player is
// created, so the mod swaps the pointer on him and hands it back when it is done.
static PlayerAgeProperties sTinyAgeProperties;
static PlayerAgeProperties* sVanillaAgeProperties;
static s16 sVanillaCylinderRadius;

static bool IsTiny(void) {
    return sSizePhase != MINISH_SIZE_NORMAL;
}

static void ApplyTinyProperties(Player* player) {
    sVanillaAgeProperties = player->ageProperties;
    sTinyAgeProperties = *player->ageProperties;
    sTinyAgeProperties.wallCheckRadius *= MINISH_SIZE_FACTOR;
    player->ageProperties = &sTinyAgeProperties;
    sVanillaCylinderRadius = player->cylinder.dim.radius;
}

static void RestoreTinyProperties(Player* player) {
    if (player != NULL && player->ageProperties == &sTinyAgeProperties) {
        player->ageProperties = sVanillaAgeProperties;
        player->cylinder.dim.radius = sVanillaCylinderRadius;
        Collider_UpdateCylinder(&player->actor, &player->cylinder);
    }
}

static void EndMinishSize(Player* player) {
    RestoreTinyProperties(player);
    sSizePhase = MINISH_SIZE_NORMAL;
}

static void StartShrinking(Player* player) {
    ApplyTinyProperties(player);
    sSizePhase = MINISH_SIZE_SHRINKING;
    Audio_PlaySfx(NA_SE_SY_CAMERA_ZOOM_UP);
}

// Growing back under a low ceiling would bury normal-sized Link in the rock above him.
static void StartGrowing(Player* player, PlayState* play) {
    Vec3f checkPos = player->actor.world.pos;
    CollisionPoly* ceilingPoly;
    s32 ceilingBgId;
    f32 ceilingY;

    checkPos.y += 2.0f;
    if (BgCheck_EntityCheckCeiling(&play->colCtx, &ceilingY, &checkPos, sVanillaAgeProperties->ceilingCheckHeight,
                                   &ceilingPoly, &ceilingBgId, &player->actor)) {
        Audio_PlaySfx(NA_SE_SY_ERROR);
        return;
    }
    sSizePhase = MINISH_SIZE_GROWING;
    Audio_PlaySfx(NA_SE_SY_CAMERA_ZOOM_DOWN);
}

// The per-frame code recomputes the cylinder height from the skeleton but never its radius, so Minish Link
// would keep a hitbox ten times his body.
static void ApplyTinyBody(Player* player) {
    player->cylinder.dim.radius = MINISH_BODY_EXTENT;
    player->cylinder.dim.height = MINISH_BODY_EXTENT;
    Collider_UpdateCylinder(&player->actor, &player->cylinder);
}

static void UpdateMinishSize(Player* player) {
    if (sSizePhase == MINISH_SIZE_SHRINKING) {
        sPlayerScale -= SCALE_STEP;
        if (sPlayerScale <= MINISH_SCALE) {
            sPlayerScale = MINISH_SCALE;
            sSizePhase = MINISH_SIZE_TINY;
        }
    } else if (sSizePhase == MINISH_SIZE_GROWING) {
        sPlayerScale += SCALE_STEP;
        if (sPlayerScale >= PLAYER_SCALE_NORMAL) {
            sPlayerScale = PLAYER_SCALE_NORMAL;
            EndMinishSize(player);
        }
    }

    Actor_SetScale(&player->actor, sPlayerScale);
    if (IsTiny()) {
        ApplyTinyBody(player);
    }
}

static void ResolveMotionScale(Player* player, s32 kind, f32* scale) {
    if (sSizePhase != MINISH_SIZE_TINY) {
        return;
    }
    if (kind == SOH_PLAYER_MOTION_STICK_SPEED) {
        *scale *= MINISH_SPEED_FACTOR;
        return;
    }
    // The cap the engine measured against the wall he faces: at Minish size he is next to one everywhere.
    *scale = R_RUN_SPEED_LIMIT / 100.0f;
}

static void ResolveCameraView(Camera* camera, Vec3f* eye, Vec3f* at, Vec3f* up) {
    PlayState* play = gPlayState;
    Player* player;
    f32 rigScale;

    if (camera == NULL || play == NULL || camera->camId != CAM_ID_MAIN || !IsTiny()) {
        return;
    }
    player = GET_PLAYER(play);
    if (camera->focalActor != &player->actor) {
        return;
    }
    // The whole rig shrinks toward Link with him, so the zoom eases in and out with the animation. The floor
    // keeps the eye outside the near plane.
    rigScale = player->actor.scale.x / PLAYER_SCALE_NORMAL;
    if (rigScale >= 0.999f) {
        return;
    }
    rigScale = CLAMP_MIN(rigScale, MINISH_CAMERA_FLOOR);

    eye->x = player->actor.world.pos.x + (eye->x - player->actor.world.pos.x) * rigScale;
    eye->y = player->actor.world.pos.y + (eye->y - player->actor.world.pos.y) * rigScale;
    eye->z = player->actor.world.pos.z + (eye->z - player->actor.world.pos.z) * rigScale;
    at->x = player->actor.world.pos.x + (at->x - player->actor.world.pos.x) * rigScale;
    at->y = player->actor.world.pos.y + (at->y - player->actor.world.pos.y) * rigScale;
    at->z = player->actor.world.pos.z + (at->z - player->actor.world.pos.z) * rigScale;
}

static void BlockLegPlanting(bool* should, va_list args) {
    if (IsTiny()) {
        *should = false;
    }
}

static bool CanUseCap(Player* player, PlayState* play) {
    return play->pauseCtx.state == PAUSE_STATE_OFF && play->transitionTrigger == TRANS_TRIGGER_OFF;
}

static void UseCap(PlayState* play, Player* player) {
    sIsUsePending = true;
}

// A press resolves here, at the end of the player's update, so nothing the rest of the frame chooses runs over
// the size change.
static void StartPendingUse(Player* player, PlayState* play) {
    if (!sIsUsePending) {
        return;
    }
    sIsUsePending = false;
    if (!CanUseCap(player, play)) {
        return;
    }

    if (sSizePhase == MINISH_SIZE_TINY) {
        StartGrowing(player, play);
        return;
    }
    if (sSizePhase != MINISH_SIZE_NORMAL) {
        return;
    }
    // A form owns Link's scale; two owners would fight over it every frame.
    if (sApi->GetActiveForm() != NULL) {
        Audio_PlaySfx(NA_SE_SY_ERROR);
        return;
    }
    StartShrinking(player);
}

static void ResetOnSceneLoad(Player* player) {
    if (IsTiny()) {
        EndMinishSize(player);
    }
    sPlayerScale = PLAYER_SCALE_NORMAL;
    sIsUsePending = false;
}

static void UpdateMinishCap(Player* player) {
    PlayState* play = gPlayState;
    bool isSceneLoaded;

    if (play == NULL) {
        return;
    }
    isSceneLoaded = (play->sceneId != sLastScene) || (play->state.frames < sLastFrames);
    sLastScene = play->sceneId;
    sLastFrames = play->state.frames;
    if (isSceneLoaded) {
        ResetOnSceneLoad(player);
    }

    StartPendingUse(player, play);
    if (IsTiny()) {
        UpdateMinishSize(player);
    }
}

// Shuffled once by the randomizer; no vanilla logic needs it.
static const S2HCustomItemRandomizer sRandomizerItem = {
    sizeof(S2HCustomItemRandomizer), 0, S2H_CUSTOM_ITEM_TYPE_ITEM, 0, 1, MINISH_CAP_KEY, MINISH_CAP_KEY, "",
};

S2H_MOD_EXPORT void ModSetApi(const S2HModApi* api) {
    sApi = api;
}

S2H_MOD_EXPORT const S2HModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

S2H_MOD_EXPORT void ModInit(void) {
    S2HCustomItemDefinition cap = Z64Items_Define(MINISH_CAP_KEY, sIconTex, sNameTex);

    if (!S2H_MOD_API_HAS(sApi, GetActiveForm)) {
        return;
    }
    Z64Items_SetForms(&cap, S2H_CUSTOM_ITEM_FORM_BIT(PLAYER_FORM_HUMAN));
    Z64Items_SetButtons(&cap, S2H_CUSTOM_ITEM_C_BUTTON | S2H_CUSTOM_ITEM_DPAD);
    Z64Items_SetPlacement(&cap, 0, 18, 0);
    Z64Items_SetTextbox(&cap, "You got %rThe Minish Cap%w!&Ezlo, who was a sage before he was&a hat, and is in a "
                              "mood about it.^Press %y\xA1%w to become %yMinish%w:&small and slow, but small enough "
                              "for&a crawlspace. Press again, or load a&scene, to grow back.");
    Z64Items_SetPauseText(&cap, "%rThe Minish Cap&%wPress %y\xA1%w to shrink or grow.");
    Z64Items_SetCanUse(&cap, CanUseCap);
    Z64Items_SetAction(&cap, UseCap, NULL);
    Z64Items_SetGetItemModel(&cap, sCapDL);
    cap.flags |= S2H_CUSTOM_ITEM_INSTANT;

    Z64Items_SetLogic(&cap, &sRandomizerItem);
    if (!Z64Items_Register(sApi, &cap)) {
        return;
    }
    S2H_REGISTER_HOOK(sApi, OnPlayerUpdate, UpdateMinishCap);
    S2H_REGISTER_HOOK(sApi, OnCameraResolveView, ResolveCameraView);
    S2H_REGISTER_HOOK(sApi, OnPlayerResolveMotionScale, ResolveMotionScale);
    sApi->RegisterVB(VB_PLAYER_ADJUST_LEGS_TO_FLOOR, BlockLegPlanting);
}
