/**
 * Roc's Boots: half gravity, and a floor wherever Link lands on water or on lava, quicksand and the void.
 */

#include <string.h>

#include "z64items.h"
#include "soh/ResourceManagerHelpers.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"

#define ROC_BOOTS_KEY "nei.equip.roc_boots"
// OoT child Link stands about three quarters of adult Link's height, as oot_equipment scales its pieces.
#define ADULT_BOOT_TO_CHILD_SCALE 0.75f
#define DEKU_LEAF_KEY "nei.deku_leaf"
#define STATES_WITHOUT_GRAVITY                                                                              \
    (PLAYER_STATE1_8000000 | PLAYER_STATE1_4 | PLAYER_STATE1_2000 | PLAYER_STATE1_4000 | PLAYER_STATE1_200000 | \
     PLAYER_STATE1_800000 | PLAYER_STATE1_DEAD)

static const ALIGN_ASSET(2) char sIcon[] = "__OTR__textures/icon_item_custom/gItemIconRocBootsTex";
static const ALIGN_ASSET(2) char sName[] = "__OTR__textures/item_name_custom/gRocBootsNameTex";
static const ALIGN_ASSET(2) char sLeftBootDL[] = "__OTR__objects/oot_object_link_boy/gLinkAdultLeftHoverBootDL";
static const ALIGN_ASSET(2) char sRightBootDL[] = "__OTR__objects/oot_object_link_boy/gLinkAdultRightHoverBootDL";

s32 func_808340AC(FloorType floorType);
bool func_808340D4(FloorType floorType);

static const S2HModApi* sApi;

static bool IsWorn(void) {
    return CustomEquipRegistry_IsWorn(ROC_BOOTS_KEY);
}

static bool IsGrounded(Player* player) {
    return (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) != 0;
}

// Hot floors burn on a timer (func_808340AC); the other three sink Link or void him out (func_808340D4).
static bool IsDeadlyFloor(FloorType floorType) {
    return func_808340AC(floorType) >= 0 || func_808340D4(floorType);
}

static bool IsIntegratingGravity(PlayState* play, Player* player) {
    return !(player->stateFlags1 & STATES_WITHOUT_GRAVITY) && !Player_InCsMode(play) &&
           !(player->stateFlags3 & PLAYER_STATE3_FLYING_WITH_HOOKSHOT);
}

// The leaf pins the fall speed itself and halves it for these boots.
static bool IsGliding(void) {
    const char* held = CustomItemRegistry_GetHeldKey();

    return held != NULL && strcmp(held, DEKU_LEAF_KEY) == 0;
}

// Link's gravity is rewritten every frame before it is integrated, so the half is given back afterwards: half of
// g to the velocity and half of g's step to the position leave both exactly where g/2 would have put them.
static void HalveGravity(Player* player) {
    PlayState* play = gPlayState;
    f32 gravity;

    if (play == NULL || !IsWorn()) {
        return;
    }
    gravity = player->actor.gravity;
    if (gravity >= 0.0f || IsGrounded(player) || !IsIntegratingGravity(play, player) || IsGliding()) {
        return;
    }
    if (player->actor.velocity.y <= player->actor.terminalVelocity) {
        return;
    }
    player->actor.velocity.y -= gravity * 0.5f;
    player->actor.world.pos.y -= gravity * 0.5f * (play->state.framerateDivisor * 0.5f);
}

// While Link is out of the water and not rising, its surface is his floor.
static void WalkOnWater(CollisionContext* colCtx, Vec3f* pos, Actor* actor, CollisionPoly** poly, int32_t* bgId,
                        float* floorY) {
    PlayState* play = gPlayState;
    WaterBox* waterBox;
    f32 surface;

    if (play == NULL || !IsWorn() || actor != &GET_PLAYER(play)->actor) {
        return;
    }
    if ((((Player*)actor)->stateFlags1 & PLAYER_STATE1_8000000) || actor->velocity.y > 0.0f) {
        return;
    }
    if (!WaterBox_GetSurface1(play, colCtx, pos->x, pos->z, &surface, &waterBox) || surface <= *floorY) {
        return;
    }
    *floorY = surface;
}

// Skipping the assignment keeps the previous floor type, which these boots never let be a deadly one.
static void StandOnDeadlyFloors(bool* should, va_list args) {
    Player* player = va_arg(args, Player*);
    PlayState* play = gPlayState;

    if (!IsWorn() || play == NULL || !IsGrounded(player) || player->actor.floorPoly == NULL) {
        return;
    }
    if (IsDeadlyFloor(SurfaceType_GetFloorType(&play->colCtx, player->actor.floorPoly, player->actor.floorBgId))) {
        *should = false;
    }
}

// The emitted env colour carries custom tunics and cosmetics, so it is put back after the tint.
static u32 GetCurrentEnvColor(PlayState* play) {
    Gfx* begin = (Gfx*)play->state.gfxCtx->polyOpa.start;
    Gfx* cursor = play->state.gfxCtx->polyOpa.p;
    while (cursor > begin) {
        --cursor;
        if (((cursor->words.w0 >> 24) & 0xFF) == G_SETENVCOLOR) {
            return (u32)cursor->words.w1;
        }
    }
    return 0xFFFFFFFF;
}

// Each boot reloads its foot's skin matrix from segment 0x0D at instruction 2, which would drop the scale.
static void KeepBootsOnLoadedMatrix(void) {
    static bool sIsPatched;
    if (sIsPatched) {
        return;
    }
    Gfx skip = gsDPNoOp();
    ResourceMgr_PatchGfxByName(sLeftBootDL, "rocBootsBootMatrix", 2, skip);
    ResourceMgr_PatchGfxByName(sRightBootDL, "rocBootsBootMatrix", 2, skip);
    sIsPatched = true;
}

// Only the look changes: OoT's hover boots tinted gold, seated on the live foot matrix like oot_equipment's.
static void DrawGoldBootsOnFeet(Player* player, int32_t limbIndex) {
    PlayState* play = gPlayState;
    if (play == NULL || player != GET_PLAYER(play) || player->transformation != PLAYER_FORM_HUMAN || !IsWorn() ||
        (limbIndex != PLAYER_LIMB_LEFT_FOOT && limbIndex != PLAYER_LIMB_RIGHT_FOOT)) {
        return;
    }
    const char* model = limbIndex == PLAYER_LIMB_LEFT_FOOT ? sLeftBootDL : sRightBootDL;
    if (!sApi->HasResource(model)) {
        return;
    }
    KeepBootsOnLoadedMatrix();
    const u32 envColor = GetCurrentEnvColor(play);
    OPEN_DISPS(play->state.gfxCtx);
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetGrayscaleColor(POLY_OPA_DISP++, 255, 198, 65, 255);
    gSPGrayscale(POLY_OPA_DISP++, true);
    Matrix_Push();
    Matrix_Scale(ADULT_BOOT_TO_CHILD_SCALE, ADULT_BOOT_TO_CHILD_SCALE, ADULT_BOOT_TO_CHILD_SCALE, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)model);
    Matrix_Pop();
    gSPGrayscale(POLY_OPA_DISP++, false);
    gDPSetEnvColor(POLY_OPA_DISP++, envColor >> 24, (envColor >> 16) & 0xFF, (envColor >> 8) & 0xFF, envColor & 0xFF);
    CLOSE_DISPS(play->state.gfxCtx);
}

static const char* const sRequiredHooks[] = { "OnPlayerUpdate", "OnBgCheckRaycastFloor", "OnPlayerPostLimbDraw" };

static const S2HModRequirements sRequirements = { sizeof(S2HModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

S2H_MOD_EXPORT void ModSetApi(const S2HModApi* api) {
    sApi = api;
}

S2H_MOD_EXPORT const S2HModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

// Pickup renderer from the authoritative OoT mod, including its complete mesh and materials.
#define GI_DL_CAPACITY 512
static const ALIGN_ASSET(2) char gGiHoverBootsDL[] = "__OTR__objects/nei_get_item/roc_boots/object_gi_hoverboots/gGiHoverBootsDL";
static u32 GoldRamp(u32 rgba) {
    u8 r = (rgba >> 24) & 0xFF;
    u8 g = (rgba >> 16) & 0xFF;
    u8 b = (rgba >> 8) & 0xFF;
    f32 lum = 0.299f * r + 0.587f * g + 0.114f * b;

    return ((u32)CLAMP_MAX(lum * 1.6f, 255.0f) << 24) | ((u32)CLAMP_MAX(lum * 1.22f, 255.0f) << 16) |
           ((u32)CLAMP_MAX(lum * 0.5f, 255.0f) << 8) | (rgba & 0xFF);
}

static bool IsTwoWordCommand(u8 opcode) {
    return opcode == 0x20 || opcode == 0x24 || opcode == 0x25 || opcode == 0x27 || opcode == 0x31 ||
           opcode == 0x32 || opcode == 0x33 || opcode == 0x35 || opcode == 0x36 || opcode == 0x42;
}

static Gfx* BuildGoldBootsDL(void) {
    static Gfx sDL[GI_DL_CAPACITY];
    static bool sBuilt;
    Gfx* source;
    s32 count = 0;

    if (sBuilt) {
        return sDL;
    }
    source = ResourceMgr_LoadGfxByName(gGiHoverBootsDL);
    if (source == NULL) {
        return NULL;
    }
    while (count < GI_DL_CAPACITY) {
        u8 opcode = (source[count].words.w0 >> 24) & 0xFF;

        sDL[count] = source[count];
        count++;
        if (opcode == G_ENDDL) {
            break;
        }
        if (IsTwoWordCommand(opcode) && count < GI_DL_CAPACITY) {
            sDL[count] = source[count];
            count++;
            continue;
        }
        if (opcode == G_SETPRIMCOLOR || opcode == G_SETENVCOLOR) {
            sDL[count - 1].words.w1 = GoldRamp((u32)sDL[count - 1].words.w1);
        }
    }
    sBuilt = true;
    return sDL;
}

static void DrawGetItem(PlayState* play, S2HGetItemEntry* entry) {
    Gfx* boots = BuildGoldBootsDL();

    if (boots == NULL) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    Matrix_RotateYF((s16)(play->gameplayFrames * 2) * 0.01f, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, boots);
    CLOSE_DISPS(play->state.gfxCtx);
}

S2H_MOD_EXPORT void ModInit(void) {
    S2HCustomEquipDefinition definition = { 0 };

    definition.getItemEntry.drawFunc = DrawGetItem;
    definition.structSize = sizeof(definition);
    definition.key = ROC_BOOTS_KEY;
    definition.iconPath = sIcon;
    definition.namePath = sName;
    definition.pauseText = "%rRoc's Boots&%wYou fall at half speed, and water, lava and quicksand hold you up.";
    definition.getItemText = "You got the %rRoc's Boots%w!&Boots as light as the great bird's feathers. Every jump "
                             "carries further, and you can walk across water and lava.";
    definition.slot = S2H_EQUIP_SLOT_BOOTS;
    definition.page = 0;
    definition.row = S2H_EQUIP_SLOT_BOOTS;
    definition.column = 3;
    definition.formMask = S2H_CUSTOM_ITEM_FORM_BIT(PLAYER_FORM_HUMAN);
    definition.toggles = 1;
    if (!CustomEquipRegistry_Register(&definition)) {
        return;
    }

    S2H_REGISTER_HOOK(sApi, OnPlayerUpdate, HalveGravity);
    S2H_REGISTER_HOOK(sApi, OnPlayerPostLimbDraw, DrawGoldBootsOnFeet);
    S2H_REGISTER_HOOK(sApi, OnBgCheckRaycastFloor, WalkOnWater);
    sApi->RegisterVB(VB_SET_STATIC_FLOOR_TYPE, StandOnDeadlyFloors);
}
