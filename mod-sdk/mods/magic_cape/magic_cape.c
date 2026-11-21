#include <math.h>
#include <string.h>

#include "z64items.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"

#define MAGIC_CAPE_KEY "nei.equip.magic_cape"
#define MAGIC_CAPE_VISUAL_CHANNEL "nei.magic_cape.visual"
#define LENS_DRAIN_INTERVAL 80
#define CAPE_JOINTS 12
#define CAPE_STRANDS 12
#define CAPE_JOINT_LENGTH 4.5f
#define CAPE_GRAVITY (-3.0f)
#define CAPE_BACK_PUSH (-4.0f)
#define CAPE_MIN_DIST 8.0f
#define CAPE_FLOOR_OFFSET (-200.0f)
#define CAPE_DAMPING 0.8f
#define CAPE_VEL_CLAMP 5.0f
#define CAPE_DECEL 0.1f
#define CAPE_BACK_SWAY 0.3f
#define CAPE_SIDE_SWAY 0.15f
#define CAPE_TEX_WIDTH 32
#define CAPE_TEX_HEIGHT 64
#define LEFT_SHOULDER_CAPTURED (1 << 0)
#define RIGHT_SHOULDER_CAPTURED (1 << 1)
#define BOTH_SHOULDERS_CAPTURED (LEFT_SHOULDER_CAPTURED | RIGHT_SHOULDER_CAPTURED)
#define REMOTE_VISUAL_INTERVAL 5u
#define REMOTE_CAPE_CAPACITY 64
#define REMOTE_CAPE_STALE_FRAMES 30u

static const ALIGN_ASSET(2) char sIcon[] = "__OTR__textures/icon_item_custom/gItemIconMagicCapeTex";
static const ALIGN_ASSET(2) char sName[] = "__OTR__textures/item_name_custom/gMagicCapeNameTex";
static const ALIGN_ASSET(2) char sGetItemCapeDL[] = "__OTR__objects/object_nei_magic_cape/gNeiMagicCapeDL";
static const ALIGN_ASSET(2) char sGetItemCapeWaveDL[] = "__OTR__objects/object_nei_magic_cape/gNeiMagicCapeWaveDL";
static const ALIGN_ASSET(2) char sMantMaterialDL[] = "__OTR__overlays/oot_ovl_En_Ganon_Mant/gMantMaterialDL";
static const ALIGN_ASSET(2) char sMantDL[] = "__OTR__overlays/oot_ovl_En_Ganon_Mant/gMantDL";
static const ALIGN_ASSET(2) char sMantTex[] = "__OTR__overlays/oot_ovl_En_Ganon_Mant/gMantTex";
static const ALIGN_ASSET(2) char sMant1Vtx[] = "__OTR__overlays/oot_ovl_En_Ganon_Mant/gMant1Vtx";
static const ALIGN_ASSET(2) char sMant2Vtx[] = "__OTR__overlays/oot_ovl_En_Ganon_Mant/gMant2Vtx";

Vtx* ResourceMgr_LoadVtxByName(const char* path);
void Gfx_RegisterBlendedTexture(const char* name, u8* mask, u8* replacement);

typedef struct {
    Vec3f root;
    Vec3f joints[CAPE_JOINTS];
    Vec3f rotations[CAPE_JOINTS];
    Vec3f velocities[CAPE_JOINTS];
} CapeStrand;

static const f32 sBackSwayCoeff[CAPE_JOINTS] = {
    0.0f, 1.0f, 0.5f, 0.25f, 0.1f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
};

static const f32 sSideSwayCoeff[CAPE_JOINTS] = {
    0.0f, 1.0f, 0.9f, 0.8f, 0.7f, 0.6f, 0.5f, 0.4f, 0.3f, 0.2f, 0.1f, 0.0f,
};

static const f32 sDistMult[CAPE_JOINTS] = {
    0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.1f, 1.2f, 1.3f, 1.4f, 1.5f, 1.6f, 1.7f,
};

#define CAPE_MAP_STRAND(n)                                                                                             \
    (n) + CAPE_JOINTS * 0, (n) + CAPE_JOINTS * 1, (n) + CAPE_JOINTS * 2, (n) + CAPE_JOINTS * 3, (n) + CAPE_JOINTS * 4, \
        (n) + CAPE_JOINTS * 5, (n) + CAPE_JOINTS * 6, (n) + CAPE_JOINTS * 7, (n) + CAPE_JOINTS * 8,                    \
        (n) + CAPE_JOINTS * 9, (n) + CAPE_JOINTS * 10, (n) + CAPE_JOINTS * 11

static const u16 sVertexMap[CAPE_STRANDS * CAPE_JOINTS] = {
    CAPE_MAP_STRAND(11), CAPE_MAP_STRAND(10), CAPE_MAP_STRAND(9), CAPE_MAP_STRAND(8),
    CAPE_MAP_STRAND(7),  CAPE_MAP_STRAND(6),  CAPE_MAP_STRAND(5), CAPE_MAP_STRAND(4),
    CAPE_MAP_STRAND(3),  CAPE_MAP_STRAND(2),  CAPE_MAP_STRAND(1), CAPE_MAP_STRAND(0),
};

static const char* const sRequiredHooks[] = { "OnPlayerUpdate",     "OnPlayerPostLimbDraw", "OnActorDrawEnd",
                                              "OnMagicResolveCost", "ShouldActorInit",      "OnActorInit",
                                              "ShouldActorUpdate",  "OnActorUpdate",        "OnSceneInit" };

static const S2HModRequirements sRequirements = { sizeof(S2HModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

typedef struct {
    int16_t scene;
    uint8_t active;
    uint8_t adult;
    uint8_t flags[8];
    float values[12];
} RemoteVisualState;

typedef struct {
    Vec3f shoulders[2];
    f32 baseYaw;
    u8 frame;
    u8 capturedShoulders;
    bool isInitialized;
    bool needsSnap;
    bool hasUpdatedThisFrame;
} ClothState;

typedef struct {
    uint32_t peer;
    uint32_t seen;
    uint32_t simulated;
    int16_t scene;
    uint8_t captured;
    bool initialized;
    Vec3f shoulders[2];
    ClothState cloth;
    CapeStrand strands[CAPE_STRANDS];
} RemoteCape;

static const S2HModApi* sApi;
static CapeStrand sStrands[CAPE_STRANDS];
static ClothState sCloth;
static u8 sMaskTex[CAPE_TEX_WIDTH * CAPE_TEX_HEIGHT];
static bool sIsTexRegistered;
static bool sIsSpinAttackCharging;
static s16 sLensTimerSeen;
static Vtx* sRemoteCapeVertices;
static f32 sRemoteCapeSpeed;
static RemoteCape sRemoteCapes[REMOTE_CAPE_CAPACITY];

static bool ReadRemoteVisual(Player* player, RemoteVisualState* visual);

static bool IsOwned(void) {
    return sApi->IsEquipmentOwned(MAGIC_CAPE_KEY);
}

// The toggle only hides the cloth: the discount belongs to owning the cape.
static bool IsVisible(void) {
    return IsOwned() && sApi->IsEquipmentToggleOn(MAGIC_CAPE_KEY);
}

static void HalveSpellCost(int16_t* amount) {
    if (IsOwned() && !sIsSpinAttackCharging && *amount > 0) {
        *amount /= 2;
    }
}

static void EnterSpinAttackCost(Actor* actor, bool* should) {
    sIsSpinAttackCharging = true;
}

static void LeaveSpinAttackCost(Actor* actor) {
    sIsSpinAttackCharging = false;
}

static void HalveLensDrain(PlayState* play) {
    s16 timer = play->interfaceCtx.magicConsumptionTimer;
    if (IsOwned() && gSaveContext.magicState == MAGIC_STATE_CONSUME_LENS && timer > sLensTimerSeen &&
        timer <= LENS_DRAIN_INTERVAL) {
        timer += LENS_DRAIN_INTERVAL;
        play->interfaceCtx.magicConsumptionTimer = timer;
    }
    sLensTimerSeen = timer;
}

static void ResetCloth(void) {
    sCloth.isInitialized = false;
    sCloth.capturedShoulders = 0;
    sCloth.needsSnap = true;
}

// Registered once per session: re-registering while the pipeline still referenced the old mask crashed.
static void InitCloth(void) {
    if (sCloth.isInitialized) {
        return;
    }
    memset(sStrands, 0, sizeof(sStrands));
    sCloth.capturedShoulders = 0;
    sCloth.needsSnap = true;
    if (!sIsTexRegistered) {
        memset(sMaskTex, 0, sizeof(sMaskTex));
        Gfx_RegisterBlendedTexture(sMantTex, sMaskTex, NULL);
        sIsTexRegistered = true;
    }
    sCloth.isInitialized = true;
}

static bool IsShoulderLimb(s32 limbIndex) {
    return limbIndex == PLAYER_LIMB_LEFT_SHOULDER || limbIndex == PLAYER_LIMB_RIGHT_SHOULDER;
}

static void CaptureShoulderInto(s32 limbIndex, Vec3f* shoulders, u8* captured) {
    Vec3f origin = { 0.0f, 200.0f, 0.0f };
    u32 side = limbIndex == PLAYER_LIMB_LEFT_SHOULDER ? 0 : 1;
    Matrix_MultVec3f(&origin, &shoulders[side]);
    *captured |= side == 0 ? LEFT_SHOULDER_CAPTURED : RIGHT_SHOULDER_CAPTURED;
}

static RemoteCape* FindRemoteCape(Player* player, PlayState* play) {
    uint32_t peer = sApi->GetNetPeerIdForActor(&player->actor);
    RemoteCape* available = NULL;
    if (peer == 0) {
        return NULL;
    }
    for (uint32_t i = 0; i < REMOTE_CAPE_CAPACITY; i++) {
        RemoteCape* cape = &sRemoteCapes[i];
        if (cape->peer == peer) {
            bool isStale = cape->scene != play->sceneId ||
                           (uint32_t)(play->gameplayFrames - cape->seen) > REMOTE_CAPE_STALE_FRAMES;
            if (isStale) {
                memset(cape, 0, sizeof(*cape));
                cape->peer = peer;
                cape->scene = play->sceneId;
            }
            cape->seen = play->gameplayFrames;
            return cape;
        }
        S2HNetPeer info = { 0 };
        info.structSize = sizeof(info);
        if (cape->peer == 0 || !sApi->FindNetPeer(cape->peer, &info) || !info.isOnline) {
            available = cape;
        }
    }
    if (available != NULL) {
        memset(available, 0, sizeof(*available));
        available->peer = peer;
        available->scene = play->sceneId;
        available->seen = play->gameplayFrames;
    }
    return available;
}

static void CaptureShoulder(Player* player, s32 limbIndex) {
    PlayState* play = gPlayState;
    if (play == NULL || !IsShoulderLimb(limbIndex)) {
        return;
    }
    if (player == GET_PLAYER(play)) {
        if (IsVisible()) {
            CaptureShoulderInto(limbIndex, sCloth.shoulders, &sCloth.capturedShoulders);
        }
        return;
    }
    RemoteVisualState visual;
    RemoteCape* cape = ReadRemoteVisual(player, &visual) ? FindRemoteCape(player, play) : NULL;
    if (cape != NULL) {
        CaptureShoulderInto(limbIndex, cape->shoulders, &cape->captured);
    }
}

static void ClampVelocity(Vec3f* velocity) {
    velocity->x = CLAMP(velocity->x, -CAPE_VEL_CLAMP, CAPE_VEL_CLAMP);
    velocity->y = CLAMP(velocity->y, -CAPE_VEL_CLAMP, CAPE_VEL_CLAMP);
    velocity->z = CLAMP(velocity->z, -CAPE_VEL_CLAMP, CAPE_VEL_CLAMP);
}

static void UpdateStrand(Vec3f* actorPos, CapeStrand* strand, CapeStrand* neighbor, s16 strandNum, f32 backSwayMag,
                         f32 sideSwayMag, f32 minY) {
    Vec3f delta;
    Vec3f step;
    Vec3f backSway;
    Vec3f sideSway;

    strand->joints[0] = strand->root;
    for (s16 i = 1; i < CAPE_JOINTS; i++) {
        Vec3f* pos = &strand->joints[i];
        Vec3f* prev = &strand->joints[i - 1];
        Vec3f* vel = &strand->velocities[i];
        Vec3f old = *pos;

        Math_ApproachZeroF(&vel->x, 1.0f, CAPE_DECEL);
        Math_ApproachZeroF(&vel->y, 1.0f, CAPE_DECEL);
        Math_ApproachZeroF(&vel->z, 1.0f, CAPE_DECEL);

        delta.x = 0.0f;
        delta.y = 0.0f;
        delta.z = (CAPE_BACK_PUSH + sinf((strandNum * (2 * M_PIf)) / 2.1f) * backSwayMag) * sBackSwayCoeff[i];
        Matrix_RotateYF(sCloth.baseYaw, MTXMODE_NEW);
        Matrix_MultVec3f(&delta, &backSway);
        delta.x = cosf((strandNum * M_PIf) / (CAPE_STRANDS - 1.0f)) * sideSwayMag * sSideSwayCoeff[i];
        delta.z = 0.0f;
        Matrix_MultVec3f(&delta, &sideSway);

        const f32 x = ((pos->x + vel->x) - prev->x) + (backSway.x + sideSway.x);
        const f32 y = ((pos->y + vel->y) - prev->y) + CAPE_GRAVITY;
        const f32 z = ((pos->z + vel->z) - prev->z) + (backSway.z + sideSway.z);
        const f32 yaw = Math_Atan2F(z, x);
        const f32 pitch = -Math_Atan2F(sqrtf(SQ(x) + SQ(z)), y);
        strand->rotations[i - 1].x = pitch;

        delta.x = 0.0f;
        delta.y = 0.0f;
        delta.z = CAPE_JOINT_LENGTH;
        Matrix_RotateYF(yaw, MTXMODE_NEW);
        Matrix_RotateXF(pitch, MTXMODE_APPLY);
        Matrix_MultVec3f(&delta, &step);
        pos->x = prev->x + step.x;
        pos->y = prev->y + step.y;
        pos->z = prev->z + step.z;

        const f32 xDiff = pos->x - actorPos->x;
        const f32 zDiff = pos->z - actorPos->z;
        if (sqrtf(SQ(xDiff) + SQ(zDiff)) < sDistMult[i] * CAPE_MIN_DIST) {
            delta.x = 0.0f;
            delta.z = CAPE_MIN_DIST * sDistMult[i];
            Matrix_RotateYF(Math_Atan2F(zDiff, xDiff), MTXMODE_NEW);
            Matrix_MultVec3f(&delta, &step);
            pos->x = actorPos->x + step.x;
            pos->z = actorPos->z + step.z;
        }
        pos->y = MAX(pos->y, minY);

        vel->x = (pos->x - old.x) * CAPE_DAMPING;
        vel->y = (pos->y - old.y) * CAPE_DAMPING;
        vel->z = (pos->z - old.z) * CAPE_DAMPING;
        ClampVelocity(vel);
        strand->rotations[i - 1].y = Math_Atan2F(pos->z - neighbor->joints[i].z, pos->x - neighbor->joints[i].x);
    }
    strand->rotations[CAPE_JOINTS - 1] = strand->rotations[CAPE_JOINTS - 2];
}

// A remote cape writes into its own frame buffer; the local one writes into the shared resource it draws from.
static Vtx* GetCurrentVertices(void) {
    if (sRemoteCapeVertices != NULL) {
        return sRemoteCapeVertices;
    }
    return ResourceMgr_LoadVtxByName((sCloth.frame % 2) ? sMant1Vtx : sMant2Vtx);
}

static void UpdateVertices(void) {
    Vtx* vertices = GetCurrentVertices();
    Vec3f up = { 0.0f, 30.0f, 0.0f };
    Vec3f normal;

    if (vertices == NULL) {
        return;
    }
    for (s16 i = 0; i < CAPE_STRANDS; i++) {
        for (s16 j = 0; j < CAPE_JOINTS; j++) {
            Vtx* vtx = &vertices[sVertexMap[i + j * CAPE_JOINTS]];

            vtx->n.ob[0] = sStrands[i].joints[j].x;
            vtx->n.ob[1] = sStrands[i].joints[j].y;
            vtx->n.ob[2] = sStrands[i].joints[j].z;
            Matrix_RotateYF(sStrands[i].rotations[j].y, MTXMODE_NEW);
            Matrix_RotateXF(sStrands[i].rotations[j].x, MTXMODE_APPLY);
            Matrix_MultVec3f(&up, &normal);
            vtx->n.n[0] = normal.x;
            vtx->n.n[1] = normal.y;
            vtx->n.n[2] = normal.z;
        }
    }
}

static void SimulateCloth(Player* player) {
    Vec3f* left = &sCloth.shoulders[0];
    Vec3f* right = &sCloth.shoulders[1];
    const f32 xDiff = left->x - right->x;
    const f32 yDiff = left->y - right->y;
    const f32 zDiff = left->z - right->z;
    const Vec3f midpoint = { right->x + xDiff * 0.5f, right->y + yDiff * 0.5f, right->z + zDiff * 0.5f };
    const f32 yaw = Math_Atan2F(zDiff, xDiff);
    const f32 halfSpan = sqrtf(SQ(xDiff) + SQ(yDiff) + SQ(zDiff)) * 0.5f;
    const f32 speed = sRemoteCapeVertices != NULL ? sRemoteCapeSpeed : player->actor.speed;
    const f32 minY = player->actor.world.pos.y + CAPE_FLOOR_OFFSET;

    Matrix_RotateYF(yaw, MTXMODE_NEW);
    Matrix_RotateXF(-Math_Atan2F(sqrtf(SQ(xDiff) + SQ(zDiff)), yDiff), MTXMODE_APPLY);
    sCloth.baseYaw = yaw - M_PIf / 2.0f;

    for (s16 i = 0; i < CAPE_STRANDS; i++) {
        const f32 t = (i * M_PIf) / (CAPE_STRANDS - 1);
        Vec3f offset = { sinf(t) * halfSpan, 0.0f, -cosf(t) * halfSpan };
        Vec3f rooted;
        const s16 neighbor = (i + 1 >= CAPE_STRANDS) ? i - 1 : i + 1;

        Matrix_Push();
        Matrix_MultVec3f(&offset, &rooted);
        sStrands[i].root.x = midpoint.x + rooted.x;
        sStrands[i].root.y = midpoint.y + rooted.y;
        sStrands[i].root.z = midpoint.z + rooted.z;
        if (sCloth.needsSnap) {
            for (s32 j = 0; j < CAPE_JOINTS; j++) {
                sStrands[i].joints[j] = sStrands[i].root;
                sStrands[i].velocities[j] = (Vec3f){ 0.0f, 0.0f, 0.0f };
            }
        }
        UpdateStrand(&player->actor.world.pos, &sStrands[i], &sStrands[neighbor], i, speed * CAPE_BACK_SWAY,
                     speed * CAPE_SIDE_SWAY, minY);
        Matrix_Pop();
    }
    UpdateVertices();
    sCloth.needsSnap = false;
}

// MM's 20000000 is a cutscene and 800000 is riding Epona, the bits OoT names IN_CUTSCENE and ON_HORSE.
static bool IsClothHidden(Player* player) {
    return (player->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_20000000 | PLAYER_STATE1_800000 |
                                   PLAYER_STATE1_1 | PLAYER_STATE1_10000000)) ||
           player->transformation != PLAYER_FORM_HUMAN;
}

static void TickCloth(Player* player) {
    PlayState* play = gPlayState;

    sIsSpinAttackCharging = false;
    if (play == NULL || player != GET_PLAYER(play)) {
        return;
    }
    HalveLensDrain(play);
    if (!IsVisible() || IsClothHidden(player)) {
        if (sCloth.isInitialized) {
            ResetCloth();
        }
        return;
    }
    InitCloth();
    sCloth.frame++;
    sCloth.hasUpdatedThisFrame = true;
}

static void DrawSimulatedCloth(Player* player, PlayState* play) {
    if (sCloth.hasUpdatedThisFrame) {
        SimulateCloth(player);
        sCloth.hasUpdatedThisFrame = false;
    }
    Vtx* vertices = GetCurrentVertices();
    if (vertices == NULL) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    gSPInvalidateTexCache(POLY_OPA_DISP++, (uintptr_t)sMaskTex);
    Matrix_Translate(0.0f, 0.0f, 0.0f, MTXMODE_NEW);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sMantMaterialDL);
    gSPSegment(POLY_OPA_DISP++, 0x0C, (uintptr_t)vertices);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sMantDL);
    // The skeleton's cull jump lives on segment 0x0C: left on the cape's vertices, the next limb would run them as
    // opcodes.
    gSPSegment(POLY_OPA_DISP++, 0x0C, (uintptr_t)gCullBackDList);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawCloth(Actor* actor, PlayState* play) {
    Player* player = (Player*)actor;
    if (actor != &GET_PLAYER(play)->actor) {
        return;
    }
    if (sCloth.isInitialized && !IsClothHidden(player) && !(player->stateFlags2 & PLAYER_STATE2_20000000) &&
        sCloth.capturedShoulders == BOTH_SHOULDERS_CAPTURED) {
        DrawSimulatedCloth(player, play);
    }
    sCloth.capturedShoulders = 0;
}

// Each remote cape keeps its own simulation; it is swapped into the shared state only for its own draw.
static void DrawRemoteCape(Player* player, PlayState* play, RemoteVisualState* visual) {
    const uint32_t vertexCount = CAPE_STRANDS * CAPE_JOINTS;
    RemoteCape* cape = FindRemoteCape(player, play);
    if (cape == NULL || cape->captured != BOTH_SHOULDERS_CAPTURED) {
        return;
    }
    Vtx* source = ResourceMgr_LoadVtxByName(sMant1Vtx);
    Vtx* vertices = GRAPH_ALLOC(play->state.gfxCtx, vertexCount * sizeof(Vtx));
    if (source == NULL || vertices == NULL) {
        return;
    }
    memcpy(vertices, source, vertexCount * sizeof(Vtx));
    ClothState localCloth = sCloth;
    CapeStrand localStrands[CAPE_STRANDS];
    memcpy(localStrands, sStrands, sizeof(sStrands));
    sCloth = cape->cloth;
    memcpy(sStrands, cape->strands, sizeof(sStrands));
    InitCloth();
    sCloth.shoulders[0] = cape->shoulders[0];
    sCloth.shoulders[1] = cape->shoulders[1];
    sCloth.hasUpdatedThisFrame = !cape->initialized || cape->simulated != play->gameplayFrames;
    if (sCloth.hasUpdatedThisFrame) {
        sCloth.frame++;
    }
    sRemoteCapeSpeed = visual->values[0];
    sRemoteCapeVertices = vertices;
    if (!sCloth.hasUpdatedThisFrame) {
        UpdateVertices();
    }
    DrawSimulatedCloth(player, play);
    sRemoteCapeVertices = NULL;
    cape->cloth = sCloth;
    memcpy(cape->strands, sStrands, sizeof(sStrands));
    cape->initialized = true;
    cape->simulated = play->gameplayFrames;
    cape->captured = 0;
    sCloth = localCloth;
    memcpy(sStrands, localStrands, sizeof(sStrands));
}

static void DrawGetItem(PlayState* play, S2HGetItemEntry* entry) {
    const char* cape = ((play->gameplayFrames >> 3) & 1) ? sGetItemCapeWaveDL : sGetItemCapeDL;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    Matrix_RotateYF((s16)(play->gameplayFrames * 2) * 0.01f, MTXMODE_APPLY);
    Matrix_Translate(0.0f, 22.0f, 0.0f, MTXMODE_APPLY);
    Matrix_Scale(0.55f, 0.55f, 0.55f, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)cape);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void ForgetSceneState(s16 sceneId, s8 spawnNum) {
    ResetCloth();
    sIsSpinAttackCharging = false;
}

static void HideCloth(const char* key) {
    ResetCloth();
}

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
        sApi->GetNetPeerState(peerId, MAGIC_CAPE_VISUAL_CHANNEL, visual, sizeof(*visual)) != sizeof(*visual)) {
        return false;
    }
    if (visual->active != 1 || visual->scene != gPlayState->sceneId ||
        (player->stateFlags2 & PLAYER_STATE2_20000000)) {
        return false;
    }
    for (uint32_t i = 0; i < ARRAY_COUNT(visual->values); i++) {
        if (!isfinite(visual->values[i]) || fabsf(visual->values[i]) > 10000000.0f) {
            return false;
        }
    }
    return true;
}

static void PublishRemoteVisual(void) {
    if (!sRemoteVisualReady) {
        return;
    }
    RemoteVisualState visual = { 0 };
    PlayState* play = gPlayState;
    visual.scene = -1;
    if (play != NULL) {
        Player* player = GET_PLAYER(play);
        visual.scene = play->sceneId;
        visual.active = IsVisible() && !IsClothHidden(player);
        if (visual.active) {
            visual.values[0] = player->actor.speed;
        }
        if (sRemoteVisualSent && (uint32_t)(play->gameplayFrames - sRemoteVisualFrame) < REMOTE_VISUAL_INTERVAL) {
            return;
        }
    }
    if (sRemoteVisualSent && memcmp(&visual, &sLastRemoteVisual, sizeof(visual)) == 0) {
        return;
    }
    if (sApi->SetNetPeerState(MAGIC_CAPE_VISUAL_CHANNEL, &visual, sizeof(visual))) {
        sLastRemoteVisual = visual;
        sRemoteVisualSent = true;
        sRemoteVisualFrame = play != NULL ? play->gameplayFrames : 0;
    }
}

static void DrawRemotePlayer(Actor* actor, PlayState* play) {
    RemoteVisualState visual;
    if (actor->id == ACTOR_PLAYER || !sRemoteVisualReady || sApi->GetNetPeerIdForActor(actor) == 0 ||
        !ReadRemoteVisual((Player*)actor, &visual)) {
        return;
    }
    DrawRemoteCape((Player*)actor, play, &visual);
}

static void InitRemoteVisuals(void) {
    if (!S2H_MOD_API_HAS(sApi, RegisterNetChannel) || !S2H_MOD_API_HAS(sApi, SetNetPeerState) ||
        !S2H_MOD_API_HAS(sApi, GetNetPeerState) || !S2H_MOD_API_HAS(sApi, GetNetPeerIdForActor) ||
        !S2H_MOD_API_HAS(sApi, FindNetPeer)) {
        return;
    }
    sRemoteVisualReady = sApi->RegisterNetChannel(MAGIC_CAPE_VISUAL_CHANNEL, 1, ReceiveRemoteVisual);
    if (sRemoteVisualReady) {
        S2H_REGISTER_HOOK(sApi, OnGameFrameUpdate, PublishRemoteVisual);
        S2H_REGISTER_HOOK(sApi, OnActorDrawEnd, DrawRemotePlayer);
    }
}

S2H_MOD_EXPORT void ModSetApi(const S2HModApi* api) {
    sApi = api;
}

S2H_MOD_EXPORT const S2HModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

S2H_MOD_EXPORT void ModInit(void) {
    InitRemoteVisuals();
    S2HCustomEquipDefinition definition = { 0 };

    definition.structSize = sizeof(definition);
    definition.key = MAGIC_CAPE_KEY;
    definition.iconPath = sIcon;
    definition.namePath = sName;
    definition.pauseText = "%rMagic Cape&%wEvery spell costs half its magic&"
                           "while you own it. %y\xB0%w hides the cloth.";
    definition.getItemText = "You got the %rMagic Cape%w! Cloth&cut from the King of Evil's own&mantle.^"
                             "Magic flows through it so&freely that every spell costs&you half, rounded down.";
    definition.slot = S2H_EQUIP_SLOT_UPGRADE;
    definition.page = 0;
    definition.row = 0;
    definition.column = 0;
    definition.toggles = 1;
    definition.onUnequip = HideCloth;
    definition.formMask = S2H_CUSTOM_ITEM_FORM_BIT(PLAYER_FORM_HUMAN);
    definition.getItemModelPath = sGetItemCapeDL;
    definition.getItemEntry.drawFunc = DrawGetItem;
    static const S2HCustomItemRandomizer randomizer = {
        .structSize = sizeof(S2HCustomItemRandomizer),
        .type = S2H_CUSTOM_ITEM_TYPE_EQUIP,
        .poolCount = 1,
        .logicKey = MAGIC_CAPE_KEY,
        .hintKey = MAGIC_CAPE_KEY,
    };
    definition.randomizer = &randomizer;
    if (!sApi->RegisterCustomEquipment(&definition)) {
        return;
    }

    S2H_REGISTER_HOOK(sApi, OnPlayerUpdate, TickCloth);
    S2H_REGISTER_HOOK(sApi, OnPlayerPostLimbDraw, CaptureShoulder);
    S2H_REGISTER_HOOK_FOR_ID(sApi, OnActorDrawEnd, ACTOR_PLAYER, DrawCloth);
    S2H_REGISTER_HOOK(sApi, OnMagicResolveCost, HalveSpellCost);
    S2H_REGISTER_HOOK(sApi, OnSceneInit, ForgetSceneState);
    S2H_REGISTER_HOOK_FOR_ID(sApi, ShouldActorInit, ACTOR_EN_M_THUNDER, EnterSpinAttackCost);
    S2H_REGISTER_HOOK_FOR_ID(sApi, OnActorInit, ACTOR_EN_M_THUNDER, LeaveSpinAttackCost);
    S2H_REGISTER_HOOK_FOR_ID(sApi, ShouldActorUpdate, ACTOR_EN_M_THUNDER, EnterSpinAttackCost);
    S2H_REGISTER_HOOK_FOR_ID(sApi, OnActorUpdate, ACTOR_EN_M_THUNDER, LeaveSpinAttackCost);
}
