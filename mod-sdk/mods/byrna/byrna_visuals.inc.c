static SkelAnime sFairies[2];
static Vec3s sFairyJoints[2][FAIRY_LIMB_MAX];
static Vec3s sFairyMorph[2][FAIRY_LIMB_MAX];
static bool sFairiesReady;
static bool sTrailDrawing;
static u32 sFairyFrame;

typedef struct {
    Actor* actor;
    Collider* collider;
} ProjectileCollider;

static ProjectileCollider sProjectiles[64];

static void EnsureTrail(PlayState* play) {
    if (sTrail >= 0) {
        return;
    }
    EffectBlureInit2 init = {
        0, 4, 0, { 210, 220, 255, 200 }, { 130, 150, 255, 150 }, { 210, 220, 255, 0 }, { 130, 150, 255, 0 }, 6,
        0, 2, 0, { 255, 255, 255, 255 }, { 255, 255, 255, 255 },
    };
    Effect_Add(play, &sTrail, EFFECT_BLURE2, 0, 0, &init);
}

static void TraceCaneTrail(PlayState* play) {
    if (!sByrna.active || sByrna.move != BYRNA_AIR_B) {
        if (sTrailDrawing && sTrail >= 0) {
            EffectBlure* trail = Effect_GetByIndex(sTrail);
            if (trail != NULL) {
                EffectBlure_AddSpace(trail);
            }
        }
        sTrailDrawing = false;
        return;
    }
    EnsureTrail(play);
    EffectBlure* trail = sTrail < 0 ? NULL : Effect_GetByIndex(sTrail);
    if (trail == NULL) {
        return;
    }
    if (!sTrailDrawing) {
        EffectBlure_AddSpace(trail);
    }
    sTrailDrawing = true;
    Vec3f tip = { 0, CANE_AXIS_MAX * CANE_SCALE_Y, 0 };
    Vec3f base = { 0, CANE_AXIS_MIN * CANE_SCALE_Y, 0 };
    Matrix_Push();
    ApplyCaneMatrix();
    Matrix_MultVec3f(&tip, &tip);
    Matrix_MultVec3f(&base, &base);
    Matrix_Pop();
    EffectBlure_AddVertex(trail, &tip, &base);
}

static void DrawCane(Player* player, s32 limbIndex) {
    PlayState* play = gPlayState;
    if (play == NULL || limbIndex != PLAYER_LIMB_LEFT_HAND) {
        return;
    }
    RemoteVisualState visual;
    bool isLocal = player == GET_PLAYER(play);
    bool isVisible = isLocal ? IsWielding(player) : ReadRemoteVisual(player, &visual);
    if (!isVisible) {
        return;
    }
    if (isLocal) {
        TraceCaneTrail(play);
    }
    uint32_t savedRemoteEnv = CaptureRemoteLimbEnv(play);
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    Matrix_Push();
    ApplyCaneMatrix();
    Matrix_Scale(CANE_SCALE_X, CANE_SCALE_Y, CANE_SCALE_Z, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sCaneDL);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
    if (!isLocal) {
        RestoreRemoteLimbEnv(play, savedRemoteEnv);
    }
}

static s32 DrawFairyLimb(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, Actor* actor,
                         Gfx** gfx) {
    if (limbIndex == FAIRY_LIMB_4) {
        *dList = NULL;
    }
    if (limbIndex == FAIRY_LIMB_6) {
        Vec3f zero = { 0.0f, 0.0f, 0.0f };
        Vec3f world;
        Matrix_MultVec3f(&zero, &world);
        Matrix_Translate(world.x, world.y, world.z, MTXMODE_NEW);
        Matrix_Scale(0.012f, 0.012f, 0.012f, MTXMODE_APPLY);
    }
    return false;
}

static void DrawDashFairies(PlayState* play, Player* player) {
    if (!sFairiesReady) {
        for (s32 i = 0; i < 2; i++) {
            SkelAnime_Init(play, &sFairies[i], (SkeletonHeader*)gameplay_keep_Skel_02AF58,
                           (AnimationHeader*)gameplay_keep_Anim_029140, sFairyJoints[i], sFairyMorph[i],
                           FAIRY_LIMB_MAX);
        }
        sFairiesReady = true;
    }
    if (sFairyFrame != play->gameplayFrames) {
        for (s32 i = 0; i < 2; i++) {
            SkelAnime_Update(&sFairies[i]);
        }
        sFairyFrame = play->gameplayFrames;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL27_Xlu(play->state.gfxCtx);
    Gfx* color = GRAPH_ALLOC(play->state.gfxCtx, 4 * sizeof(Gfx));
    Gfx* cursor = color;
    gDPPipeSync(cursor++);
    gDPSetPrimColor(cursor++, 0, 0x01, 190, 190, 190, 230);
    gDPSetRenderMode(cursor++, G_RM_PASS, G_RM_ZB_CLD_SURF2);
    gSPEndDisplayList(cursor++);
    gSPSegment(POLY_XLU_DISP++, 0x08, (uintptr_t)color);
    gDPSetEnvColor(POLY_XLU_DISP++, 90, 90, 90, 180);
    for (s32 i = 0; i < 2; i++) {
        f32 angle = play->gameplayFrames * 0.35f + i * M_PIf;
        Matrix_Push();
        Matrix_Translate(player->actor.world.pos.x + sinf(angle) * 45,
                         player->actor.world.pos.y + 45 + cosf(angle * 2) * 12,
                         player->actor.world.pos.z + cosf(angle) * 45, MTXMODE_NEW);
        Matrix_Scale(0.008f, 0.008f, 0.008f, MTXMODE_APPLY);
        POLY_XLU_DISP = SkelAnime_Draw(play, sFairies[i].skeleton, sFairies[i].jointTable, DrawFairyLimb, NULL,
                                       &player->actor, POLY_XLU_DISP);
        Matrix_Pop();
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawGuardCircle(PlayState* play, Player* player) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gGreatFairyParticleSetupDL);
    gDPPipeSync(POLY_XLU_DISP++);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 170, 225, 255, 180);
    gDPSetEnvColor(POLY_XLU_DISP++, 70, 100, 230, 100);
    for (s32 i = 0; i < 24; i++) {
        f32 angle = (i % 12) * (2 * M_PIf / 12) + play->gameplayFrames * 0.05f;
        Matrix_Push();
        Matrix_Translate(player->actor.world.pos.x + sinf(angle) * 60, player->actor.world.pos.y + (i < 12 ? 12 : 85),
                         player->actor.world.pos.z + cosf(angle) * 60, MTXMODE_NEW);
        Matrix_ReplaceRotation(&play->billboardMtxF);
        Matrix_Scale(0.035f, 0.035f, 0.035f, MTXMODE_APPLY);
        MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gGreatFairyParticleDL);
        Matrix_Pop();
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawByrnaEffects(Actor* actor, PlayState* play) {
    if (actor != &GET_PLAYER(play)->actor || !IsWorn() || !sByrna.active) {
        return;
    }
    Player* player = (Player*)actor;
    if (sByrna.move == BYRNA_DASH && sByrna.frame >= 13 && sByrna.frame <= 50) {
        DrawDashFairies(play, player);
    }
    if (sByrna.move == BYRNA_GUARD) {
        DrawGuardCircle(play, player);
    }
}

static bool IsProjectile(Actor* actor) {
    if (actor->category == ACTORCAT_MISC || actor->category == ACTORCAT_ITEMACTION ||
        actor->id == ACTOR_EN_NUTSBALL) {
        return true;
    }
    return actor->id == ACTOR_EN_OKUTA && (EN_OKUTA_GET_TYPE(actor) == EN_OKUTA_TYPE_RED_PROJECTILE ||
                                           EN_OKUTA_GET_TYPE(actor) == EN_OKUTA_TYPE_BLUE_PROJECTILE);
}

// Remembered so the shield can reflect it in ShouldActorUpdate, before the projectile's own hit handling destroys it.
static void CacheProjectileColliders(Actor* actor) {
    PlayState* play = gPlayState;
    if (play == NULL || !IsWorn() || !sByrna.active || sByrna.move != BYRNA_GUARD || !IsProjectile(actor)) {
        return;
    }
    for (s32 c = 0; c < play->colChkCtx.colATCount; c++) {
        Collider* collider = play->colChkCtx.colAT[c];
        if (collider == NULL || collider->actor != actor) {
            continue;
        }
        for (s32 i = 0; i < ARRAY_COUNT(sProjectiles); i++) {
            if (sProjectiles[i].actor == NULL || sProjectiles[i].collider == collider) {
                sProjectiles[i].actor = actor;
                sProjectiles[i].collider = collider;
                break;
            }
        }
    }
}

static void ForgetProjectile(Actor* actor) {
    for (s32 i = 0; i < ARRAY_COUNT(sProjectiles); i++) {
        if (sProjectiles[i].actor == actor) {
            sProjectiles[i].actor = NULL;
        }
    }
}

static void SendProjectileBack(PlayState* play, Actor* actor, Collider* collider) {
    Player* player = GET_PLAYER(play);
    s16 yaw = Actor_WorldYawTowardActor(&player->actor, actor);
    f32 planarSpeed = sqrtf(SQ(actor->velocity.x) + SQ(actor->velocity.z));
    collider->atFlags &= ~(AT_HIT | AT_BOUNCED | AT_TYPE_ENEMY);
    collider->atFlags |= AT_TYPE_PLAYER;
    collider->at = NULL;
    actor->world.rot.y = yaw;
    actor->shape.rot.y = yaw;
    actor->speed = MAX(MAX(fabsf(actor->speed), planarSpeed), 4.0f);
    actor->velocity.x = Math_SinS(yaw) * actor->speed;
    actor->velocity.z = Math_CosS(yaw) * actor->speed;
    sGuard.base.acFlags &= ~AC_HIT;
    sGuard.base.ac = NULL;
    Audio_PlaySfx(NA_SE_IT_SHIELD_REFLECT_SW);
}

static void ReflectProjectile(Actor* actor, bool* should) {
    PlayState* play = gPlayState;
    if (play == NULL || !sCollidersReady || !sByrna.active || sByrna.move != BYRNA_GUARD ||
        !(sGuard.base.acFlags & AC_HIT) || sGuard.base.ac != actor) {
        return;
    }
    for (s32 i = 0; i < ARRAY_COUNT(sProjectiles); i++) {
        if (sProjectiles[i].actor == actor) {
            SendProjectileBack(play, actor, sProjectiles[i].collider);
            return;
        }
    }
}
