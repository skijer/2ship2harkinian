#include <stdbool.h>

#include "z64items.h"
#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"

#define OOT_ICON(name) "__OTR__textures/oot_icon_item_static/gItemIcon" name "Tex"
#define OOT_NAME(name) "__OTR__textures/oot_item_name_static/g" name "ItemNameENGTex"
#define OOT_MODEL(object, displayList) "__OTR__objects/oot_object_gi_" object "/" displayList
#define TEXTS(label, hint) \
    label, "%r" label "&%wBelongs to Ocarina of Time.", "You got %r" label "%w!&Belongs to Ocarina of Time. " hint

#define ICON_SIZE 32
#define ICON_HALF_EXTENT 12

// Bit 0 of a pointer in a graphics command marks it segmented, so these paths must sit on even addresses.
static const ALIGN_ASSET(2) char sEggMaterialDL[] = OOT_MODEL("egg", "gGiEggMaterialDL");
static const ALIGN_ASSET(2) char sEggDL[] = OOT_MODEL("egg", "gGiEggDL");
static const ALIGN_ASSET(2) char sChickenColorDL[] = OOT_MODEL("niwatori", "gGiChickenColorDL");
static const ALIGN_ASSET(2) char sCojiroColorDL[] = OOT_MODEL("niwatori", "gGiCojiroColorDL");
static const ALIGN_ASSET(2) char sChickenDL[] = OOT_MODEL("niwatori", "gGiChickenDL");
static const ALIGN_ASSET(2) char sChickenEyesDL[] = OOT_MODEL("niwatori", "gGiChickenEyesDL");
static const ALIGN_ASSET(2) char sLetterDL[] = OOT_MODEL("letter", "gGiLetterDL");
static const ALIGN_ASSET(2) char sLetterWritingDL[] = OOT_MODEL("letter", "gGiLetterWritingDL");
static const ALIGN_ASSET(2) char sSkullMaskDL[] = OOT_MODEL("skj_mask", "gGiSkullMaskDL");
static const ALIGN_ASSET(2) char sSpookyMaskDL[] = OOT_MODEL("redead_mask", "gGiSpookyMaskDL");
static const ALIGN_ASSET(2) char sGerudoMaskDL[] = OOT_MODEL("gerudomask", "gGiGerudoMaskDL");
static const ALIGN_ASSET(2) char sOddMushroomDL[] = OOT_MODEL("mushroom", "gGiOddMushroomDL");
static const ALIGN_ASSET(2) char sOddPotionDL[] = OOT_MODEL("powder", "gGiOddPotionDL");
static const ALIGN_ASSET(2) char sSawDL[] = OOT_MODEL("saw", "gGiSawDL");
static const ALIGN_ASSET(2) char sBrokenSwordDL[] = OOT_MODEL("brokensword", "gGiBrokenGoronSwordDL");
static const ALIGN_ASSET(2) char sPrescriptionDL[] = OOT_MODEL("prescription", "gGiPrescriptionDL");
static const ALIGN_ASSET(2) char sPrescriptionWritingDL[] = OOT_MODEL("prescription", "gGiPrescriptionWritingDL");
static const ALIGN_ASSET(2) char sFrogDL[] = OOT_MODEL("frog", "gGiFrogDL");
static const ALIGN_ASSET(2) char sFrogEyesDL[] = OOT_MODEL("frog", "gGiFrogEyesDL");
static const ALIGN_ASSET(2) char sEyeDropsCapDL[] = OOT_MODEL("eye_lotion", "gGiEyeDropsCapDL");
static const ALIGN_ASSET(2) char sEyeDropsBottleDL[] = OOT_MODEL("eye_lotion", "gGiEyeDropsBottleDL");
static const ALIGN_ASSET(2) char sClaimCheckDL[] = OOT_MODEL("ticketstone", "gGiClaimCheckDL");
static const ALIGN_ASSET(2) char sClaimCheckWritingDL[] = OOT_MODEL("ticketstone", "gGiClaimCheckWritingDL");
static const ALIGN_ASSET(2) char sFishingPoleIcon[] = OOT_ICON("FishingPole");

typedef struct {
    const char* opaque[2];
    const char* translucent;
    bool isUnfogged;
} ComboModel;

typedef struct {
    const char* key;
    const char* icon;
    const char* name;
    ComboModel model;
    const char* label;
    const char* pause;
    const char* text;
} ComboItem;

static const ComboItem sItems[] = {
    { "nei.combo.weird_egg",
      OOT_ICON("WeirdEgg"),
      OOT_NAME("WeirdEgg"),
      { { sEggMaterialDL, sEggDL }, NULL, true },
      TEXTS("Weird Egg", "Wait for this egg to hatch into the chicken that wakes Talon.") },
    { "nei.combo.chicken",
      OOT_ICON("Chicken"),
      OOT_NAME("Cucco"),
      { { sChickenColorDL, sChickenDL }, sChickenEyesDL, false },
      TEXTS("Chicken", "Use its crow to wake Talon outside Hyrule Castle.") },
    { "nei.combo.zeldas_letter",
      OOT_ICON("ZeldasLetter"),
      OOT_NAME("ZeldasLetter"),
      { { sLetterDL }, sLetterWritingDL, false },
      TEXTS("Zelda's Letter", "Show Zelda's letter to the guard at the Death Mountain gate.") },
    { "nei.combo.skull_mask",
      OOT_ICON("MaskSkull"),
      OOT_NAME("SkullMask"),
      { { sSkullMaskDL }, NULL, false },
      TEXTS("Skull Mask", "Sell this mask to the Skull Kid in the Lost Woods.") },
    { "nei.combo.spooky_mask",
      OOT_ICON("MaskSpooky"),
      OOT_NAME("SpookyMask"),
      { { sSpookyMaskDL }, NULL, false },
      TEXTS("Spooky Mask", "Sell this frightening mask to the boy in Kakariko Graveyard.") },
    { "nei.combo.gerudo_mask",
      OOT_ICON("MaskGerudo"),
      OOT_NAME("GerudoMask"),
      { { sGerudoMaskDL }, NULL, true },
      TEXTS("Gerudo Mask", "Wear this Gerudo disguise to hear different reactions from people.") },
    { "nei.combo.pocket_egg",
      OOT_ICON("PocketEgg"),
      OOT_NAME("PocketEgg"),
      { { sEggMaterialDL, sEggDL }, NULL, true },
      TEXTS("Pocket Egg", "Hatch this egg to begin the adult Cucco trade.") },
    { "nei.combo.pocket_cucco",
      OOT_ICON("PocketCucco"),
      OOT_NAME("PocketCucco"),
      { { sChickenColorDL, sChickenDL }, sChickenEyesDL, false },
      TEXTS("Pocket Cucco", "Wake Talon with this Cucco, then return it to the Cucco Lady.") },
    { "nei.combo.cojiro",
      OOT_ICON("Cojiro"),
      OOT_NAME("Cojiro"),
      { { sCojiroColorDL, sChickenDL }, sChickenEyesDL, false },
      TEXTS("Cojiro", "Bring this blue Cucco to its owner in the Lost Woods.") },
    { "nei.combo.odd_mushroom",
      OOT_ICON("OddMushroom"),
      OOT_NAME("OddMushroom"),
      { { sOddMushroomDL }, NULL, false },
      TEXTS("Odd Mushroom", "Rush this mushroom to the old potion brewer in Kakariko.") },
    { "nei.combo.odd_potion",
      OOT_ICON("OddPotion"),
      OOT_NAME("OddPotion"),
      { { sOddPotionDL }, NULL, false },
      TEXTS("Odd Potion", "Return this potion to the Lost Woods for the adult trade quest.") },
    { "nei.combo.poachers_saw",
      OOT_ICON("PoachersSaw"),
      OOT_NAME("PoachersSaw"),
      { { sSawDL }, NULL, false },
      TEXTS("Poacher's Saw", "Trade this saw to the carpenter boss in Gerudo Valley.") },
    { "nei.combo.broken_gorons_sword",
      OOT_ICON("BrokenGoronsSword"),
      OOT_NAME("BrokenGoronsSword"),
      { { sBrokenSwordDL }, NULL, false },
      TEXTS("Broken Goron's Sword", "Take the broken blade to Biggoron for repairs.") },
    { "nei.combo.prescription",
      OOT_ICON("Prescription"),
      OOT_NAME("Prescription"),
      { { sPrescriptionDL }, sPrescriptionWritingDL, false },
      TEXTS("Prescription", "Show Biggoron's prescription to King Zora to obtain an Eyeball Frog.") },
    { "nei.combo.eyeball_frog",
      OOT_ICON("EyeballFrog"),
      OOT_NAME("EyeBallFrog"),
      { { sFrogDL }, sFrogEyesDL, false },
      TEXTS("Eyeball Frog", "Rush this frog to the Lakeside Laboratory for eye drops.") },
    { "nei.combo.eye_drops",
      OOT_ICON("EyeDrops"),
      OOT_NAME("EyeDrops"),
      { { sEyeDropsCapDL }, sEyeDropsBottleDL, false },
      TEXTS("World's Finest Eye Drops", "Bring these eye drops to Biggoron before they expire.") },
    { "nei.combo.claim_check",
      OOT_ICON("ClaimCheck"),
      OOT_NAME("ClaimCheck"),
      { { sClaimCheckDL }, sClaimCheckWritingDL, false },
      TEXTS("Claim Check", "Exchange this claim check with Biggoron when the sword is ready.") },
    { "nei.combo.fishing_pole",
      sFishingPoleIcon,
      NULL,
      { { NULL }, NULL, false },
      TEXTS("Fishing Pole", "Use this rod to fish at the Lake Hylia Fishing Pond.") },
};

static const S2HModApi* sApi;
static const S2HModRequirements sRequirements = { sizeof(S2HModRequirements), NULL, 0 };

static void DrawModel(PlayState* play, const ComboModel* model) {
    OPEN_DISPS(play->state.gfxCtx);
    if (model->isUnfogged) {
        Gfx_SetupDL26_Opa(play->state.gfxCtx);
        // OoT's setup DL 26 is one-cycle and these models repeat their combiner; MM's second cycle reads a stale texel.
        gDPSetCycleType(POLY_OPA_DISP++, G_CYC_1CYCLE);
        gDPSetRenderMode(POLY_OPA_DISP++, G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2);
    } else {
        Gfx_SetupDL25_Opa(play->state.gfxCtx);
    }
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    for (u32 i = 0; i < ARRAY_COUNT(model->opaque) && model->opaque[i] != NULL; i++) {
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)model->opaque[i]);
    }
    if (model->translucent != NULL) {
        Gfx_SetupDL25_Xlu(play->state.gfxCtx);
        MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)model->translucent);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawIconBillboard(PlayState* play, const char* icon) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    Vtx* corners = GRAPH_ALLOC(play->state.gfxCtx, 4 * sizeof(Vtx));
    const s16 texel = ICON_SIZE << 5;
    corners[0] = (Vtx)VTX(-ICON_HALF_EXTENT, ICON_HALF_EXTENT, 0, 0, 0, 255, 255, 255, 255);
    corners[1] = (Vtx)VTX(ICON_HALF_EXTENT, ICON_HALF_EXTENT, 0, texel, 0, 255, 255, 255, 255);
    corners[2] = (Vtx)VTX(-ICON_HALF_EXTENT, -ICON_HALF_EXTENT, 0, 0, texel, 255, 255, 255, 255);
    corners[3] = (Vtx)VTX(ICON_HALF_EXTENT, -ICON_HALF_EXTENT, 0, texel, texel, 255, 255, 255, 255);
    gSPClearGeometryMode(POLY_XLU_DISP++, G_CULL_BOTH | G_LIGHTING);
    gDPSetRenderMode(POLY_XLU_DISP++, G_RM_FOG_SHADE_A, G_RM_AA_ZB_XLU_SURF2);
    gDPSetCombineMode(POLY_XLU_DISP++, G_CC_MODULATEIA_PRIM, G_CC_PASS2);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, 255);
    gDPLoadTextureBlock(POLY_XLU_DISP++, icon, G_IM_FMT_RGBA, G_IM_SIZ_32b, ICON_SIZE, ICON_SIZE, 0, G_TX_CLAMP,
                        G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gSPVertex(POLY_XLU_DISP++, (uintptr_t)corners, 4, 0);
    gSP2Triangles(POLY_XLU_DISP++, 0, 2, 1, 0, 1, 2, 3, 0);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawReward(PlayState* play, S2HGetItemEntry* entry) {
    if (entry->drawItemId >= ARRAY_COUNT(sItems)) {
        return;
    }
    const ComboItem* item = &sItems[entry->drawItemId];
    if (item->model.opaque[0] == NULL) {
        DrawIconBillboard(play, item->icon);
        return;
    }
    if (sApi->HasResource(item->model.opaque[0])) {
        DrawModel(play, &item->model);
    }
}

S2H_MOD_EXPORT void ModSetApi(const S2HModApi* api) {
    sApi = api;
}

S2H_MOD_EXPORT const S2HModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

S2H_MOD_EXPORT void ModInit(void) {
    for (u16 i = 0; i < ARRAY_COUNT(sItems); i++) {
        const ComboItem* item = &sItems[i];
        S2HCustomItemDefinition definition = Z64Items_Define(item->key, item->icon, item->name);
        S2HCustomItemRandomizer randomizer = { 0 };
        randomizer.structSize = sizeof(randomizer);
        randomizer.type = S2H_CUSTOM_ITEM_TYPE_ITEM;
        randomizer.poolCount = 0;
        randomizer.logicKey = item->key;
        randomizer.hintKey = item->key;
        definition.displayName = item->label;
        definition.pauseText = item->pause;
        definition.getItemText = item->text;
        definition.getItemEntry.drawFunc = DrawReward;
        definition.getItemEntry.drawItemId = i;
        definition.presentationFlags = S2H_ITEM_HIDE_FROM_KALEIDO;
        definition.randomizer = &randomizer;
        if (!Z64Items_Register(sApi, &definition)) {
            return;
        }
    }
}
