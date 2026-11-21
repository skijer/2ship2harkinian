typedef enum {
    BYRNA_NONE,
    BYRNA_DRAW,
    BYRNA_SHEATHE,
    BYRNA_SLASH,
    BYRNA_DASH,
    BYRNA_LAUNCH,
    BYRNA_AIR_IDLE,
    BYRNA_AIR_B,
    BYRNA_AIR_A,
    BYRNA_REBOUND,
    BYRNA_POUND,
    BYRNA_LAND,
    BYRNA_GUARD
} ByrnaMove;

typedef struct {
    PlayerAnimationHeader header;
    s16* values;
} ByrnaClip;

#include "byrna_vault_floor.inc.c"

static ByrnaClip sClips[CLIP_MAX];
static struct {
    ByrnaMove move;
    ClipId clip;
    f32 frame;
    f32 gravity;
    f32 terminalVelocity;
    f32 launchFloor;
    s16 ticks;
    s16 shieldTicks;
    bool active;
    bool landedAttack;
    bool impulse;
    bool immuneOwned;
    bool bodyAcOwned;
} sByrna;
static ColliderCylinder sForward;
static ColliderCylinder sGuard;
static ColliderTris sAirCylinder;
static ColliderTrisElement sAirElements[BYRNA_CYL_TRIS];
static ColliderTrisElementInit sAirInitElements[BYRNA_CYL_TRIS];
static bool sCollidersReady;
static s32 sTrail = -1;
static bool sGuardRejected;
static void ByrnaAction(Player* player, PlayState* play);
static void StartAirIdle(PlayState* play, Player* player);

static bool IsWorn(void) {
    return CustomEquipRegistry_IsWorn(BYRNA_KEY);
}

static bool IsSwordAction(s32 itemAction) {
    return itemAction >= PLAYER_IA_SWORD_KOKIRI && itemAction <= PLAYER_IA_SWORD_GILDED;
}

static bool IsWielding(Player* player) {
    return IsWorn() && player->transformation == PLAYER_FORM_HUMAN && IsSwordAction(player->heldItemAction);
}

static bool IsGrounded(Player* player) {
    return (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) != 0;
}

static bool IsPlayerBusy(Player* player) {
    return (player->stateFlags1 &
            (PLAYER_STATE1_DEAD | PLAYER_STATE1_1 | PLAYER_STATE1_20000000 | PLAYER_STATE1_10000000 |
             PLAYER_STATE1_400 | PLAYER_STATE1_4000000 | PLAYER_STATE1_8000000 | PLAYER_STATE1_800000 |
             PLAYER_STATE1_TALKING | PLAYER_STATE1_200000 | PLAYER_STATE1_4000)) != 0;
}

static bool IsEnemy(Actor* actor) {
    return actor != NULL && actor->update != NULL &&
           (actor->category == ACTORCAT_ENEMY || actor->category == ACTORCAT_BOSS);
}

static bool IsActorAlive(PlayState* play, Actor* actor) {
    if (actor == NULL) {
        return false;
    }
    for (s32 category = 0; category < ACTORCAT_MAX; category++) {
        for (Actor* other = play->actorCtx.actorLists[category].first; other != NULL; other = other->next) {
            if (other == actor) {
                return other->update != NULL;
            }
        }
    }
    return false;
}

static s16 InterpolateAngle(s16 from, s16 to, f32 t) {
    return (s16)(from + (s16)(to - from) * t);
}

static s32 GetVaultFloorRow(void) {
    return FormRegistry_IsCurrentFormAdult() ? 0 : 1;
}

static PlayerAnimationHeader* LoadClip(ClipId id) {
    ByrnaClip* clip = &sClips[id];
    if (clip->values != NULL) {
        return &clip->header;
    }
    PlayerAnimationHeader* raw = ResourceMgr_LoadPlayerAnimAsHeader(sClipSources[id].path);
    if (raw == NULL || raw->common.frameCount < 2) {
        return NULL;
    }
    // Native locomotion samples fixed phases directly, bypassing playSpeed (29 walk / 20 run).
    s32 phaseCount = id == CLIP_WALK ? 29 : (id == CLIP_RUN ? 20 : 0);
    s32 rawCount = raw->common.frameCount;
    s32 count = phaseCount != 0 ? phaseCount : (rawCount - 1) * BYRNA_SAMPLES + 1;
    clip->values = malloc((size_t)count * ANIM_VALUES_PER_FRAME * sizeof(s16));
    if (clip->values == NULL) {
        return NULL;
    }
    s16* src = (s16*)raw->segmentVoid;
    for (s32 i = 0; i < count; i++) {
        f32 sourceFrame = phaseCount != 0 ? i * (f32)rawCount / phaseCount : i / (f32)BYRNA_SAMPLES;
        s32 from = (s32)sourceFrame;
        s32 to = phaseCount != 0 ? (from + 1) % rawCount : MIN(from + 1, rawCount - 1);
        f32 t = sourceFrame - from;
        s16* out = &clip->values[i * ANIM_VALUES_PER_FRAME];
        s16* a = &src[from * ANIM_VALUES_PER_FRAME];
        s16* b = &src[to * ANIM_VALUES_PER_FRAME];
        for (s32 j = 0; j < ANIM_VALUES_PER_FRAME; j++) {
            out[j] = j < 3 ? (s16)(a[j] + (b[j] - a[j]) * t) : InterpolateAngle(a[j], b[j], t);
        }
        out[0] = 0;
        out[2] = 0;
        if (!sClipSources[id].ground) {
            out[1] = src[1];
        }
    }
    clip->header.common.frameCount = (s16)count;
    clip->header.segmentVoid = clip->values;
    return &clip->header;
}

static f32 GetOriginalFrame(Player* player) {
    return player->skelAnime.curFrame / BYRNA_SAMPLES;
}

static f32 GetLastOriginalFrame(ClipId id) {
    PlayerAnimationHeader* clip = LoadClip(id);
    return clip != NULL ? Animation_GetLastFrame(clip) / BYRNA_SAMPLES : 0;
}

static bool HasCrossedFrame(f32 before, f32 now, f32 event) {
    return before < event && now >= event;
}

static bool IsInHitWindow(f32 before, f32 now, f32 start, f32 end) {
    return now >= start && before <= end;
}

static void PlayClip(PlayState* play, Player* player, ClipId clip, bool loop) {
    PlayerAnimationHeader* anim = LoadClip(clip);
    if (anim == NULL) {
        return;
    }
    sByrna.clip = clip;
    sByrna.frame = 0;
    PlayerAnimation_Change(play, &player->skelAnime, anim, BYRNA_PLAY_SPEED * BYRNA_SAMPLES, 0,
                           Animation_GetLastFrame(anim), loop ? ANIMMODE_LOOP : ANIMMODE_ONCE, -3.0f);
}

static void SilenceVanillaBlade(Player* player) {
    player->meleeWeaponState = 0;
    player->meleeWeaponQuads[0].base.atFlags &= ~AT_ON;
    player->meleeWeaponQuads[1].base.atFlags &= ~AT_ON;
}

static void SetImmunity(Player* player, bool enabled) {
    bool isTimerOurs = player->invincibilityTimer >= -2 && player->invincibilityTimer <= 0;
    if (enabled) {
        if (isTimerOurs) {
            player->invincibilityTimer = -2;
            sByrna.immuneOwned = true;
        }
        return;
    }
    if (!sByrna.immuneOwned) {
        return;
    }
    if (isTimerOurs) {
        player->invincibilityTimer = 0;
    }
    sByrna.immuneOwned = false;
}

static void ReleaseGuard(Player* player) {
    if (sByrna.bodyAcOwned) {
        player->cylinder.base.acFlags |= AC_ON;
        sByrna.bodyAcOwned = false;
    }
    player->stateFlags1 &= ~PLAYER_STATE1_400000;
    if (sCollidersReady) {
        sGuard.base.acFlags &= ~AC_ON;
    }
}

static void EndMove(PlayState* play, Player* player) {
    bool isOwnAction = player->actionFunc == ByrnaAction;
    ReleaseGuard(player);
    SetImmunity(player, false);
    SilenceVanillaBlade(player);
    player->actor.gravity = sByrna.gravity;
    player->actor.terminalVelocity = sByrna.terminalVelocity;
    player->stateFlags3 &= ~PLAYER_STATE3_2;
    player->stateFlags2 &= ~PLAYER_STATE2_40;
    sByrna.active = false;
    sByrna.move = BYRNA_NONE;
    if (isOwnAction && !IsPlayerBusy(player)) {
        func_8083B1A0(player, play);
    }
}

static void BeginMove(PlayState* play, Player* player, ByrnaMove move, ClipId clip, bool loop) {
    if (!sByrna.active) {
        sByrna.gravity = player->actor.gravity;
        sByrna.terminalVelocity = player->actor.terminalVelocity;
    }
    if (sByrna.move == BYRNA_GUARD) {
        ReleaseGuard(player);
    }
    SetImmunity(player, false);
    sByrna.active = true;
    sByrna.move = move;
    sByrna.ticks = 0;
    sByrna.impulse = false;
    sByrna.landedAttack = false;
    Player_SetAction(play, player, ByrnaAction, 0);
    SilenceVanillaBlade(player);
    if (sCollidersReady) {
        sForward.base.atFlags &= ~(AT_HIT | AT_ON);
        sAirCylinder.base.atFlags &= ~(AT_HIT | AT_ON);
    }
    PlayClip(play, player, clip, loop);
}

static void HandOver(Player* player, ItemId item) {
    player->heldItemId = item;
    player->heldItemAction = item == ITEM_NONE ? PLAYER_IA_NONE : Player_ItemToItemAction(player, item);
    player->itemAction = player->heldItemAction;
    func_80123C58(player);
}

static void FaceTarget(PlayState* play, Player* player) {
    if (!IsActorAlive(play, player->focusActor)) {
        return;
    }
    s16 yaw = Actor_WorldYawTowardActor(&player->actor, player->focusActor);
    Math_SmoothStepToS(&player->actor.shape.rot.y, yaw, 1, 0x1800, 0);
    player->actor.world.rot.y = player->actor.shape.rot.y;
    player->yaw = player->actor.shape.rot.y;
}

static void SetAirborne(Player* player, f32 gravity) {
    player->actor.gravity = gravity;
    player->actor.terminalVelocity = -26.0f;
    player->stateFlags3 |= PLAYER_STATE3_2;
}

static void EnsureColliders(PlayState* play, Player* player) {
    if (sCollidersReady) {
        return;
    }
    ColliderCylinderInit attack = {
        { COL_MATERIAL_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_CYLINDER },
        { ELEM_MATERIAL_UNK2,
          { DMG_SWORD, 0, BYRNA_NORMAL_DAMAGE },
          { 0, 0, 0 },
          ATELEM_ON | ATELEM_SFX_NORMAL,
          ACELEM_NONE,
          OCELEM_NONE },
        { 42, 75, 0, { 0, 0, 0 } },
    };
    ColliderCylinderInit guard = {
        { COL_MATERIAL_METAL, AT_NONE, AC_ON | AC_HARD | AC_TYPE_ENEMY, OC1_NONE, OC2_TYPE_PLAYER,
          COLSHAPE_CYLINDER },
        { ELEM_MATERIAL_UNK2, { 0, 0, 0 }, { 0xFFFFFFFF, 0, 0 }, ATELEM_NONE, ACELEM_ON, OCELEM_NONE },
        { 60, 100, 0, { 0, 0, 0 } },
    };
    Collider_InitCylinder(play, &sForward);
    Collider_SetCylinder(play, &sForward, &player->actor, &attack);
    Collider_InitCylinder(play, &sGuard);
    Collider_SetCylinder(play, &sGuard, &player->actor, &guard);
    for (s32 i = 0; i < BYRNA_CYL_TRIS; i++) {
        ColliderTrisElementInit* element = &sAirInitElements[i];
        memset(element, 0, sizeof(*element));
        element->base.elemMaterial = ELEM_MATERIAL_UNK2;
        element->base.atDmgInfo.dmgFlags = DMG_SWORD;
        element->base.atDmgInfo.damage = BYRNA_NORMAL_DAMAGE;
        element->base.atElemFlags = ATELEM_ON | ATELEM_SFX_NORMAL;
    }
    ColliderTrisInit tris = {
        { COL_MATERIAL_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_TRIS },
        BYRNA_CYL_TRIS,
        sAirInitElements,
    };
    Collider_InitTris(play, &sAirCylinder);
    Collider_SetTris(play, &sAirCylinder, &player->actor, &tris, sAirElements);
    sCollidersReady = true;
}

// MM reads one damage-table row per hit, so the pound swaps the sword flag for the Goron pound instead of adding it.
static void ForwardHit(PlayState* play, Player* player, bool pound, bool impact) {
    if (sByrna.landedAttack) {
        return;
    }
    EnsureColliders(play, player);
    f32 reach = pound ? 0 : 55;
    sForward.dim.pos.x = player->actor.world.pos.x + Math_SinS(player->actor.shape.rot.y) * reach;
    sForward.dim.pos.z = player->actor.world.pos.z + Math_CosS(player->actor.shape.rot.y) * reach;
    sForward.dim.pos.y = player->actor.world.pos.y + (pound ? -50 : 5);
    sForward.dim.radius = impact ? 85 : (pound ? 38 : 42);
    sForward.dim.height = impact ? 60 : 75;
    sForward.elem.atDmgInfo.dmgFlags = pound ? DMG_GORON_POUND : DMG_SWORD;
    sForward.elem.atDmgInfo.damage = BYRNA_NORMAL_DAMAGE;
    sForward.base.atFlags |= AT_ON;
    CollisionCheck_SetAT(play, &play->colChkCtx, &sForward.base);
}

static Vec3f GetCylinderPoint(Player* player, f32 right, f32 angle) {
    f32 up = 35 + cosf(angle) * 38;
    f32 forward = 48 + sinf(angle) * 38;
    f32 sinYaw = Math_SinS(player->actor.shape.rot.y);
    f32 cosYaw = Math_CosS(player->actor.shape.rot.y);
    Vec3f point = { player->actor.world.pos.x + right * cosYaw + forward * sinYaw, player->actor.world.pos.y + up,
                    player->actor.world.pos.z + forward * cosYaw - right * sinYaw };
    return point;
}

static void AirCylinderHit(PlayState* play, Player* player) {
    if (sByrna.landedAttack) {
        return;
    }
    EnsureColliders(play, player);
    Vec3f centerL = GetCylinderPoint(player, -45, 0);
    Vec3f centerR = GetCylinderPoint(player, 45, 0);
    centerL.y -= 38;
    centerR.y -= 38;
    for (s32 i = 0; i < BYRNA_CYL_SEGMENTS; i++) {
        f32 a = i * (2.0f * M_PIf / BYRNA_CYL_SEGMENTS);
        f32 b = (i + 1) * (2.0f * M_PIf / BYRNA_CYL_SEGMENTS);
        Vec3f l0 = GetCylinderPoint(player, -45, a);
        Vec3f l1 = GetCylinderPoint(player, -45, b);
        Vec3f r0 = GetCylinderPoint(player, 45, a);
        Vec3f r1 = GetCylinderPoint(player, 45, b);
        Collider_SetTrisVertices(&sAirCylinder, i * 4, &l0, &r0, &r1);
        Collider_SetTrisVertices(&sAirCylinder, i * 4 + 1, &l0, &r1, &l1);
        Collider_SetTrisVertices(&sAirCylinder, i * 4 + 2, &centerL, &l1, &l0);
        Collider_SetTrisVertices(&sAirCylinder, i * 4 + 3, &centerR, &r0, &r1);
    }
    sAirCylinder.base.atFlags |= AT_ON;
    CollisionCheck_SetAT(play, &play->colChkCtx, &sAirCylinder.base);
}

static bool ProcessHit(PlayState* play, Player* player) {
    if (!sCollidersReady || sByrna.landedAttack) {
        return false;
    }
    Collider* hit = NULL;
    if (sAirCylinder.base.atFlags & AT_HIT) {
        hit = &sAirCylinder.base;
    } else if (sForward.base.atFlags & AT_HIT) {
        hit = &sForward.base;
    }
    if (hit == NULL) {
        return false;
    }
    Actor* enemy = hit->at;
    sByrna.landedAttack = true;
    sForward.base.atFlags &= ~(AT_ON | AT_HIT);
    sAirCylinder.base.atFlags &= ~(AT_ON | AT_HIT);
    if ((sByrna.move != BYRNA_AIR_A && sByrna.move != BYRNA_AIR_B) || !IsEnemy(enemy)) {
        return false;
    }
    BeginMove(play, player, BYRNA_REBOUND, CLIP_REBOUND, false);
    player->speedXZ *= 0.5f;
    player->actor.velocity.y = 0;
    SetAirborne(player, 0);
    return true;
}

static void StartAirIdle(PlayState* play, Player* player) {
    BeginMove(play, player, BYRNA_AIR_IDLE, CLIP_AIR_IDLE, true);
    SetAirborne(player, BYRNA_AIR_GRAVITY);
}

static void StartLaunch(PlayState* play, Player* player) {
    BeginMove(play, player, BYRNA_LAUNCH, CLIP_LAUNCH, false);
    sByrna.launchFloor = player->actor.world.pos.y;
    player->speedXZ = 0;
    player->actor.velocity.y = 0;
    player->actor.gravity = 0;
}

static void StartDash(PlayState* play, Player* player, bool jump) {
    BeginMove(play, player, BYRNA_DASH, CLIP_DASH, false);
    FaceTarget(play, player);
    player->speedXZ = 0;
    if (jump) {
        player->actor.velocity.y = 4.5f;
        player->actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND;
        SetAirborne(player, BYRNA_AIR_GRAVITY);
    }
}

static bool PayShield(void) {
    if (gSaveContext.save.saveInfo.playerData.magic < 2) {
        return false;
    }
    gSaveContext.save.saveInfo.playerData.magic -= 2;
    return true;
}

static void StartGuard(PlayState* play, Player* player) {
    if (!PayShield()) {
        if (!sGuardRejected) {
            Audio_PlaySfx(NA_SE_SY_ERROR);
        }
        sGuardRejected = true;
        return;
    }
    BeginMove(play, player, BYRNA_GUARD, CLIP_IDLE, true);
    sByrna.shieldTicks = 0;
    player->speedXZ = 0;
    player->stateFlags1 |= PLAYER_STATE1_400000;
}

static void GuardTick(PlayState* play, Player* player) {
    bool isHeld = CHECK_BTN_ALL(play->state.input[0].cur.button, BTN_R);
    if (!isHeld || (++sByrna.shieldTicks >= 20 && !PayShield())) {
        EndMove(play, player);
        return;
    }
    if (sByrna.shieldTicks >= 20) {
        sByrna.shieldTicks = 0;
    }
    EnsureColliders(play, player);
    player->stateFlags1 |= PLAYER_STATE1_400000;
    player->speedXZ = 0;
    if (player->cylinder.base.acFlags & AC_ON) {
        sByrna.bodyAcOwned = true;
        player->cylinder.base.acFlags &= ~AC_ON;
    }
    SetImmunity(player, true);
    sGuard.base.acFlags |= AC_ON;
    Collider_UpdateCylinder(&player->actor, &sGuard);
    CollisionCheck_SetAC(play, &play->colChkCtx, &sGuard.base);
}

static void LandPound(PlayState* play, Player* player) {
    Vec3f zero = { 0.0f, 0.0f, 0.0f };
    sByrna.move = BYRNA_LAND;
    sByrna.ticks = 0;
    player->speedXZ = 0;
    player->actor.velocity.y = 0;
    player->actor.gravity = sByrna.gravity;
    player->stateFlags3 &= ~PLAYER_STATE3_2;
    EffectSsBlast_SpawnWhiteShockwave(play, &player->actor.world.pos, &zero, &zero);
    Audio_PlaySfx(NA_SE_IT_HAMMER_HIT);
    Rumble_Request(0.0f, 200, 12, 80);
}

static void ReadAirButtons(PlayState* play, Player* player) {
    u16 press = play->state.input[0].press.button;
    if (press & BTN_R) {
        BeginMove(play, player, BYRNA_POUND, CLIP_POUND, false);
        player->speedXZ = 0;
        SetAirborne(player, BYRNA_NORMAL_GRAVITY);
        player->actor.velocity.y = MIN(player->actor.velocity.y, -3);
    } else if ((press & BTN_B) && sByrna.move != BYRNA_AIR_B) {
        BeginMove(play, player, BYRNA_AIR_B, CLIP_AIR_B, false);
        SetAirborne(player, BYRNA_NORMAL_GRAVITY);
    } else if ((press & BTN_A) && sByrna.move == BYRNA_AIR_IDLE) {
        BeginMove(play, player, BYRNA_AIR_A, CLIP_AIR_A, false);
        SetAirborne(player, BYRNA_AIR_GRAVITY);
        FaceTarget(play, player);
        player->yaw = player->actor.shape.rot.y;
        player->speedXZ = BYRNA_AIR_SPEED;
    }
}

static void UpdateGroundSwing(PlayState* play, Player* player, f32 before, f32 frame, bool done) {
    bool isDraw = sByrna.move == BYRNA_DRAW;
    Math_StepToF(&player->speedXZ, 0, 1.0f);
    if (IsInHitWindow(before, frame, isDraw ? 18 : 0, isDraw ? 52 : 40)) {
        ForwardHit(play, player, false, false);
    }
    if (done) {
        EndMove(play, player);
    }
}

static void UpdateDash(PlayState* play, Player* player, f32 before, f32 frame, bool done) {
    bool isRushing = frame >= 13 && frame < 50;
    SetImmunity(player, frame >= 13 && frame <= 50);
    if (isRushing) {
        FaceTarget(play, player);
        player->yaw = player->actor.shape.rot.y;
        f32 distance = IsActorAlive(play, player->focusActor)
                           ? Actor_WorldDistXZToActor(&player->actor, player->focusActor)
                           : 9999;
        player->speedXZ =
            distance < 45 ? 0 : MIN(18.0f, (distance - 40) / MAX(R_UPDATE_RATE * 0.5f, 0.1f)) / 3.0f;
    } else {
        player->speedXZ = 0;
    }
    if (IsInHitWindow(before, frame, 50, GetLastOriginalFrame(CLIP_DASH))) {
        ForwardHit(play, player, false, false);
    }
    if (!done) {
        return;
    }
    if (IsGrounded(player)) {
        EndMove(play, player);
    } else {
        StartAirIdle(play, player);
    }
}

static void UpdateLaunch(PlayState* play, Player* player, f32 before, f32 frame, bool done) {
    if (sByrna.impulse && !IsGrounded(player)) {
        ReadAirButtons(play, player);
        if (sByrna.move != BYRNA_LAUNCH) {
            return;
        }
    }
    s32 sample = MIN((s32)(frame * BYRNA_SAMPLES), ARRAY_COUNT(sVaultFloorLift[0]) - 1);
    f32 minimumY = sByrna.launchFloor + sVaultFloorLift[GetVaultFloorRow()][sample];
    if (player->actor.world.pos.y < minimumY) {
        player->actor.world.pos.y = minimumY;
        player->actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND;
    }
    if (HasCrossedFrame(before, frame, 16)) {
        player->actor.velocity.y = 3;
        player->actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND;
        SetAirborne(player, 0);
        sByrna.impulse = true;
    } else if (sByrna.impulse) {
        player->actor.velocity.y = 0;
    }
    if (done) {
        StartAirIdle(play, player);
        player->actor.velocity.y = 11;
        player->actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND;
    }
}

static void UpdateAirIdle(PlayState* play, Player* player) {
    Input* input = &play->state.input[0];
    f32 magnitude = sqrtf(SQ(input->rel.stick_x) + SQ(input->rel.stick_y));
    SetAirborne(player, BYRNA_AIR_GRAVITY);
    if (magnitude > 10) {
        player->yaw =
            Camera_GetCamDirYaw(GET_ACTIVE_CAM(play)) + Math_Atan2S_XY(input->rel.stick_y, -input->rel.stick_x);
        player->speedXZ = MIN(magnitude / 60, 1.0f) * 8;
    } else {
        Math_StepToF(&player->speedXZ, 0, 0.5f);
    }
    ReadAirButtons(play, player);
}

static void UpdateAirStrike(PlayState* play, Player* player, bool done) {
    ByrnaMove move = sByrna.move;
    ReadAirButtons(play, player);
    if (sByrna.move != move) {
        return;
    }
    if (move == BYRNA_AIR_B) {
        SetAirborne(player, BYRNA_NORMAL_GRAVITY);
        AirCylinderHit(play, player);
    } else {
        bool isAtTarget = IsActorAlive(play, player->focusActor) &&
                          Actor_WorldDistXZToActor(&player->actor, player->focusActor) < 40;
        SetAirborne(player, BYRNA_AIR_GRAVITY);
        FaceTarget(play, player);
        player->yaw = player->actor.shape.rot.y;
        player->speedXZ = isAtTarget ? 0 : BYRNA_AIR_SPEED;
        ForwardHit(play, player, false, false);
    }
    if (done) {
        StartAirIdle(play, player);
    }
}

static void UpdateRebound(PlayState* play, Player* player, f32 frame, bool done) {
    ReadAirButtons(play, player);
    if (sByrna.move != BYRNA_REBOUND) {
        return;
    }
    if (!sByrna.impulse && frame >= GetLastOriginalFrame(CLIP_REBOUND) * 0.5f) {
        sByrna.impulse = true;
        player->actor.velocity.y = BYRNA_REBOUND_IMPULSE;
        player->actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND;
    }
    SetAirborne(player, sByrna.impulse ? BYRNA_AIR_GRAVITY : 0);
    if (!sByrna.impulse) {
        player->actor.velocity.y = 0;
    }
    if (done) {
        StartAirIdle(play, player);
    }
}

static bool IsAirMove(ByrnaMove move) {
    return move >= BYRNA_AIR_IDLE && move <= BYRNA_POUND;
}

static void ByrnaAction(Player* player, PlayState* play) {
    if (!IsWielding(player) || IsPlayerBusy(player)) {
        EndMove(play, player);
        return;
    }
    SilenceVanillaBlade(player);
    if (ProcessHit(play, player)) {
        ReadAirButtons(play, player);
        return;
    }
    PlayerAnimationHeader* clip = LoadClip(sByrna.clip);
    if (clip != NULL && player->skelAnime.animation != (void*)clip) {
        bool loop = sByrna.move == BYRNA_AIR_IDLE || sByrna.move == BYRNA_GUARD;
        PlayerAnimation_Change(play, &player->skelAnime, clip, BYRNA_PLAY_SPEED * BYRNA_SAMPLES,
                               sByrna.frame * BYRNA_SAMPLES, Animation_GetLastFrame(clip),
                               loop ? ANIMMODE_LOOP : ANIMMODE_ONCE, 0);
    }
    f32 before = sByrna.frame;
    bool done = PlayerAnimation_Update(play, &player->skelAnime);
    f32 frame = GetOriginalFrame(player);
    sByrna.frame = frame;
    sByrna.ticks++;
    if (sByrna.move == BYRNA_GUARD) {
        GuardTick(play, player);
        return;
    }
    if (IsAirMove(sByrna.move)) {
        player->stateFlags3 |= PLAYER_STATE3_2;
        if (IsGrounded(player) && player->actor.velocity.y <= 0 && sByrna.move != BYRNA_REBOUND) {
            if (sByrna.move == BYRNA_POUND) {
                LandPound(play, player);
            } else {
                EndMove(play, player);
            }
            return;
        }
    }
    switch (sByrna.move) {
        case BYRNA_DRAW:
        case BYRNA_SLASH:
            UpdateGroundSwing(play, player, before, frame, done);
            break;
        case BYRNA_SHEATHE:
            player->speedXZ = 0;
            if (done) {
                HandOver(player, ITEM_NONE);
                EndMove(play, player);
            }
            break;
        case BYRNA_DASH:
            UpdateDash(play, player, before, frame, done);
            break;
        case BYRNA_LAUNCH:
            UpdateLaunch(play, player, before, frame, done);
            break;
        case BYRNA_AIR_IDLE:
            UpdateAirIdle(play, player);
            break;
        case BYRNA_AIR_B:
        case BYRNA_AIR_A:
            UpdateAirStrike(play, player, done);
            break;
        case BYRNA_REBOUND:
            UpdateRebound(play, player, frame, done);
            break;
        case BYRNA_POUND:
            SetAirborne(player, BYRNA_NORMAL_GRAVITY);
            ForwardHit(play, player, true, false);
            if (done) {
                player->skelAnime.curFrame = player->skelAnime.endFrame;
                player->skelAnime.playSpeed = 0;
            }
            break;
        case BYRNA_LAND:
            ForwardHit(play, player, true, true);
            if (sByrna.ticks >= 3) {
                EndMove(play, player);
            }
            break;
        default:
            EndMove(play, player);
            break;
    }
}

static void InterceptDraw(bool* should, va_list args) {
    s32 item = va_arg(args, s32);
    PlayState* play = gPlayState;
    if (play == NULL || !*should || !IsWorn() || item < ITEM_SWORD_KOKIRI || item > ITEM_SWORD_GILDED) {
        return;
    }
    Player* player = GET_PLAYER(play);
    if (player->transformation != PLAYER_FORM_HUMAN || IsWielding(player) || sByrna.active ||
        IsPlayerBusy(player) || !IsGrounded(player)) {
        return;
    }
    *should = false;
    HandOver(player, item);
    BeginMove(play, player, BYRNA_DRAW, CLIP_DRAW, false);
}

static bool CanStartGroundMove(PlayState* play, Player* player, bool consumed, u16 button, bool held) {
    Input* input = &play->state.input[0];
    u16 buttons = held ? input->cur.button : input->press.button;
    return !consumed && IsWielding(player) && IsGrounded(player) && !IsPlayerBusy(player) && !sByrna.active &&
           (buttons & button);
}

static void TakeMeleeButton(PlayState* play, Player* player, s32 action, bool* consumed, bool* started) {
    if (!CanStartGroundMove(play, player, *consumed, BTN_B, false)) {
        return;
    }
    bool isDash = fabsf(player->speedXZ) > 3.0f &&
                  player->controlStickDirections[player->controlStickDataIndex] == PLAYER_STICK_DIR_FORWARD;
    if (isDash) {
        StartDash(play, player, false);
    } else {
        BeginMove(play, player, BYRNA_SLASH, CLIP_SLASH, false);
    }
    *consumed = true;
    *started = true;
}

static void TakeAButton(PlayState* play, Player* player, s32 action, bool* consumed, bool* started) {
    if (!CanStartGroundMove(play, player, *consumed, BTN_A, false)) {
        return;
    }
    Input* input = &play->state.input[0];
    bool isTargeting = Player_IsZTargeting(player);
    s32 direction = player->controlStickDirections[player->controlStickDataIndex];
    if ((action == SOH_PLAYER_ACTION_ROLL && isTargeting) || (action == SOH_PLAYER_ACTION_ZTARGET_A && !isTargeting)) {
        return;
    }
    if (isTargeting && direction > PLAYER_STICK_DIR_FORWARD) {
        return;
    }
    if (isTargeting && direction == PLAYER_STICK_DIR_FORWARD && IsEnemy(player->focusActor)) {
        StartDash(play, player, true);
    } else if (!isTargeting && ABS(input->rel.stick_x) + ABS(input->rel.stick_y) < 20) {
        if (player->putAwayCooldownTimer != 0) {
            return;
        }
        BeginMove(play, player, BYRNA_SHEATHE, CLIP_SHEATHE, false);
    } else {
        StartLaunch(play, player);
    }
    *consumed = true;
    *started = true;
}

static void TakeShieldButton(PlayState* play, Player* player, s32 action, bool* consumed, bool* started) {
    if (!CanStartGroundMove(play, player, *consumed, BTN_R, true)) {
        return;
    }
    StartGuard(play, player);
    *consumed = true;
    *started = sByrna.active;
}

static void KeepButtonsFromVanilla(Player* player, Input* input) {
    const u16 byrnaButtons = BTN_A | BTN_B | BTN_R;
    if (!(input->cur.button & BTN_R)) {
        sGuardRejected = false;
    }
    if (gPlayState == NULL || !IsWielding(player) || IsPlayerBusy(player)) {
        return;
    }
    if (sByrna.active) {
        input->cur.button &= ~byrnaButtons;
        input->press.button &= ~byrnaButtons;
        return;
    }
    if (!IsGrounded(player) && !(player->stateFlags2 & PLAYER_STATE2_80000) && (input->press.button & byrnaButtons)) {
        StartAirIdle(gPlayState, player);
        ReadAirButtons(gPlayState, player);
        input->press.button &= ~byrnaButtons;
    }
}

static bool IsStandingGroup(s32 group) {
    return group == PLAYER_ANIMGROUP_wait || group == PLAYER_ANIMGROUP_waitL || group == PLAYER_ANIMGROUP_waitR ||
           group == PLAYER_ANIMGROUP_normal2fighter;
}

static bool IsWalkingGroup(s32 group) {
    return group == PLAYER_ANIMGROUP_walk || group == PLAYER_ANIMGROUP_run || group == PLAYER_ANIMGROUP_side_walk ||
           group == PLAYER_ANIMGROUP_side_walkL || group == PLAYER_ANIMGROUP_side_walkR;
}

static void ResolveStaffAnimation(s32 group, s32 animType, PlayerAnimationHeader** anim) {
    if (gPlayState == NULL) {
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    if (!IsWielding(player)) {
        return;
    }
    if (IsStandingGroup(group) && Player_IsZTargeting(player)) {
        *anim = LoadClip(CLIP_IDLE);
    } else if (IsWalkingGroup(group)) {
        *anim = LoadClip(group == PLAYER_ANIMGROUP_run ? CLIP_RUN : CLIP_WALK);
    }
}

static void ResolveStaffHop(s32 site, s32 index, PlayerAnimationHeader** anim) {
    if (gPlayState == NULL || site != SOH_PLAYER_ANIM_SITE_HOP || index % 3 != 0 ||
        !IsWielding(GET_PLAYER(gPlayState))) {
        return;
    }
    switch (index / 3) {
        case PLAYER_STICK_DIR_LEFT:
            *anim = LoadClip(CLIP_HOP_RIGHT);
            break;
        case PLAYER_STICK_DIR_BACKWARD:
            *anim = LoadClip(CLIP_BACKFLIP);
            break;
        case PLAYER_STICK_DIR_RIGHT:
            *anim = LoadClip(CLIP_HOP_LEFT);
            break;
        default:
            break;
    }
}

static void TickByrna(Player* player) {
    PlayState* play = gPlayState;
    if (play == NULL || player != GET_PLAYER(play)) {
        return;
    }
    if (sByrna.active && (!IsWielding(player) || IsPlayerBusy(player))) {
        EndMove(play, player);
        return;
    }
    if (sByrna.active && player->actionFunc != ByrnaAction) {
        if (sByrna.move >= BYRNA_LAUNCH && sByrna.move <= BYRNA_POUND) {
            Player_SetAction(play, player, ByrnaAction, 0);
            player->stateFlags3 |= PLAYER_STATE3_2;
        } else {
            EndMove(play, player);
        }
    }
    if (sByrna.active || !IsWielding(player) || IsPlayerBusy(player)) {
        return;
    }
    // Clips played by vanilla (idle, hops) need the resampled speed; walk and run sample fixed phases instead.
    for (s32 i = 0; i < CLIP_WALK; i++) {
        if (sClips[i].values != NULL && player->skelAnime.animation == (void*)&sClips[i].header) {
            player->skelAnime.playSpeed = BYRNA_PLAY_SPEED * BYRNA_SAMPLES;
        }
    }
}
