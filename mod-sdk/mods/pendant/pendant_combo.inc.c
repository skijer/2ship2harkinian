static const ALIGN_ASSET(2) char sHopLeft[] = "__OTR__misc/link_animetion/nei_pendant_hop_left";
static const ALIGN_ASSET(2) char sHopRight[] = "__OTR__misc/link_animetion/nei_pendant_hop_right";
static Actor* sBoundsActor;
static f32 sBoundsTop;
static f32 sArcHeight;
static f32 sArcImpact;
static bool sIsRolling;
static PlayerAnimationHeader* sSecondHopClip;

static bool IsParryTarget(Actor* target) {
    return target != NULL && target->update != NULL && target->category == ACTORCAT_ENEMY &&
           (target->flags & ACTOR_FLAG_ATTENTION_ENABLED) != 0;
}

static f32 FindColliderTop(Collider* collider) {
    f32 top = -1.0e20f;

    switch (collider->shape) {
        case COLSHAPE_CYLINDER: {
            Cylinder16* dim = &((ColliderCylinder*)collider)->dim;

            return dim->pos.y + dim->yShift + dim->height;
        }
        case COLSHAPE_JNTSPH: {
            ColliderJntSph* spheres = (ColliderJntSph*)collider;

            for (s32 i = 0; i < spheres->count; i++) {
                Sphere16* sphere = &spheres->elements[i].dim.worldSphere;

                top = MAX(top, sphere->center.y + sphere->radius);
            }
            return top;
        }
        case COLSHAPE_QUAD:
            for (s32 i = 0; i < 4; i++) {
                top = MAX(top, ((ColliderQuad*)collider)->dim.quad[i].y);
            }
            return top;
        case COLSHAPE_TRIS: {
            ColliderTris* tris = (ColliderTris*)collider;

            for (s32 i = 0; i < tris->count; i++) {
                for (s32 corner = 0; corner < 3; corner++) {
                    top = MAX(top, tris->elements[i].dim.vtx[corner].y);
                }
            }
            return top;
        }
        default:
            return top;
    }
}

// The leap clears the target's real hurtbox, read from the AC list after the actor placed it this frame.
static void SampleTargetBounds(Actor* actor) {
    PlayState* play = gPlayState;

    if (play == NULL || !IsActive() || !IsParryTarget(actor)) {
        return;
    }
    Player* player = GET_PLAYER(play);
    if (actor != player->focusActor && actor != sPendant.target) {
        return;
    }
    f32 top = -1.0e20f;
    for (s32 i = 0; i < play->colChkCtx.colACCount; i++) {
        Collider* collider = play->colChkCtx.colAC[i];

        if (collider != NULL && collider->actor == actor && (collider->acFlags & AC_ON)) {
            top = MAX(top, FindColliderTop(collider));
        }
    }
    if (top > -1.0e19f) {
        sBoundsActor = actor;
        sBoundsTop = top;
    }
}

static void SilenceSwordQuads(Player* player) {
    player->meleeWeaponQuads[0].base.atFlags &= ~AT_ON;
    player->meleeWeaponQuads[1].base.atFlags &= ~AT_ON;
    player->meleeWeaponState = PLAYER_MELEE_WEAPON_STATE_0;
}

static bool MoveAlongArc(PlayState* play, Player* player, Vec3f* next) {
    Vec3f from = player->actor.world.pos;
    Vec3f to = *next;
    Vec3f hit;
    CollisionPoly* poly = NULL;
    s32 bgId;

    from.y += 20.0f;
    to.y += 20.0f;
    if (BgCheck_EntityLineTest1(&play->colCtx, &from, &to, &hit, &poly, true, true, true, true, &bgId)) {
        return false;
    }
    player->actor.world.pos = *next;
    HoldStill(player);
    player->actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND;
    player->stateFlags3 |= PLAYER_STATE3_MIDAIR;
    return true;
}

static f32 FindLandingFloor(PlayState* play, Player* player, Vec3f* pos) {
    Vec3f ray = *pos;
    CollisionPoly* poly = NULL;
    s32 bgId;

    ray.y += 150.0f;
    f32 floor = BgCheck_EntityRaycastFloor5(&play->colCtx, &poly, &bgId, &player->actor, &ray);
    return floor > BGCHECK_Y_MIN && fabsf(floor - pos->y) < 150.0f ? floor : pos->y;
}

static void DisarmCombo(void) {
    sPendant.hops = 0;
    sPendant.isLeapArmed = false;
    sComboLandingPending = false;
}

// Site index is stickDirection * 3; directions 1 and 3 are the side hops.
static void ResolveComboHop(s32 site, s32 index, PlayerAnimationHeader** anim) {
    PlayState* play = gPlayState;

    if (play == NULL || site != SOH_PLAYER_ANIM_SITE_HOP || index % 3 != 0 || !IsActive()) {
        return;
    }
    Player* player = GET_PLAYER(play);
    s32 direction = index / 3;
    if (sPendant.state != PENDANT_IDLE || IsPlayerBusy(play, player)) {
        return;
    }
    if ((direction != 1 && direction != 3) || !Player_IsZTargeting(player) || !IsSwordOut(player)) {
        DisarmCombo();
        return;
    }
    bool isFreshCombo = sPendant.hops >= 3 || sPendant.hopTimer > PARRY_HOP_WINDOW;
    sPendant.hops = isFreshCombo ? 1 : sPendant.hops + 1;
    sPendant.hopTimer = 0;
    sPendant.isLeapArmed = false;
    sComboLandingPending = true;
    sComboWasAirborne = false;
    if (sPendant.hops != 2) {
        return;
    }
    PlayerAnimationHeader* clip = ResourceMgr_LoadPlayerAnimAsHeader(direction == 3 ? sHopLeft : sHopRight);
    if (clip == NULL) {
        return;
    }
    *anim = clip;
    sSecondHopClip = clip;
    sSecondHopPending = true;
}

static f32 GetMotionStep(void) {
    return MAX(R_UPDATE_RATE * 0.5f, 0.1f);
}

static f32 SideHopAirTicks(Player* player) {
    f32 velocity = player->actor.velocity.y;
    f32 height = MAX(player->actor.world.pos.y - player->actor.floorHeight, 0.0f);
    f32 step = GetMotionStep();
    s32 ticks;

    for (ticks = 1; ticks < 60; ticks++) {
        velocity += player->actor.gravity;
        height += velocity * step;
        if (height <= 0.0f && velocity <= 0.0f) {
            break;
        }
    }
    return (f32)ticks;
}

// The roll clip is stretched over the hop's real airtime so it lands on its last frame.
static void ConfigureSecondHopAnimation(Player* player) {
    sSecondHopPending = false;
    if (sSecondHopClip == NULL || player->skelAnime.animation != (void*)sSecondHopClip) {
        return;
    }
    f32 lastFrame = Animation_GetLastFrame(sSecondHopClip);
    player->skelAnime.playSpeed = lastFrame / MAX(SideHopAirTicks(player) * GetMotionStep(), 1.0f);
}

static void ExtendComboHopTravel(Actor* actor, f32* scale) {
    if (gPlayState == NULL || actor != &GET_PLAYER(gPlayState)->actor || !IsActive()) {
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    if (sPendant.state != PENDANT_IDLE || IsPlayerBusy(gPlayState, player) || !sComboLandingPending ||
        !(player->stateFlags2 & PLAYER_STATE2_HOPPING) || (IsGrounded(player) && actor->velocity.y <= 0.0f)) {
        return;
    }
    f32 multiplier = sPendant.hops == 2 ? 2.0f : sPendant.hops == 3 ? 1.5f : 1.0f;
    actor->velocity.x *= multiplier;
    actor->velocity.z *= multiplier;
}

static void ArmLeapOnThirdLanding(Player* player) {
    if (!IsGrounded(player)) {
        sComboWasAirborne = true;
        return;
    }
    if (!sComboWasAirborne || player->actor.velocity.y > 0.0f) {
        return;
    }
    sComboLandingPending = false;
    if (sPendant.hops == 3 && IsParryTarget(player->focusActor)) {
        sPendant.isLeapArmed = true;
        sPendant.hopTimer = PARRY_TIMING_FRAMES;
        Audio_PlaySfx(NA_SE_VO_NAVY_ENEMY);
    }
}

static void TrackSideHops(Player* player) {
    if (!Player_IsZTargeting(player)) {
        DisarmCombo();
        return;
    }
    if (sPendant.isLeapArmed) {
        if (--sPendant.hopTimer <= 0) {
            sPendant.isLeapArmed = false;
            sPendant.hops = 0;
        }
        return;
    }
    if (++sPendant.hopTimer > PARRY_HOP_WINDOW) {
        sPendant.hops = 0;
        sComboLandingPending = false;
        return;
    }
    if (sComboLandingPending) {
        ArmLeapOnThirdLanding(player);
    }
}

static void PlayRollLoop(Player* player, PlayState* play) {
    ChangeAnim(play, player, sRollAnim, 3.0f, 0.0f, GetLastFrame(sRollAnim), ANIMMODE_LOOP, -3.0f);
}

static f32 FindTargetTop(Actor* target) {
    if (sBoundsActor == target) {
        return sBoundsTop;
    }
    return MAX(target->focus.pos.y, target->world.pos.y + target->colChkInfo.cylHeight);
}

// The arc peaks over the hurtbox at the point the leap crosses the target.
static void StartParryLeap(Player* player, PlayState* play) {
    Actor* target = player->focusActor;
    f32 dx = target->world.pos.x - player->actor.world.pos.x;
    f32 dz = target->world.pos.z - player->actor.world.pos.z;
    f32 dist = MAX(sqrtf(SQ(dx) + SQ(dz)), 1.0f);
    s16 yaw = Actor_WorldYawTowardActor(&player->actor, target);

    sPendant.state = PENDANT_PARRY_ARC;
    sPendant.timer = 0;
    sPendant.target = target;
    sPendant.hasParryHit = false;
    sIsRolling = false;
    sPendant.isLeapArmed = false;
    sPendant.hops = 0;
    sPendant.arcStart = player->actor.world.pos;
    sPendant.arcEnd = target->world.pos;
    sPendant.arcEnd.x += dx / dist * PARRY_LAND_DIST;
    sPendant.arcEnd.z += dz / dist * PARRY_LAND_DIST;
    sPendant.arcEnd.y = FindLandingFloor(play, player, &sPendant.arcEnd);
    sArcImpact = CLAMP(dist / (dist + PARRY_LAND_DIST), 0.1f, 0.9f);
    f32 base = sPendant.arcStart.y + (sPendant.arcEnd.y - sPendant.arcStart.y) * sArcImpact;
    sArcHeight = MAX(35.0f, (FindTargetTop(target) - base) / (4.0f * sArcImpact * (1.0f - sArcImpact)));
    sSavedGravity = player->actor.gravity;
    HoldStill(player);
    player->actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND;
    player->stateFlags3 |= PLAYER_STATE3_MIDAIR;
    player->actor.shape.rot.y = yaw;
    player->yaw = yaw;
    ChangeAnim(play, player, sRunJumpAnim, 1.0f, 0.0f, GetLastFrame(sRunJumpAnim), ANIMMODE_ONCE, -3.0f);
    EnsureParryCollider(play, player);
    sParryCollider.base.atFlags &= ~AT_HIT;
    SilenceSwordQuads(player);
    PlaySfx(player, NA_SE_VO_LI_SWORD_N);
}

static void StrikeParryTarget(Player* player, PlayState* play) {
    SilenceSwordQuads(player);
    if (sPendant.hasParryHit) {
        return;
    }
    if (sParryCollider.base.atFlags & AT_HIT) {
        sPendant.hasParryHit = true;
        sParryCollider.base.atFlags &= ~(AT_HIT | AT_ON);
        PlaySfx(player, NA_SE_IT_SWORD_STRIKE);
        return;
    }
    Sphere16* sphere = &sParryElement.dim.worldSphere;
    sphere->center.x = player->actor.world.pos.x;
    sphere->center.y = player->actor.world.pos.y + 18.0f;
    sphere->center.z = player->actor.world.pos.z;
    sphere->radius = 36;
    sParryCollider.base.atFlags |= AT_ON;
    CollisionCheck_SetAT(play, &play->colChkCtx, &sParryCollider.base);
}

static void EndParryArc(Player* player, PlayState* play) {
    sPendant.state = PENDANT_PARRY_LANDING;
    sPendant.timer = 0;
    player->actor.gravity = sSavedGravity;
    sParryCollider.base.atFlags &= ~AT_ON;
    player->stateFlags3 &= ~PLAYER_STATE3_MIDAIR;
    player->cylinder.base.ocFlags1 |= OC1_ON;
    ChangeAnim(play, player, sLandingAnim, 1.5f, 0.0f, GetLastFrame(sLandingAnim), ANIMMODE_ONCE, -3.0f);
}

static void UpdateParryArc(Player* player, PlayState* play) {
    if (!IsActorAlive(play, sPendant.target) || !IsParryTarget(sPendant.target)) {
        EndParryArc(player, play);
        return;
    }
    f32 t = MIN((f32)++sPendant.timer / PARRY_ARC_FRAMES, 1.0f);
    Vec3f next;
    next.x = sPendant.arcStart.x + (sPendant.arcEnd.x - sPendant.arcStart.x) * t;
    next.z = sPendant.arcStart.z + (sPendant.arcEnd.z - sPendant.arcStart.z) * t;
    next.y = sPendant.arcStart.y + (sPendant.arcEnd.y - sPendant.arcStart.y) * t + sArcHeight * 4.0f * t * (1.0f - t);
    if (!MoveAlongArc(play, player, &next)) {
        EndParryArc(player, play);
        return;
    }
    if (!sIsRolling && t >= sArcImpact - 0.1f) {
        PlayRollLoop(player, play);
        sIsRolling = true;
    }
    PlayerAnimation_Update(play, &player->skelAnime);
    if (sIsRolling) {
        StrikeParryTarget(player, play);
    }
    SilenceSwordQuads(player);
    if (t >= 1.0f) {
        EndParryArc(player, play);
    }
}

static void UpdateParryLanding(Player* player) {
    if (!IsGrounded(player) && ++sPendant.timer < GPOUND_TIMEOUT) {
        return;
    }
    player->stateFlags3 &= ~PLAYER_STATE3_MIDAIR;
    player->cylinder.base.ocFlags1 |= OC1_ON;
    ReturnToIdle();
}

static bool CanStartParryLeap(Player* player, Input* input) {
    return sPendant.isLeapArmed && sPendant.hopTimer > 0 && sPendant.state == PENDANT_IDLE &&
           Player_IsZTargeting(player) && IsGrounded(player) && IsParryTarget(player->focusActor) &&
           IsSwordOut(player) && (input->press.button & BTN_B);
}

// Swallows the B that starts the leap, and A/B during it, so vanilla never reads them as a slash or roll.
static void ReadParryInput(Player* player, Input* input) {
    if (gPlayState == NULL || player != GET_PLAYER(gPlayState) || !IsActive() || IsPlayerBusy(gPlayState, player)) {
        return;
    }
    if (CanStartParryLeap(player, input)) {
        StartParryLeap(player, gPlayState);
        input->press.button &= ~BTN_B;
        input->cur.button &= ~BTN_B;
    } else if (sPendant.state == PENDANT_PARRY_ARC) {
        input->press.button &= ~(BTN_A | BTN_B);
        input->cur.button &= ~(BTN_A | BTN_B);
    }
}

static bool IsParryCueShown(Player* player) {
    return IsActive() && sPendant.isLeapArmed && sPendant.hopTimer > 0 && IsSwordOut(player);
}

static bool IsParryCueVisible(Player* player) {
    RemoteVisualState visual;

    if (player->stateFlags2 & PLAYER_STATE2_20000000) {
        return false;
    }
    if (player == GET_PLAYER(gPlayState)) {
        return IsParryCueShown(player);
    }
    return ReadRemoteVisual(player, &visual);
}

static void DrawParryCue(Player* player, s32 limbIndex) {
    PlayState* play = gPlayState;

    if (play == NULL || limbIndex != PLAYER_LIMB_LEFT_HAND || !IsParryCueVisible(player)) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    Matrix_Push();
    Matrix_Translate(0.0f, 220.0f, 0.0f, MTXMODE_APPLY);
    Matrix_Scale(-0.7f, -0.6f, -0.4f, MTXMODE_APPLY);
    Matrix_RotateXF(M_PIf / 2.0f, MTXMODE_APPLY);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 230, 255, 180, 120);
    gDPSetEnvColor(POLY_XLU_DISP++, 70, 180, 100, 80);
    gSPSegment(POLY_XLU_DISP++, 0x09,
               (uintptr_t)Gfx_TwoTexScroll(play->state.gfxCtx, 0, play->gameplayFrames * 5, 0, 32, 32, 1, 0,
                                           play->gameplayFrames * 10, 8, 8));
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gSpinAttackChargingDL);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}
