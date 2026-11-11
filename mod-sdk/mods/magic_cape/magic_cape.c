/**
 * Magic Cape: while switched on, every spell costs half and the Lens of Truth drains at half speed.
 * Ganondorf's cloak (OoT En_Ganon_Mant, copied by build_assets.py) hangs from Link's shoulders with Verlet physics.
 */

#include <math.h>
#include <string.h>

#include "z64items.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"

#define MAGIC_CAPE_KEY "nei.equip.magic_cape"
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
#define LEFT_SHOULDER_CAPTURED (1 << 0)
#define RIGHT_SHOULDER_CAPTURED (1 << 1)
#define BOTH_SHOULDERS_CAPTURED (LEFT_SHOULDER_CAPTURED | RIGHT_SHOULDER_CAPTURED)

static const ALIGN_ASSET(2) char sIcon[] = "__OTR__textures/icon_item_custom/gItemIconMagicCapeTex";
static const ALIGN_ASSET(2) char sName[] = "__OTR__textures/item_name_custom/gMagicCapeNameTex";
static const ALIGN_ASSET(2) char sGetItemCapeDL[] = "__OTR__objects/object_nei_magic_cape/gNeiMagicCapeDL";
static const ALIGN_ASSET(2) char sGetItemCapeWaveDL[] = "__OTR__objects/object_nei_magic_cape/gNeiMagicCapeWaveDL";
static const ALIGN_ASSET(2) char sMantMaterialDL[] = "__OTR__objects/object_nei_magic_cape/gMantMaterialDL";
static const ALIGN_ASSET(2) char sMantDL[] = "__OTR__objects/object_nei_magic_cape/gMantDL";
static const ALIGN_ASSET(2) char sMant1Vtx[] = "__OTR__objects/object_nei_magic_cape/gMant1Vtx";
static const ALIGN_ASSET(2) char sMant2Vtx[] = "__OTR__objects/object_nei_magic_cape/gMant2Vtx";

Vtx* ResourceMgr_LoadVtxByName(const char* path);

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

#define CAPE_MAP_STRAND(n)                                                                                       \
    (n) + CAPE_JOINTS * 0, (n) + CAPE_JOINTS * 1, (n) + CAPE_JOINTS * 2, (n) + CAPE_JOINTS * 3,                   \
        (n) + CAPE_JOINTS * 4, (n) + CAPE_JOINTS * 5, (n) + CAPE_JOINTS * 6, (n) + CAPE_JOINTS * 7,               \
        (n) + CAPE_JOINTS * 8, (n) + CAPE_JOINTS * 9, (n) + CAPE_JOINTS * 10, (n) + CAPE_JOINTS * 11

// The vertex order of Ganondorf's own cloak mesh.
static const u16 sVertexMap[CAPE_STRANDS * CAPE_JOINTS] = {
    CAPE_MAP_STRAND(11), CAPE_MAP_STRAND(10), CAPE_MAP_STRAND(9), CAPE_MAP_STRAND(8),
    CAPE_MAP_STRAND(7),  CAPE_MAP_STRAND(6),  CAPE_MAP_STRAND(5), CAPE_MAP_STRAND(4),
    CAPE_MAP_STRAND(3),  CAPE_MAP_STRAND(2),  CAPE_MAP_STRAND(1), CAPE_MAP_STRAND(0),
};

static const char* const sRequiredHooks[] = { "OnPlayerUpdate",  "OnPlayerPostLimbDraw", "OnActorDrawEnd",
                                              "OnMagicResolveCost", "ShouldActorInit",   "OnActorInit",
                                              "ShouldActorUpdate",  "OnActorUpdate",     "OnSceneInit" };

static const S2HModRequirements sRequirements = { sizeof(S2HModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const S2HModApi* sApi;
static CapeStrand sStrands[CAPE_STRANDS];

static struct {
    Vec3f shoulders[2];
    f32 baseYaw;
    u8 frame;
    u8 capturedShoulders;
    bool isInitialized;
    bool needsSnap;
    bool hasUpdatedThisFrame;
    bool isSpinAttackCharging;
    s16 lensTimerSeen;
} sCape;

static bool IsActive(void) {
    return sApi->IsEquipmentOwned(MAGIC_CAPE_KEY) && sApi->IsEquipmentToggleOn(MAGIC_CAPE_KEY);
}

// The spin attack keeps its full price: its charge previews one cost and its release pays another.
static void HalveSpellCost(int16_t* amount) {
    if (IsActive() && !sCape.isSpinAttackCharging && *amount > 0) {
        *amount /= 2;
    }
}

static void EnterSpinAttackCost(Actor* actor, bool* should) {
    sCape.isSpinAttackCharging = true;
}

static void LeaveSpinAttackCost(Actor* actor) {
    sCape.isSpinAttackCharging = false;
}

// The lens pays one point each time its timer runs out; the timer jumping back up is that payment.
static void HalveLensDrain(PlayState* play) {
    s16 timer = play->interfaceCtx.magicConsumptionTimer;

    if (IsActive() && gSaveContext.magicState == MAGIC_STATE_CONSUME_LENS && timer > sCape.lensTimerSeen &&
        timer <= LENS_DRAIN_INTERVAL) {
        timer += LENS_DRAIN_INTERVAL;
        play->interfaceCtx.magicConsumptionTimer = timer;
    }
    sCape.lensTimerSeen = timer;
}

static void ResetCloth(void) {
    sCape.isInitialized = false;
    sCape.capturedShoulders = 0;
    sCape.needsSnap = true;
}

static void InitCloth(void) {
    if (sCape.isInitialized) {
        return;
    }
    memset(sStrands, 0, sizeof(sStrands));
    sCape.capturedShoulders = 0;
    sCape.needsSnap = true;
    sCape.isInitialized = true;
}

static void CaptureShoulder(Player* player, int32_t limbIndex) {
    Vec3f origin = { 0.0f, 200.0f, 0.0f };

    if (gPlayState == NULL || player != GET_PLAYER(gPlayState) || !IsActive()) {
        return;
    }
    if (limbIndex == PLAYER_LIMB_LEFT_SHOULDER) {
        Matrix_MultVec3f(&origin, &sCape.shoulders[0]);
        sCape.capturedShoulders |= LEFT_SHOULDER_CAPTURED;
    } else if (limbIndex == PLAYER_LIMB_RIGHT_SHOULDER) {
        Matrix_MultVec3f(&origin, &sCape.shoulders[1]);
        sCape.capturedShoulders |= RIGHT_SHOULDER_CAPTURED;
    }
}

static void ClampVelocity(Vec3f* velocity) {
    velocity->x = CLAMP(velocity->x, -CAPE_VEL_CLAMP, CAPE_VEL_CLAMP);
    velocity->y = CLAMP(velocity->y, -CAPE_VEL_CLAMP, CAPE_VEL_CLAMP);
    velocity->z = CLAMP(velocity->z, -CAPE_VEL_CLAMP, CAPE_VEL_CLAMP);
}

// EnGanonMant_UpdateStrand, scaled to Link.
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
        Matrix_RotateYF(sCape.baseYaw, MTXMODE_NEW);
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

// Two vertex buffers alternate, as OoT's own cloak does, so a frame never edits the one being drawn.
static const char* GetCurrentVertices(void) {
    return (sCape.frame % 2) ? sMant1Vtx : sMant2Vtx;
}

static void UpdateVertices(void) {
    Vtx* vertices = ResourceMgr_LoadVtxByName(GetCurrentVertices());
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

// The roots fan out in a semicircle between the shoulders; the first tick after an init collapses every joint
// onto its root so the cloth does not settle in from the world origin.
static void SimulateCloth(Player* player) {
    Vec3f* left = &sCape.shoulders[0];
    Vec3f* right = &sCape.shoulders[1];
    const f32 xDiff = left->x - right->x;
    const f32 yDiff = left->y - right->y;
    const f32 zDiff = left->z - right->z;
    const Vec3f midpoint = { right->x + xDiff * 0.5f, right->y + yDiff * 0.5f, right->z + zDiff * 0.5f };
    const f32 yaw = Math_Atan2F(zDiff, xDiff);
    const f32 halfSpan = sqrtf(SQ(xDiff) + SQ(yDiff) + SQ(zDiff)) * 0.5f;
    const f32 speed = player->actor.speed;
    const f32 minY = player->actor.world.pos.y + CAPE_FLOOR_OFFSET;

    Matrix_RotateYF(yaw, MTXMODE_NEW);
    Matrix_RotateXF(-Math_Atan2F(sqrtf(SQ(xDiff) + SQ(zDiff)), yDiff), MTXMODE_APPLY);
    sCape.baseYaw = yaw - M_PIf / 2.0f;

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
        if (sCape.needsSnap) {
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
    sCape.needsSnap = false;
}

// MM's 20000000 is a cutscene and 800000 is riding Epona, the bits OoT names IN_CUTSCENE and ON_HORSE.
static bool IsClothHidden(Player* player) {
    return (player->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_20000000 | PLAYER_STATE1_800000)) ||
           player->transformation != PLAYER_FORM_HUMAN;
}

static void TickCape(Player* player) {
    PlayState* play = gPlayState;

    // A skipped update never reaches OnActorUpdate, which would leave every later spell at full price.
    sCape.isSpinAttackCharging = false;
    if (play == NULL || player != GET_PLAYER(play)) {
        return;
    }
    HalveLensDrain(play);
    if (!IsActive() || IsClothHidden(player)) {
        if (sCape.isInitialized) {
            ResetCloth();
        }
        return;
    }
    InitCloth();
    sCape.frame++;
    sCape.hasUpdatedThisFrame = true;
}

// The cloth runs its physics where it draws, once per updated frame, off the shoulders the limbs just placed.
static void DrawCloth(Actor* actor, PlayState* play) {
    Player* player = (Player*)actor;

    if (actor != &GET_PLAYER(play)->actor) {
        return;
    }
    if (!sCape.isInitialized || IsClothHidden(player) || sCape.capturedShoulders != BOTH_SHOULDERS_CAPTURED) {
        sCape.capturedShoulders = 0;
        return;
    }
    if (sCape.hasUpdatedThisFrame) {
        SimulateCloth(player);
        sCape.hasUpdatedThisFrame = false;
    }
    Vtx* vertices = ResourceMgr_LoadVtxByName(GetCurrentVertices());
    if (vertices == NULL) {
        return;
    }

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    Matrix_Translate(0.0f, 0.0f, 0.0f, MTXMODE_NEW);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sMantMaterialDL);
    gSPSegment(POLY_OPA_DISP++, 0x0C, (uintptr_t)vertices);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sMantDL);
    // The skeleton's cull jump lives on segment 0x0C: left on the cape's vertices, the next limb drawn would run
    // vertex data as opcodes.
    gSPSegment(POLY_OPA_DISP++, 0x0C, (uintptr_t)gCullBackDList);
    CLOSE_DISPS(play->state.gfxCtx);
    sCape.capturedShoulders = 0;
}

static void ForgetSceneState(s16 sceneId, s8 spawnNum) {
    ResetCloth();
    sCape.isSpinAttackCharging = false;
}

static void HideCloth(const char* key) {
    ResetCloth();
}

S2H_MOD_EXPORT void ModSetApi(const S2HModApi* api) {
    sApi = api;
}

S2H_MOD_EXPORT const S2HModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

// Two authored poses of the hanging cloth, alternated so it waves while it hangs.
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

S2H_MOD_EXPORT void ModInit(void) {
    S2HCustomEquipDefinition definition = { 0 };

    definition.getItemEntry.drawFunc = DrawGetItem;
    definition.structSize = sizeof(definition);
    definition.key = MAGIC_CAPE_KEY;
    definition.iconPath = sIcon;
    definition.namePath = sName;
    definition.pauseText = "%rMagic Cape&%w%y\xB0%w toggles the cape. While it is&active, spells cost half their&"
                           "magic.";
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
    if (!sApi->RegisterCustomEquipment(&definition)) {
        return;
    }

    S2H_REGISTER_HOOK(sApi, OnPlayerUpdate, TickCape);
    S2H_REGISTER_HOOK(sApi, OnPlayerPostLimbDraw, CaptureShoulder);
    S2H_REGISTER_HOOK_FOR_ID(sApi, OnActorDrawEnd, ACTOR_PLAYER, DrawCloth);
    S2H_REGISTER_HOOK(sApi, OnMagicResolveCost, HalveSpellCost);
    S2H_REGISTER_HOOK(sApi, OnSceneInit, ForgetSceneState);
    S2H_REGISTER_HOOK_FOR_ID(sApi, ShouldActorInit, ACTOR_EN_M_THUNDER, EnterSpinAttackCost);
    S2H_REGISTER_HOOK_FOR_ID(sApi, OnActorInit, ACTOR_EN_M_THUNDER, LeaveSpinAttackCost);
    S2H_REGISTER_HOOK_FOR_ID(sApi, ShouldActorUpdate, ACTOR_EN_M_THUNDER, EnterSpinAttackCost);
    S2H_REGISTER_HOOK_FOR_ID(sApi, OnActorUpdate, ACTOR_EN_M_THUNDER, LeaveSpinAttackCost);
}
