/*
 * Ocarina of Time's water Stinger (En_Weiyer): it swims inside a water box, rams a player who comes near
 * and leaps out at one standing by the surface.
 */
#include "oot_enemy_stinger.h"

#define WEIYER_BGCHECK_FLAGS (UPDBGCHECKINFO_FLAG_1 | UPDBGCHECKINFO_FLAG_2 | UPDBGCHECKINFO_FLAG_4)
#define WEIYER_SURFACE_MARGIN 5.0f
#define WEIYER_ATTACK_RANGE 400.0f
#define WEIYER_WATER_EDGE_INSET 30.0f

struct EnWeiyer;
typedef void (*EnWeiyerActionFunc)(struct EnWeiyer*, PlayState*);

typedef struct EnWeiyer {
    Actor actor;
    SkelAnime skelAnime;
    EnWeiyerActionFunc actionFunc;
    s16 timer;
    s16 targetYaw;
    Vec3s jointTable[EI_LIMB_MAX];
    Vec3s morphTable[EI_LIMB_MAX];
    f32 swimHeight;
    f32 targetSwimHeight;
    ColliderCylinder collider;
} EnWeiyer;

static ColliderCylinderInit sColCylInit = {
    {
        COL_MATERIAL_HIT0,
        AT_TYPE_ENEMY,
        AC_ON | AC_TYPE_PLAYER,
        OC1_ON | OC1_TYPE_ALL,
        OC2_TYPE_1,
        COLSHAPE_CYLINDER,
    },
    {
        ELEM_MATERIAL_UNK0,
        { 0xF7CFFFFF, 0x00, 0x08 },
        { STINGER_EXPOSED_DMG_FLAGS, 0x00, 0x00 },
        ATELEM_ON | ATELEM_SFX_HARD,
        ACELEM_ON,
        OCELEM_ON,
    },
    { 16, 10, -6, { 0, 0, 0 } },
};

static void EnWeiyer_InitInsideWaterBox(EnWeiyer* this, PlayState* play);
static void EnWeiyer_FreeSwim(EnWeiyer* this, PlayState* play);
static void EnWeiyer_TurnAround(EnWeiyer* this, PlayState* play);
static void EnWeiyer_StuckOnFloor(EnWeiyer* this, PlayState* play);
static void EnWeiyer_Attack(EnWeiyer* this, PlayState* play);
static void EnWeiyer_Inactive(EnWeiyer* this, PlayState* play);
static void EnWeiyer_Hurt(EnWeiyer* this, PlayState* play);
static void EnWeiyer_Stunned(EnWeiyer* this, PlayState* play);
static void EnWeiyer_Die(EnWeiyer* this, PlayState* play);
static void EnWeiyer_Dead(EnWeiyer* this, PlayState* play);
static void EnWeiyer_OutOfWater(EnWeiyer* this, PlayState* play);

static f32 EnWeiyer_GetWaterDepth(EnWeiyer* this) {
    return this->actor.home.pos.y - this->actor.floorHeight;
}

static bool EnWeiyer_IsPlayerAboveWater(EnWeiyer* this, PlayState* play) {
    return (this->actor.home.pos.y + 20.0f) <= GET_PLAYER(play)->actor.world.pos.y;
}

static void EnWeiyer_SetupFreeSwim(EnWeiyer* this) {
    this->targetYaw = this->actor.shape.rot.y;
    this->swimHeight = (cosf(-M_PIf / 8) * 3.0f) + this->actor.world.pos.y;
    Animation_MorphToLoop(&this->skelAnime, ANIM(sHitAnim), -5.0f);
    this->timer = 30;
    this->actor.speed = CLAMP_MAX(this->actor.speed, 2.5f);
    this->collider.base.atFlags &= ~AT_ON;
    this->targetSwimHeight = this->actor.floorHeight;
    this->actionFunc = EnWeiyer_FreeSwim;
}

static void EnWeiyer_SetupTurnAround(EnWeiyer* this) {
    Animation_MorphToLoop(&this->skelAnime, ANIM(sHitAnim), -5.0f);
    this->collider.base.atFlags |= AT_ON;
    this->timer = 0;
    this->actor.speed = 5.0f;
    this->actionFunc = EnWeiyer_TurnAround;
}

static void EnWeiyer_SetupStuckOnFloor(EnWeiyer* this) {
    Animation_Change(&this->skelAnime, ANIM(sPopOutAnim), 2.0f, 0.0f, 0.0f, ANIMMODE_LOOP, -8.0f);
    this->timer = 40;
    this->collider.base.atFlags |= AT_ON;
    this->actionFunc = EnWeiyer_StuckOnFloor;
}

static void EnWeiyer_SetupAttack(EnWeiyer* this) {
    this->timer = 200;
    this->collider.base.atFlags |= AT_ON;
    this->skelAnime.playSpeed = 3.0f;
    this->actionFunc = EnWeiyer_Attack;
}

static void EnWeiyer_SetupInactive(EnWeiyer* this) {
    this->timer = 200;
    this->targetYaw = this->actor.yawTowardsPlayer + 0x8000;
    this->swimHeight = this->actor.world.pos.y;
    this->actor.speed = CLAMP_MAX(this->actor.speed, 4.0f);
    this->collider.base.atFlags &= ~AT_ON;
    this->skelAnime.playSpeed = 1.0f;
    this->actionFunc = EnWeiyer_Inactive;
}

static void EnWeiyer_SetupHurt(EnWeiyer* this) {
    Animation_Change(&this->skelAnime, ANIM(sHitAnim), 2.0f, 0.0f, 0.0f, ANIMMODE_LOOP, -3.0f);
    this->timer = 40;
    this->collider.base.atFlags &= ~AT_ON;
    this->collider.base.acFlags &= ~AC_ON;
    this->actor.gravity = 0.0f;
    this->actor.velocity.y = 0.0f;
    this->actor.speed = 3.0f;
    Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_RED, 200, COLORFILTER_BUFFLAG_OPA, 40);
    this->collider.dim.height = sColCylInit.dim.height;
    this->actionFunc = EnWeiyer_Hurt;
}

static void EnWeiyer_SetupStunned(EnWeiyer* this) {
    Animation_Change(&this->skelAnime, ANIM(sPopOutAnim), 2.0f, 0.0f, 0.0f, ANIMMODE_LOOP, -8.0f);
    this->timer = 80;
    this->actor.speed = 0.0f;
    this->actor.velocity.y = 0.0f;
    this->actor.gravity = -1.0f;
    this->collider.dim.height = sColCylInit.dim.height + 15;
    Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_BLUE, 200, COLORFILTER_BUFFLAG_OPA, 80);
    this->collider.base.atFlags &= ~AT_ON;
    Actor_PlaySfx(&this->actor, STINGER_SFX_FREEZE);
    this->actionFunc = EnWeiyer_Stunned;
}

static void EnWeiyer_SetupDie(EnWeiyer* this) {
    Animation_MorphToLoop(&this->skelAnime, ANIM(sHitAnim), -5.0f);
    this->timer = 20;
    Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_RED, 200, COLORFILTER_BUFFLAG_OPA, 40);
    this->collider.base.atFlags &= ~AT_ON;
    this->collider.base.acFlags &= ~AC_ON;
    this->actor.speed = 3.0f;
    this->actionFunc = EnWeiyer_Die;
}

static void EnWeiyer_SetupDead(EnWeiyer* this) {
    this->actor.colorFilterParams |= COLORFILTER_BUFFLAG_XLU;
    this->actor.speed = 0.0f;
    this->actor.velocity.y = 0.0f;
    this->actionFunc = EnWeiyer_Dead;
}

static void EnWeiyer_SetupOutOfWater(EnWeiyer* this) {
    this->actor.shape.rot.x = -0x2000;
    // -1 waits for the first frame above the surface.
    this->timer = -1;
    this->actor.speed = 5.0f;
    this->actionFunc = EnWeiyer_OutOfWater;
}

// Casts down from point->y; on success point->y becomes the water surface.
static bool EnWeiyer_IsSwimmableAt(EnWeiyer* this, PlayState* play, Vec3f* point, f32* floorY) {
    CollisionPoly* floorPoly;
    s32 bgId;
    *floorY = BgCheck_EntityRaycastFloor5(&play->colCtx, &floorPoly, &bgId, &this->actor, point);
    WaterBox* waterBox;
    f32 surfaceY;
    bool hasWater = WaterBox_GetSurface1(play, &play->colCtx, point->x, point->z, &surfaceY, &waterBox);
    if (!hasWater || (*floorY <= BGCHECK_Y_MIN) || ((surfaceY - WEIYER_SURFACE_MARGIN) <= *floorY)) {
        return false;
    }
    point->y = surfaceY;
    return true;
}

static f32 EnWeiyer_ClampIntoSpan(f32 value, s32 min, s32 length) {
    if (length <= (2.0f * WEIYER_WATER_EDGE_INSET)) {
        return min + (length / 2.0f);
    }
    return CLAMP(value, min + WEIYER_WATER_EDGE_INSET, (min + length) - WEIYER_WATER_EDGE_INSET);
}

// Tries the box point nearest to the stinger, then the box centre.
static bool EnWeiyer_FindSwimmableInBox(EnWeiyer* this, PlayState* play, WaterBox* box, Vec3f* point, f32* floorY) {
    Vec3f nearest = { EnWeiyer_ClampIntoSpan(this->actor.world.pos.x, box->minPos.x, box->xLength), box->minPos.y,
                      EnWeiyer_ClampIntoSpan(this->actor.world.pos.z, box->minPos.z, box->zLength) };
    if (EnWeiyer_IsSwimmableAt(this, play, &nearest, floorY)) {
        *point = nearest;
        return true;
    }
    Vec3f centre = { box->minPos.x + (box->xLength / 2.0f), box->minPos.y, box->minPos.z + (box->zLength / 2.0f) };
    if (EnWeiyer_IsSwimmableAt(this, play, &centre, floorY)) {
        *point = centre;
        return true;
    }
    return false;
}

static bool EnWeiyer_FindNearestWaterInRoom(EnWeiyer* this, PlayState* play, Vec3f* swimPos, f32* swimFloorY) {
    CollisionHeader* colHeader = play->colCtx.colHeader;
    bool isFound = false;
    f32 nearestDist = 0.0f;

    if ((colHeader->numWaterBoxes == 0) || (colHeader->waterBoxes == NULL)) {
        return false;
    }
    for (u32 i = 0; i < colHeader->numWaterBoxes; i++) {
        WaterBox* box = &colHeader->waterBoxes[i];
        bool isInRoom = (box->room == play->roomCtx.curRoom.num) || (box->room == -1);
        Vec3f point;
        f32 floorY;
        if (!isInRoom || (box->properties & WATERBOX_FLAG_19) ||
            !EnWeiyer_FindSwimmableInBox(this, play, box, &point, &floorY)) {
            continue;
        }
        f32 dist = Math_Vec3f_DistXZ(&this->actor.world.pos, &point);
        if (!isFound || (dist < nearestDist)) {
            isFound = true;
            nearestDist = dist;
            *swimPos = point;
            *swimFloorY = floorY;
        }
    }
    return isFound;
}

// The randomizer only knows the room has water, so a spawn on dry land swims off to the nearest water box.
static void EnWeiyer_InitInsideWaterBox(EnWeiyer* this, PlayState* play) {
    Vec3f swimPos = this->actor.world.pos;
    f32 floorY;

    swimPos.y += 0.5f;
    bool hasWater = EnWeiyer_IsSwimmableAt(this, play, &swimPos, &floorY) ||
                    EnWeiyer_FindNearestWaterInRoom(this, play, &swimPos, &floorY);
    if (!hasWater) {
        ReplaceWithLandStinger(&this->actor, play);
        return;
    }
    this->actor.floorHeight = floorY;
    this->actor.home.pos.y = swimPos.y - WEIYER_SURFACE_MARGIN;
    this->actor.world.pos.x = swimPos.x;
    this->actor.world.pos.y = (this->actor.home.pos.y + this->actor.floorHeight) / 2.0f;
    this->actor.world.pos.z = swimPos.z;
    this->actor.prevPos = this->actor.world.pos;
    EnWeiyer_SetupFreeSwim(this);
}

static void EnWeiyer_SwimForward(EnWeiyer* this) {
    f32 curFrame = this->skelAnime.curFrame;

    this->actor.world.pos.y = this->swimHeight - cosf((curFrame - 5.0f) * (M_PIf / 40)) * 3.0f;
    Math_StepToF(&this->actor.speed, (curFrame <= 45.0f) ? 1.0f : 1.3f, 0.03f);
}

static void EnWeiyer_FreeSwim(EnWeiyer* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    Math_ScaledStepToS(&this->actor.shape.rot.x, 0, 0x800);
    bool isLoopStart = Animation_OnFrame(&this->skelAnime, 0.0f);
    Math_StepToF(&this->swimHeight, this->targetSwimHeight, 0.5f);
    EnWeiyer_SwimForward(this);
    if (this->actor.bgCheckFlags & BGCHECKFLAG_WALL) {
        this->targetYaw = this->actor.wallYaw;
        this->timer = 30;
    }
    if (Math_ScaledStepToS(&this->actor.shape.rot.y, this->targetYaw, 0xB6)) {
        if (this->timer != 0) {
            this->timer--;
        }
        if (this->timer == 0) {
            this->targetYaw =
                Rand_S16Offset(0x2000, 0x2000) * ((Rand_ZeroOne() < 0.5f) ? -1 : 1) + this->actor.shape.rot.y;
            this->timer = 30;
            if (Rand_ZeroOne() < 0.3333f) {
                Actor_PlaySfx(&this->actor, STINGER_SFX_CRY);
            }
        }
    }
    if (this->actor.home.pos.y < this->actor.world.pos.y) {
        if (this->actor.home.pos.y < this->actor.floorHeight) {
            EnWeiyer_SetupTurnAround(this);
            return;
        }
        this->actor.world.pos.y = this->actor.home.pos.y;
        this->targetSwimHeight = Rand_ZeroOne() * (EnWeiyer_GetWaterDepth(this) / 2.0f) + this->actor.floorHeight;
        return;
    }
    if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) {
        this->targetSwimHeight = this->actor.home.pos.y - Rand_ZeroOne() * (EnWeiyer_GetWaterDepth(this) / 2.0f);
    } else if (isLoopStart && (Rand_ZeroOne() < 0.1f)) {
        this->targetSwimHeight = Rand_ZeroOne() * EnWeiyer_GetWaterDepth(this) + this->actor.floorHeight;
    }
    if ((this->actor.xzDistToPlayer < WEIYER_ATTACK_RANGE) && (fabsf(this->actor.playerHeightRel) < 250.0f) &&
        !EnWeiyer_IsPlayerAboveWater(this, play)) {
        EnWeiyer_SetupAttack(this);
    }
}

static void EnWeiyer_TurnAround(EnWeiyer* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    if (this->timer == 0) {
        if (Math_ScaledStepToS(&this->actor.shape.rot.x, -0x4000, 0x800)) {
            this->actor.shape.rot.z = 0;
            this->actor.shape.rot.y += 0x8000;
            this->timer = 1;
        } else {
            this->actor.shape.rot.z = this->actor.shape.rot.x * 2;
        }
        return;
    }
    Math_ScaledStepToS(&this->actor.shape.rot.x, 0x1800, 0x800);
    if (this->actor.world.pos.y < this->actor.home.pos.y) {
        if (this->actor.shape.rot.x > 0) {
            EffectSsGSplash_Spawn(play, &this->actor.world.pos, NULL, NULL, 1, 400);
            Actor_PlaySfx(&this->actor, STINGER_SFX_SINK);
        }
        EnWeiyer_SetupInactive(this);
    } else if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) {
        EnWeiyer_SetupStuckOnFloor(this);
    }
}

static void EnWeiyer_StuckOnFloor(EnWeiyer* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    if (Animation_OnFrame(&this->skelAnime, 0.0f)) {
        Actor_PlaySfx(&this->actor, STINGER_SFX_FLUTTER);
    }
    Math_ScaledStepToS(&this->actor.shape.rot.x, 0, 0x800);
    Math_StepToF(&this->actor.speed, 0.0f, 1.0f);
    if (this->timer != 0) {
        this->timer--;
    }
    if (this->timer == 0) {
        EnWeiyer_SetupTurnAround(this);
    } else if (this->actor.world.pos.y < this->actor.home.pos.y) {
        EnWeiyer_SetupFreeSwim(this);
    }
}

static s16 EnWeiyer_PitchTowardPlayer(EnWeiyer* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    Vec3f focus = { player->actor.world.pos.x, player->actor.world.pos.y + 20.0f, player->actor.world.pos.z };

    return Actor_WorldPitchTowardPoint(&this->actor, &focus);
}

static void EnWeiyer_Attack(EnWeiyer* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    if (this->timer != 0) {
        this->timer--;
    }
    if ((this->timer == 0) || EnWeiyer_IsPlayerAboveWater(this, play) || (this->collider.base.atFlags & AT_HIT)) {
        EnWeiyer_SetupInactive(this);
        return;
    }
    Math_StepToF(&this->actor.speed, Actor_IsFacingPlayer(&this->actor, 0x2800) ? 4.0f : 1.3f, 0.2f);
    if (this->actor.home.pos.y < this->actor.world.pos.y) {
        if (this->actor.home.pos.y < this->actor.floorHeight) {
            this->actor.shape.rot.x = 0;
            EnWeiyer_SetupTurnAround(this);
            return;
        }
        this->actor.world.pos.y = this->actor.home.pos.y;
        Math_SmoothStepToS(&this->actor.shape.rot.x, 0x1000, 2, 0x100, 0x40);
    } else {
        Math_SmoothStepToS(&this->actor.shape.rot.x, EnWeiyer_PitchTowardPlayer(this, play), 2, 0x100, 0x40);
    }
    Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 2, 0x200, 0x80);
    if ((GET_PLAYER(play)->actor.depthInWater < 50.0f) && (this->actor.depthInWater < 20.0f) &&
        Actor_IsFacingPlayer(&this->actor, 0x2000)) {
        EnWeiyer_SetupOutOfWater(this);
    }
}

static void EnWeiyer_Inactive(EnWeiyer* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    Math_ScaledStepToS(&this->actor.shape.rot.x, 0, 0x800);
    Math_StepToF(&this->swimHeight, EnWeiyer_GetWaterDepth(this) / 4.0f + this->actor.floorHeight, 1.0f);
    EnWeiyer_SwimForward(this);
    if (this->timer != 0) {
        this->timer--;
    }
    if (this->actor.bgCheckFlags & BGCHECKFLAG_WALL) {
        this->targetYaw = this->actor.wallYaw;
    }
    if (Math_SmoothStepToS(&this->actor.shape.rot.y, this->targetYaw, 2, 0x200, 0x80) == 0) {
        this->targetYaw = this->actor.yawTowardsPlayer + 0x8000;
    }
    if (this->actor.home.pos.y < this->actor.world.pos.y) {
        if (this->actor.home.pos.y < this->actor.floorHeight) {
            EnWeiyer_SetupTurnAround(this);
        } else {
            this->actor.world.pos.y = this->actor.home.pos.y;
        }
    }
    if (this->timer == 0) {
        EnWeiyer_SetupFreeSwim(this);
    }
}

static void EnWeiyer_Hurt(EnWeiyer* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    if (this->timer != 0) {
        this->timer--;
    }
    if (this->actor.bgCheckFlags & BGCHECKFLAG_WALL) {
        this->targetYaw = this->actor.wallYaw;
    } else {
        this->targetYaw = this->actor.yawTowardsPlayer + 0x8000;
    }
    Math_ScaledStepToS(&this->actor.world.rot.y, this->targetYaw, 0x38E);
    Math_ScaledStepToS(&this->actor.shape.rot.x, 0, 0x200);
    this->actor.shape.rot.z = sinf(this->timer * (M_PIf / 5)) * 5120.0f;
    if (this->timer == 0) {
        this->actor.shape.rot.z = 0;
        this->collider.base.acFlags |= AC_ON;
        EnWeiyer_SetupFreeSwim(this);
    }
}

static void EnWeiyer_Die(EnWeiyer* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    Math_ScaledStepToS(&this->actor.shape.rot.x, -0x4000, 0x400);
    this->actor.shape.rot.z += 0x1000;
    if (this->timer != 0) {
        this->timer--;
    }
    if ((this->timer == 0) || (this->actor.bgCheckFlags & BGCHECKFLAG_CEILING)) {
        EnWeiyer_SetupDead(this);
    }
}

static void EnWeiyer_Dead(EnWeiyer* this, PlayState* play) {
    this->actor.shape.shadowAlpha = CLAMP_MIN((s16)(this->actor.shape.shadowAlpha - 5), 0);
    this->actor.world.pos.y -= 2.0f;
    if (this->actor.shape.shadowAlpha == 0) {
        Item_DropCollectibleRandom(play, &this->actor, &this->actor.world.pos, 0xE0);
        Actor_Kill(&this->actor);
    }
}

static void EnWeiyer_Stunned(EnWeiyer* this, PlayState* play) {
    if (this->timer != 0) {
        this->timer--;
    }
    Math_ScaledStepToS(&this->actor.shape.rot.x, 0, 0x200);
    Math_ScaledStepToS(&this->actor.shape.rot.z, 0, 0x200);
    SkelAnime_Update(&this->skelAnime);
    bool isStranded = this->actor.home.pos.y < this->actor.floorHeight;
    if (isStranded && Animation_OnFrame(&this->skelAnime, 0.0f)) {
        Actor_PlaySfx(&this->actor, STINGER_SFX_FLUTTER);
    }
    if (isStranded && (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND_TOUCH)) {
        Actor_PlaySfx(&this->actor, STINGER_SFX_LAND);
    }
    if (this->timer == 0) {
        this->actor.gravity = 0.0f;
        this->actor.velocity.y = 0.0f;
        this->collider.dim.height = sColCylInit.dim.height;
        EnWeiyer_SetupFreeSwim(this);
    }
}

static void EnWeiyer_OutOfWater(EnWeiyer* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    bool shouldGiveUp = EnWeiyer_IsPlayerAboveWater(this, play) || (this->collider.base.atFlags & AT_HIT);
    if (this->timer == -1) {
        if (shouldGiveUp) {
            EnWeiyer_SetupInactive(this);
        } else if (this->actor.depthInWater < 0.0f) {
            this->timer = 10;
            EffectSsGSplash_Spawn(play, &this->actor.world.pos, NULL, NULL, 1, 400);
            Actor_PlaySfx(&this->actor, NA_SE_EN_OCTAROCK_JUMP);
        }
        return;
    }
    if (shouldGiveUp) {
        this->timer = 0;
    } else if (this->timer != 0) {
        this->timer--;
    }
    s16 targetPitch = 0x1800;
    if (this->timer != 0) {
        targetPitch = CLAMP_MIN(EnWeiyer_PitchTowardPlayer(this, play), 0);
    }
    if (this->actor.shape.rot.x < targetPitch) {
        Math_ScaledStepToS(&this->actor.shape.rot.x, targetPitch, 0x400);
    }
    if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) {
        EnWeiyer_SetupTurnAround(this);
    } else if ((this->actor.bgCheckFlags & BGCHECKFLAG_WATER) && (this->actor.shape.rot.x > 0)) {
        EffectSsGSplash_Spawn(play, &this->actor.world.pos, NULL, NULL, 1, 400);
        Actor_PlaySfx(&this->actor, STINGER_SFX_SINK);
        EnWeiyer_SetupInactive(this);
    } else {
        Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 8, 0x100, 0x80);
    }
}

static void EnWeiyer_UpdateDamage(EnWeiyer* this, PlayState* play) {
    if (!(this->collider.base.acFlags & AC_HIT)) {
        return;
    }
    this->collider.base.acFlags &= ~AC_HIT;
    Actor_SetDropFlag(&this->actor, &this->collider.elem);
    if ((this->actor.colChkInfo.damageEffect == STINGER_DMGEFF_NONE) && (this->actor.colChkInfo.damage == 0)) {
        return;
    }
    if (this->actor.colChkInfo.damageEffect == STINGER_DMGEFF_STUN) {
        if (this->actionFunc != EnWeiyer_Stunned) {
            EnWeiyer_SetupStunned(this);
        }
        return;
    }
    if (Actor_ApplyDamage(&this->actor) != 0) {
        EnWeiyer_SetupHurt(this);
        return;
    }
    Enemy_StartFinishingBlow(play, &this->actor);
    Actor_PlaySfx(&this->actor, STINGER_SFX_DEAD);
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    EnWeiyer_SetupDie(this);
}

static void EnWeiyer_Update(Actor* thisx, PlayState* play) {
    EnWeiyer* this = (EnWeiyer*)thisx;

    // home.y follows the water surface.
    this->actor.home.pos.y = this->actor.depthInWater + this->actor.world.pos.y - WEIYER_SURFACE_MARGIN;
    EnWeiyer_UpdateDamage(this, play);
    this->actionFunc(this, play);
    this->actor.world.rot.y = this->actor.shape.rot.y;
    this->actor.world.rot.x = -this->actor.shape.rot.x;
    if ((this->actor.world.rot.x == 0) || (this->actionFunc == EnWeiyer_Stunned)) {
        Actor_MoveWithGravity(&this->actor);
    } else {
        Actor_MoveWithoutGravity(&this->actor);
    }
    Actor_UpdateBgCheckInfo(play, &this->actor, 10.0f, 30.0f, 45.0f, WEIYER_BGCHECK_FLAGS);
    Actor_SetFocus(&this->actor, 0.0f);
    if (this->collider.base.atFlags & AT_HIT) {
        this->collider.base.atFlags &= ~(AT_ON | AT_HIT);
        Actor_PlaySfx(&this->actor, STINGER_SFX_ATTACK);
    }
    Collider_UpdateCylinder(&this->actor, &this->collider);
    if (this->collider.base.atFlags & AT_ON) {
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
    }
    if (this->collider.base.acFlags & AC_ON) {
        CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
    }
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
}

static void EnWeiyer_Init(Actor* thisx, PlayState* play) {
    EnWeiyer* this = (EnWeiyer*)thisx;

    Actor_SetScale(&this->actor, 0.003f);
    this->actor.lockOnArrowOffset = 2500.0f;
    ActorShape_Init(&this->actor.shape, 1000.0f, ActorShadow_DrawCircle, 65.0f);
    SkelAnime_Init(play, &this->skelAnime, (SkeletonHeader*)sSkel, ANIM(sIdleAnim), this->jointTable, this->morphTable,
                   EI_LIMB_MAX);
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, &this->actor, &sColCylInit);
    CollisionCheck_SetInfo(&this->actor.colChkInfo, &sDamageTable, &sColChkInfoInit);
    this->actionFunc = EnWeiyer_InitInsideWaterBox;
}

static void EnWeiyer_Destroy(Actor* thisx, PlayState* play) {
    EnWeiyer* this = (EnWeiyer*)thisx;

    Collider_DestroyCylinder(play, &this->collider);
}

static s32 EnWeiyer_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, Actor* thisx,
                                     Gfx** gfx) {
    if (limbIndex == EI_LIMB_ROOT) {
        pos->z += 2000.0f;
    }
    return false;
}

static void EnWeiyer_Draw(Actor* thisx, PlayState* play) {
    EnWeiyer* this = (EnWeiyer*)thisx;

    OPEN_DISPS(play->state.gfxCtx);
    if (this->actionFunc != EnWeiyer_Dead) {
        Gfx_SetupDL25_Opa(play->state.gfxCtx);
        gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)sEmptyDL);
        gDPSetEnvColor(POLY_OPA_DISP++, 255, 255, 255, 255);
        POLY_OPA_DISP = SkelAnime_Draw(play, this->skelAnime.skeleton, this->skelAnime.jointTable,
                                       EnWeiyer_OverrideLimbDraw, NULL, &this->actor, POLY_OPA_DISP);
    } else {
        Gfx_SetupDL25_Xlu(play->state.gfxCtx);
        gSPSegment(POLY_XLU_DISP++, 0x08, (uintptr_t)sXluRenderModeDL);
        gDPSetEnvColor(POLY_XLU_DISP++, 255, 255, 255, this->actor.shape.shadowAlpha);
        POLY_XLU_DISP = SkelAnime_Draw(play, this->skelAnime.skeleton, this->skelAnime.jointTable,
                                       EnWeiyer_OverrideLimbDraw, NULL, &this->actor, POLY_XLU_DISP);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

void SetWaterStingerFunctions(S2HActorDefinition* definition) {
    definition->instanceSize = sizeof(EnWeiyer);
    definition->init = EnWeiyer_Init;
    definition->destroy = EnWeiyer_Destroy;
    definition->update = EnWeiyer_Update;
    definition->draw = EnWeiyer_Draw;
}
