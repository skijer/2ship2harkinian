/*
 * Ocarina of Time's Big Octo (En_Bigokuta) on Majora's Mask, drawn from the OoT Asset Loader's oot.o2r.
 * Vanilla circles Jabu-Jabu's pillar at a fixed height with its own camera; this one circles its spawn point at the
 * water surface, or burrows through the floor where there is no water.
 */
#include "2s2h/ModApi/ModApi.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "objects/gameplay_keep/gameplay_keep.h"

#define BIG_OCTO_KEY "oot.big_octo"
#define BIG_OCTO_SOUL_KEY "soul.oot.big_octo"
#define BO_OBJECT "objects/oot_object_bigokuta/"
#define BO_ASSET(name) "__OTR__" BO_OBJECT name

static const ALIGN_ASSET(2) char sSkel[] = BO_ASSET("object_bigokuta_Skel_006BC0");
static const ALIGN_ASSET(2) char sHurtAnim[] = BO_ASSET("object_bigokuta_Anim_000444");
static const ALIGN_ASSET(2) char sDieAnim[] = BO_ASSET("object_bigokuta_Anim_000A74");
static const ALIGN_ASSET(2) char sStunAnim[] = BO_ASSET("object_bigokuta_Anim_000D1C");
static const ALIGN_ASSET(2) char sIdleAnim[] = BO_ASSET("object_bigokuta_Anim_0014B8");
static const ALIGN_ASSET(2) char sSwimAnim[] = BO_ASSET("object_bigokuta_Anim_001CA4");

#define ANIM(asset) ((AnimationHeader*)(asset))

#define BO_LIMB_COUNT 20
#define BO_ORBIT_RADIUS_MAX 263.0f
#define BO_ORBIT_RADIUS_MIN 120.0f
#define BO_WALL_MARGIN 90.0f
#define BO_ORBIT_PROBES 16
#define BO_SUBMERGE_DEPTH 200.0f
#define BO_WAKE_DISTANCE 500.0f
#define BO_EYE_LIMB 15
#define BO_BODY_LIMB 10
#define BO_MOUTH_LIMB 17
#define BO_DMGEFF_STUN 0x1
#define BO_DMGEFF_BOMB 0xF

#define BO_FLAGS                                                                              \
    (ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE | ACTOR_FLAG_UPDATE_CULLING_DISABLED | \
     ACTOR_FLAG_DRAW_CULLING_DISABLED)

static const S2HModApi* sApi;

static Color_RGBA8 sDeathDustPrim = { 255, 255, 255, 255 };
static Color_RGBA8 sDeathDustEnv = { 100, 255, 255, 255 };
static Vec3f sZeroVec = { 0.0f, 0.0f, 0.0f };

static Gfx sXluRenderModeDL[] = {
    gsDPSetRenderMode(G_RM_FOG_SHADE_A, AA_EN | Z_CMP | Z_UPD | IM_RD | CLR_ON_CVG | CVG_DST_WRAP | ZMODE_XLU |
                                            FORCE_BL | GBL_c2(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_1MA)),
    gsDPSetAlphaCompare(G_AC_THRESHOLD),
    gsSPEndDisplayList(),
};
static Gfx sEmptyDL[] = {
    gsSPEndDisplayList(),
};

struct EnBigokuta;
typedef void (*EnBigokutaActionFunc)(struct EnBigokuta*, PlayState*);

typedef struct EnBigokuta {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[BO_LIMB_COUNT];
    Vec3s morphTable[BO_LIMB_COUNT];
    EnBigokutaActionFunc actionFunc;
    ColliderJntSph collider;
    ColliderJntSphElement element;
    ColliderCylinder cylinder[2];
    f32 orbitRadius;
    s16 timer;
    s16 sideTimer;
    s16 turnTargetYaw;
    s8 orbitDir;
    u8 isAlert;
    u8 isInWater;
    u8 isBgmPlaying;
} EnBigokuta;

static ColliderJntSphElementInit sJntSphElementInit[1] = {
    {
        {
            ELEM_MATERIAL_UNK1,
            { 0x20000000, 0x00, 0x08 },
            { 0xF7CFFFFF, 0x00, 0x00 },
            ATELEM_ON | ATELEM_SFX_HARD,
            ACELEM_ON,
            OCELEM_ON,
        },
        { 1, { { 0, 45, -30 }, 75 }, 100 },
    },
};

static ColliderJntSphInit sJntSphInit = {
    {
        COL_MATERIAL_HIT0,
        AT_ON | AT_TYPE_ENEMY,
        AC_ON | AC_TYPE_PLAYER,
        OC1_ON | OC1_TYPE_ALL,
        OC2_TYPE_1,
        COLSHAPE_JNTSPH,
    },
    ARRAY_COUNT(sJntSphElementInit),
    sJntSphElementInit,
};

static ColliderCylinderInit sCylinderInit[2] = {
    {
        {
            COL_MATERIAL_HARD,
            AT_ON | AT_TYPE_ENEMY,
            AC_ON | AC_HARD | AC_TYPE_PLAYER,
            OC1_ON | OC1_TYPE_ALL,
            OC2_TYPE_1,
            COLSHAPE_CYLINDER,
        },
        {
            ELEM_MATERIAL_UNK1,
            { 0x20000000, 0x00, 0x08 },
            { 0xF7CFFFE7, 0x00, 0x00 },
            ATELEM_ON | ATELEM_SFX_HARD,
            ACELEM_ON,
            OCELEM_ON,
        },
        { 50, 100, 0, { 30, 0, 12 } },
    },
    {
        {
            COL_MATERIAL_HARD,
            AT_ON | AT_TYPE_ENEMY,
            AC_ON | AC_HARD | AC_TYPE_PLAYER,
            OC1_ON | OC1_TYPE_ALL,
            OC2_TYPE_1,
            COLSHAPE_CYLINDER,
        },
        {
            ELEM_MATERIAL_UNK1,
            { 0x20000000, 0x00, 0x08 },
            { 0xF7CFFFE7, 0x00, 0x00 },
            ATELEM_ON | ATELEM_SFX_HARD,
            ACELEM_ON,
            OCELEM_ON,
        },
        { 50, 100, 0, { -30, 0, 12 } },
    },
};

static CollisionCheckInfoInit sColChkInfoInit = { 4, 130, 120, 200 };

static DamageTable sDamageTable = {
    /* Deku Nut       */ DMG_ENTRY(0, BO_DMGEFF_STUN),
    /* Deku Stick     */ DMG_ENTRY(1, 0x0),
    /* Horse trample  */ DMG_ENTRY(0, 0x0),
    /* Explosives     */ DMG_ENTRY(0, BO_DMGEFF_BOMB),
    /* Zora boomerang */ DMG_ENTRY(0, BO_DMGEFF_STUN),
    /* Normal arrow   */ DMG_ENTRY(0, 0x0),
    /* UNK_DMG_0x06   */ DMG_ENTRY(0, 0x0),
    /* Hookshot       */ DMG_ENTRY(0, 0x0),
    /* Goron punch    */ DMG_ENTRY(0, 0x0),
    /* Sword          */ DMG_ENTRY(1, 0x0),
    /* Goron pound    */ DMG_ENTRY(0, 0x0),
    /* Fire arrow     */ DMG_ENTRY(0, 0x0),
    /* Ice arrow      */ DMG_ENTRY(0, 0x0),
    /* Light arrow    */ DMG_ENTRY(0, 0x0),
    /* Goron spikes   */ DMG_ENTRY(0, 0x0),
    /* Deku spin      */ DMG_ENTRY(0, 0x0),
    /* Deku bubble    */ DMG_ENTRY(0, 0x0),
    /* Deku launch    */ DMG_ENTRY(0, 0x0),
    /* UNK_DMG_0x12   */ DMG_ENTRY(0, 0x0),
    /* Zora barrier   */ DMG_ENTRY(0, 0x0),
    /* Normal shield  */ DMG_ENTRY(0, 0x0),
    /* Light ray      */ DMG_ENTRY(0, 0x0),
    /* Thrown object  */ DMG_ENTRY(0, 0x0),
    /* Zora punch     */ DMG_ENTRY(0, 0x0),
    /* Spin attack    */ DMG_ENTRY(1, 0x0),
    /* Sword beam     */ DMG_ENTRY(1, 0x0),
    /* Normal Roll    */ DMG_ENTRY(0, 0x0),
    /* UNK_DMG_0x1B   */ DMG_ENTRY(0, 0x0),
    /* UNK_DMG_0x1C   */ DMG_ENTRY(0, 0x0),
    /* Unblockable    */ DMG_ENTRY(0, 0x0),
    /* UNK_DMG_0x1E   */ DMG_ENTRY(0, 0x0),
    /* Powder Keg     */ DMG_ENTRY(0, BO_DMGEFF_BOMB),
};

static InitChainEntry sInitChain[] = {
    ICHAIN_F32(lockOnArrowOffset, 2000, ICHAIN_CONTINUE),
    ICHAIN_U8(attentionRangeType, ATTENTION_RANGE_2, ICHAIN_CONTINUE),
    ICHAIN_F32(gravity, -1, ICHAIN_CONTINUE),
    ICHAIN_VEC3F_DIV1000(scale, 33, ICHAIN_STOP),
};

static void EnBigokuta_SetupSwim(EnBigokuta* this);
static void EnBigokuta_SetupSpin(EnBigokuta* this);
static void EnBigokuta_SetupTurn(EnBigokuta* this);
static void EnBigokuta_SetupRise(EnBigokuta* this, PlayState* play);
static void EnBigokuta_Spin(EnBigokuta* this, PlayState* play);
static void EnBigokuta_Submerge(EnBigokuta* this, PlayState* play);
static void EnBigokuta_Swim(EnBigokuta* this, PlayState* play);
static void EnBigokuta_Stunned(EnBigokuta* this, PlayState* play);
static void EnBigokuta_Hurt(EnBigokuta* this, PlayState* play);
static void EnBigokuta_Die(EnBigokuta* this, PlayState* play);
static void EnBigokuta_Lurk(EnBigokuta* this, PlayState* play);

static void EnBigokuta_StartBgm(EnBigokuta* this) {
    if (this->isBgmPlaying) {
        return;
    }
    Audio_PlayBgm_StorePrevBgm(NA_BGM_MINI_BOSS);
    this->isBgmPlaying = true;
}

static void EnBigokuta_StopBgm(EnBigokuta* this) {
    if (!this->isBgmPlaying) {
        return;
    }
    Audio_RestorePrevBgm();
    this->isBgmPlaying = false;
}

static void EnBigokuta_FindSurface(EnBigokuta* this, PlayState* play) {
    CollisionPoly* poly = NULL;
    WaterBox* waterBox = NULL;
    s32 bgId = 0;
    f32 waterY = 0.0f;
    Vec3f probe = { this->actor.home.pos.x, this->actor.home.pos.y + 50.0f, this->actor.home.pos.z };
    f32 floorY = BgCheck_EntityRaycastFloor5(&play->colCtx, &poly, &bgId, &this->actor, &probe);

    if (floorY <= BGCHECK_Y_MIN) {
        floorY = this->actor.home.pos.y;
    }
    bool hasWater =
        WaterBox_GetSurface1(play, &play->colCtx, this->actor.home.pos.x, this->actor.home.pos.z, &waterY, &waterBox);

    this->isInWater = hasWater && (waterY > floorY);
    this->actor.home.pos.y = this->isInWater ? waterY : floorY;
}

static f32 EnBigokuta_MeasureOrbitRadius(EnBigokuta* this, PlayState* play) {
    Vec3f center = { this->actor.home.pos.x, this->actor.home.pos.y + 40.0f, this->actor.home.pos.z };
    f32 reach = BO_ORBIT_RADIUS_MAX + BO_WALL_MARGIN;
    f32 radius = BO_ORBIT_RADIUS_MAX;

    for (s32 i = 0; i < BO_ORBIT_PROBES; i++) {
        s16 yaw = i * (0x10000 / BO_ORBIT_PROBES);
        Vec3f end = { center.x + Math_SinS(yaw) * reach, center.y, center.z + Math_CosS(yaw) * reach };
        Vec3f hit;
        CollisionPoly* poly = NULL;
        s32 bgId = 0;

        if (BgCheck_EntityLineTest1(&play->colCtx, &center, &end, &hit, &poly, true, false, false, true, &bgId)) {
            f32 wallDistance = sqrtf(SQ(hit.x - center.x) + SQ(hit.z - center.z)) - BO_WALL_MARGIN;
            radius = MIN(radius, wallDistance);
        }
    }
    return MAX(radius, BO_ORBIT_RADIUS_MIN);
}

static void EnBigokuta_PlaceOnOrbit(EnBigokuta* this) {
    this->actor.world.rot.y = this->actor.shape.rot.y + this->orbitDir * -0x4000;
    this->actor.world.pos.x = Math_SinS(this->actor.world.rot.y) * this->orbitRadius + this->actor.home.pos.x;
    this->actor.world.pos.z = Math_CosS(this->actor.world.rot.y) * this->orbitRadius + this->actor.home.pos.z;
}

static void EnBigokuta_SpawnSurfaceBurst(EnBigokuta* this, PlayState* play, Vec3f* pos, s16 scale) {
    Vec3f accel = { 0.0f, 0.3f, 0.0f };

    if (this->isInWater) {
        EffectSsGSplash_Spawn(play, pos, NULL, NULL, 1, scale);
        return;
    }
    Vec3f floorPos = { pos->x, this->actor.home.pos.y, pos->z };
    func_800B1210(play, &floorPos, &sZeroVec, &accel, scale / 8, 20);
}

static void EnBigokuta_SpawnRestingRipples(EnBigokuta* this, PlayState* play) {
    f32 yDistFromHome = this->actor.world.pos.y - this->actor.home.pos.y;
    Vec3f pos = { this->actor.world.pos.x, this->actor.home.pos.y + 3.0f, this->actor.world.pos.z };

    if (!this->isInWater || ((play->gameplayFrames % 7) != 0) || (yDistFromHome > 0.0f) || (yDistFromHome <= -100.0f)) {
        return;
    }
    EffectSsGRipple_Spawn(play, &pos, 800, 1300, 0);
}

static void EnBigokuta_SpawnWake(EnBigokuta* this, PlayState* play) {
    bool isSubmerging = this->actionFunc == EnBigokuta_Submerge;
    s16 spread = Rand_S16Offset(0x1200, 0xC00);
    s16 rot = this->actor.shape.rot.y - this->orbitDir * 0xA00 + ((play->gameplayFrames & 1) ? spread : -spread);
    bool isNear = isSubmerging || (this->actionFunc == EnBigokuta_Spin) || (play->gameplayFrames & 2);
    f32 distance = isSubmerging ? 50.0f : (isNear ? 80.0f : 120.0f);
    Vec3f pos = { this->actor.world.pos.x - Math_SinS(rot) * distance, this->actor.home.pos.y + (isNear ? 1.0f : 5.0f),
                  this->actor.world.pos.z - Math_CosS(rot) * distance };

    if (isNear && this->isInWater) {
        EffectSsGRipple_Spawn(play, &pos, 100, 500, 0);
    }
    EnBigokuta_SpawnSurfaceBurst(this, play, &pos, 800);
    if (!isSubmerging) {
        Actor_PlaySfx_Flagged(&this->actor,
                              (this->isInWater ? NA_SE_EN_COMMON_WATER_SLW : NA_SE_EN_PAMET_ROLL) - SFX_FLAG);
    }
}

static void EnBigokuta_Land(EnBigokuta* this, PlayState* play) {
    for (s32 i = 0; i < 4; i++) {
        Vec3f pos = { this->actor.world.pos.x + ((i >= 2) ? 60.0f : -60.0f), this->actor.world.pos.y,
                      this->actor.world.pos.z + ((i & 1) ? 60.0f : -60.0f) };

        EnBigokuta_SpawnSurfaceBurst(this, play, &pos, 2000);
    }
    Actor_PlaySfx(&this->actor, NA_SE_EN_DAIOCTA_LAND);
    Actor_PlaySfx(&this->actor, NA_SE_EN_GOLON_LAND_BIG);
    Actor_RequestQuakeAndRumble(&this->actor, play, 10, 8);
}

static void EnBigokuta_Lurk(EnBigokuta* this, PlayState* play) {
    if (Actor_WorldDistXZToPoint(&GET_PLAYER(play)->actor, &this->actor.home.pos) > BO_WAKE_DISTANCE) {
        return;
    }
    EnBigokuta_StartBgm(this);
    Actor_PlaySfx(&this->actor, NA_SE_EN_DAIOCTA_REVERSE);
    EnBigokuta_SetupRise(this, play);
}

static void EnBigokuta_Swim(EnBigokuta* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    Vec3f playerLocal;

    SkelAnime_Update(&this->skelAnime);
    if (Animation_OnFrame(&this->skelAnime, 0.0f)) {
        Actor_PlaySfx(&this->actor, this->isInWater ? NA_SE_EN_COMMON_WATER_MID : NA_SE_EN_PAMET_WALK);
    }
    if (this->timer < 0) {
        this->actor.shape.rot.y += this->orbitDir * 0x200;
        EnBigokuta_PlaceOnOrbit(this);
        this->timer++;
        if (this->timer == 0) {
            this->timer = 350;
        }
        EnBigokuta_SpawnWake(this, play);
        return;
    }
    s16 turnSpeed =
        (Actor_WorldDistXZToPoint(&player->actor, &this->actor.home.pos) - (this->orbitRadius - 83.0f)) * (8.0f / 15);
    Actor_WorldToActorCoords(&this->actor, &playerLocal, &player->actor.world.pos);
    bool isPlayerBehind = (playerLocal.z > 0.0f) && !Actor_IsFacingPlayer(&this->actor, 0x1B00) &&
                          !Player_IsFacingActor(&this->actor, 0x2000, play);
    if ((fabsf(playerLocal.x) > this->orbitRadius) || isPlayerBehind) {
        turnSpeed -= 0x80;
        if (this->timer != 0) {
            this->timer--;
        }
    }
    if ((this->actor.xzDistToPlayer < 250.0f) && !Actor_IsFacingPlayer(&this->actor, 0x6000)) {
        if (this->sideTimer != 0) {
            this->sideTimer--;
        }
        if (this->actor.xzDistToPlayer < 180.0f) {
            turnSpeed += 0x20;
        }
    } else {
        this->sideTimer = 80;
    }
    if (this->actor.colChkInfo.health == 1) {
        turnSpeed = (turnSpeed + 0x130) * 1.1f;
    } else {
        turnSpeed += 0x130;
    }
    this->actor.shape.rot.y += turnSpeed * this->orbitDir;
    EnBigokuta_PlaceOnOrbit(this);
    EnBigokuta_SpawnWake(this, play);
    if (this->sideTimer == 0) {
        this->orbitDir = (Rand_ZeroOne() < 0.5f) ? -1 : 1;
        this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
        this->cylinder[0].base.atFlags &= ~AT_ON;
        Actor_PlaySfx(&this->actor, NA_SE_EN_DAIOCTA_SINK);
        this->actionFunc = EnBigokuta_Submerge;
        return;
    }
    if (this->timer == 0) {
        EnBigokuta_SetupTurn(this);
        return;
    }
    if (!this->isAlert) {
        return;
    }
    s16 yawToPlayer = ABS((s16)(this->actor.yawTowardsPlayer - this->actor.shape.rot.y));
    if ((yawToPlayer < 0x4100) && (yawToPlayer > 0x3F00)) {
        if (Rand_ZeroOne() < 0.6f) {
            this->timer = 0;
            EnBigokuta_SetupTurn(this);
        }
        this->isAlert = false;
    }
}

static void EnBigokuta_SetupSwim(EnBigokuta* this) {
    Animation_MorphToLoop(&this->skelAnime, ANIM(sSwimAnim), -5.0f);
    this->timer = 350;
    this->sideTimer = 80;
    EnBigokuta_PlaceOnOrbit(this);
    this->cylinder[0].base.atFlags |= AT_ON;
    this->collider.base.acFlags |= AC_ON;
    this->actionFunc = EnBigokuta_Swim;
}

static void EnBigokuta_BombJump(EnBigokuta* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    if (this->timer != 0) {
        this->timer--;
    }
    this->actor.world.pos.y = sinf(this->timer * (M_PIf / 16)) * 100.0f + this->actor.home.pos.y;
    if (this->timer == 0) {
        EnBigokuta_Land(this, play);
        EnBigokuta_SetupSwim(this);
    }
}

static void EnBigokuta_SetupBombJump(EnBigokuta* this) {
    this->timer = 16;
    this->collider.base.acFlags &= ~AC_ON;
    this->actor.colorFilterTimer = 0;
    this->actionFunc = EnBigokuta_BombJump;
}

static void EnBigokuta_Turn(EnBigokuta* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    if (this->timer != 0) {
        this->timer--;
    }
    if (this->timer == 20) {
        Actor_PlaySfx(&this->actor, NA_SE_EN_DAIOCTA_REVERSE);
    }
    if ((this->timer == 0) && Math_ScaledStepToS(&this->actor.shape.rot.y, this->turnTargetYaw, 0x800)) {
        this->orbitDir = -this->orbitDir;
        EnBigokuta_SetupSwim(this);
    }
}

static void EnBigokuta_SetupTurn(EnBigokuta* this) {
    Animation_MorphToLoop(&this->skelAnime, ANIM(sIdleAnim), -5.0f);
    this->isAlert = true;
    this->turnTargetYaw = this->actor.shape.rot.y + 0x8000;
    this->collider.base.acFlags &= ~AC_ON;
    this->cylinder[0].base.atFlags |= AT_ON;
    this->actionFunc = EnBigokuta_Turn;
}

static void EnBigokuta_Stunned(EnBigokuta* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    if (this->timer != 0) {
        this->timer--;
    }
    SkelAnime_Update(&this->skelAnime);
    bool isBodyTouched = (this->collider.base.ocFlags1 & OC1_HIT) != 0;
    if (isBodyTouched || (this->cylinder[0].base.ocFlags1 & OC1_HIT) || (this->cylinder[1].base.ocFlags1 & OC1_HIT)) {
        f32 push = CLAMP_MIN(player->actor.speed, 1.0f);

        if (!isBodyTouched) {
            this->cylinder[0].base.ocFlags1 &= ~OC1_HIT;
            this->cylinder[1].base.ocFlags1 &= ~OC1_HIT;
            push = -push;
        }
        player->actor.world.pos.x -= push * Math_SinS(this->actor.shape.rot.y);
        player->actor.world.pos.z -= push * Math_CosS(this->actor.shape.rot.y);
    }
    if (this->timer == 0) {
        EnBigokuta_SetupSpin(this);
    }
}

static void EnBigokuta_SetupStun(EnBigokuta* this) {
    ColliderElement* hitBy = this->element.base.acHitElem;

    Animation_MorphToPlayOnce(&this->skelAnime, ANIM(sStunAnim), -5.0f);
    this->cylinder[0].base.atFlags |= AT_ON;
    Actor_PlaySfx(&this->actor, NA_SE_EN_COMMON_FREEZE);
    this->isAlert = (hitBy != NULL) && (hitBy->atDmgInfo.dmgFlags & DMG_DEKU_NUT);
    this->timer = this->isAlert ? 20 : 80;
    Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_BLUE, 255, COLORFILTER_BUFFLAG_OPA, this->timer);
    this->actionFunc = EnBigokuta_Stunned;
}

static void EnBigokuta_Hurt(EnBigokuta* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    if (this->timer != 0) {
        this->timer--;
    }
    if (this->timer == 0) {
        if (this->actor.colChkInfo.health != 0) {
            EnBigokuta_SetupTurn(this);
            return;
        }
        Animation_MorphToPlayOnce(&this->skelAnime, ANIM(sDieAnim), -5.0f);
        Actor_PlaySfx(&this->actor, NA_SE_EN_DAIOCTA_DEAD2);
        this->timer = 38;
        this->sideTimer = 10;
        this->actionFunc = EnBigokuta_Die;
        return;
    }
    if (this->timer < 8) {
        return;
    }
    this->actor.shape.rot.y += this->orbitDir * 0x200;
    this->actor.world.pos.y = sinf((this->timer - 8) * (M_PIf / 16)) * 100.0f + this->actor.home.pos.y;
    EnBigokuta_PlaceOnOrbit(this);
    if (this->timer == 8) {
        EnBigokuta_Land(this, play);
    }
}

static void EnBigokuta_SetupHurt(EnBigokuta* this) {
    Animation_MorphToPlayOnce(&this->skelAnime, ANIM(sHurtAnim), -5.0f);
    this->timer = 24;
    this->cylinder[0].base.atFlags &= ~AT_ON;
    Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_RED, 255, COLORFILTER_BUFFLAG_OPA, 24);
    this->actionFunc = EnBigokuta_Hurt;
}

static void EnBigokuta_Die(EnBigokuta* this, PlayState* play) {
    if (this->timer != 0) {
        this->timer--;
        if (this->timer >= 10) {
            this->actor.shape.rot.y += 0x2000;
        }
        return;
    }
    if (!SkelAnime_Update(&this->skelAnime)) {
        return;
    }
    if (this->sideTimer != 0) {
        this->sideTimer--;
    }
    if (this->sideTimer == 6) {
        Vec3f pos = { this->actor.world.pos.x, this->actor.world.pos.y + 150.0f, this->actor.world.pos.z };

        func_800B0DE0(play, &pos, &sZeroVec, &sZeroVec, &sDeathDustPrim, &sDeathDustEnv, 1200, 20);
        Actor_PlaySfx(&this->actor, NA_SE_EN_OCTAROCK_DEAD2);
    }
    if ((this->sideTimer != 0) || !Math_StepToF(&this->actor.scale.y, 0.0f, 0.001f)) {
        return;
    }
    EnBigokuta_StopBgm(this);
    SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 50, NA_SE_EN_COMMON_WATER_MID);
    Item_DropCollectibleRandom(play, &this->actor, &this->actor.world.pos, 0xB0);
    Actor_Kill(&this->actor);
}

static void EnBigokuta_Spin(EnBigokuta* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    if (this->timer != 0) {
        this->timer--;
        this->actor.shape.rot.y += 0x2000;
    }
    if (this->timer == 0) {
        this->orbitDir = ((s16)(this->actor.shape.rot.y - this->actor.world.rot.y) > 0) ? 1 : -1;
        EnBigokuta_SetupSwim(this);
        if (!this->isAlert) {
            this->timer = -40;
        }
    }
    EnBigokuta_SpawnWake(this, play);
}

static void EnBigokuta_SetupSpin(EnBigokuta* this) {
    if (!this->isAlert) {
        this->timer = (Rand_ZeroOne() < 0.5f) ? 24 : 28;
    } else if (ABS((s16)(this->actor.shape.rot.y - this->actor.yawTowardsPlayer)) >= 0x4000) {
        this->timer = 4;
    } else {
        this->timer = 0;
    }
    this->collider.base.acFlags &= ~AC_ON;
    this->actionFunc = EnBigokuta_Spin;
}

static void EnBigokuta_Submerge(EnBigokuta* this, PlayState* play) {
    this->actor.world.pos.y -= 10.0f;
    this->actor.shape.rot.y += 0x2000;
    if (this->actor.world.pos.y < (this->actor.home.pos.y - BO_SUBMERGE_DEPTH)) {
        EnBigokuta_SetupRise(this, play);
    }
    EnBigokuta_SpawnWake(this, play);
}

static void EnBigokuta_Rise(EnBigokuta* this, PlayState* play) {
    if (Math_StepToF(&this->actor.world.pos.y, this->actor.home.pos.y, 10.0f)) {
        this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;
        EnBigokuta_SetupSwim(this);
    }
}

static void EnBigokuta_SetupRise(EnBigokuta* this, PlayState* play) {
    this->actor.world.rot.y = Actor_WorldYawTowardPoint(&GET_PLAYER(play)->actor, &this->actor.home.pos);
    this->actor.shape.rot.y = this->actor.world.rot.y + this->orbitDir * 0x4000;
    EnBigokuta_PlaceOnOrbit(this);
    this->actionFunc = EnBigokuta_Rise;
}

static void EnBigokuta_CheckBump(EnBigokuta* this, PlayState* play) {
    bool hasHit = (this->cylinder[0].base.atFlags & AT_HIT) || (this->cylinder[1].base.atFlags & AT_HIT) ||
                  (this->collider.base.atFlags & AT_HIT);

    if (!hasHit) {
        return;
    }
    this->cylinder[0].base.atFlags &= ~AT_HIT;
    this->cylinder[1].base.atFlags &= ~AT_HIT;
    this->collider.base.atFlags &= ~AT_HIT;
    s16 yawDiff = this->actor.yawTowardsPlayer - this->actor.world.rot.y;
    s16 knockYaw;
    if (yawDiff > 0x4000) {
        knockYaw = 0x4000;
    } else if (yawDiff > 0) {
        knockYaw = 0x6000;
    } else if (yawDiff < -0x4000) {
        knockYaw = -0x4000;
    } else {
        knockYaw = -0x6000;
    }
    func_800B8D98(play, &this->actor, 10.0f, this->actor.world.rot.y + knockYaw, 5.0f);
    if (this->actionFunc == EnBigokuta_Swim) {
        EnBigokuta_SetupTurn(this);
        this->timer = 40;
    } else if ((this->actionFunc == EnBigokuta_Spin) && ((knockYaw * this->orbitDir) > 0)) {
        this->orbitDir = -this->orbitDir;
        this->timer += 4;
    }
}

static void EnBigokuta_CheckDamage(EnBigokuta* this, PlayState* play) {
    if (!(this->collider.base.acFlags & AC_HIT)) {
        return;
    }
    this->collider.base.acFlags &= ~AC_HIT;
    u8 effect = this->actor.colChkInfo.damageEffect;
    if ((effect == 0) && (this->actor.colChkInfo.damage == 0)) {
        return;
    }
    if (effect == BO_DMGEFF_STUN) {
        if (this->actionFunc != EnBigokuta_Stunned) {
            EnBigokuta_SetupStun(this);
        }
        return;
    }
    if (effect == BO_DMGEFF_BOMB) {
        EnBigokuta_SetupBombJump(this);
        return;
    }
    // Only its back is soft: hits from the front glance off.
    if (Actor_IsFacingPlayer(&this->actor, 0x4000)) {
        return;
    }
    if (Actor_ApplyDamage(&this->actor) == 0) {
        Actor_PlaySfx(&this->actor, NA_SE_EN_DAIOCTA_DEAD);
        Enemy_StartFinishingBlow(play, &this->actor);
    } else {
        Actor_PlaySfx(&this->actor, NA_SE_EN_DAIOCTA_DAMAGE);
    }
    EnBigokuta_SetupHurt(this);
}

static void EnBigokuta_UpdateColliderPositions(EnBigokuta* this) {
    f32 sin = Math_SinS(this->actor.shape.rot.y);
    f32 cos = Math_CosS(this->actor.shape.rot.y);
    ColliderJntSphElementDim* sphere = &this->element.dim;

    sphere->worldSphere.center.x =
        sphere->modelSphere.center.z * sin + this->actor.world.pos.x + sphere->modelSphere.center.x * cos;
    sphere->worldSphere.center.z =
        this->actor.world.pos.z + sphere->modelSphere.center.z * cos - sphere->modelSphere.center.x * sin;
    sphere->worldSphere.center.y = sphere->modelSphere.center.y + this->actor.world.pos.y;
    for (s32 i = 0; i < ARRAY_COUNT(this->cylinder); i++) {
        Vec3f* offset = &sCylinderInit[i].dim.pos;

        this->cylinder[i].dim.pos.x = this->actor.world.pos.x + offset->z * sin + offset->x * cos;
        this->cylinder[i].dim.pos.z = this->actor.world.pos.z + offset->z * cos - offset->x * sin;
        this->cylinder[i].dim.pos.y = this->actor.world.pos.y;
    }
}

static void EnBigokuta_UpdateCollision(EnBigokuta* this, PlayState* play) {
    CollisionCheckContext* colChkCtx = &play->colChkCtx;

    if (this->cylinder[0].base.atFlags & AT_ON) {
        for (s32 i = 0; i < ARRAY_COUNT(this->cylinder); i++) {
            if (this->actionFunc != EnBigokuta_Stunned) {
                CollisionCheck_SetAT(play, colChkCtx, &this->cylinder[i].base);
            } else {
                CollisionCheck_SetOC(play, colChkCtx, &this->cylinder[i].base);
            }
            CollisionCheck_SetAC(play, colChkCtx, &this->cylinder[i].base);
        }
        if (this->actionFunc != EnBigokuta_Stunned) {
            this->actor.flags |= ACTOR_FLAG_SFX_FOR_PLAYER_BODY_HIT;
        }
        if (this->collider.base.acFlags & AC_ON) {
            CollisionCheck_SetAC(play, colChkCtx, &this->collider.base);
        }
    }
    if (this->collider.base.acFlags & AC_ON) {
        CollisionCheck_SetOC(play, colChkCtx, &this->collider.base);
    } else {
        CollisionCheck_SetAT(play, colChkCtx, &this->collider.base);
    }
}

static void EnBigokuta_Init(Actor* thisx, PlayState* play) {
    EnBigokuta* this = (EnBigokuta*)thisx;

    Actor_ProcessInitChain(&this->actor, sInitChain);
    SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)sSkel, ANIM(sIdleAnim), this->jointTable,
                       this->morphTable, BO_LIMB_COUNT);
    Collider_InitJntSph(play, &this->collider);
    Collider_SetJntSph(play, &this->collider, &this->actor, &sJntSphInit, &this->element);
    this->element.dim.worldSphere.radius = this->element.dim.modelSphere.radius;
    for (s32 i = 0; i < ARRAY_COUNT(sCylinderInit); i++) {
        Collider_InitCylinder(play, &this->cylinder[i]);
        Collider_SetCylinder(play, &this->cylinder[i], &this->actor, &sCylinderInit[i]);
    }
    CollisionCheck_SetInfo(&this->actor.colChkInfo, &sDamageTable, &sColChkInfoInit);
    this->orbitDir = 1;
    EnBigokuta_FindSurface(this, play);
    this->orbitRadius = EnBigokuta_MeasureOrbitRadius(this, play);
    this->actor.world.pos.y = this->actor.home.pos.y - BO_SUBMERGE_DEPTH;
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    this->actionFunc = EnBigokuta_Lurk;
}

static void EnBigokuta_Destroy(Actor* thisx, PlayState* play) {
    EnBigokuta* this = (EnBigokuta*)thisx;

    EnBigokuta_StopBgm(this);
    Collider_DestroyJntSph(play, &this->collider);
    for (s32 i = 0; i < ARRAY_COUNT(this->cylinder); i++) {
        Collider_DestroyCylinder(play, &this->cylinder[i]);
    }
}

static void EnBigokuta_Update(Actor* thisx, PlayState* play) {
    EnBigokuta* this = (EnBigokuta*)thisx;

    if (this->actionFunc == EnBigokuta_Lurk) {
        EnBigokuta_Lurk(this, play);
        return;
    }
    EnBigokuta_CheckBump(this, play);
    EnBigokuta_CheckDamage(this, play);
    this->actionFunc(this, play);
    EnBigokuta_UpdateColliderPositions(this);
    EnBigokuta_UpdateCollision(this, play);
    Actor_SetFocus(&this->actor, this->actor.scale.y * 25.0f * 100.0f);
    EnBigokuta_SpawnRestingRipples(this, play);
}

static f32 EnBigokuta_GetPulse(s16 timer, s32 period) {
    s32 phase = timer % period;
    s32 rise = period * 2 / 3;
    f32 angle = (phase >= rise) ? (period - phase) * (M_PIf / (period - rise) / 2) : phase * (M_PIf / rise / 2);

    return sinf(angle) * 125.0f + 130.0f;
}

static s32 EnBigokuta_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                       Actor* thisx) {
    EnBigokuta* this = (EnBigokuta*)thisx;
    bool isStunned = this->actionFunc == EnBigokuta_Stunned;
    bool isDying = this->actionFunc == EnBigokuta_Die;

    OPEN_DISPS(play->state.gfxCtx);
    if ((limbIndex == BO_EYE_LIMB) && (isStunned || (this->actionFunc == EnBigokuta_Hurt))) {
        u8 glow = EnBigokuta_GetPulse(this->timer, isStunned ? 12 : 6);

        gDPPipeSync(POLY_OPA_DISP++);
        gDPSetEnvColor(POLY_OPA_DISP++, glow, glow, glow, 255);
    } else if (limbIndex == BO_BODY_LIMB) {
        u8 intensity = isDying ? (u8)(this->timer * (255.0f / 38)) : 255;

        gDPPipeSync(POLY_OPA_DISP++);
        gDPSetEnvColor(POLY_OPA_DISP++, intensity, intensity, intensity, intensity);
    } else if ((limbIndex == BO_MOUTH_LIMB) && isDying) {
        if (this->sideTimer < 5) {
            Matrix_Scale((this->sideTimer * 0.2f * 0.25f) + 1.0f, 1.0f, 1.0f, MTXMODE_APPLY);
        } else if (this->sideTimer < 8) {
            f32 swell = (this->sideTimer - 5) * (1.0f / 12);
            Matrix_Scale(1.25f - swell, 1.0f + swell, 1.0f + swell, MTXMODE_APPLY);
        } else {
            f32 swell = (this->sideTimer - 8) * 0.125f;
            Matrix_Scale(1.0f, 1.25f - swell, 1.25f - swell, MTXMODE_APPLY);
        }
    }
    CLOSE_DISPS(play->state.gfxCtx);
    return false;
}

static void EnBigokuta_DrawFading(EnBigokuta* this, PlayState* play) {
    SkelAnime* skelAnime = &this->skelAnime;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    gSPSegment(POLY_XLU_DISP++, 0x0C, (uintptr_t)sXluRenderModeDL);
    gDPSetEnvColor(POLY_XLU_DISP++, 0, 0, 0, (u8)(this->actor.scale.y * (255 / 0.033f)));
    POLY_XLU_DISP = SkelAnime_DrawFlex(play, skelAnime->skeleton, skelAnime->jointTable, skelAnime->dListCount, NULL,
                                       NULL, NULL, POLY_XLU_DISP);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void EnBigokuta_Draw(Actor* thisx, PlayState* play) {
    EnBigokuta* this = (EnBigokuta*)thisx;
    SkelAnime* skelAnime = &this->skelAnime;

    if ((this->actionFunc == EnBigokuta_Die) && (this->timer == 0) && (this->sideTimer == 0)) {
        EnBigokuta_DrawFading(this, play);
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x0C, (uintptr_t)sEmptyDL);
    gDPSetEnvColor(POLY_OPA_DISP++, 255, 255, 255, 255);
    bool isShaking = ((this->actionFunc == EnBigokuta_Hurt) && (this->timer >= 8)) ||
                     ((this->actionFunc == EnBigokuta_Die) && (this->timer >= 10));
    if ((this->timer & 1) && isShaking) {
        f32 rotX = Rand_ZeroOne() * (M_PIf * 2.0f);
        f32 rotY = Rand_ZeroOne() * (M_PIf * 2.0f);

        Matrix_RotateYF(rotY, MTXMODE_APPLY);
        Matrix_RotateXFApply(rotX);
        Matrix_Scale(0.78999996f, 1.3f, 0.78999996f, MTXMODE_APPLY);
        Matrix_RotateXFApply(-rotX);
        Matrix_RotateYF(-rotY, MTXMODE_APPLY);
    }
    SkelAnime_DrawFlexOpa(play, skelAnime->skeleton, skelAnime->jointTable, skelAnime->dListCount,
                          EnBigokuta_OverrideLimbDraw, NULL, &this->actor);
    CLOSE_DISPS(play->state.gfxCtx);
}

// The soul shows its enemy in an idle loop, as Majora's Mask souls do.
static void DrawSoulModel(PlayState* play) {
    static SkelAnime sSoulSkelAnime;
    static Vec3s sSoulJointTable[BO_LIMB_COUNT];
    static Vec3s sSoulMorphTable[BO_LIMB_COUNT];
    static bool sIsSoulPosed = false;
    static u32 sSoulFrame = 0;

    if (!sIsSoulPosed) {
        SkelAnime_InitFlex(play, &sSoulSkelAnime, (FlexSkeletonHeader*)sSkel, ANIM(sIdleAnim), sSoulJointTable,
                           sSoulMorphTable, BO_LIMB_COUNT);
        sIsSoulPosed = true;
    }
    if (sSoulFrame != play->state.frames) {
        sSoulFrame = play->state.frames;
        SkelAnime_Update(&sSoulSkelAnime);
    }
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
    // Animations place the root far from the origin; the soul floats on its root instead.
    Matrix_Translate(-sSoulJointTable[0].x, -sSoulJointTable[0].y, -sSoulJointTable[0].z, MTXMODE_APPLY);
    // Eyes and effects read segments the actor fills; with no actor they point at harmless lists.
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)sEmptyDL);
    gSPSegment(POLY_OPA_DISP++, 0x09, (uintptr_t)sEmptyDL);
    gSPSegment(POLY_OPA_DISP++, 0x0A, (uintptr_t)sEmptyDL);
    gSPSegment(POLY_OPA_DISP++, 0x0B, (uintptr_t)sEmptyDL);
    gSPSegment(POLY_OPA_DISP++, 0x0C, (uintptr_t)sEmptyDL);
    SkelAnime_DrawFlexOpa(play, sSoulSkelAnime.skeleton, sSoulSkelAnime.jointTable, sSoulSkelAnime.dListCount, NULL,
                          NULL, NULL);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawSoul(PlayState* play, S2HGetItemEntry* entry) {
    u32 frames = play->state.frames;

    DrawSoulModel(play);

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               (uintptr_t)Gfx_TwoTexScroll(play->state.gfxCtx, 0, 0, 0, 0x10, 0x20, 1, (frames * 2) & 0x3F,
                                           (frames * -6) & 0x7F, 0x10, 0x20));
    gDPSetPrimColor(POLY_XLU_DISP++, 0xC0, 0xC0, 60, 160, 255, 0);
    gDPSetEnvColor(POLY_XLU_DISP++, 60, 160, 255, 0);
    Matrix_Scale(0.13333333f, 0.13333333f, 0.13333333f, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gameplay_keep_DL_01ACF0);
    CLOSE_DISPS(play->state.gfxCtx);
}

static bool CanKillBigOcto(void* userData) {
    return sApi->LogicHasCapability("CanUseSword") || sApi->LogicCanUseItem(RI_DEKU_STICK);
}

static void RegisterSoul(void) {
    static S2HCustomItemRandomizer sSoulLogic = { 0 };
    S2HCustomItemDefinition soul = { 0 };

    // Pool membership follows the host's soul shuffle and soul pool; the mod only declares the item.
    sSoulLogic.structSize = sizeof(sSoulLogic);
    sSoulLogic.flags = S2H_CUSTOM_ITEM_RANDO_ADVANCEMENT;
    sSoulLogic.type = S2H_CUSTOM_ITEM_TYPE_ITEM;
    sSoulLogic.logicKey = BIG_OCTO_SOUL_KEY;
    sSoulLogic.hintKey = BIG_OCTO_SOUL_KEY;
    soul.structSize = sizeof(soul);
    soul.key = BIG_OCTO_SOUL_KEY;
    soul.presentationFlags = S2H_ITEM_HIDE_FROM_KALEIDO;
    soul.getItemText = "You got the %rSoul of Big Octo%w!&It can be defeated now.";
    soul.getItemEntry.drawFunc = DrawSoul;
    soul.randomizer = &sSoulLogic;
    if (!sApi->RegisterCustomItem(&soul)) {
        sApi->Log("Could not register " BIG_OCTO_SOUL_KEY);
    }
}

static s16 RegisterBigOcto(void) {
    S2HActorDefinition definition = { 0 };

    definition.structSize = sizeof(definition);
    definition.key = BIG_OCTO_KEY;
    definition.description = "Big Octo";
    definition.category = ACTORCAT_ENEMY;
    definition.actorFlags = BO_FLAGS;
    definition.objectId = GAMEPLAY_KEEP;
    definition.instanceSize = sizeof(EnBigokuta);
    definition.init = EnBigokuta_Init;
    definition.destroy = EnBigokuta_Destroy;
    definition.update = EnBigokuta_Update;
    definition.draw = EnBigokuta_Draw;
    definition.enemyFlags = S2H_ACTOR_ENEMY | S2H_ACTOR_ENEMY_MINIBOSS | S2H_ACTOR_ENEMY_NOT_IN_TIMED_ROOMS;
    definition.naviHint = "Big Octo&Its tough hide only gives at the back.&Stun it, or get behind it!";
    // The host finds the soul item as "soul." + soulKey, so this is the family key.
    definition.soulKey = BIG_OCTO_KEY;
    definition.logicKill = CanKillBigOcto;
    return sApi->RegisterActor(&definition);
}

typedef bool (*OotAssetsRequireFunc)(const char* path);

// oot_assets offers to extract the ROM again when a resource is missing.
static bool RequireOotAsset(const char* path) {
    OotAssetsRequireFunc require = (OotAssetsRequireFunc)sApi->FindService("oot_assets.require");

    return (require != NULL) ? require(path) : (sApi->HasResource(path) != 0);
}

S2H_MOD_EXPORT void ModSetApi(const S2HModApi* api) {
    sApi = api;
}

S2H_MOD_EXPORT const S2HModRequirements* ModGetRequirements(void) {
    static const S2HModRequirements requirements = { sizeof(S2HModRequirements), NULL, 0 };
    return &requirements;
}

S2H_MOD_EXPORT void ModInit(void) {
    if (!S2H_MOD_API_HAS(sApi, FindService)) {
        return;
    }
    if (!RequireOotAsset(BO_OBJECT "object_bigokuta_Skel_006BC0")) {
        sApi->Log(BIG_OCTO_KEY ": OoT Asset Loader resources are missing; extract them to enable this enemy.");
        return;
    }
    if (RegisterBigOcto() < 0) {
        sApi->Log("Could not register " BIG_OCTO_KEY);
        return;
    }
    RegisterSoul();
}
