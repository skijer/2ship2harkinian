
#include <math.h>

#include "z64items.h"
#include "soh/ResourceManagerHelpers.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "z64quake.h"
#include "objects/gameplay_keep/gameplay_keep.h"

#define PEGASUS_BOOTS_KEY "nei.equip.pegasus_boots"
#define PEGASUS_WINDUP_FRAMES 10
#define PEGASUS_DASH_SPEED 18.0f
#define PEGASUS_LEG_FRAMES_PER_UPDATE 3.0f
#define PEGASUS_STAB_FRAME 2
#define PEGASUS_BONK_DECEL 2.0f
#define PEGASUS_MAGIC_INTERVAL 15
#define PEGASUS_TURN_RATE 5.0f
#define PEGASUS_STICK_DEADZONE 10.0f
#define PEGASUS_COL_RADIUS 50
#define PEGASUS_COL_HEIGHT 80
#define PEGASUS_COL_FORWARD 40.0f
#define PEGASUS_CONE_FORWARD 80.0f
#define PEGASUS_CONE_HEIGHT 40.0f
#define PEGASUS_CONE_SCALE 0.015f
#define WIND_TEXTURE_WIDTH 64
#define WIND_TEXTURE_HEIGHT 64
#define WIND_TEXTURE_MASK 6
#define PEGASUS_VISUAL_CHANNEL "nei.pegasus_boots.visual"
#define REMOTE_VISUAL_INTERVAL 5u

static const ALIGN_ASSET(2) char sWindTex[] = "__OTR__overlays/oot_ovl_Magic_Wind/sTex";

static const ALIGN_ASSET(2) char sIcon[] = "__OTR__textures/icon_item_custom/gItemIconPegasusBootsTex";
static const ALIGN_ASSET(2) char sName[] = "__OTR__textures/item_name_custom/gPegasusBootsNameTex";

void func_8085B384(Player* player, PlayState* play);

typedef enum {
    PEGASUS_IDLE,
    PEGASUS_WINDUP,
    PEGASUS_RUNNING,
    PEGASUS_BONK,
} PegasusState;

static const S2HModApi* sApi;

static ColliderCylinder sWindCollider;

static ColliderCylinderInit sWindColliderInit = {
    { COL_MATERIAL_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_CYLINDER },
    { ELEM_MATERIAL_UNK2,
      { DMG_SWORD, 0x00, 0x04 },
      { 0, 0, 0 },
      ATELEM_ON | ATELEM_SFX_NORMAL,
      ACELEM_NONE,
      OCELEM_NONE },
    { PEGASUS_COL_RADIUS, PEGASUS_COL_HEIGHT, 0, { 0, 0, 0 } },
};

static struct {
    PegasusState state;
    s16 timer;
    s16 magicTick;
    f32 legFrame;
    bool isColliderReady;
    bool smashedCrate;
    bool needsResetOnLanding;
} sPegasus;

static u8 sUpperBodyMap[PLAYER_LIMB_MAX];
static Vec3s sStabPose[PLAYER_LIMB_MAX];
static Vec3s sRunPose[PLAYER_LIMB_MAX];

static Vtx sConeVtx[] = {
    VTX(0, 0, 0, 512, 2048, 0xFF, 0xFF, 0xFF, 0xFF),          VTX(4000, 8000, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0x00),
    VTX(2828, 8000, 2828, 256, 0, 0xFF, 0xFF, 0xFF, 0x00),    VTX(0, 8000, 4000, 512, 0, 0xFF, 0xFF, 0xFF, 0x00),
    VTX(-2828, 8000, 2828, 768, 0, 0xFF, 0xFF, 0xFF, 0x00),   VTX(-4000, 8000, 0, 1024, 0, 0xFF, 0xFF, 0xFF, 0x00),
    VTX(-2828, 8000, -2828, 1280, 0, 0xFF, 0xFF, 0xFF, 0x00), VTX(0, 8000, -4000, 1536, 0, 0xFF, 0xFF, 0xFF, 0x00),
    VTX(2828, 8000, -2828, 1792, 0, 0xFF, 0xFF, 0xFF, 0x00),
};

static Gfx sConeDL[] = {
    gsDPSetCombineLERP(TEXEL1, PRIMITIVE, PRIM_LOD_FRAC, TEXEL0, TEXEL1, TEXEL0, PRIM_LOD_FRAC, TEXEL0, PRIMITIVE,
                       ENVIRONMENT, COMBINED, ENVIRONMENT, COMBINED, 0, SHADE, 0),
    gsDPSetRenderMode(G_RM_PASS, G_RM_AA_ZB_XLU_SURF2),
    gsSPClearGeometryMode(G_CULL_BACK | G_FOG | G_LIGHTING | G_TEXTURE_GEN | G_TEXTURE_GEN_LINEAR),
    gsDPSetPrimColor(0, 0x80, 255, 255, 170, 255),
    gsDPSetEnvColor(100, 255, 50, 0),
    gsSPDisplayList(0x08000001),
    gsSPVertex(sConeVtx, 9, 0),
    gsSP2Triangles(0, 1, 2, 0, 0, 2, 3, 0),
    gsSP2Triangles(0, 3, 4, 0, 0, 4, 5, 0),
    gsSP2Triangles(0, 5, 6, 0, 0, 6, 7, 0),
    gsSP2Triangles(0, 7, 8, 0, 0, 8, 1, 0),
    gsSPEndDisplayList(),
};

static bool IsWorn(void) {
    return CustomEquipRegistry_IsWorn(PEGASUS_BOOTS_KEY);
}

static bool IsGrounded(Player* player) {
    return (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) != 0;
}

static bool IsPlayerBusy(PlayState* play, Player* player) {
    return (player->stateFlags1 & PLAYER_STATE1_DEAD) != 0 || Player_InCsMode(play);
}

static bool HoldsSword(Player* player) {
    return player->heldItemAction >= PLAYER_IA_SWORD_MIN && player->heldItemAction <= PLAYER_IA_SWORD_TWO_HANDED;
}

static bool IsDashing(void) {
    return sPegasus.state == PEGASUS_RUNNING || sPegasus.state == PEGASUS_BONK;
}

static void PlaySfx(Player* player, u16 sfxId) {
    Audio_PlaySfx_AtPos(&player->actor.world.pos, sfxId);
}

static void ResetToIdle(Player* player, PlayState* play) {
    player->speedXZ = 0.0f;
    player->actor.speed = 0.0f;
    player->unk_B08 = 0.0f;
    player->actor.world.rot.y = player->actor.shape.rot.y;
    player->yaw = player->actor.shape.rot.y;
    func_8085B384(player, play);
    sPegasus.needsResetOnLanding = false;
}

static void StopDash(Player* player, PlayState* play, bool resetAction) {
    bool wasDashing = IsDashing();

    sPegasus.state = PEGASUS_IDLE;
    sPegasus.timer = 0;
    sPegasus.magicTick = 0;
    sPegasus.smashedCrate = false;
    player->stateFlags1 &= ~PLAYER_STATE1_CHARGING_SPIN_ATTACK;
    player->stateFlags2 &= ~PLAYER_STATE2_20;
    sWindCollider.base.atFlags &= ~AT_ON;
    if (!resetAction || !wasDashing) {
        return;
    }
    if (IsGrounded(player)) {
        ResetToIdle(player, play);
    } else {
        sPegasus.needsResetOnLanding = true;
    }
}

static void StrikeWithWind(Player* player, PlayState* play) {
    Actor* struck;

    if (!sPegasus.isColliderReady) {
        Collider_InitCylinder(play, &sWindCollider);
        Collider_SetCylinder(play, &sWindCollider, &player->actor, &sWindColliderInit);
        sPegasus.isColliderReady = true;
    }
    sWindCollider.dim.pos.x =
        (s16)(player->actor.world.pos.x + Math_SinS(player->actor.shape.rot.y) * PEGASUS_COL_FORWARD);
    sWindCollider.dim.pos.y = (s16)player->actor.world.pos.y;
    sWindCollider.dim.pos.z =
        (s16)(player->actor.world.pos.z + Math_CosS(player->actor.shape.rot.y) * PEGASUS_COL_FORWARD);
    sWindCollider.base.atFlags |= AT_ON | AT_TYPE_PLAYER;
    CollisionCheck_SetAT(play, &play->colChkCtx, &sWindCollider.base);
    if (!(sWindCollider.base.atFlags & AT_HIT)) {
        return;
    }
    struck = sWindCollider.base.at;
    if (struck != NULL && (struck->id == ACTOR_OBJ_KIBAKO || struck->id == ACTOR_OBJ_KIBAKO2)) {
        sPegasus.smashedCrate = true;
    }
    PlaySfx(player, NA_SE_IT_SWORD_STRIKE);
    sWindCollider.base.atFlags &= ~AT_HIT;
}

static void PayForWind(Player* player, PlayState* play) {
    if (gSaveContext.save.saveInfo.playerData.magic <= 0) {
        sWindCollider.base.atFlags &= ~AT_ON;
        sPegasus.magicTick = 0;
        return;
    }
    StrikeWithWind(player, play);
    if (++sPegasus.magicTick >= PEGASUS_MAGIC_INTERVAL) {
        sPegasus.magicTick = 0;
        gSaveContext.save.saveInfo.playerData.magic--;
    }
}

// The charge walk's queued leg interpolation runs later and would overwrite direct loads, so poses are queued behind
// it.
static void PoseDash(Player* player, PlayState* play) {
    f32 lastFrame = Animation_GetLastFrame((void*)gPlayerAnim_link_normal_run_free);

    sPegasus.legFrame += PEGASUS_LEG_FRAMES_PER_UPDATE;
    if (sPegasus.legFrame > lastFrame) {
        sPegasus.legFrame -= lastFrame;
    }
    AnimTaskQueue_AddLoadPlayerFrame(play, (PlayerAnimationHeader*)gPlayerAnim_link_normal_run_free,
                                     (s32)sPegasus.legFrame, player->skelAnime.limbCount, sRunPose);
    AnimTaskQueue_AddCopy(play, player->skelAnime.limbCount, player->skelAnime.jointTable, sRunPose);
    AnimTaskQueue_AddLoadPlayerFrame(play, (PlayerAnimationHeader*)gPlayerAnim_link_fighter_Lpierce_kiru,
                                     PEGASUS_STAB_FRAME, player->skelAnime.limbCount, sStabPose);
    AnimTaskQueue_AddCopyUsingMap(play, player->skelAnime.limbCount, player->skelAnime.jointTable, sStabPose,
                                  sUpperBodyMap);
}

static bool IsFacingWall(Player* player) {
    s16 yawToWall = player->yaw - (s16)(player->actor.wallYaw + 0x8000);

    return (player->actor.bgCheckFlags & BGCHECKFLAG_WALL) && ABS(yawToWall) < 0x2000;
}

static bool HitsObstacle(Player* player, PlayState* play) {
    Actor* touched = player->cylinder.base.oc;

    if (sPegasus.smashedCrate) {
        sPegasus.smashedCrate = false;
        return true;
    }
    if (IsFacingWall(player)) {
        if (player->actor.wallBgId != BGCHECK_SCENE) {
            DynaPolyActor* wallActor = DynaPoly_GetActor(&play->colCtx, player->actor.wallBgId);

            if (wallActor != NULL) {
                wallActor->actor.home.rot.z = 1;
            }
        }
        return true;
    }
    if ((player->cylinder.base.ocFlags1 & OC1_HIT) && touched != NULL && touched->id == ACTOR_EN_WOOD02 &&
        ABS((s16)(player->actor.world.rot.y - touched->yawTowardsPlayer)) > 0x6000) {
        touched->home.rot.y = 1;
        return true;
    }
    return false;
}

static void Bonk(Player* player, PlayState* play) {
    PlayerAnimation_Change(play, &player->skelAnime, (PlayerAnimationHeader*)gPlayerAnim_link_normal_hip_down_free,
                           1.0f, 0.0f, Animation_GetLastFrame((void*)gPlayerAnim_link_normal_hip_down_free),
                           ANIMMODE_ONCE, -6.0f);
    player->speedXZ = -player->speedXZ;
    player->stateFlags1 &= ~PLAYER_STATE1_CHARGING_SPIN_ATTACK;
    Quake_Request(Play_GetCamera(play, CAM_ID_MAIN), QUAKE_TYPE_3);
    Rumble_Request(255.0f, 20, 150, 0);
    PlaySfx(player, NA_SE_PL_BODY_HIT);
    PlaySfx(player, NA_SE_VO_LI_CLIMB_END);
    sWindCollider.base.atFlags &= ~AT_ON;
    sPegasus.state = PEGASUS_BONK;
}

static void UpdateIdle(Player* player, Input* input) {
    if (!CHECK_BTN_ALL(input->cur.button, BTN_B) || !IsGrounded(player) ||
        (player->stateFlags1 & PLAYER_STATE1_8000000) || !HoldsSword(player)) {
        return;
    }
    if (player->unk_B08 < 0.1f && !(player->stateFlags1 & PLAYER_STATE1_CHARGING_SPIN_ATTACK)) {
        return;
    }
    player->unk_B08 = 0.0f;
    player->stateFlags1 |= PLAYER_STATE1_CHARGING_SPIN_ATTACK;
    player->stateFlags2 |= PLAYER_STATE2_20;
    sPegasus.state = PEGASUS_WINDUP;
    sPegasus.timer = PEGASUS_WINDUP_FRAMES;
    PlaySfx(player, NA_SE_PL_WALK_GROUND);
}

static void UpdateWindup(Player* player, PlayState* play, Input* input) {
    if (!CHECK_BTN_ALL(input->cur.button, BTN_B)) {
        StopDash(player, play, false);
        return;
    }
    player->unk_B08 = 0.0f;
    player->actor.speed = 0.0f;
    player->speedXZ = 0.0f;
    if (--sPegasus.timer > 0) {
        return;
    }
    sPegasus.state = PEGASUS_RUNNING;
    sPegasus.magicTick = 0;
    sPegasus.legFrame = 0.0f;
    PlaySfx(player, NA_SE_IT_SWORD_SWING_HARD);
}

static void UpdateRunning(Player* player, PlayState* play, Input* input) {
    if (!CHECK_BTN_ALL(input->cur.button, BTN_B) || (player->stateFlags1 & PLAYER_STATE1_8000000)) {
        StopDash(player, play, true);
        return;
    }
    player->unk_B08 = 0.0f;
    player->stateFlags1 |= PLAYER_STATE1_CHARGING_SPIN_ATTACK;
    if (fabsf((f32)input->rel.stick_x) > PEGASUS_STICK_DEADZONE) {
        player->actor.shape.rot.y -= (s16)(input->rel.stick_x * PEGASUS_TURN_RATE);
    }
    player->actor.world.rot.y = player->actor.shape.rot.y;
    player->yaw = player->actor.shape.rot.y;
    player->speedXZ = PEGASUS_DASH_SPEED;
    player->actor.speed = PEGASUS_DASH_SPEED;

    PoseDash(player, play);
    PayForWind(player, play);
    Actor_PlaySfx_Flagged(&player->actor, NA_SE_PL_WALK_GROUND - SFX_FLAG);
    if (HitsObstacle(player, play)) {
        Bonk(player, play);
    }
}

static void UpdateBonk(Player* player, PlayState* play) {
    player->stateFlags1 &= ~PLAYER_STATE1_CHARGING_SPIN_ATTACK;
    Math_StepToF(&player->speedXZ, 0.0f, PEGASUS_BONK_DECEL);
    player->actor.speed = fabsf(player->speedXZ);
    if (PlayerAnimation_Update(play, &player->skelAnime)) {
        StopDash(player, play, true);
    }
}

static void TickDash(Player* player) {
    PlayState* play = gPlayState;
    Input* input;

    if (play == NULL || !IsWorn()) {
        return;
    }
    input = &play->state.input[0];
    if (IsPlayerBusy(play, player)) {
        if (sPegasus.state != PEGASUS_IDLE) {
            StopDash(player, play, false);
        }
        sPegasus.needsResetOnLanding = false;
        return;
    }
    if (sPegasus.needsResetOnLanding && IsGrounded(player)) {
        ResetToIdle(player, play);
    }
    switch (sPegasus.state) {
        case PEGASUS_IDLE:
            UpdateIdle(player, input);
            break;
        case PEGASUS_WINDUP:
            UpdateWindup(player, play, input);
            break;
        case PEGASUS_RUNNING:
            UpdateRunning(player, play, input);
            break;
        case PEGASUS_BONK:
            UpdateBonk(player, play);
            break;
        default:
            StopDash(player, play, true);
            break;
    }
}

static void HoldStillWhileBonked(Player* player, Input* input) {
    if (sPegasus.state != PEGASUS_BONK) {
        return;
    }
    input->cur.stick_x = 0;
    input->cur.stick_y = 0;
    input->rel.stick_x = 0;
    input->rel.stick_y = 0;
    input->press.button = 0;
    input->cur.button = 0;
}

static void DrawWindCone(Player* player, PlayState* play) {
    u32 frames = play->gameplayFrames;
    f32 sinY = Math_SinS(player->actor.shape.rot.y);
    f32 cosY = Math_CosS(player->actor.shape.rot.y);

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    Matrix_Push();
    Matrix_Translate(player->actor.world.pos.x + sinY * PEGASUS_CONE_FORWARD,
                     player->actor.world.pos.y + PEGASUS_CONE_HEIGHT,
                     player->actor.world.pos.z + cosY * PEGASUS_CONE_FORWARD, MTXMODE_NEW);
    Matrix_RotateYS(player->actor.shape.rot.y, MTXMODE_APPLY);
    Matrix_RotateXS(-0x4000, MTXMODE_APPLY);
    Matrix_Scale(PEGASUS_CONE_SCALE, PEGASUS_CONE_SCALE, PEGASUS_CONE_SCALE, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, Matrix_Finalize(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gDPPipeSync(POLY_XLU_DISP++);
    gDPSetTextureLUT(POLY_XLU_DISP++, G_TT_NONE);
    gSPTexture(POLY_XLU_DISP++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
    gDPLoadTextureBlock(POLY_XLU_DISP++, sWindTex, G_IM_FMT_I, G_IM_SIZ_8b, WIND_TEXTURE_WIDTH, WIND_TEXTURE_HEIGHT, 0,
                        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, WIND_TEXTURE_MASK, WIND_TEXTURE_MASK,
                        G_TX_NOLOD, G_TX_NOLOD);
    gDPLoadMultiBlock(POLY_XLU_DISP++, sWindTex, 0x0100, 1, G_IM_FMT_I, G_IM_SIZ_8b, WIND_TEXTURE_WIDTH,
                      WIND_TEXTURE_HEIGHT, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, WIND_TEXTURE_MASK,
                      WIND_TEXTURE_MASK, 14, 14);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               (uintptr_t)Gfx_TwoTexScroll(play->state.gfxCtx, 0, -(s32)frames, (s32)(frames * 20), WIND_TEXTURE_WIDTH,
                                           WIND_TEXTURE_HEIGHT, 1, -(s32)(frames * 2), (s32)(frames * 10),
                                           WIND_TEXTURE_WIDTH, WIND_TEXTURE_HEIGHT));
    gSPDisplayList(POLY_XLU_DISP++, sConeDL);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
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
        sApi->GetNetPeerState(peerId, PEGASUS_VISUAL_CHANNEL, visual, sizeof(*visual)) != sizeof(*visual)) {
        return false;
    }
    return visual->active == 1 && visual->scene == gPlayState->sceneId &&
           !(player->stateFlags2 & PLAYER_STATE2_20000000);
}

static bool IsWindBlowing(void) {
    return sPegasus.state == PEGASUS_RUNNING && gSaveContext.save.saveInfo.playerData.magic > 0;
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
        visual.active = IsWorn();
        if (visual.active) {
            visual.flags[0] = IsWindBlowing();
        }
        if (sRemoteVisualSent && (uint32_t)(play->gameplayFrames - sRemoteVisualFrame) < REMOTE_VISUAL_INTERVAL) {
            return;
        }
    }
    if (sRemoteVisualSent && memcmp(&visual, &sLastRemoteVisual, sizeof(visual)) == 0) {
        return;
    }
    if (sApi->SetNetPeerState(PEGASUS_VISUAL_CHANNEL, &visual, sizeof(visual))) {
        sLastRemoteVisual = visual;
        sRemoteVisualSent = true;
        sRemoteVisualFrame = play != NULL ? play->gameplayFrames : 0;
    }
}

static void DrawOnPlayer(Actor* actor, PlayState* play) {
    Player* player = (Player*)actor;
    bool isBlowing;
    if (actor == &GET_PLAYER(play)->actor) {
        isBlowing = IsWorn() && IsWindBlowing();
    } else {
        RemoteVisualState visual;
        isBlowing = ReadRemoteVisual(player, &visual) && visual.flags[0];
    }
    if (isBlowing && !(player->stateFlags2 & PLAYER_STATE2_20000000)) {
        DrawWindCone(player, play);
    }
}

static void DrawRemotePlayer(Actor* actor, PlayState* play) {
    if (actor->id != ACTOR_PLAYER && sRemoteVisualReady && sApi->GetNetPeerIdForActor(actor) != 0) {
        DrawOnPlayer(actor, play);
    }
}

static void InitRemoteVisuals(void) {
    if (!S2H_MOD_API_HAS(sApi, RegisterNetChannel) || !S2H_MOD_API_HAS(sApi, SetNetPeerState) ||
        !S2H_MOD_API_HAS(sApi, GetNetPeerState) || !S2H_MOD_API_HAS(sApi, GetNetPeerIdForActor)) {
        return;
    }
    sRemoteVisualReady = sApi->RegisterNetChannel(PEGASUS_VISUAL_CHANNEL, 1, ReceiveRemoteVisual);
    if (sRemoteVisualReady) {
        S2H_REGISTER_HOOK(sApi, OnGameFrameUpdate, PublishRemoteVisual);
        S2H_REGISTER_HOOK(sApi, OnActorDrawEnd, DrawRemotePlayer);
    }
}

static void ForgetSceneState(s16 sceneId, s8 spawnNum) {
    sPegasus.state = PEGASUS_IDLE;
    sPegasus.isColliderReady = false;
    sPegasus.needsResetOnLanding = false;
}

static void ReleaseDash(const char* key) {
    if (gPlayState != NULL && sPegasus.state != PEGASUS_IDLE) {
        StopDash(GET_PLAYER(gPlayState), gPlayState, true);
    }
}

static const char* const sRequiredHooks[] = { "OnPlayerUpdate", "OnPlayerFilterInput", "OnActorDrawEnd",
                                              "OnSceneInit" };

static const S2HModRequirements sRequirements = { sizeof(S2HModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

S2H_MOD_EXPORT void ModSetApi(const S2HModApi* api) {
    sApi = api;
}

S2H_MOD_EXPORT const S2HModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

#define GI_DL_CAPACITY 512
static const ALIGN_ASSET(2) char gGiHoverBootsDL[] =
    "__OTR__objects/nei_get_item/pegasus_boots/object_gi_hoverboots/gGiHoverBootsDL";
static u32 CrimsonRamp(u32 rgba) {
    u8 r = (rgba >> 24) & 0xFF;
    u8 g = (rgba >> 16) & 0xFF;
    u8 b = (rgba >> 8) & 0xFF;
    f32 lum = 0.299f * r + 0.587f * g + 0.114f * b;

    return ((u32)CLAMP_MAX(lum * 1.7f, 255.0f) << 24) | ((u32)(lum * 0.35f) << 16) | ((u32)(lum * 0.42f) << 8) |
           (rgba & 0xFF);
}

static bool IsTwoWordCommand(u8 opcode) {
    return opcode == 0x20 || opcode == 0x24 || opcode == 0x25 || opcode == 0x27 || opcode == 0x31 || opcode == 0x32 ||
           opcode == 0x33 || opcode == 0x35 || opcode == 0x36 || opcode == 0x42;
}

static Gfx* BuildCrimsonBootsDL(void) {
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
            sDL[count - 1].words.w1 = CrimsonRamp((u32)sDL[count - 1].words.w1);
        }
    }
    sBuilt = true;
    return sDL;
}

static void DrawGetItem(PlayState* play, S2HGetItemEntry* entry) {
    Gfx* boots = BuildCrimsonBootsDL();

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
    InitRemoteVisuals();
    S2HCustomEquipDefinition definition = { 0 };

    for (s32 limb = PLAYER_LIMB_UPPER_ROOT; limb < PLAYER_LIMB_MAX; limb++) {
        sUpperBodyMap[limb] = true;
    }

    definition.getItemEntry.drawFunc = DrawGetItem;
    definition.structSize = sizeof(definition);
    definition.key = PEGASUS_BOOTS_KEY;
    definition.iconPath = sIcon;
    definition.namePath = sName;
    definition.pauseText = "%rPegasus Boots&%wHold %y\xB1%w where a spin attack would charge and you dash instead. "
                           "The wind in front costs magic.";
    definition.getItemText = "You got the %rPegasus Boots%w!&Boots that never tire. Hold %y\xB1%w as a spin would "
                             "charge and you run blade first, behind a wind that strikes whatever stands in the way.";
    definition.slot = S2H_EQUIP_SLOT_BOOTS;
    definition.page = 0;
    definition.row = S2H_EQUIP_SLOT_BOOTS;
    definition.column = 1;
    definition.formMask = S2H_CUSTOM_ITEM_FORM_BIT(PLAYER_FORM_HUMAN);
    definition.toggles = 1;
    definition.onUnequip = ReleaseDash;
    static const S2HCustomItemRandomizer randomizer = {
        .structSize = sizeof(S2HCustomItemRandomizer),
        .type = S2H_CUSTOM_ITEM_TYPE_EQUIP,
        .poolCount = 1,
        .logicKey = PEGASUS_BOOTS_KEY,
        .hintKey = PEGASUS_BOOTS_KEY,
    };
    definition.randomizer = &randomizer;
    if (!CustomEquipRegistry_Register(&definition)) {
        return;
    }

    S2H_REGISTER_HOOK(sApi, OnPlayerUpdate, TickDash);
    S2H_REGISTER_HOOK(sApi, OnPlayerFilterInput, HoldStillWhileBonked);
    S2H_REGISTER_HOOK(sApi, OnSceneInit, ForgetSceneState);
    S2H_REGISTER_HOOK_FOR_ID(sApi, OnActorDrawEnd, ACTOR_PLAYER, DrawOnPlayer);
}
