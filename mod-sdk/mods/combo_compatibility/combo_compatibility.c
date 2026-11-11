#include "z64items.h"
#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include <string.h>

typedef struct {
    const char* key;
    const char* label;
    const char* icon;
    const char* name;
    const char* pause;
    const char* text;
} ComboItem;

static const ALIGN_ASSET(2) char sIcon0[] = "__OTR__textures/nei_combo_mm/weird_egg";
static const ALIGN_ASSET(2) char sName0[] = "__OTR__textures/nei_combo_mm/weird_egg_name";
static const ALIGN_ASSET(2) char sIcon1[] = "__OTR__textures/nei_combo_mm/chicken";
static const ALIGN_ASSET(2) char sName1[] = "__OTR__textures/nei_combo_mm/chicken_name";
static const ALIGN_ASSET(2) char sIcon2[] = "__OTR__textures/nei_combo_mm/zeldas_letter";
static const ALIGN_ASSET(2) char sName2[] = "__OTR__textures/nei_combo_mm/zeldas_letter_name";
static const ALIGN_ASSET(2) char sIcon3[] = "__OTR__textures/nei_combo_mm/skull_mask";
static const ALIGN_ASSET(2) char sName3[] = "__OTR__textures/nei_combo_mm/skull_mask_name";
static const ALIGN_ASSET(2) char sIcon4[] = "__OTR__textures/nei_combo_mm/spooky_mask";
static const ALIGN_ASSET(2) char sName4[] = "__OTR__textures/nei_combo_mm/spooky_mask_name";
static const ALIGN_ASSET(2) char sIcon5[] = "__OTR__textures/nei_combo_mm/gerudo_mask";
static const ALIGN_ASSET(2) char sName5[] = "__OTR__textures/nei_combo_mm/gerudo_mask_name";
static const ALIGN_ASSET(2) char sIcon6[] = "__OTR__textures/nei_combo_mm/pocket_egg";
static const ALIGN_ASSET(2) char sName6[] = "__OTR__textures/nei_combo_mm/pocket_egg_name";
static const ALIGN_ASSET(2) char sIcon7[] = "__OTR__textures/nei_combo_mm/pocket_cucco";
static const ALIGN_ASSET(2) char sName7[] = "__OTR__textures/nei_combo_mm/pocket_cucco_name";
static const ALIGN_ASSET(2) char sIcon8[] = "__OTR__textures/nei_combo_mm/cojiro";
static const ALIGN_ASSET(2) char sName8[] = "__OTR__textures/nei_combo_mm/cojiro_name";
static const ALIGN_ASSET(2) char sIcon9[] = "__OTR__textures/nei_combo_mm/odd_mushroom";
static const ALIGN_ASSET(2) char sName9[] = "__OTR__textures/nei_combo_mm/odd_mushroom_name";
static const ALIGN_ASSET(2) char sIcon10[] = "__OTR__textures/nei_combo_mm/odd_potion";
static const ALIGN_ASSET(2) char sName10[] = "__OTR__textures/nei_combo_mm/odd_potion_name";
static const ALIGN_ASSET(2) char sIcon11[] = "__OTR__textures/nei_combo_mm/poachers_saw";
static const ALIGN_ASSET(2) char sName11[] = "__OTR__textures/nei_combo_mm/poachers_saw_name";
static const ALIGN_ASSET(2) char sIcon12[] = "__OTR__textures/nei_combo_mm/broken_gorons_sword";
static const ALIGN_ASSET(2) char sName12[] = "__OTR__textures/nei_combo_mm/broken_gorons_sword_name";
static const ALIGN_ASSET(2) char sIcon13[] = "__OTR__textures/nei_combo_mm/prescription";
static const ALIGN_ASSET(2) char sName13[] = "__OTR__textures/nei_combo_mm/prescription_name";
static const ALIGN_ASSET(2) char sIcon14[] = "__OTR__textures/nei_combo_mm/eyeball_frog";
static const ALIGN_ASSET(2) char sName14[] = "__OTR__textures/nei_combo_mm/eyeball_frog_name";
static const ALIGN_ASSET(2) char sIcon15[] = "__OTR__textures/nei_combo_mm/eye_drops";
static const ALIGN_ASSET(2) char sName15[] = "__OTR__textures/nei_combo_mm/eye_drops_name";
static const ALIGN_ASSET(2) char sIcon16[] = "__OTR__textures/nei_combo_mm/claim_check";
static const ALIGN_ASSET(2) char sName16[] = "__OTR__textures/nei_combo_mm/claim_check_name";
static const ALIGN_ASSET(2) char sIcon17[] = "__OTR__textures/nei_combo_mm/fishing_pole";
static const ALIGN_ASSET(2) char sName17[] = "__OTR__textures/nei_combo_mm/fishing_pole_name";
static const ComboItem sItems[] = {
    { "nei.combo.weird_egg", "Weird Egg", sIcon0, sName0, "%rWeird Egg&%wBelongs to Ocarina of Time.", "You got %rWeird Egg%w!&Belongs to Ocarina of Time. Wait for this egg to hatch into the chicken that wakes Talon." },
    { "nei.combo.chicken", "Chicken", sIcon1, sName1, "%rChicken&%wBelongs to Ocarina of Time.", "You got %rChicken%w!&Belongs to Ocarina of Time. Use its crow to wake Talon outside Hyrule Castle." },
    { "nei.combo.zeldas_letter", "Zelda's Letter", sIcon2, sName2, "%rZelda's Letter&%wBelongs to Ocarina of Time.", "You got %rZelda's Letter%w!&Belongs to Ocarina of Time. Show Zelda's letter to the guard at the Death Mountain gate." },
    { "nei.combo.skull_mask", "Skull Mask", sIcon3, sName3, "%rSkull Mask&%wBelongs to Ocarina of Time.", "You got %rSkull Mask%w!&Belongs to Ocarina of Time. Sell this mask to the Skull Kid in the Lost Woods." },
    { "nei.combo.spooky_mask", "Spooky Mask", sIcon4, sName4, "%rSpooky Mask&%wBelongs to Ocarina of Time.", "You got %rSpooky Mask%w!&Belongs to Ocarina of Time. Sell this frightening mask to the boy in Kakariko Graveyard." },
    { "nei.combo.gerudo_mask", "Gerudo Mask", sIcon5, sName5, "%rGerudo Mask&%wBelongs to Ocarina of Time.", "You got %rGerudo Mask%w!&Belongs to Ocarina of Time. Wear this Gerudo disguise to hear different reactions from people." },
    { "nei.combo.pocket_egg", "Pocket Egg", sIcon6, sName6, "%rPocket Egg&%wBelongs to Ocarina of Time.", "You got %rPocket Egg%w!&Belongs to Ocarina of Time. Hatch this egg to begin the adult Cucco trade." },
    { "nei.combo.pocket_cucco", "Pocket Cucco", sIcon7, sName7, "%rPocket Cucco&%wBelongs to Ocarina of Time.", "You got %rPocket Cucco%w!&Belongs to Ocarina of Time. Wake Talon with this Cucco, then return it to the Cucco Lady." },
    { "nei.combo.cojiro", "Cojiro", sIcon8, sName8, "%rCojiro&%wBelongs to Ocarina of Time.", "You got %rCojiro%w!&Belongs to Ocarina of Time. Bring this blue Cucco to its owner in the Lost Woods." },
    { "nei.combo.odd_mushroom", "Odd Mushroom", sIcon9, sName9, "%rOdd Mushroom&%wBelongs to Ocarina of Time.", "You got %rOdd Mushroom%w!&Belongs to Ocarina of Time. Rush this mushroom to the old potion brewer in Kakariko." },
    { "nei.combo.odd_potion", "Odd Potion", sIcon10, sName10, "%rOdd Potion&%wBelongs to Ocarina of Time.", "You got %rOdd Potion%w!&Belongs to Ocarina of Time. Return this potion to the Lost Woods for the adult trade quest." },
    { "nei.combo.poachers_saw", "Poacher's Saw", sIcon11, sName11, "%rPoacher's Saw&%wBelongs to Ocarina of Time.", "You got %rPoacher's Saw%w!&Belongs to Ocarina of Time. Trade this saw to the carpenter boss in Gerudo Valley." },
    { "nei.combo.broken_gorons_sword", "Broken Goron's Sword", sIcon12, sName12, "%rBroken Goron's Sword&%wBelongs to Ocarina of Time.", "You got %rBroken Goron's Sword%w!&Belongs to Ocarina of Time. Take the broken blade to Biggoron for repairs." },
    { "nei.combo.prescription", "Prescription", sIcon13, sName13, "%rPrescription&%wBelongs to Ocarina of Time.", "You got %rPrescription%w!&Belongs to Ocarina of Time. Show Biggoron's prescription to King Zora to obtain an Eyeball Frog." },
    { "nei.combo.eyeball_frog", "Eyeball Frog", sIcon14, sName14, "%rEyeball Frog&%wBelongs to Ocarina of Time.", "You got %rEyeball Frog%w!&Belongs to Ocarina of Time. Rush this frog to the Lakeside Laboratory for eye drops." },
    { "nei.combo.eye_drops", "World's Finest Eye Drops", sIcon15, sName15, "%rWorld's Finest Eye Drops&%wBelongs to Ocarina of Time.", "You got %rWorld's Finest Eye Drops%w!&Belongs to Ocarina of Time. Bring these eye drops to Biggoron before they expire." },
    { "nei.combo.claim_check", "Claim Check", sIcon16, sName16, "%rClaim Check&%wBelongs to Ocarina of Time.", "You got %rClaim Check%w!&Belongs to Ocarina of Time. Exchange this claim check with Biggoron when the sword is ready." },
    { "nei.combo.fishing_pole", "Fishing Pole", sIcon17, sName17, "%rFishing Pole&%wBelongs to Ocarina of Time.", "You got %rFishing Pole%w!&Belongs to Ocarina of Time. Use this rod to fish at the Lake Hylia Fishing Pond." },
};
static const S2HModApi* sApi;
static const S2HModRequirements sRequirements = { sizeof(S2HModRequirements), NULL, 0 };

static void DrawReward(PlayState* play, S2HGetItemEntry* entry) {
    if (entry->drawItemId >= ARRAY_COUNT(sItems)) {
        return;
    }
    const char* icon = sItems[entry->drawItemId].icon;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    Vtx* vertices = GRAPH_ALLOC(play->state.gfxCtx, 4 * sizeof(Vtx));
    vertices[0] = (Vtx)VTX(-12, 12, 0, 0, 0, 255, 255, 255, 255);
    vertices[1] = (Vtx)VTX(12, 12, 0, 1024, 0, 255, 255, 255, 255);
    vertices[2] = (Vtx)VTX(-12, -12, 0, 0, 1024, 255, 255, 255, 255);
    vertices[3] = (Vtx)VTX(12, -12, 0, 1024, 1024, 255, 255, 255, 255);
    gDPSetCombineMode(POLY_XLU_DISP++, G_CC_MODULATERGBA, G_CC_MODULATERGBA);
    gDPLoadTextureBlock(POLY_XLU_DISP++, icon, G_IM_FMT_RGBA, G_IM_SIZ_32b, 32, 32, 0,
                       G_TX_CLAMP, G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gSPVertex(POLY_XLU_DISP++, (uintptr_t)vertices, 4, 0);
    gSP2Triangles(POLY_XLU_DISP++, 0, 1, 2, 0, 1, 3, 2, 0);
    CLOSE_DISPS(play->state.gfxCtx);
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
        // Presentation items never add another copy of the owning game's reward.
        randomizer.poolCount = 0;
        randomizer.logicKey = item->key;
        randomizer.hintKey = item->key;
        definition.displayName = item->label;
        definition.pauseText = item->pause;
        definition.getItemText = item->text;
        definition.getItemEntry.drawFunc = DrawReward;
        definition.getItemEntry.drawItemId = i;
        definition.preferredPage = S2H_ITEM_PAGE_QUEST_VANILLA + 5 + i / 12;
        definition.preferredSlot = i % 12;
        definition.randomizer = &randomizer;
        if (!Z64Items_Register(sApi, &definition)) {
            return;
        }
    }
}
