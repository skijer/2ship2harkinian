#include <stdbool.h>
#include <stdint.h>

#include "global.h"
#include "2s2h/ModApi/ModApi.h"
#include "2s2h/ModApi/ModMenu/ModUI.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include "objects/object_tree/object_tree.h"
#include "overlays/actors/ovl_En_Cow/z_en_cow.h"
#include "overlays/actors/ovl_En_Sw/z_en_sw.h"

#define STYLE_SERVICE "rando.container_style"
#define PREVIEW_SERVICE "container_colors.preview_style"
#define COLORS_SIDEBAR "Container Colors"
#define FULL_TINT 255
#define SKULLTULA_TILT -0x3C72

typedef uint8_t (*ContainerStyleFunc)(Actor* actor);

typedef struct {
    const char* label;
    const char* cvar;
    uint32_t defaultColor;
} StyleTint;

typedef struct {
    s32 limbIndex;
    Gfx* goldDList;
} GoldLimb;

static const StyleTint sTints[] = {
    [S2H_CONTAINER_STYLE_LESSER] = { "Lesser Item", "gMods.ContainerColors.Lesser", 0xFA7828 },
    [S2H_CONTAINER_STYLE_MAJOR] = { "Major Item", "gMods.ContainerColors.Major", 0xFAB928 },
    [S2H_CONTAINER_STYLE_HEALTH] = { "Heart", "gMods.ContainerColors.Health", 0xFA283C },
    [S2H_CONTAINER_STYLE_SMALL_KEY] = { "Small Key", "gMods.ContainerColors.SmallKey", 0xBEBEC8 },
    [S2H_CONTAINER_STYLE_BOSS_KEY] = { "Boss Key", "gMods.ContainerColors.BossKey", 0x00C8FF },
    [S2H_CONTAINER_STYLE_TOKEN] = { "Skulltula Token", "gMods.ContainerColors.Token", 0xFFFABE },
    [S2H_CONTAINER_STYLE_MASK] = { "Mask", "gMods.ContainerColors.Mask", 0xB43CDC },
    [S2H_CONTAINER_STYLE_STRAY_FAIRY] = { "Stray Fairy", "gMods.ContainerColors.StrayFairy", 0xFF82DC },
};

static const GoldLimb sGoldLimbs[] = {
    { OBJECT_ST_LIMB_17, (Gfx*)object_st_DL_004788 }, { OBJECT_ST_LIMB_08, (Gfx*)object_st_DL_0046F0 },
    { OBJECT_ST_LIMB_0E, (Gfx*)object_st_DL_004658 }, { OBJECT_ST_LIMB_0B, (Gfx*)object_st_DL_0045C0 },
    { OBJECT_ST_LIMB_1A, (Gfx*)object_st_DL_004820 }, { OBJECT_ST_LIMB_14, (Gfx*)object_st_DL_0048B8 },
    { OBJECT_ST_LIMB_11, (Gfx*)object_st_DL_004950 }, { OBJECT_ST_LIMB_1D, (Gfx*)object_st_DL_0049E8 },
    { OBJECT_ST_LIMB_05, (Gfx*)object_st_DL_003FB0 }, { OBJECT_ST_LIMB_04, (Gfx*)object_st_DL_0043D8 },
};

static const char* const sRequiredHooks[] = { "ShouldActorDraw", "OnActorResolveGrayscale", "OnActorDrawBegin",
                                              "OnActorDrawEnd" };
static const S2HModRequirements sRequirements = { sizeof(S2HModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const S2HModApi* sApi;
static ContainerStyleFunc sGetContainerStyle;
static ContainerStyleFunc sGetPreviewStyle;
static bool sAreServicesResolved;
static Color_RGB8 sLimbTint;
static Actor* sTintedLeaves;
// Vanilla EnCow focus point, in head limb space.
static Vec3f sCowFocusOffset = { 0.0f, -1300.0f, 1100.0f };

static bool IsTintedStyle(uint8_t style) {
    return style < ARRAY_COUNT(sTints) && sTints[style].cvar != NULL;
}

static Color_RGB8 GetStyleColor(uint8_t style) {
    uint32_t packed = (uint32_t)CVarGetInteger(sTints[style].cvar, (int32_t)sTints[style].defaultColor);
    Color_RGB8 color = { (u8)(packed >> 16), (u8)(packed >> 8), (u8)packed };
    return color;
}

static void SetStyleColor(uint8_t style, const float* rgb) {
    uint32_t red = (uint32_t)(rgb[0] * 255.0f + 0.5f);
    uint32_t green = (uint32_t)(rgb[1] * 255.0f + 0.5f);
    uint32_t blue = (uint32_t)(rgb[2] * 255.0f + 0.5f);
    CVarSetInteger(sTints[style].cvar, (int32_t)((red << 16) | (green << 8) | blue));
    CVarSave();
}

// Looked up on the first draw, once every mod has published its services.
static void ResolveStyleServices(void) {
    sGetContainerStyle = (ContainerStyleFunc)sApi->FindService(STYLE_SERVICE);
    sGetPreviewStyle = (ContainerStyleFunc)sApi->FindService(PREVIEW_SERVICE);
    sAreServicesResolved = true;
}

static uint8_t GetActorStyle(Actor* actor) {
    if (!sAreServicesResolved) {
        ResolveStyleServices();
    }
    uint8_t preview = sGetPreviewStyle != NULL ? sGetPreviewStyle(actor) : S2H_CONTAINER_STYLE_NONE;
    if (preview != S2H_CONTAINER_STYLE_NONE) {
        return preview;
    }
    return sGetContainerStyle != NULL ? sGetContainerStyle(actor) : S2H_CONTAINER_STYLE_NONE;
}

static bool ResolveTint(Actor* actor, Color_RGB8* tint) {
    uint8_t style = GetActorStyle(actor);

    if (!IsTintedStyle(style)) {
        return false;
    }
    *tint = GetStyleColor(style);
    return true;
}

static void BeginOpaTint(PlayState* play, Color_RGB8 tint) {
    OPEN_DISPS(play->state.gfxCtx);
    gDPSetGrayscaleColor(POLY_OPA_DISP++, tint.r, tint.g, tint.b, FULL_TINT);
    gSPGrayscale(POLY_OPA_DISP++, true);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void EndOpaTint(PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    gSPGrayscale(POLY_OPA_DISP++, false);
    CLOSE_DISPS(play->state.gfxCtx);
}

static s32 ShowCowNoseRing(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, Actor* thisx) {
    EnCow* cow = (EnCow*)thisx;

    if (limbIndex == COW_LIMB_HEAD) {
        rot->y += cow->headTilt.y;
        rot->x += cow->headTilt.x;
    }
    if (limbIndex == COW_LIMB_NOSE_RING) {
        BeginOpaTint(play, sLimbTint);
    }
    return false;
}

static void FinishCowLimb(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, Actor* thisx) {
    if (limbIndex == COW_LIMB_HEAD) {
        Matrix_MultVec3f(&sCowFocusOffset, &thisx->focus.pos);
    }
    if (limbIndex == COW_LIMB_NOSE_RING) {
        EndOpaTint(play);
    }
}

// Vanilla hides the nose ring; a cow holding an item wears it in the item's color.
static void DrawStyledCow(Actor* actor, bool* should) {
    EnCow* cow = (EnCow*)actor;

    if (!*should || EN_COW_TYPE(actor) == EN_COW_TYPE_TAIL || !ResolveTint(actor, &sLimbTint)) {
        return;
    }
    *should = false;
    Gfx_SetupDL37_Opa(gPlayState->state.gfxCtx);
    SkelAnime_DrawFlexOpa(gPlayState, cow->skelAnime.skeleton, cow->skelAnime.jointTable, cow->skelAnime.dListCount,
                          ShowCowNoseRing, FinishCowLimb, actor);
}

static bool IsSkulltulaBody(s32 limbIndex) {
    return limbIndex == OBJECT_ST_LIMB_04 || limbIndex == OBJECT_ST_LIMB_05;
}

static s32 DressGoldSkulltula(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, Actor* thisx) {
    for (s32 i = 0; i < ARRAY_COUNT(sGoldLimbs); i++) {
        if (sGoldLimbs[i].limbIndex == limbIndex) {
            *dList = sGoldLimbs[i].goldDList;
            break;
        }
    }
    if (IsSkulltulaBody(limbIndex)) {
        BeginOpaTint(play, sLimbTint);
    }
    return false;
}

static void FinishSkulltulaLimb(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, Actor* thisx) {
    if (IsSkulltulaBody(limbIndex)) {
        EndOpaTint(play);
    }
}

static bool HasDamageEffect(const EnSw* skulltula) {
    for (s32 i = 0; i < skulltula->unk_462 && i < ENSW_BODYPART_MAX; i++) {
        if (skulltula->unk_464[i] != 0) {
            return true;
        }
    }
    return false;
}

// Mirrors EnSw_Draw for gold Skulltulas; while an elemental hit plays, vanilla draws its effects instead.
static void DrawStyledSkulltula(Actor* actor, bool* should) {
    EnSw* skulltula = (EnSw*)actor;

    if (!*should || !ENSW_GET_3(actor) || HasDamageEffect(skulltula) || !ResolveTint(actor, &sLimbTint)) {
        return;
    }
    *should = false;
    if (!(skulltula->unk_410 & 4)) {
        return;
    }
    func_800B8050(actor, gPlayState, 0);
    Gfx_SetupDL25_Opa(gPlayState->state.gfxCtx);
    Matrix_RotateXS(SKULLTULA_TILT, MTXMODE_APPLY);
    SkelAnime_DrawOpa(gPlayState, skulltula->skelAnime.skeleton, skulltula->skelAnime.jointTable, DressGoldSkulltula,
                      FinishSkulltulaLimb, actor);
}

// Mirrors ObjTree_Draw, tinting only the leaves.
static void DrawStyledTree(Actor* actor, bool* should) {
    Color_RGB8 tint;

    if (!*should || !ResolveTint(actor, &tint)) {
        return;
    }
    *should = false;
    PlayState* play = gPlayState;
    s16 xRot = actor->shape.rot.x;
    s16 zRot = actor->shape.rot.z;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gTreeBodyDL);
    Matrix_RotateZYX(xRot, 0, zRot, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gDPSetGrayscaleColor(POLY_OPA_DISP++, tint.r, tint.g, tint.b, FULL_TINT);
    gSPGrayscale(POLY_OPA_DISP++, true);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gTreeLeavesDL);
    gSPGrayscale(POLY_OPA_DISP++, false);
    CLOSE_DISPS(play->state.gfxCtx);
}

// EnWood02 draws its trunk opaque and its leaves translucent, so tinting the XLU list tints only the leaves.
static void BeginLeavesTint(Actor* actor, PlayState* play) {
    Color_RGB8 tint;

    if (!ResolveTint(actor, &tint)) {
        return;
    }
    sTintedLeaves = actor;
    OPEN_DISPS(play->state.gfxCtx);
    gDPSetGrayscaleColor(POLY_XLU_DISP++, tint.r, tint.g, tint.b, FULL_TINT);
    gSPGrayscale(POLY_XLU_DISP++, true);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void EndLeavesTint(Actor* actor, PlayState* play) {
    if (sTintedLeaves != actor) {
        return;
    }
    sTintedLeaves = NULL;
    OPEN_DISPS(play->state.gfxCtx);
    gSPGrayscale(POLY_XLU_DISP++, false);
    CLOSE_DISPS(play->state.gfxCtx);
}

// Another mod's tint (a time stop) wins over the container color.
static void TintWholeActor(Actor* actor, PlayState* play, Color_RGBA8* grayscale) {
    Color_RGB8 tint;

    if (grayscale->a != 0 || !ResolveTint(actor, &tint)) {
        return;
    }
    grayscale->r = tint.r;
    grayscale->g = tint.g;
    grayscale->b = tint.b;
    grayscale->a = FULL_TINT;
}

static void ResetColors(void) {
    for (uint8_t style = 0; style < ARRAY_COUNT(sTints); style++) {
        if (IsTintedStyle(style)) {
            CVarClear(sTints[style].cvar);
        }
    }
    CVarSave();
}

static void DrawColorMenu(void) {
    ModUI_Text("Shown once Container Style Matches Contents is on. Junk keeps the vanilla look.");
    for (uint8_t style = 0; style < ARRAY_COUNT(sTints); style++) {
        if (!IsTintedStyle(style)) {
            continue;
        }
        Color_RGB8 color = GetStyleColor(style);
        float rgb[3] = { color.r / 255.0f, color.g / 255.0f, color.b / 255.0f };

        if (ModUI_ColorEdit3(sTints[style].label, rgb, 0)) {
            SetStyleColor(style, rgb);
        }
    }
    if (ModUI_Button("Reset Colors", 0.0f, 0.0f)) {
        ResetColors();
    }
}

static void RegisterMenu(void) {
    if (!S2H_MOD_API_HAS(sApi, RegisterMenuSidebar)) {
        return;
    }
    sApi->RegisterMenuSidebar("Mods", COLORS_SIDEBAR, "gMods.Menu.Page", 1);

    S2HModMenuWidget colors = { sizeof(S2HModMenuWidget) };
    colors.section = "Mods";
    colors.sidebar = COLORS_SIDEBAR;
    colors.type = S2H_MOD_MENU_CUSTOM;
    colors.label = COLORS_SIDEBAR;
    colors.onChange = DrawColorMenu;
    sApi->RegisterMenuWidget(&colors);
}

static void RegisterTints(void) {
    static const s16 sWholeTintActors[] = { ACTOR_OBJ_COMB, ACTOR_OBJ_SNOWBALL, ACTOR_OBJ_SNOWBALL2, ACTOR_OBJ_YASI,
                                            ACTOR_EN_SNOWWD };

    S2H_REGISTER_HOOK_FOR_ID(sApi, ShouldActorDraw, ACTOR_EN_COW, DrawStyledCow);
    S2H_REGISTER_HOOK_FOR_ID(sApi, ShouldActorDraw, ACTOR_EN_SW, DrawStyledSkulltula);
    S2H_REGISTER_HOOK_FOR_ID(sApi, ShouldActorDraw, ACTOR_OBJ_TREE, DrawStyledTree);
    S2H_REGISTER_HOOK_FOR_ID(sApi, OnActorDrawBegin, ACTOR_EN_WOOD02, BeginLeavesTint);
    S2H_REGISTER_HOOK_FOR_ID(sApi, OnActorDrawEnd, ACTOR_EN_WOOD02, EndLeavesTint);
    for (s32 i = 0; i < ARRAY_COUNT(sWholeTintActors); i++) {
        S2H_REGISTER_HOOK_FOR_ID(sApi, OnActorResolveGrayscale, sWholeTintActors[i], TintWholeActor);
    }
}

S2H_MOD_EXPORT void ModSetApi(const S2HModApi* api) {
    sApi = api;
}

S2H_MOD_EXPORT const S2HModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

S2H_MOD_EXPORT void ModInit(void) {
    RegisterMenu();
    RegisterTints();
}
