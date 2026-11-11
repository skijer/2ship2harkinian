/**
 * Climb Boots: full traction. Ice keeps no momentum, a steep slope neither slides Link down nor slows him
 * climbing it, and A still jumps from one.
 */

#include "z64items.h"
#include "soh/ResourceManagerHelpers.h"

#include "align_asset_macro.h"
#include "attributes.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"

#define CLIMB_BOOTS_KEY "nei.equip.climb_boots"
// OoT child Link stands about three quarters of adult Link's height, as oot_equipment scales its pieces.
#define ADULT_BOOT_TO_CHILD_SCALE 0.75f

static const ALIGN_ASSET(2) char sIcon[] = "__OTR__textures/icon_item_custom/gItemIconClimbBootsTex";
static const ALIGN_ASSET(2) char sName[] = "__OTR__textures/item_name_custom/gClimbBootsNameTex";
static const ALIGN_ASSET(2) char sLeftBootDL[] = "__OTR__objects/oot_object_link_boy/gLinkAdultLeftIronBootDL";
static const ALIGN_ASSET(2) char sRightBootDL[] = "__OTR__objects/oot_object_link_boy/gLinkAdultRightIronBootDL";

// Imported by address: a plain declaration would compare actionFunc against the DLL's own thunk.
extern HOST_DATA void Player_Action_SlideOnSlope(Player* player, PlayState* play);
bool Player_IsZTargeting(Player* player);
bool Player_CanUpdateItems(Player* player);
void func_808369F4(Player* player, PlayState* play);
void func_80836B3C(PlayState* play, Player* player, f32 arg2);
s32 func_80839A84(PlayState* play, Player* player);
void func_80839860(Player* player, PlayState* play, s32 controlStickDirection);
void func_808395F0(PlayState* play, Player* player, PlayerMeleeWeaponAnimation meleeWeaponAnim, f32 linearVelocity,
                   f32 yVelocity);

static const S2HModApi* sApi;

static bool IsWorn(void) {
    return CustomEquipRegistry_IsWorn(CLIMB_BOOTS_KEY);
}

static bool IsGrounded(Player* player) {
    return (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) != 0;
}

static bool IsOnSteepSlope(Player* player, PlayState* play) {
    CollisionPoly* floor = player->actor.floorPoly;

    return floor != NULL &&
           SurfaceType_GetFloorEffect(&play->colCtx, floor, player->actor.floorBgId) == FLOOR_EFFECT_1;
}

// Skipping the assignment keeps the previous, non-ice floor type: that is what stops the skid.
static void KeepGripOnIce(bool* should, va_list args) {
    Player* player = va_arg(args, Player*);
    PlayState* play = gPlayState;

    if (!IsWorn() || play == NULL || !IsGrounded(player) || player->actor.floorPoly == NULL) {
        return;
    }
    if (SurfaceType_GetFloorType(&play->colCtx, player->actor.floorPoly, player->actor.floorBgId) == FLOOR_TYPE_5) {
        *should = false;
    }
}

// The slide starts inside the scene collision; ending it here, after the whole update, costs no frame. Walking
// downhill restarts it every frame, so the walk's own speed and heading are handed back each time.
static void KeepFootingOnSlopes(Player* player) {
    PlayState* play = gPlayState;

    if (play == NULL || !IsWorn() || !IsGrounded(player)) {
        return;
    }
    if (player->actionFunc == Player_Action_SlideOnSlope) {
        f32 speed = player->speedXZ;
        s16 heading = player->yaw;

        func_808369F4(player, play);
        player->speedXZ = speed;
        player->yaw = heading;
    }
    if (IsOnSteepSlope(player, play)) {
        player->pushedSpeed = 0.0f;
    }
}

// Player_ActionHandler_10 without its steep-slope gate, which is the only thing standing between a slope
// and the hop, backflip or jump slash.
static bool StartSlopeJump(Player* player, PlayState* play) {
    s32 stickDirection = player->controlStickDirections[player->controlStickDataIndex];

    if (stickDirection > PLAYER_STICK_DIR_FORWARD) {
        func_80839860(player, play, stickDirection);
        return true;
    }
    if (!Player_IsZTargeting(player)) {
        return false;
    }
    if (!(player->stateFlags1 & PLAYER_STATE1_8000000) && Player_GetMeleeWeaponHeld(player) != PLAYER_MELEEWEAPON_NONE &&
        Player_CanUpdateItems(player) && player->transformation != PLAYER_FORM_GORON) {
        func_808395F0(play, player, PLAYER_MWA_JUMPSLASH_START, 5.0f, 5.0f);
    } else if (!func_80839A84(play, player)) {
        func_80836B3C(play, player, 0.0f);
    }
    return true;
}

static void JumpFromSteepSlope(PlayState* play, Player* player, s32 action, bool* consumed, bool* startedAction) {
    if (*consumed || !IsWorn() || !IsOnSteepSlope(player, play)) {
        return;
    }
    if (!CHECK_BTN_ALL(play->state.input[0].press.button, BTN_A) ||
        play->roomCtx.curRoom.type == ROOM_TYPE_INDOORS) {
        return;
    }
    *consumed = true;
    *startedAction = StartSlopeJump(player, play);
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
    ResourceMgr_PatchGfxByName(sLeftBootDL, "climbBootsBootMatrix", 2, skip);
    ResourceMgr_PatchGfxByName(sRightBootDL, "climbBootsBootMatrix", 2, skip);
    sIsPatched = true;
}

// Only the look changes: OoT's iron boots tinted silver, seated on the live foot matrix like oot_equipment's.
static void DrawClimbBootsOnFeet(Player* player, int32_t limbIndex) {
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
    gDPSetGrayscaleColor(POLY_OPA_DISP++, 205, 210, 220, 150);
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

static const char* const sRequiredHooks[] = { "OnPlayerUpdate", "OnPlayerActionHandler", "OnPlayerPostLimbDraw" };

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
static const ALIGN_ASSET(2) char gGiIronBootsDL[] = "__OTR__objects/nei_get_item/climb_boots/object_gi_boots_2/gGiIronBootsDL";
static const ALIGN_ASSET(2) char gGiIronBootsRivetsDL[] = "__OTR__objects/nei_get_item/climb_boots/object_gi_boots_2/gGiIronBootsRivetsDL";
static u32 YellowIronRamp(u32 rgba) {
    u8 r = (rgba >> 24) & 0xFF;
    u8 g = (rgba >> 16) & 0xFF;
    u8 b = (rgba >> 8) & 0xFF;
    f32 lum = 0.299f * r + 0.587f * g + 0.114f * b;
    bool isLeather = r > b + 30;
    f32 newR = isLeather ? lum * 2.2f : lum * 1.2f + 25.0f;
    f32 newG = isLeather ? lum * 1.75f : lum * 1.25f + 25.0f;
    f32 newB = isLeather ? lum * 0.35f : lum * 1.35f + 28.0f;

    return ((u32)CLAMP_MAX(newR, 255.0f) << 24) | ((u32)CLAMP_MAX(newG, 255.0f) << 16) |
           ((u32)CLAMP_MAX(newB, 255.0f) << 8) | (rgba & 0xFF);
}

static bool IsTwoWordCommand(u8 opcode) {
    return opcode == 0x20 || opcode == 0x24 || opcode == 0x25 || opcode == 0x27 || opcode == 0x31 ||
           opcode == 0x32 || opcode == 0x33 || opcode == 0x35 || opcode == 0x36 || opcode == 0x42;
}

static Gfx* BuildRecoloredDL(const char* path, Gfx* out, bool* built, bool gray) {
    Gfx* source;
    s32 count = 0;

    if (*built) {
        return out;
    }
    source = ResourceMgr_LoadGfxByName(path);
    if (source == NULL) {
        return NULL;
    }
    while (count < GI_DL_CAPACITY) {
        u8 opcode = (source[count].words.w0 >> 24) & 0xFF;

        out[count] = source[count];
        count++;
        if (opcode == G_ENDDL) {
            break;
        }
        if (IsTwoWordCommand(opcode) && count < GI_DL_CAPACITY) {
            out[count] = source[count];
            count++;
            continue;
        }
        if (opcode == G_SETPRIMCOLOR || opcode == G_SETENVCOLOR) {
            u32 rgba = (u32)out[count - 1].words.w1;
            if (gray) {
                u32 lum = (((rgba >> 24) & 255) * 3 + ((rgba >> 16) & 255) * 6 + ((rgba >> 8) & 255)) / 10;
                lum = CLAMP_MAX(lum * 1.15f + 30.0f, 255.0f);
                out[count - 1].words.w1 = (lum << 24) | (lum << 16) | (lum << 8) | (rgba & 255);
            } else { out[count - 1].words.w1 = YellowIronRamp(rgba); }
        }
    }
    *built = true;
    return out;
}

static void DrawGetItem(PlayState* play, S2HGetItemEntry* entry) {
    static Gfx sBootsDL[GI_DL_CAPACITY];
    static Gfx sRivetsDL[GI_DL_CAPACITY];
    bool sBootsBuilt = false;
    bool sRivetsBuilt = false;
    Gfx* boots = BuildRecoloredDL(gGiIronBootsDL, sBootsDL, &sBootsBuilt, false);
    Gfx* rivets = BuildRecoloredDL(gGiIronBootsRivetsDL, sRivetsDL, &sRivetsBuilt, false);

    if (boots == NULL || rivets == NULL) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    Matrix_RotateYF((s16)(play->gameplayFrames * 2) * 0.01f, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, boots);
    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, rivets);
    CLOSE_DISPS(play->state.gfxCtx);
}

S2H_MOD_EXPORT void ModInit(void) {
    S2HCustomEquipDefinition definition = { 0 };

    definition.getItemEntry.drawFunc = DrawGetItem;
    definition.structSize = sizeof(definition);
    definition.key = CLIMB_BOOTS_KEY;
    definition.iconPath = sIcon;
    definition.namePath = sName;
    definition.pauseText = "%rClimb Boots&%wIce and steep slopes no longer move you, and %y\xB0%w jumps even from "
                           "a slope.";
    definition.getItemText = "You got the %rClimb Boots%w!&Iron soles with a miner's grip. Wear them and neither "
                             "ice nor a steep slope will carry you away.";
    definition.slot = S2H_EQUIP_SLOT_BOOTS;
    definition.page = 0;
    definition.row = S2H_EQUIP_SLOT_BOOTS;
    definition.column = 2;
    definition.formMask = S2H_CUSTOM_ITEM_FORM_BIT(PLAYER_FORM_HUMAN);
    definition.toggles = 1;
    if (!CustomEquipRegistry_Register(&definition)) {
        return;
    }

    S2H_REGISTER_HOOK(sApi, OnPlayerUpdate, KeepFootingOnSlopes);
    S2H_REGISTER_HOOK(sApi, OnPlayerPostLimbDraw, DrawClimbBootsOnFeet);
    S2H_REGISTER_HOOK_FOR_ID(sApi, OnPlayerActionHandler, SOH_PLAYER_ACTION_ZTARGET_A, JumpFromSteepSlope);
    sApi->RegisterVB(VB_SET_STATIC_FLOOR_TYPE, KeepGripOnIce);
}
