#include <math.h>
#include <stdbool.h>
#include <string.h>
#include <libultraship/bridge/consolevariablebridge.h>

#include "z64items.h"
#include "z64aiming.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"

#define SWITCH_HOOK_KEY "nei.switch_hook"
#define SWITCH_HOOK_VISUAL_CHANNEL "nei.switch_hook.visual"
#define SWITCH_HOOK_SPEED 20.0f
#define SWITCH_HOOK_FLIGHT_FRAMES 26
#define SWITCH_HOOK_RETURN_SPEED 40.0f
#define SWITCH_HOOK_ARRIVE_DISTANCE 30.0f
#define SWITCH_HOOK_SELECT_RANGE 520.0f
#define SWITCH_HOOK_SELECT_CONE 0x1800
#define SWITCH_HOOK_SELECT_MIN_DISTANCE 30.0f
#define SWITCH_HOOK_GRAB_DISTANCE 60.0f
#define SWITCH_HOOK_BLIND_GRAB_DISTANCE 45.0f
#define SWITCH_HOOK_ARMED_DISTANCE 50.0f
#define SWITCH_HOOK_FOCUS_TRUST_DISTANCE 150.0f
#define SWITCH_HOOK_HOLD_FRAMES 2
#define SWITCH_HOOK_SETTLE_FRAMES 40
#define SWITCH_HOOK_TINT_FRAMES 80
#define SWITCH_HOOK_INVINCIBLE_FRAMES 20
#define SWITCH_HOOK_MAX_CHARGES 5
#define SWITCH_HOOK_RECHARGE_FRAMES 400
#define SWITCH_HOOK_DEPLETED_FRAMES 2400
#define SWITCH_HOOK_ANCHOR_SLOTS 2
#define SWITCH_HOOK_ANCHORS_PER_ACTOR 16
#define SWITCH_HOOK_CHAIN_LINK_SPACING 15.0f
#define SWITCH_HOOK_CHAIN_LINK_MAX 40
#define SWITCH_HOOK_CHAIN_SCALE 0.015f
#define SWITCH_HOOK_TIP_SCALE 1.0f
#define SWITCH_HOOK_GIVE_SCALE 1.0f
#define REMOTE_VISUAL_INTERVAL 5u

typedef enum {
    SWITCH_HOOK_STOWED,
    SWITCH_HOOK_IN_HAND,
    SWITCH_HOOK_AIMING,
    SWITCH_HOOK_FLYING,
    SWITCH_HOOK_SWAPPING,
    SWITCH_HOOK_RETURNING,
} SwitchHookPhase;

typedef struct {
    Collider* collider;
    Vec3f offset;
    Vec3f lastSet;
    bool hasLastSet;
} ColliderAnchor;

// A swapped actor keeps its hitboxes where it used to stand: they are world-space geometry that only
// the owner refreshes, and many actors build theirs once, in their init.
typedef struct {
    Actor* owner;
    Vec3f basePos;
    bool isActive;
    u8 count;
    Vec3f focusOffset;
    Vec3f focusLastSet;
    bool hasFocusLast;
    ColliderAnchor anchors[SWITCH_HOOK_ANCHORS_PER_ACTOR];
} AnchorSlot;

typedef struct {
    int16_t scene;
    uint8_t active;
    uint8_t adult;
    uint8_t flags[8];
    float values[12];
} RemoteVisualState;

static const ALIGN_ASSET(2) char sIconTex[] = "__OTR__textures/icon_item_custom/gItemIconSwitchHookTex";
static const ALIGN_ASSET(2) char sNameTex[] = "__OTR__textures/item_name_custom/gSwitchHookNameTex";
static const ALIGN_ASSET(2) char sGiveDL[] = "__OTR__objects/object_nei_switchhook/gSwitchHookGiveDL";
static const ALIGN_ASSET(2) char sHeldDL[] = "__OTR__objects/object_nei_switchhook/gSwitchHookHeldDL";
static const ALIGN_ASSET(2) char sBodyDL[] = "__OTR__objects/object_nei_switchhook/gSwitchHookBodyDL";
static const ALIGN_ASSET(2) char sTipDL[] = "__OTR__objects/object_nei_switchhook/gSwitchHookHeadDL";
static const ALIGN_ASSET(2) char sChainLinkDL[] = "__OTR__objects/gameplay_keep/gHookshotChainDL";
static const ALIGN_ASSET(2) char sAimAnim[] = "__OTR__objects/gameplay_keep/gPlayerAnim_link_hook_shot_ready";
static const ALIGN_ASSET(2) char sThrowAnim[] = "__OTR__objects/oot_gameplay_keep/gPlayerAnim_link_boom_throwR";

static const u8 sSwappableCategories[] = { ACTORCAT_ENEMY, ACTORCAT_PROP, ACTORCAT_CHEST, ACTORCAT_NPC };

static const char* const sRequiredHooks[] = { "OnGameFrameUpdate", "OnPlayerUpdate",     "OnActorDrawEnd",
                                              "OnSceneInit",       "OnInterfaceDrawEnd", "OnActorUpdate" };
static const S2HModRequirements sRequirements = { sizeof(S2HModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const S2HModApi* sApi;
static u8 sPhase;
static bool sIsThrowAnimating;
static s16 sFlightTimer;
static s16 sHoldTimer;
static s16 sSettleTimer;
static s16 sShotYaw;
static s16 sShotPitch;
static Vec3f sHookPos;
static Actor* sSelection;
static Actor* sSwapTarget;
static u8 sCharges = SWITCH_HOOK_MAX_CHARGES;
static s16 sRechargeTimer;
static s16 sDepletedTimer;
static AnchorSlot sAnchorSlots[SWITCH_HOOK_ANCHOR_SLOTS];
static bool sRemoteVisualReady;
static bool sRemoteVisualSent;
static uint32_t sRemoteVisualFrame;
static RemoteVisualState sLastRemoteVisual;

bool Player_UpdateUpperBody(Player* player, PlayState* play);
s32 Player_UpperAction_ChangeHeldItem(Player* player, PlayState* play);
void func_8083B1A0(Player* player, PlayState* play);
void Player_StopHorizontalMovement(Player* player);
s32 Player_SetAction(PlayState* play, Player* player, PlayerActionFunc actionFunc, s32 flags);
void Player_SetAction_PreserveItemAction(PlayState* play, Player* player, PlayerActionFunc actionFunc, s32 flags);
void Interface_DrawAmmoCount(PlayState* play, s16 button, s16 alpha);
void Interface_Dpad_DrawAmmoCount(PlayState* play, s16 button, s16 alpha);

static void ShotAction(Player* player, PlayState* play);
static void AimAction(Player* player, PlayState* play);

// The hand frame follows the forearm-to-hand bone; the held pose matrices were calibrated against it.
static void HandFrame(Player* player, MtxF* frame) {
    Vec3f hand = player->bodyPartsPos[PLAYER_BODYPART_LEFT_HAND];
    Vec3f arm = player->bodyPartsPos[PLAYER_BODYPART_LEFT_FOREARM];
    f32 dx = hand.x - arm.x;
    f32 dy = hand.y - arm.y;
    f32 dz = hand.z - arm.z;
    f32 horizontal = sqrtf(SQ(dx) + SQ(dz));
    f32 length = sqrtf(SQ(dx) + SQ(dy) + SQ(dz));
    f32 sinYaw = 0.0f;
    f32 cosYaw = 1.0f;
    f32 sinPitch = 0.0f;
    f32 cosPitch = 1.0f;

    if (length > 0.0001f) {
        sinPitch = dy / length;
        cosPitch = horizontal / length;
    }
    if (horizontal > 0.0001f) {
        sinYaw = dx / horizontal;
        cosYaw = dz / horizontal;
    }
    frame->xx = cosYaw;
    frame->yx = 0.0f;
    frame->zx = -sinYaw;
    frame->wx = 0.0f;
    frame->xy = -sinYaw * sinPitch;
    frame->yy = cosPitch;
    frame->zy = -cosYaw * sinPitch;
    frame->wy = 0.0f;
    frame->xz = sinYaw * cosPitch;
    frame->yz = sinPitch;
    frame->zz = cosYaw * cosPitch;
    frame->wz = 0.0f;
    frame->xw = hand.x;
    frame->yw = hand.y;
    frame->zw = hand.z;
    frame->ww = 1.0f;
}

// Where the chain leaves the held body, in the hand frame.
static Vec3f HandPosition(Player* player) {
    static const Vec3f sSocket = { -0.99972534f, 0.13365173f, 3.96459961f };
    MtxF frame;

    HandFrame(player, &frame);
    Vec3f pos = { frame.xw + frame.xx * sSocket.x + frame.xy * sSocket.y + frame.xz * sSocket.z,
                  frame.yw + frame.yx * sSocket.x + frame.yy * sSocket.y + frame.yz * sSocket.z,
                  frame.zw + frame.zx * sSocket.x + frame.zy * sSocket.y + frame.zz * sSocket.z };
    return pos;
}

static void TickCharges(void) {
    if (sDepletedTimer > 0) {
        if (--sDepletedTimer == 0) {
            sCharges = SWITCH_HOOK_MAX_CHARGES;
        }
        return;
    }
    if (sCharges >= SWITCH_HOOK_MAX_CHARGES || ++sRechargeTimer < SWITCH_HOOK_RECHARGE_FRAMES) {
        return;
    }
    sRechargeTimer = 0;
    sCharges++;
}

static bool SpendCharge(void) {
    if (sDepletedTimer > 0 || sCharges == 0) {
        return false;
    }
    sRechargeTimer = 0;
    if (--sCharges == 0) {
        sDepletedTimer = SWITCH_HOOK_DEPLETED_FRAMES;
    }
    return true;
}

static int32_t GetHookCharges(const char* key) {
    return sDepletedTimer > 0 ? 0 : sCharges;
}

static bool IsSwappable(Actor* actor) {
    for (u32 i = 0; i < ARRAY_COUNT(sSwappableCategories); i++) {
        if (actor->category == sSwappableCategories[i]) {
            return actor->update != NULL;
        }
    }
    return false;
}

static Actor* ScanForSelection(Player* player, PlayState* play) {
    Actor* best = NULL;
    s16 bestError = SWITCH_HOOK_SELECT_CONE;

    for (u32 i = 0; i < ARRAY_COUNT(sSwappableCategories); i++) {
        for (Actor* actor = play->actorCtx.actorLists[sSwappableCategories[i]].first; actor != NULL;
             actor = actor->next) {
            f32 dx = actor->world.pos.x - player->actor.world.pos.x;
            f32 dz = actor->world.pos.z - player->actor.world.pos.z;
            f32 distance = sqrtf(SQ(dx) + SQ(dz));

            if (!IsSwappable(actor) || distance < SWITCH_HOOK_SELECT_MIN_DISTANCE ||
                distance > SWITCH_HOOK_SELECT_RANGE) {
                continue;
            }
            s16 error = ABS((s16)(Math_Atan2S_XY(dz, dx) - player->actor.shape.rot.y));

            if (error < bestError) {
                bestError = error;
                best = actor;
            }
        }
    }
    return best;
}

static Actor* FindSwappableNear(PlayState* play, Vec3f* pos, f32 radius) {
    for (u32 i = 0; i < ARRAY_COUNT(sSwappableCategories); i++) {
        for (Actor* actor = play->actorCtx.actorLists[sSwappableCategories[i]].first; actor != NULL;
             actor = actor->next) {
            if (IsSwappable(actor) && Math_Vec3f_DistXYZ(pos, &actor->world.pos) < radius) {
                return actor;
            }
        }
    }
    return NULL;
}

static bool IsActorAlive(PlayState* play, Actor* actor) {
    if (actor == NULL) {
        return false;
    }
    for (u8 category = 0; category < ACTORCAT_MAX; category++) {
        for (Actor* other = play->actorCtx.actorLists[category].first; other != NULL; other = other->next) {
            if (other == actor) {
                return true;
            }
        }
    }
    return false;
}

static s16 RoundToS16(f32 value) {
    return (s16)(value >= 0.0f ? value + 0.5f : value - 0.5f);
}

static void ShiftCollider(Collider* collider, Vec3f* delta) {
    s16 dx = RoundToS16(delta->x);
    s16 dy = RoundToS16(delta->y);
    s16 dz = RoundToS16(delta->z);

    switch (collider->shape) {
        case COLSHAPE_JNTSPH: {
            ColliderJntSph* jntSph = (ColliderJntSph*)collider;

            for (s32 i = 0; i < jntSph->count; i++) {
                Sphere16* sphere = &jntSph->elements[i].dim.worldSphere;

                sphere->center.x += dx;
                sphere->center.y += dy;
                sphere->center.z += dz;
            }
            break;
        }
        case COLSHAPE_CYLINDER: {
            ColliderCylinder* cylinder = (ColliderCylinder*)collider;

            cylinder->dim.pos.x += dx;
            cylinder->dim.pos.y += dy;
            cylinder->dim.pos.z += dz;
            break;
        }
        case COLSHAPE_TRIS: {
            ColliderTris* tris = (ColliderTris*)collider;

            for (s32 i = 0; i < tris->count; i++) {
                TriNorm* tri = &tris->elements[i].dim;

                for (s32 vertex = 0; vertex < 3; vertex++) {
                    tri->vtx[vertex].x += delta->x;
                    tri->vtx[vertex].y += delta->y;
                    tri->vtx[vertex].z += delta->z;
                }
                // The plane is n . p + originDist = 0: without this every test still answers against
                // the triangle's original plane.
                tri->plane.originDist -=
                    tri->plane.normal.x * delta->x + tri->plane.normal.y * delta->y + tri->plane.normal.z * delta->z;
            }
            break;
        }
        case COLSHAPE_QUAD: {
            ColliderQuad* quad = (ColliderQuad*)collider;

            for (s32 i = 0; i < 4; i++) {
                quad->dim.quad[i].x += delta->x;
                quad->dim.quad[i].y += delta->y;
                quad->dim.quad[i].z += delta->z;
            }
            quad->dim.dcMid.x += delta->x;
            quad->dim.dcMid.y += delta->y;
            quad->dim.dcMid.z += delta->z;
            quad->dim.baMid.x += delta->x;
            quad->dim.baMid.y += delta->y;
            quad->dim.baMid.z += delta->z;
            break;
        }
        default:
            break;
    }
}

static bool GetColliderReference(Collider* collider, Vec3f* out) {
    switch (collider->shape) {
        case COLSHAPE_JNTSPH: {
            ColliderJntSph* jntSph = (ColliderJntSph*)collider;

            if (jntSph->count <= 0 || jntSph->elements == NULL) {
                return false;
            }
            out->x = jntSph->elements[0].dim.worldSphere.center.x;
            out->y = jntSph->elements[0].dim.worldSphere.center.y;
            out->z = jntSph->elements[0].dim.worldSphere.center.z;
            return true;
        }
        case COLSHAPE_CYLINDER: {
            ColliderCylinder* cylinder = (ColliderCylinder*)collider;

            out->x = cylinder->dim.pos.x;
            out->y = cylinder->dim.pos.y;
            out->z = cylinder->dim.pos.z;
            return true;
        }
        case COLSHAPE_TRIS: {
            ColliderTris* tris = (ColliderTris*)collider;

            if (tris->count <= 0 || tris->elements == NULL) {
                return false;
            }
            *out = tris->elements[0].dim.vtx[0];
            return true;
        }
        case COLSHAPE_QUAD:
            *out = ((ColliderQuad*)collider)->dim.quad[0];
            return true;
        default:
            return false;
    }
}

static bool IsColliderKnown(AnchorSlot* slot, Collider* collider) {
    for (u8 known = 0; known < slot->count; known++) {
        if (slot->anchors[known].collider == collider) {
            return true;
        }
    }
    return false;
}

// Colliders first seen after the swap may still sit at the old spot: anchor them to whichever base is closer.
static void CollectColliders(AnchorSlot* slot, Collider** list, s32 count) {
    for (s32 i = 0; i < count && slot->count < SWITCH_HOOK_ANCHORS_PER_ACTOR; i++) {
        Vec3f reference;

        if (list[i] == NULL || list[i]->actor != slot->owner || !GetColliderReference(list[i], &reference) ||
            IsColliderKnown(slot, list[i])) {
            continue;
        }
        Vec3f base = slot->owner->world.pos;

        if (Math_Vec3f_DistXYZ(&reference, &slot->basePos) < Math_Vec3f_DistXYZ(&reference, &base)) {
            base = slot->basePos;
        }
        ColliderAnchor* anchor = &slot->anchors[slot->count++];

        anchor->collider = list[i];
        anchor->offset.x = reference.x - base.x;
        anchor->offset.y = reference.y - base.y;
        anchor->offset.z = reference.z - base.z;
        anchor->hasLastSet = false;
    }
}

static void CollectAllColliders(PlayState* play, AnchorSlot* slot) {
    CollectColliders(slot, play->colChkCtx.colAT, play->colChkCtx.colATCount);
    CollectColliders(slot, play->colChkCtx.colAC, play->colChkCtx.colACCount);
    CollectColliders(slot, play->colChkCtx.colOC, play->colChkCtx.colOCCount);
}

static void CaptureColliders(PlayState* play, u8 slotIndex, Actor* actor) {
    AnchorSlot* slot = &sAnchorSlots[slotIndex];

    slot->owner = actor;
    slot->basePos = actor->world.pos;
    slot->isActive = true;
    slot->count = 0;
    slot->hasFocusLast = false;
    slot->focusOffset.x = actor->focus.pos.x - actor->world.pos.x;
    slot->focusOffset.y = actor->focus.pos.y - actor->world.pos.y;
    slot->focusOffset.z = actor->focus.pos.z - actor->world.pos.z;
    CollectAllColliders(play, slot);
}

static void ReanchorOne(AnchorSlot* slot, ColliderAnchor* anchor) {
    Vec3f reference;

    if (!GetColliderReference(anchor->collider, &reference)) {
        return;
    }
    if (anchor->hasLastSet &&
        (reference.x != anchor->lastSet.x || reference.y != anchor->lastSet.y || reference.z != anchor->lastSet.z)) {
        anchor->offset.x = reference.x - slot->owner->world.pos.x;
        anchor->offset.y = reference.y - slot->owner->world.pos.y;
        anchor->offset.z = reference.z - slot->owner->world.pos.z;
        anchor->lastSet = reference;
        return;
    }
    Vec3f delta = { slot->owner->world.pos.x + anchor->offset.x - reference.x,
                    slot->owner->world.pos.y + anchor->offset.y - reference.y,
                    slot->owner->world.pos.z + anchor->offset.z - reference.z };

    ShiftCollider(anchor->collider, &delta);
    anchor->hasLastSet = GetColliderReference(anchor->collider, &anchor->lastSet);
}

static void ReanchorSlotFocus(AnchorSlot* slot) {
    Actor* owner = slot->owner;
    bool hasOwnerMovedFocus = slot->hasFocusLast && (owner->focus.pos.x != slot->focusLastSet.x ||
                                                     owner->focus.pos.y != slot->focusLastSet.y ||
                                                     owner->focus.pos.z != slot->focusLastSet.z);

    if (hasOwnerMovedFocus) {
        slot->focusOffset.x = owner->focus.pos.x - owner->world.pos.x;
        slot->focusOffset.y = owner->focus.pos.y - owner->world.pos.y;
        slot->focusOffset.z = owner->focus.pos.z - owner->world.pos.z;
        slot->focusLastSet = owner->focus.pos;
        return;
    }
    owner->focus.pos.x = owner->world.pos.x + slot->focusOffset.x;
    owner->focus.pos.y = owner->world.pos.y + slot->focusOffset.y;
    owner->focus.pos.z = owner->world.pos.z + slot->focusOffset.z;
    slot->focusLastSet = owner->focus.pos;
    slot->hasFocusLast = true;
}

static void ReanchorColliders(PlayState* play) {
    for (u8 index = 0; index < SWITCH_HOOK_ANCHOR_SLOTS; index++) {
        AnchorSlot* slot = &sAnchorSlots[index];

        if (!slot->isActive) {
            continue;
        }
        if (!IsActorAlive(play, slot->owner)) {
            slot->isActive = false;
            slot->owner = NULL;
            slot->count = 0;
            continue;
        }
        for (u8 i = 0; i < slot->count; i++) {
            ReanchorOne(slot, &slot->anchors[i]);
        }
        ReanchorSlotFocus(slot);
    }
}

static void ClearColliderAnchors(void) {
    for (u8 index = 0; index < SWITCH_HOOK_ANCHOR_SLOTS; index++) {
        sAnchorSlots[index].isActive = false;
        sAnchorSlots[index].owner = NULL;
        sAnchorSlots[index].count = 0;
    }
}

// Colliders an actor registers only after its first post-swap update are picked up here.
static void SyncSwappedActor(Actor* actor) {
    PlayState* play = gPlayState;
    bool isTracked = false;

    if (play == NULL) {
        return;
    }
    for (u8 index = 0; index < SWITCH_HOOK_ANCHOR_SLOTS; index++) {
        AnchorSlot* slot = &sAnchorSlots[index];

        if (!slot->isActive || slot->owner != actor) {
            continue;
        }
        isTracked = true;
        CollectAllColliders(play, slot);
    }
    if (isTracked) {
        ReanchorColliders(play);
    }
}

static void PlantActorAt(Actor* actor, Vec3f* pos) {
    actor->world.pos = *pos;
    actor->prevPos = *pos;
    actor->velocity.x = 0.0f;
    actor->velocity.y = 0.0f;
    actor->velocity.z = 0.0f;
    actor->speed = 0.0f;
    // With the ground flag still set, the scene collision drags the actor back to its old floor height
    // and a downward swap would move only one of the two.
    actor->bgCheckFlags = 0;
}

static void ReturnHookToHand(Player* player, PlayState* play) {
    sPhase = SWITCH_HOOK_IN_HAND;
    sSwapTarget = NULL;
    AudioSfx_StopById(NA_SE_IT_HOOKSHOT_CHAIN);
    if (player->actionFunc == (PlayerActionFunc)ShotAction) {
        func_8083B1A0(player, play);
    }
}

static void StartSwap(Player* player, PlayState* play, Actor* target) {
    Vec3f linkDestination = target->world.pos;
    Vec3f targetDestination = player->actor.world.pos;

    CaptureColliders(play, 0, &player->actor);
    CaptureColliders(play, 1, target);
    PlantActorAt(&player->actor, &linkDestination);
    PlantActorAt(target, &targetDestination);
    target->home.pos = targetDestination;
    ReanchorColliders(play);
    player->speedXZ = 0.0f;
    player->invincibilityTimer = SWITCH_HOOK_INVINCIBLE_FRAMES;
    Actor_SetColorFilter(target, 0, 255, 0, SWITCH_HOOK_TINT_FRAMES);
    AudioSfx_StopById(NA_SE_IT_HOOKSHOT_CHAIN);
    Audio_PlaySfx_AtPos(&player->actor.world.pos, NA_SE_IT_HOOKSHOT_STICK_OBJ);
    sSwapTarget = target;
    sHookPos = targetDestination;
    sHoldTimer = SWITCH_HOOK_HOLD_FRAMES;
    sSettleTimer = SWITCH_HOOK_SETTLE_FRAMES;
    sPhase = SWITCH_HOOK_SWAPPING;
}

static void HoldSwap(Player* player, PlayState* play) {
    if (sSwapTarget == NULL || sSwapTarget->update == NULL) {
        ReturnHookToHand(player, play);
        return;
    }
    Vec3f linkPos = player->actor.world.pos;
    Vec3f targetPos = sSwapTarget->world.pos;

    PlantActorAt(&player->actor, &linkPos);
    PlantActorAt(sSwapTarget, &targetPos);
    player->speedXZ = 0.0f;
    player->invincibilityTimer = SWITCH_HOOK_INVINCIBLE_FRAMES / 2;
    if (--sHoldTimer > 0) {
        return;
    }
    Audio_PlaySfx_AtPos(&player->actor.world.pos, NA_SE_EV_ROLL_STAND);
    ReturnHookToHand(player, play);
}

static bool HasHitWall(PlayState* play, Vec3f* from, Vec3f* to) {
    CollisionPoly* poly = NULL;
    s32 bgId;
    Vec3f hit;

    if (!BgCheck_EntityLineTest1(&play->colCtx, from, to, &hit, &poly, true, true, true, true, &bgId)) {
        return false;
    }
    *to = hit;
    return true;
}

static bool IsWithinGrab(Actor* actor) {
    return Math_Vec3f_DistXYZ(&sHookPos, &actor->focus.pos) < SWITCH_HOOK_GRAB_DISTANCE ||
           Math_Vec3f_DistXYZ(&sHookPos, &actor->world.pos) < SWITCH_HOOK_GRAB_DISTANCE;
}

static Actor* FindGrabbedActor(Player* player, PlayState* play) {
    if (Math_Vec3f_DistXYZ(&sHookPos, &player->actor.world.pos) <= SWITCH_HOOK_ARMED_DISTANCE) {
        return NULL;
    }
    if (sSelection == NULL) {
        return FindSwappableNear(play, &sHookPos, SWITCH_HOOK_BLIND_GRAB_DISTANCE);
    }
    if (sSelection->update != NULL && IsWithinGrab(sSelection)) {
        return sSelection;
    }
    return NULL;
}

static void FlyHook(Player* player, PlayState* play) {
    Vec3f previous = sHookPos;
    f32 reach = Math_CosS(sShotPitch) * SWITCH_HOOK_SPEED;

    sHookPos.x += Math_SinS(sShotYaw) * reach;
    sHookPos.y -= Math_SinS(sShotPitch) * SWITCH_HOOK_SPEED;
    sHookPos.z += Math_CosS(sShotYaw) * reach;
    Actor_PlaySfx_Flagged2(&player->actor, NA_SE_IT_HOOKSHOT_CHAIN);

    Actor* target = FindGrabbedActor(player, play);

    if (target != NULL) {
        StartSwap(player, play, target);
        return;
    }
    if (HasHitWall(play, &previous, &sHookPos) || --sFlightTimer <= 0) {
        sPhase = SWITCH_HOOK_RETURNING;
    }
}

static void ReelHookIn(Player* player, PlayState* play) {
    Vec3f hand = HandPosition(player);
    f32 distance = Math_Vec3f_DistXYZ(&sHookPos, &hand);

    if (distance < SWITCH_HOOK_ARRIVE_DISTANCE) {
        Audio_PlaySfx_AtPos(&player->actor.world.pos, NA_SE_IT_HOOKSHOT_RECEIVE);
        ReturnHookToHand(player, play);
        return;
    }
    f32 step = SWITCH_HOOK_RETURN_SPEED / distance;

    sHookPos.x += (hand.x - sHookPos.x) * step;
    sHookPos.y += (hand.y - sHookPos.y) * step;
    sHookPos.z += (hand.z - sHookPos.z) * step;
    Actor_PlaySfx_Flagged2(&player->actor, NA_SE_IT_HOOKSHOT_CHAIN);
}

static void FaceShot(Player* player) {
    player->actor.shape.rot.y = sShotYaw;
    player->actor.world.rot.y = sShotYaw;
    player->yaw = sShotYaw;
}

static void ShotAction(Player* player, PlayState* play) {
    Player_StopHorizontalMovement(player);
    FaceShot(player);
    if (sIsThrowAnimating && PlayerAnimation_Update(play, &player->skelAnime)) {
        sIsThrowAnimating = false;
    }
    Z64Items_KeepHeld(sApi);
    switch (sPhase) {
        case SWITCH_HOOK_FLYING:
            FlyHook(player, play);
            break;
        case SWITCH_HOOK_SWAPPING:
            HoldSwap(player, play);
            break;
        case SWITCH_HOOK_RETURNING:
            ReelHookIn(player, play);
            break;
        default:
            ReturnHookToHand(player, play);
            break;
    }
}

static void EnterShotAction(Player* player, PlayState* play) {
    player->itemAction = player->heldItemAction;
    Player_SetAction_PreserveItemAction(play, player, (PlayerActionFunc)ShotAction, 0);
}

static void FireHook(Player* player, PlayState* play, s16 yaw, s16 pitch) {
    if (!SpendCharge()) {
        Audio_PlaySfx(NA_SE_SY_ERROR);
        return;
    }
    sShotYaw = yaw;
    sShotPitch = pitch;
    sHookPos = HandPosition(player);
    sFlightTimer = SWITCH_HOOK_FLIGHT_FRAMES;
    sPhase = SWITCH_HOOK_FLYING;
    EnterShotAction(player, play);
    PlayerAnimation_PlayOnce(play, &player->skelAnime, (PlayerAnimationHeader*)sThrowAnim);
    sIsThrowAnimating = true;
    Player_StopHorizontalMovement(player);
}

static void StopAiming(Player* player, PlayState* play) {
    Z64Aiming_Release(player, play);
    if (sPhase == SWITCH_HOOK_AIMING) {
        sPhase = SWITCH_HOOK_IN_HAND;
    }
}

static void StartAiming(Player* player, PlayState* play) {
    if (!Z64Aiming_Request(player, play)) {
        return;
    }
    sPhase = SWITCH_HOOK_AIMING;
    sSelection = NULL;
    Player_SetAction(play, player, (PlayerActionFunc)AimAction, 1);
    Player_StopHorizontalMovement(player);
    PlayerAnimation_PlayLoop(play, &player->skelAnime, (PlayerAnimationHeader*)sAimAnim);
}

static bool ShouldLeaveAim(Player* player, PlayState* play) {
    return (play->state.input[0].press.button & (BTN_CUP | BTN_A | BTN_B)) || player->invincibilityTimer < 0 ||
           (player->stateFlags1 & PLAYER_STATE1_8000000);
}

static void AimAction(Player* player, PlayState* play) {
    if (ShouldLeaveAim(player, play)) {
        StopAiming(player, play);
        func_8083B1A0(player, play);
        return;
    }
    Z64Aiming_Update(player, play);
    Player_StopHorizontalMovement(player);
    PlayerAnimation_Update(play, &player->skelAnime);
    Player_UpdateUpperBody(player, play);
    Z64Items_KeepHeld(sApi);
}

// A focus point far from the body (a chest lid, a tall NPC's head) misses: aim at the body's middle instead.
static Vec3f GetSelectionAimPoint(Actor* selection) {
    Vec3f aim = selection->focus.pos;

    if (Math_Vec3f_DistXYZ(&aim, &selection->world.pos) > SWITCH_HOOK_FOCUS_TRUST_DISTANCE) {
        aim = selection->world.pos;
        aim.y += CLAMP(selection->colChkInfo.cylHeight * 0.5f, 15.0f, 45.0f);
    }
    return aim;
}

static void AimOrFire(Player* player, PlayState* play) {
    if (sPhase == SWITCH_HOOK_AIMING) {
        s16 yaw;
        s16 pitch;

        Z64Aiming_GetDirection(player, &yaw, &pitch);
        StopAiming(player, play);
        FireHook(player, play, yaw, pitch);
        return;
    }
    if (sPhase != SWITCH_HOOK_IN_HAND) {
        return;
    }
    if (sSelection == NULL || sSelection->update == NULL) {
        FireHook(player, play, player->actor.shape.rot.y, 0);
        return;
    }
    Vec3f aim = GetSelectionAimPoint(sSelection);
    Vec3f hand = HandPosition(player);
    f32 dx = aim.x - hand.x;
    f32 dy = aim.y - hand.y;
    f32 dz = aim.z - hand.z;

    FireHook(player, play, Math_Atan2S_XY(dz, dx), Math_Atan2S_XY(sqrtf(SQ(dx) + SQ(dz)), -dy));
}

static bool CanUseSwitchHook(Player* player, PlayState* play) {
    return !(player->stateFlags1 & PLAYER_STATE1_8000000);
}

static void TakeOutSwitchHook(PlayState* play, Player* player) {
    sPhase = SWITCH_HOOK_IN_HAND;
    sSelection = NULL;
}

static void StowSwitchHook(Player* player, PlayState* play) {
    if (sPhase == SWITCH_HOOK_AIMING) {
        StopAiming(player, play);
    }
    if (player->actionFunc == (PlayerActionFunc)ShotAction || player->actionFunc == (PlayerActionFunc)AimAction) {
        func_8083B1A0(player, play);
    }
    AudioSfx_StopById(NA_SE_IT_HOOKSHOT_CHAIN);
    sPhase = SWITCH_HOOK_STOWED;
    sSelection = NULL;
    sSwapTarget = NULL;
}

static int32_t HoldSwitchHook(Player* player, PlayState* play) {
    if (sPhase == SWITCH_HOOK_IN_HAND) {
        sHookPos = HandPosition(player);
    }
    return 0;
}

static bool IsChangingHeldItem(Player* player) {
    return (player->stateFlags3 & PLAYER_STATE3_START_CHANGING_HELD_ITEM) ||
           player->upperActionFunc == Player_UpperAction_ChangeHeldItem;
}

static void AimOnCUp(Player* player, PlayState* play) {
    if (sPhase != SWITCH_HOOK_IN_HAND || player->heldItemAction != PLAYER_IA_CUSTOM || IsChangingHeldItem(player) ||
        !(play->state.input[0].press.button & BTN_CUP) || !CanUseSwitchHook(player, play)) {
        return;
    }
    StartAiming(player, play);
}

static void MarkSelection(Player* player, PlayState* play) {
    if (sPhase != SWITCH_HOOK_IN_HAND) {
        return;
    }
    sSelection = ScanForSelection(player, play);
    if (sSelection != NULL) {
        Actor_SetColorFilter(sSelection, 0, 255, 0, 4);
    }
}

static bool CanResumeShot(Player* player) {
    return !(player->stateFlags1 &
             (PLAYER_STATE1_DEAD | PLAYER_STATE1_4000000 | PLAYER_STATE1_20000000 | PLAYER_STATE1_8000000));
}

// A shot cut short by another action (a ledge, a door prompt) picks up where it was unless Link is hurt or busy.
static void ResumeInterruptedShot(Player* player, PlayState* play) {
    if (sPhase < SWITCH_HOOK_FLYING || player->actionFunc == (PlayerActionFunc)ShotAction) {
        return;
    }
    if (CanResumeShot(player)) {
        EnterShotAction(player, play);
        return;
    }
    ReturnHookToHand(player, play);
}

static void UpdateSwitchHook(Player* player) {
    PlayState* play = gPlayState;

    if (play == NULL || player != GET_PLAYER(play)) {
        return;
    }
    TickCharges();
    if (sPhase == SWITCH_HOOK_AIMING && player->actionFunc != (PlayerActionFunc)AimAction) {
        StopAiming(player, play);
    }
    ResumeInterruptedShot(player, play);
    AimOnCUp(player, play);
    MarkSelection(player, play);
    if (sSettleTimer > 0) {
        ReanchorColliders(play);
        sSettleTimer--;
    }
}

static s16 GetButtonAlpha(PlayState* play, u8 button) {
    InterfaceContext* interfaceCtx = &play->interfaceCtx;
    DpadInterface* dpad = &interfaceCtx->shipInterface.dpad;

    switch (button) {
        case EQUIP_SLOT_C_LEFT:
            return interfaceCtx->cLeftAlpha;
        case EQUIP_SLOT_C_DOWN:
            return interfaceCtx->cDownAlpha;
        case EQUIP_SLOT_C_RIGHT:
            return interfaceCtx->cRightAlpha;
        case S2H_ITEM_BUTTON_DPAD_FIRST + EQUIP_SLOT_D_RIGHT:
            return dpad->dRightAlpha;
        case S2H_ITEM_BUTTON_DPAD_FIRST + EQUIP_SLOT_D_LEFT:
            return dpad->dLeftAlpha;
        case S2H_ITEM_BUTTON_DPAD_FIRST + EQUIP_SLOT_D_DOWN:
            return dpad->dDownAlpha;
        case S2H_ITEM_BUTTON_DPAD_FIRST + EQUIP_SLOT_D_UP:
            return dpad->dUpAlpha;
        default:
            return 0;
    }
}

// The vanilla counter only draws for ammo items, so the button briefly shows Magic Beans holding the charges.
static void DrawChargesOnButton(PlayState* play, u8 button, s16 alpha) {
    bool isDpad = button >= S2H_ITEM_BUTTON_DPAD_FIRST;
    s16 slot = isDpad ? button - S2H_ITEM_BUTTON_DPAD_FIRST : button;
    u8* buttonItem = isDpad ? &DPAD_BUTTON_ITEM_EQUIP(0, slot) : &BUTTON_ITEM_EQUIP(0, slot);
    u8 savedItem = *buttonItem;
    s8 savedAmmo = AMMO(ITEM_MAGIC_BEANS);

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL39_Overlay(play->state.gfxCtx);
    gDPPipeSync(OVERLAY_DISP++);
    gDPSetEnvColor(OVERLAY_DISP++, 0, 0, 0, 0);
    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 255, 255, alpha);
    gDPSetCombineLERP(OVERLAY_DISP++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0, PRIMITIVE,
                      ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0);
    CLOSE_DISPS(play->state.gfxCtx);

    *buttonItem = ITEM_MAGIC_BEANS;
    AMMO(ITEM_MAGIC_BEANS) = (s8)GetHookCharges(SWITCH_HOOK_KEY);
    if (isDpad) {
        Interface_Dpad_DrawAmmoCount(play, slot, alpha);
    } else {
        Interface_DrawAmmoCount(play, slot, alpha);
    }
    AMMO(ITEM_MAGIC_BEANS) = savedAmmo;
    *buttonItem = savedItem;
}

static bool IsButtonShown(PlayState* play, u8 button) {
    if (button >= S2H_ITEM_BUTTON_DPAD_FIRST && !CVarGetInteger("gEnhancements.Dpad.DpadEquips", 0)) {
        return false;
    }
    return GetButtonAlpha(play, button) != 0;
}

static void DrawHookChargeCount(PlayState* play) {
    if (play->pauseCtx.state != PAUSE_STATE_OFF || play->gameOverCtx.state != GAMEOVER_INACTIVE) {
        return;
    }
    for (u8 button = S2H_ITEM_BUTTON_C_FIRST; button <= S2H_ITEM_BUTTON_DPAD_LAST; button++) {
        const char* key = sApi->GetEquippedCustomItem(button);

        if (key == NULL || strcmp(key, SWITCH_HOOK_KEY) != 0 || !IsButtonShown(play, button)) {
            continue;
        }
        DrawChargesOnButton(play, button, GetButtonAlpha(play, button));
    }
}

static void ForgetScene(s16 sceneId, s8 spawnNum) {
    Z64Aiming_Drop();
    ClearColliderAnchors();
    sSettleTimer = 0;
    sIsThrowAnimating = false;
    sSelection = NULL;
    sSwapTarget = NULL;
    sPhase = SWITCH_HOOK_STOWED;
}

static void DrawSegment(PlayState* play, Vec3f* pos, f32 yaw, f32 pitch, f32 scale, Gfx* displayList) {
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Translate(pos->x, pos->y, pos->z, MTXMODE_NEW);
    Matrix_RotateYF(yaw, MTXMODE_APPLY);
    Matrix_RotateXF(pitch, MTXMODE_APPLY);
    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, displayList);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawChain(PlayState* play, Vec3f* from, Vec3f* to) {
    f32 dx = to->x - from->x;
    f32 dy = to->y - from->y;
    f32 dz = to->z - from->z;
    f32 yaw = Math_Atan2F_XY(dz, dx);
    f32 pitch = Math_Atan2F_XY(sqrtf(SQ(dx) + SQ(dz)), -dy);
    s32 links =
        CLAMP((s32)(sqrtf(SQ(dx) + SQ(dy) + SQ(dz)) / SWITCH_HOOK_CHAIN_LINK_SPACING), 1, SWITCH_HOOK_CHAIN_LINK_MAX);

    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    for (s32 i = 0; i <= links; i++) {
        f32 t = (f32)i / links;
        Vec3f link = { from->x + dx * t, from->y + dy * t, from->z + dz * t };

        DrawSegment(play, &link, yaw, pitch, SWITCH_HOOK_CHAIN_SCALE, (Gfx*)sChainLinkDL);
    }
    DrawSegment(play, to, yaw, pitch, SWITCH_HOOK_TIP_SCALE, (Gfx*)sTipDL);
}

// In flight only the body stays in the hand; the head travels with the chain.
static void DrawHeldHook(Player* player, PlayState* play, bool isExtended) {
    MtxF frame;

    HandFrame(player, &frame);
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    Matrix_Push();
    Matrix_Put(&frame);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)(isExtended ? sBodyDL : sHeldDL));
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawHookOn(Player* player, PlayState* play, bool isExtended, Vec3f* tip) {
    DrawHeldHook(player, play, isExtended);
    if (!isExtended) {
        return;
    }
    Vec3f hand = HandPosition(player);

    DrawChain(play, &hand, tip);
}

static void DrawSwitchHook(Actor* actor, PlayState* play) {
    Player* player = (Player*)actor;

    if (sPhase == SWITCH_HOOK_STOWED || player->actor.scale.y < 0.0f ||
        (player->stateFlags2 & PLAYER_STATE2_20000000)) {
        return;
    }
    DrawHookOn(player, play, sPhase >= SWITCH_HOOK_FLYING, &sHookPos);
    if (sPhase == SWITCH_HOOK_AIMING) {
        Z64Aiming_DrawReticle(play, player, SWITCH_HOOK_SPEED * SWITCH_HOOK_FLIGHT_FRAMES,
                              (Z64AimingReticleStyle){ Z64AIMING_TOGGLE_HOOKSHOT, Z64AIMING_TINT_ATTRACT });
    }
}

static void DrawGetItem(PlayState* play, S2HGetItemEntry* entry) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    Matrix_Scale(SWITCH_HOOK_GIVE_SCALE, SWITCH_HOOK_GIVE_SCALE, SWITCH_HOOK_GIVE_SCALE, MTXMODE_APPLY);
    Matrix_RotateYF((s16)(play->gameplayFrames * 2) * 0.01f, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sGiveDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static const S2HCustomItemRandomizer sRandomizerItem = {
    sizeof(S2HCustomItemRandomizer), 0, S2H_CUSTOM_ITEM_TYPE_ITEM, 0, 1, SWITCH_HOOK_KEY, SWITCH_HOOK_KEY, "",
};

static void ReceiveRemoteVisual(const S2HNetMessage* message) {
    (void)message;
}

static bool ReadRemoteVisual(Player* player, RemoteVisualState* visual) {
    if (!sRemoteVisualReady || gPlayState == NULL || player == GET_PLAYER(gPlayState)) {
        return false;
    }
    uint32_t peerId = sApi->GetNetPeerIdForActor(&player->actor);

    if (peerId == 0 ||
        sApi->GetNetPeerState(peerId, SWITCH_HOOK_VISUAL_CHANNEL, visual, sizeof(*visual)) != sizeof(*visual)) {
        return false;
    }
    if (visual->active != 1 || visual->adult > 1 || visual->scene != gPlayState->sceneId || visual->flags[0] > 1 ||
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
        visual.scene = play->sceneId;
        visual.active = (sPhase != SWITCH_HOOK_STOWED);
        if (visual.active) {
            visual.flags[0] = sPhase >= SWITCH_HOOK_FLYING;
            visual.values[0] = sHookPos.x;
            visual.values[1] = sHookPos.y;
            visual.values[2] = sHookPos.z;
        }
        if (sRemoteVisualSent && (uint32_t)(play->gameplayFrames - sRemoteVisualFrame) < REMOTE_VISUAL_INTERVAL) {
            return;
        }
    }
    if (sRemoteVisualSent && memcmp(&visual, &sLastRemoteVisual, sizeof(visual)) == 0) {
        return;
    }
    if (sApi->SetNetPeerState(SWITCH_HOOK_VISUAL_CHANNEL, &visual, sizeof(visual))) {
        sLastRemoteVisual = visual;
        sRemoteVisualSent = true;
        sRemoteVisualFrame = play != NULL ? play->gameplayFrames : 0;
    }
}

static void DrawRemoteSwitchHook(Actor* actor, PlayState* play) {
    if (actor->id == ACTOR_PLAYER || !sRemoteVisualReady || sApi->GetNetPeerIdForActor(actor) == 0) {
        return;
    }
    Player* player = (Player*)actor;
    RemoteVisualState visual;

    if (!ReadRemoteVisual(player, &visual)) {
        return;
    }
    Vec3f tip = { visual.values[0], visual.values[1], visual.values[2] };

    DrawHookOn(player, play, visual.flags[0] != 0, &tip);
}

static void InitRemoteVisuals(void) {
    if (!S2H_MOD_API_HAS(sApi, RegisterNetChannel) || !S2H_MOD_API_HAS(sApi, SetNetPeerState) ||
        !S2H_MOD_API_HAS(sApi, GetNetPeerState) || !S2H_MOD_API_HAS(sApi, GetNetPeerIdForActor)) {
        return;
    }
    sRemoteVisualReady = sApi->RegisterNetChannel(SWITCH_HOOK_VISUAL_CHANNEL, 1, ReceiveRemoteVisual);
    if (!sRemoteVisualReady) {
        return;
    }
    S2H_REGISTER_HOOK(sApi, OnGameFrameUpdate, PublishRemoteVisual);
    S2H_REGISTER_HOOK(sApi, OnActorDrawEnd, DrawRemoteSwitchHook);
}

S2H_MOD_EXPORT void ModSetApi(const S2HModApi* api) {
    sApi = api;
}

S2H_MOD_EXPORT const S2HModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

S2H_MOD_EXPORT void ModInit(void) {
    InitRemoteVisuals();
    if (!S2H_MOD_API_HAS(sApi, RegisterCustomItem)) {
        return;
    }

    S2HCustomItemDefinition hook = Z64Items_Define(SWITCH_HOOK_KEY, sIconTex, sNameTex);

    Z64Items_SetButtons(&hook, S2H_CUSTOM_ITEM_C_BUTTON | S2H_CUSTOM_ITEM_DPAD);
    Z64Items_SetForms(&hook, S2H_CUSTOM_ITEM_FORM_BIT(PLAYER_FORM_HUMAN));
    Z64Items_SetPlacement(&hook, 0, 14, 0);
    Z64Items_SetTextbox(&hook, "You got the %rSwitch Hook%w! A&hook that trades places with&whatever it catches.^"
                               "Face a %yfoe%w, a %ypot%w or a %ychest%w&and it glows blue: press %y\xB2%w and&"
                               "you stand where it stood.^"
                               "Press %y\xB6%w first to aim it by hand.^"
                               "It holds %g5 shots%w, and they come&back on their own.");
    Z64Items_SetPauseText(&hook, "%rSwitch Hook&%wPress %y\xB2%w to trade places with the&"
                                 "blue target. %y\xB6%w aims it by hand.");
    Z64Items_SetCanUse(&hook, CanUseSwitchHook);
    Z64Items_SetAction(&hook, TakeOutSwitchHook, HoldSwitchHook);
    Z64Items_SetHeldCallbacks(&hook, AimOrFire, StowSwitchHook, NULL);
    Z64Items_SetAmmo(&hook, GetHookCharges);
    hook.modelGroup = PLAYER_MODELGROUP_DEFAULT;
    hook.getItemEntry.drawFunc = DrawGetItem;

    Z64Items_SetLogic(&hook, &sRandomizerItem);
    if (!Z64Items_Register(sApi, &hook)) {
        return;
    }
    S2H_REGISTER_HOOK(sApi, OnPlayerUpdate, UpdateSwitchHook);
    S2H_REGISTER_HOOK(sApi, OnInterfaceDrawEnd, DrawHookChargeCount);
    S2H_REGISTER_HOOK(sApi, OnActorUpdate, SyncSwappedActor);
    S2H_REGISTER_HOOK(sApi, OnSceneInit, ForgetScene);
    S2H_REGISTER_HOOK_FOR_ID(sApi, OnActorDrawEnd, ACTOR_PLAYER, DrawSwitchHook);
}
