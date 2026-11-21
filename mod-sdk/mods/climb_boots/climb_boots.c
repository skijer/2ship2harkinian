#include <math.h>
#include <string.h>

#include "z64items.h"
#include "soh/ResourceManagerHelpers.h"

#include "align_asset_macro.h"
#include "attributes.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"

#define CLIMB_BOOTS_KEY "nei.equip.climb_boots"
#define CLIMB_BOOTS_VISUAL_CHANNEL "nei.climb_boots.visual"
#define REMOTE_VISUAL_INTERVAL 5u
// The OoT adult boot shrinks for the adult form and further for MM's child-sized human body.
#define BOOT_SCALE_ADULT 0.85f
#define BOOT_SCALE_CHILD 0.63f

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

    return floor != NULL && SurfaceType_GetFloorEffect(&play->colCtx, floor, player->actor.floorBgId) == FLOOR_EFFECT_1;
}

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

static bool StartSlopeJump(Player* player, PlayState* play) {
    s32 stickDirection = player->controlStickDirections[player->controlStickDataIndex];

    if (stickDirection > PLAYER_STICK_DIR_FORWARD) {
        func_80839860(player, play, stickDirection);
        return true;
    }
    if (!Player_IsZTargeting(player)) {
        return false;
    }
    if (!(player->stateFlags1 & PLAYER_STATE1_8000000) &&
        Player_GetMeleeWeaponHeld(player) != PLAYER_MELEEWEAPON_NONE && Player_CanUpdateItems(player) &&
        player->transformation != PLAYER_FORM_GORON) {
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
    if (!CHECK_BTN_ALL(play->state.input[0].press.button, BTN_A) || play->roomCtx.curRoom.type == ROOM_TYPE_INDOORS) {
        return;
    }
    *consumed = true;
    *startedAction = StartSlopeJump(player, play);
}

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

typedef struct {
    int16_t scene;
    uint8_t active;
    uint8_t adult;
    uint8_t flags[8];
    float values[12];
} RemoteVisualState;

static bool sRemoteVisualReady;
static bool sRemoteVisualSent;
static uint32_t sRemoteVisualFrame;
static RemoteVisualState sLastRemoteVisual;

static void ReceiveRemoteVisual(const S2HNetMessage* message) {
    (void)message;
}

static bool ReadRemoteVisual(Player* player, RemoteVisualState* visual) {
    if (!sRemoteVisualReady || gPlayState == NULL || player == GET_PLAYER(gPlayState)) {
        return false;
    }
    uint32_t peerId = sApi->GetNetPeerIdForActor(&player->actor);
    if (peerId == 0 ||
        sApi->GetNetPeerState(peerId, CLIMB_BOOTS_VISUAL_CHANNEL, visual, sizeof(*visual)) != sizeof(*visual)) {
        return false;
    }
    return visual->active == 1 && visual->adult <= 1 && visual->scene == gPlayState->sceneId &&
           !(player->stateFlags2 & PLAYER_STATE2_20000000);
}

static bool IsWornByLocalHuman(Player* player) {
    return player->transformation == PLAYER_FORM_HUMAN && IsWorn();
}

static void PublishRemoteVisual(void) {
    if (!sRemoteVisualReady) {
        return;
    }
    RemoteVisualState visual = { 0 };
    PlayState* play = gPlayState;
    visual.scene = -1;
    if (play != NULL) {
        visual.scene = play->sceneId;
        visual.adult = FormRegistry_IsCurrentFormAdult();
        visual.active = IsWornByLocalHuman(GET_PLAYER(play));
        if (sRemoteVisualSent && (uint32_t)(play->gameplayFrames - sRemoteVisualFrame) < REMOTE_VISUAL_INTERVAL) {
            return;
        }
    }
    if (sRemoteVisualSent && memcmp(&visual, &sLastRemoteVisual, sizeof(visual)) == 0) {
        return;
    }
    if (sApi->SetNetPeerState(CLIMB_BOOTS_VISUAL_CHANNEL, &visual, sizeof(visual))) {
        sLastRemoteVisual = visual;
        sRemoteVisualSent = true;
        sRemoteVisualFrame = play != NULL ? play->gameplayFrames : 0;
    }
}

static void InitRemoteVisuals(void) {
    if (!S2H_MOD_API_HAS(sApi, RegisterNetChannel) || !S2H_MOD_API_HAS(sApi, SetNetPeerState) ||
        !S2H_MOD_API_HAS(sApi, GetNetPeerState) || !S2H_MOD_API_HAS(sApi, GetNetPeerIdForActor)) {
        return;
    }
    sRemoteVisualReady = sApi->RegisterNetChannel(CLIMB_BOOTS_VISUAL_CHANNEL, 1, ReceiveRemoteVisual);
    if (sRemoteVisualReady) {
        S2H_REGISTER_HOOK(sApi, OnGameFrameUpdate, PublishRemoteVisual);
    }
}

static bool ResolveBootScale(Player* player, f32* scale) {
    RemoteVisualState visual;
    if (player == GET_PLAYER(gPlayState)) {
        *scale = FormRegistry_IsCurrentFormAdult() ? BOOT_SCALE_ADULT : BOOT_SCALE_CHILD;
        return IsWornByLocalHuman(player);
    }
    if (!ReadRemoteVisual(player, &visual)) {
        return false;
    }
    *scale = visual.adult ? BOOT_SCALE_ADULT : BOOT_SCALE_CHILD;
    return true;
}

static void DrawClimbBootsOnFeet(Player* player, int32_t limbIndex) {
    PlayState* play = gPlayState;
    f32 bootScale;
    if (play == NULL || (limbIndex != PLAYER_LIMB_LEFT_FOOT && limbIndex != PLAYER_LIMB_RIGHT_FOOT) ||
        !ResolveBootScale(player, &bootScale)) {
        return;
    }
    const char* model = limbIndex == PLAYER_LIMB_LEFT_FOOT ? sLeftBootDL : sRightBootDL;
    if (!sApi->HasResource(model)) {
        return;
    }
    KeepBootsOnLoadedMatrix();
    const u32 envColor = GetCurrentEnvColor(play);
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    gDPSetGrayscaleColor(POLY_OPA_DISP++, 205, 210, 220, 150);
    gSPGrayscale(POLY_OPA_DISP++, true);
    Matrix_Push();
    Matrix_Scale(bootScale, bootScale, bootScale, MTXMODE_APPLY);
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

#define GI_DL_CAPACITY 512
static const ALIGN_ASSET(2) char gGiIronBootsDL[] =
    "__OTR__objects/nei_get_item/climb_boots/object_gi_boots_2/gGiIronBootsDL";
static const ALIGN_ASSET(2) char gGiIronBootsRivetsDL[] =
    "__OTR__objects/nei_get_item/climb_boots/object_gi_boots_2/gGiIronBootsRivetsDL";
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
    return opcode == 0x20 || opcode == 0x24 || opcode == 0x25 || opcode == 0x27 || opcode == 0x31 || opcode == 0x32 ||
           opcode == 0x33 || opcode == 0x35 || opcode == 0x36 || opcode == 0x42;
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
            } else {
                out[count - 1].words.w1 = YellowIronRamp(rgba);
            }
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
    InitRemoteVisuals();
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
    static const S2HCustomItemRandomizer randomizer = {
        .structSize = sizeof(S2HCustomItemRandomizer),
        .type = S2H_CUSTOM_ITEM_TYPE_EQUIP,
        .poolCount = 1,
        .logicKey = CLIMB_BOOTS_KEY,
        .hintKey = CLIMB_BOOTS_KEY,
    };
    definition.randomizer = &randomizer;
    if (!CustomEquipRegistry_Register(&definition)) {
        return;
    }

    S2H_REGISTER_HOOK(sApi, OnPlayerUpdate, KeepFootingOnSlopes);
    S2H_REGISTER_HOOK(sApi, OnPlayerPostLimbDraw, DrawClimbBootsOnFeet);
    S2H_REGISTER_HOOK_FOR_ID(sApi, OnPlayerActionHandler, SOH_PLAYER_ACTION_ZTARGET_A, JumpFromSteepSlope);
    sApi->RegisterVB(VB_SET_STATIC_FLOOR_TYPE, KeepGripOnIce);
}
