#include <stdbool.h>
#include <string.h>

#include "2s2h/ModApi/ModApi.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "objects/object_link_child/object_link_child.h"
#include <libultraship/bridge/consolevariablebridge.h>

#define ADULT_ASSET(name) "__OTR__objects/oot_object_link_boy/" name
#define ADULT_DL_PATH_SIZE 80
#define ADULT_FACE_PATH_SIZE 64
#define ADULT_HEIGHT 68.0f
#define DEITY_SCALE 1.5f
#define TUNIC_COLOR_CVAR "gCosmetic.Player.HumanTunic.Color"
#define TUNIC_CHANGED_CVAR "gCosmetic.Player.HumanTunic.Changed"

// The pointer in front keeps adultDL at an even address, which is how a display list is told from a segment.
typedef struct {
    const char* childDL;
    char adultDL[ADULT_DL_PATH_SIZE];
} LimbSwap;

static const LimbSwap sLimbSwaps[] = {
    { dgLinkHumanWaistDL, ADULT_ASSET("gLinkAdultWaistNearDL") },
    { dgLinkHumanLeftHandOpenDL, ADULT_ASSET("gLinkAdultLeftHandNearDL") },
    { dgLinkHumanLeftHandClosedDL, ADULT_ASSET("gLinkAdultLeftHandClosedNearDL") },
    { dgLinkHumanLeftHandHoldBottleDL, ADULT_ASSET("gLinkAdultHandHoldingBottleDL") },
    { dgLinkHumanRightHandOpenDL, ADULT_ASSET("gLinkAdultRightHandNearDL") },
    { dgLinkHumanRightHandClosedDL, ADULT_ASSET("gLinkAdultRightHandClosedNearDL") },
    { dgLinkHumanRightHandHoldingBowDL, ADULT_ASSET("gLinkAdultRightHandHoldingBowNearDL") },
    { dgLinkHumanRightHandHoldingHookshotDL, ADULT_ASSET("gLinkAdultRightHandHoldingHookshotNearDL") },
    { dgLinkHumanRightHandHoldingOcarinaDL, ADULT_ASSET("gLinkAdultRightHandHoldingOotNearDL") },
    { dgLinkHumanLeftForearmDL, ADULT_ASSET("gLinkAdultLeftArmOutNearDL") },
    { dgLinkHumanRightShoulderDL, ADULT_ASSET("gLinkAdultRightShoulderNearDL") },
    { dobject_link_child_DL_018490, ADULT_ASSET("gLinkAdultRightHandHoldingBowFirstPersonDL") },
    { dobject_link_child_DL_017B40, ADULT_ASSET("gLinkAdultRightHandHoldingHookshotFarDL") },
};

static const ALIGN_ASSET(2) char sAdultEyes[PLAYER_EYES_MAX][ADULT_FACE_PATH_SIZE] = {
    ADULT_ASSET("gLinkAdultEyesOpenTex"),      ADULT_ASSET("gLinkAdultEyesHalfTex"),
    ADULT_ASSET("gLinkAdultEyesClosedfTex"),   ADULT_ASSET("gLinkAdultEyesRollLeftTex"),
    ADULT_ASSET("gLinkAdultEyesRollRightTex"), ADULT_ASSET("gLinkAdultEyesShockTex"),
    ADULT_ASSET("gLinkAdultEyesUnk1Tex"),      ADULT_ASSET("gLinkAdultEyesUnk2Tex"),
};
static const ALIGN_ASSET(2) char sAdultMouths[PLAYER_MOUTH_MAX][ADULT_FACE_PATH_SIZE] = {
    ADULT_ASSET("gLinkAdultMouth1Tex"),
    ADULT_ASSET("gLinkAdultMouth2Tex"),
    ADULT_ASSET("gLinkAdultMouth3Tex"),
    ADULT_ASSET("gLinkAdultMouth4Tex"),
};
static const char sAdultSkeletonPath[] = "objects/oot_object_link_boy/gLinkAdultSkel";
static const Color_RGBA8 sKokiriGreen = { 30, 105, 27, 255 };

static const char* const sRequiredHooks[] = {
    "ShouldActorDraw",
    "OnActorDrawEnd",
    "OnPlayerResolveLimbDraw",
    "OnPlayerResolveFaceTextures",
};
static const S2HModRequirements sRequirements = { sizeof(S2HModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

extern SkeletonHeader* ResourceMgr_LoadSkeletonByName(const char* path, SkelAnime* skelAnime);
extern HOST_DATA PlayerAgeProperties sPlayerAgeProperties[PLAYER_FORM_MAX];
extern HOST_DATA TexturePtr sPlayerEyesTextures[PLAYER_FORM_MAX][PLAYER_EYES_MAX];
extern HOST_DATA TexturePtr sPlayerMouthTextures[PLAYER_FORM_MAX][PLAYER_MOUTH_MAX];

static const S2HModApi* sApi;
static PlayerAgeProperties sAdultBody;
static f32 sChildShadowScale;
static Player* sDrawnPlayer;
static void** sChildSkeleton;
static u8 sChildDListCount;
static Color_RGBA8 sTunicColor;

static f32* const sDeityLengths[] = {
    &sAdultBody.ceilingCheckHeight,
    &sAdultBody.unk_08,
    &sAdultBody.unk_0C,
    &sAdultBody.unk_10,
    &sAdultBody.unk_14,
    &sAdultBody.unk_18,
    &sAdultBody.unk_1C,
    &sAdultBody.unk_20,
    &sAdultBody.unk_24,
    &sAdultBody.unk_28,
    &sAdultBody.unk_2C,
    &sAdultBody.unk_30,
    &sAdultBody.wallCheckRadius,
    &sAdultBody.unk_3C,
    &sAdultBody.unk_40,
    &sAdultBody.unk_98,
    &sAdultBody.unk_9C,
};

static void BuildAdultBody(void) {
    sAdultBody = sPlayerAgeProperties[PLAYER_FORM_FIERCE_DEITY];
    for (size_t i = 0; i < ARRAY_COUNT(sDeityLengths); i++) {
        *sDeityLengths[i] /= DEITY_SCALE;
    }
}

static FlexSkeletonHeader* GetAdultSkeleton(void) {
    static FlexSkeletonHeader* sSkeleton;

    if (sSkeleton != NULL) {
        return sSkeleton;
    }
    SkeletonHeader* header = ResourceMgr_LoadSkeletonByName(sAdultSkeletonPath, NULL);
    if (header == NULL || header->segment == NULL || header->limbCount != PLAYER_LIMB_MAX - 1) {
        return NULL;
    }
    sSkeleton = (FlexSkeletonHeader*)header;
    return sSkeleton;
}

static Color_RGBA8 GetTunicColor(void) {
    if (CVarGetInteger(TUNIC_CHANGED_CVAR, 0) == 0) {
        return sKokiriGreen;
    }
    return CVarGetColor(TUNIC_COLOR_CVAR, sKokiriGreen);
}

static void RestoreChildSkeleton(void) {
    if (sDrawnPlayer == NULL) {
        return;
    }
    sDrawnPlayer->skelAnime.skeleton = sChildSkeleton;
    sDrawnPlayer->skelAnime.dListCount = sChildDListCount;
    sDrawnPlayer = NULL;
}

static void WearAdultSkeleton(Actor* actor, bool* should) {
    Player* player = (Player*)actor;

    RestoreChildSkeleton();
    if (!*should || player->transformation != PLAYER_FORM_HUMAN || !sApi->IsFormActive(S2H_FORM_KEY_ADULT)) {
        return;
    }
    FlexSkeletonHeader* skeleton = GetAdultSkeleton();
    if (skeleton == NULL) {
        return;
    }
    sDrawnPlayer = player;
    sChildSkeleton = player->skelAnime.skeleton;
    sChildDListCount = player->skelAnime.dListCount;
    player->skelAnime.skeleton = skeleton->sh.segment;
    player->skelAnime.dListCount = skeleton->dListCount;
    sTunicColor = GetTunicColor();
}

static void TakeOffAdultSkeleton(Actor* actor, PlayState* play) {
    RestoreChildSkeleton();
}

// OoT tints adult Link's tunic, cap and collar with the environment colour. It is set again for every limb
// because what MM draws at a hand or the head in between is free to change it.
static void TintTunic(void) {
    PlayState* play = gPlayState;

    OPEN_DISPS(play->state.gfxCtx);
    gDPSetEnvColor(POLY_OPA_DISP++, sTunicColor.r, sTunicColor.g, sTunicColor.b, 255);
    CLOSE_DISPS(play->state.gfxCtx);
}

static const char* FindAdultDL(const char* childDL) {
    for (size_t i = 0; i < ARRAY_COUNT(sLimbSwaps); i++) {
        if (strcmp(childDL, sLimbSwaps[i].childDL) == 0) {
            return sLimbSwaps[i].adultDL;
        }
    }
    return NULL;
}

static void ResolveAdultLimb(Player* player, s32 limbIndex, Gfx** dList, Gfx* limbDList, Vec3f* pos) {
    if (player != sDrawnPlayer) {
        return;
    }
    TintTunic();
    if (*dList == NULL) {
        return;
    }
    const char* adultDL = FindAdultDL((const char*)*dList);
    if (adultDL != NULL) {
        *dList = (Gfx*)adultDL;
    }
}

static void SwapFaceTexture(const char** texture, const TexturePtr* childSet,
                            const char (*adultSet)[ADULT_FACE_PATH_SIZE], s32 count) {
    for (s32 i = 0; i < count; i++) {
        if ((const void*)*texture == (const void*)childSet[i]) {
            *texture = adultSet[i];
            return;
        }
    }
}

static void ResolveAdultFace(const char** eyes, const char** mouth) {
    if (sDrawnPlayer == NULL) {
        return;
    }
    SwapFaceTexture(eyes, sPlayerEyesTextures[PLAYER_FORM_HUMAN], sAdultEyes, PLAYER_EYES_MAX);
    SwapFaceTexture(mouth, sPlayerMouthTextures[PLAYER_FORM_HUMAN], sAdultMouths, PLAYER_MOUTH_MAX);
}

static void EnterAdult(PlayState* play, Player* player) {
    player->actor.shape.shadowScale = sAdultBody.shadowScale;
}

static void ExitAdult(PlayState* play, Player* player) {
    if (player->transformation == PLAYER_FORM_HUMAN) {
        player->actor.shape.shadowScale = sChildShadowScale;
    }
}

static void RegisterForm(void) {
    S2HFormDefinition adult = { 0 };

    adult.structSize = sizeof(adult);
    adult.key = S2H_FORM_KEY_ADULT;
    adult.label = "Adult Link";
    adult.kind = S2H_FORM_KIND_LINK;
    adult.body = &sAdultBody;
    adult.height = ADULT_HEIGHT;
    adult.onEnter = EnterAdult;
    adult.onExit = ExitAdult;
    adult.isAdult = true;
    sApi->RegisterForm(&adult);
}

S2H_MOD_EXPORT void ModSetApi(const S2HModApi* api) {
    sApi = api;
}

S2H_MOD_EXPORT const S2HModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

S2H_MOD_EXPORT void ModInit(void) {
    if (!S2H_MOD_API_HAS(sApi, IsFormActive)) {
        return;
    }
    BuildAdultBody();
    sChildShadowScale = sPlayerAgeProperties[PLAYER_FORM_HUMAN].shadowScale;
    RegisterForm();
    S2H_REGISTER_HOOK_FOR_ID(sApi, ShouldActorDraw, ACTOR_PLAYER, WearAdultSkeleton);
    S2H_REGISTER_HOOK_FOR_ID(sApi, OnActorDrawEnd, ACTOR_PLAYER, TakeOffAdultSkeleton);
    S2H_REGISTER_HOOK(sApi, OnPlayerResolveLimbDraw, ResolveAdultLimb);
    S2H_REGISTER_HOOK(sApi, OnPlayerResolveFaceTextures, ResolveAdultFace);
}
