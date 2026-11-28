/*
 * Ocarina of Time's Tailpasaran (En_Tp) on Majora's Mask, drawn from the OoT Asset Loader's oot.o2r.
 * The armoured head drags seven electric tail segments; only the tail can be hurt, and a sword gets shocked.
 */
#include "2s2h/ModApi/ModApi.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "objects/gameplay_keep/gameplay_keep.h"

#define TAILPASARAN_KEY "oot.tailpasaran"
#define TAILPASARAN_TAIL_KEY "oot.tailpasaran.tail"
#define TP_OBJECT "objects/oot_object_tp/"
#define TP_ASSET(name) "__OTR__" TP_OBJECT name

static const ALIGN_ASSET(2) char sHeadDL[] = TP_ASSET("gTailpasaranHeadDL");
static const ALIGN_ASSET(2) char sTailSegmentDL[] = TP_ASSET("gTailpasaranTailSegmentDL");
static const ALIGN_ASSET(2) char sTailSegmentTex[] = TP_ASSET("gTailpasaranTailSegmentTex");

#define TP_SFX_FREEZE NA_SE_EN_COMMON_FREEZE

#define TP_HEAD_FLAGS (ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE | ACTOR_FLAG_UPDATE_CULLING_DISABLED)
#define TP_BGCHECK_FLAGS (UPDBGCHECKINFO_FLAG_1 | UPDBGCHECKINFO_FLAG_4)
#define TP_TAIL_SEGMENTS 7
#define TP_TARGETABLE_SEGMENT 2
#define TP_WAKE_RANGE 200.0f
#define TP_STUN_DURATION 80
// OoT's PLAYER_STATE1_DAMAGED is MM's unnamed bit 26.
#define TP_PLAYER_STATE1_DAMAGED PLAYER_STATE1_4000000

typedef enum {
    TP_TYPE_HEAD = -1,
    TP_TYPE_TAIL = 0,
    TP_TYPE_FRAGMENT = 10,
    TP_TYPE_TAIL_DYING = 11,
    TP_TYPE_HEAD_DYING = 12,
} TailpasaranType;

typedef enum {
    TP_ACTION_FRAGMENT_FADE = 0,
    TP_ACTION_DIE = 1,
    TP_ACTION_TAIL_FOLLOW_HEAD = 2,
    TP_ACTION_HEAD_WAIT = 4,
    TP_ACTION_HEAD_APPROACH_PLAYER = 7,
    TP_ACTION_HEAD_TAKE_OFF = 8,
    TP_ACTION_HEAD_BURROW_RETURN_HOME = 9,
} TailpasaranAction;

typedef enum {
    TP_STATE_RESTING,
    TP_STATE_AIRBORNE,
    TP_STATE_REMOVED,
} TailpasaranState;

typedef enum {
    TP_DMGEFF_NONE = 0,
    TP_DMGEFF_DEKU_NUT = 1,
    TP_DMGEFF_SHOCKING = 14,
    TP_DMGEFF_INSULATING = 15,
} TailpasaranDamageEffect;

static const S2HModApi* sApi;
static s16 sTailId = -1;

struct EnTp;
typedef void (*EnTpActionFunc)(struct EnTp*, PlayState*);

typedef struct EnTp {
    Actor actor;
    s32 actionIndex;
    s32 state;
    EnTpActionFunc actionFunc;
    u8 damageEffect;
    s16 timer;
    s16 wavePhase;
    s16 alpha;
    s16 red;
    s16 kiraSpawnTimer;
    f32 heightPhase;
    f32 horizontalVariation;
    f32 extraHeightVariation;
    ColliderJntSph collider;
    ColliderJntSphElement colliderItems[1];
    struct EnTp* head;
} EnTp;

static ColliderJntSphElementInit sJntSphElementsInit[1] = {
    {
        {
            ELEM_MATERIAL_UNK0,
            { 0xF7CFFFFF, 0x03, 0x08 },
            { 0xF7CFFFFF, 0x01, 0x00 },
            ATELEM_ON | ATELEM_SFX_NORMAL,
            ACELEM_ON,
            OCELEM_NONE,
        },
        { 0, { { 0, 0, 0 }, 4 }, 100 },
    },
};

static ColliderJntSphInit sJntSphInit = {
    {
        COL_MATERIAL_HIT1,
        AT_ON | AT_TYPE_ENEMY,
        AC_ON | AC_TYPE_PLAYER,
        OC1_NONE,
        OC2_TYPE_1,
        COLSHAPE_JNTSPH,
    },
    1,
    sJntSphElementsInit,
};

// Melee shocks the player; the stick, boomerang and sword beam do not.
static DamageTable sDamageTable = {
    /* Deku Nut       */ DMG_ENTRY(0, TP_DMGEFF_DEKU_NUT),
    /* Deku Stick     */ DMG_ENTRY(1, TP_DMGEFF_INSULATING),
    /* Horse trample  */ DMG_ENTRY(0, TP_DMGEFF_NONE),
    /* Explosives     */ DMG_ENTRY(0, TP_DMGEFF_NONE),
    /* Zora boomerang */ DMG_ENTRY(1, TP_DMGEFF_INSULATING),
    /* Normal arrow   */ DMG_ENTRY(0, TP_DMGEFF_NONE),
    /* UNK_DMG_0x06   */ DMG_ENTRY(0, TP_DMGEFF_NONE),
    /* Hookshot       */ DMG_ENTRY(0, TP_DMGEFF_NONE),
    /* Goron punch    */ DMG_ENTRY(1, TP_DMGEFF_SHOCKING),
    /* Sword          */ DMG_ENTRY(1, TP_DMGEFF_SHOCKING),
    /* Goron pound    */ DMG_ENTRY(1, TP_DMGEFF_SHOCKING),
    /* Fire arrow     */ DMG_ENTRY(0, TP_DMGEFF_NONE),
    /* Ice arrow      */ DMG_ENTRY(0, TP_DMGEFF_NONE),
    /* Light arrow    */ DMG_ENTRY(0, TP_DMGEFF_NONE),
    /* Goron spikes   */ DMG_ENTRY(1, TP_DMGEFF_SHOCKING),
    /* Deku spin      */ DMG_ENTRY(1, TP_DMGEFF_SHOCKING),
    /* Deku bubble    */ DMG_ENTRY(0, TP_DMGEFF_NONE),
    /* Deku launch    */ DMG_ENTRY(0, TP_DMGEFF_NONE),
    /* UNK_DMG_0x12   */ DMG_ENTRY(0, TP_DMGEFF_NONE),
    /* Zora barrier   */ DMG_ENTRY(0, TP_DMGEFF_NONE),
    /* Normal shield  */ DMG_ENTRY(0, TP_DMGEFF_NONE),
    /* Light ray      */ DMG_ENTRY(0, TP_DMGEFF_NONE),
    /* Thrown object  */ DMG_ENTRY(0, TP_DMGEFF_NONE),
    /* Zora punch     */ DMG_ENTRY(1, TP_DMGEFF_SHOCKING),
    /* Spin attack    */ DMG_ENTRY(1, TP_DMGEFF_SHOCKING),
    /* Sword beam     */ DMG_ENTRY(1, TP_DMGEFF_INSULATING),
    /* Normal Roll    */ DMG_ENTRY(0, TP_DMGEFF_NONE),
    /* UNK_DMG_0x1B   */ DMG_ENTRY(0, TP_DMGEFF_NONE),
    /* UNK_DMG_0x1C   */ DMG_ENTRY(0, TP_DMGEFF_NONE),
    /* Unblockable    */ DMG_ENTRY(0, TP_DMGEFF_NONE),
    /* UNK_DMG_0x1E   */ DMG_ENTRY(0, TP_DMGEFF_NONE),
    /* Powder Keg     */ DMG_ENTRY(0, TP_DMGEFF_NONE),
};

static void EnTp_Tail_FollowHead(EnTp* this, PlayState* play);
static void EnTp_Head_ApproachPlayer(EnTp* this, PlayState* play);
static void EnTp_Head_TakeOff(EnTp* this, PlayState* play);
static void EnTp_Head_Wait(EnTp* this, PlayState* play);
static void EnTp_Head_BurrowReturnHome(EnTp* this, PlayState* play);
static void EnTp_Die(EnTp* this, PlayState* play);
static void EnTp_Fragment_Fade(EnTp* this, PlayState* play);

static bool EnTp_IsHead(EnTp* this) {
    return this->actor.params <= TP_TYPE_HEAD;
}

static void EnTp_SetAction(EnTp* this, s32 actionIndex, EnTpActionFunc actionFunc) {
    this->actionIndex = actionIndex;
    this->actionFunc = actionFunc;
}

static void EnTp_PlayFlySfx(EnTp* this) {
    Actor_PlaySfx_Flagged(&this->actor, NA_SE_EN_TAIL_FLY - SFX_FLAG);
}

static void EnTp_Tail_FollowHead(EnTp* this, PlayState* play) {
    // The head or the segment ahead is gone without dying (culled, or killed by a soulless spawn): the rest of the
    // tail falls apart, since it follows that segment.
    const bool isDetached = (this->head == NULL) || (this->actor.parent == NULL);
    if (isDetached && (this->actor.params != TP_TYPE_TAIL_DYING)) {
        this->actor.params = TP_TYPE_TAIL_DYING;
        this->actor.colChkInfo.health = 0;
    }
    if (this->actor.params == TP_TYPE_TAIL_DYING) {
        this->actionIndex = TP_ACTION_DIE;
        if (this->actor.parent == NULL) {
            this->timer = 2;
            EnTp_SetAction(this, TP_ACTION_DIE, EnTp_Die);
        }
        return;
    }
    if (this->state != TP_STATE_RESTING) {
        this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;
    }
    if (this->head->state != TP_STATE_RESTING) {
        this->actor.speed = 0.0f;
        this->red = 0;
        this->actor.velocity.y = 0.0f;
        this->heightPhase = 0.0f;
        if (this->actor.world.pos.y < this->head->actor.home.pos.y) {
            this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
        }
        this->actor.world.pos = this->actor.parent->prevPos;
        return;
    }
    Math_SmoothStepToF(&this->actor.world.pos.y, this->actor.parent->world.pos.y - 4.0f, 1.0f, 1.0f, 0.0f);
    s16 angle = this->head->actor.shape.rot.y + 0x4000;
    s16 phase = 2000 * (this->head->wavePhase + this->timer);
    this->actor.world.pos.x =
        this->actor.home.pos.x + Math_SinS(phase) * (Math_SinS(angle) * this->horizontalVariation);
    this->actor.world.pos.z =
        this->actor.home.pos.z + Math_SinS(phase) * (Math_CosS(angle) * this->horizontalVariation);
}

static void EnTp_Head_SetupBurrowReturnHome(EnTp* this) {
    this->timer = 0;
    EnTp_SetAction(this, TP_ACTION_HEAD_BURROW_RETURN_HOME, EnTp_Head_BurrowReturnHome);
}

static void EnTp_Head_ApproachPlayer(EnTp* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    Math_SmoothStepToF(&this->actor.world.pos.y, player->actor.world.pos.y + 30.0f, 1.0f, 0.5f, 0.0f);
    EnTp_PlayFlySfx(this);
    if (this->collider.base.atFlags & AT_HIT) {
        this->collider.base.atFlags &= ~AT_HIT;
        if (&player->actor == this->collider.base.at) {
            this->timer = 1;
        }
    }
    if (this->red < 255) {
        this->red += 15;
    }
    if (Math_CosF(this->heightPhase) == 0.0f) {
        this->extraHeightVariation = 2.0f * Rand_ZeroOne();
    }
    this->actor.world.pos.y += Math_CosF(this->heightPhase) * (2.0f + this->extraHeightVariation);
    this->heightPhase += 0.2f;
    Math_SmoothStepToF(&this->actor.speed, 2.5f, 0.1f, 0.2f, 0.0f);
    this->timer--;
    if (this->timer == 0) {
        EnTp_Head_SetupBurrowReturnHome(this);
        return;
    }
    Math_SmoothStepToS(&this->actor.world.rot.y, this->actor.yawTowardsPlayer, 1, 750, 0);
    this->actor.shape.rot.y = this->actor.world.rot.y;
}

static void EnTp_SetupDie(EnTp* this) {
    this->timer = 2;
    if (EnTp_IsHead(this)) {
        for (Actor* tail = this->actor.child; tail != NULL; tail = tail->child) {
            tail->params = TP_TYPE_TAIL_DYING;
            tail->colChkInfo.health = 0;
        }
        this->timer = 13;
        Actor_PlaySfx(&this->actor, NA_SE_EN_TAIL_DEAD);
    }
    EnTp_SetAction(this, TP_ACTION_DIE, EnTp_Die);
}

static void EnTp_SpawnHeadDeathEffects(EnTp* this, PlayState* play) {
    static Color_RGBA8 sPrimColor = { 255, 255, 255, 255 };
    static Color_RGBA8 sEnvColor = { 0, 0, 255, 0 };
    Vec3f velAccel = { 0.0f, 0.5f, 0.0f };

    for (s32 i = 0; i < 2; i++) {
        Vec3f pos = { ((Rand_ZeroOne() - 0.5f) * 15.0f) + this->actor.world.pos.x,
                      ((Rand_ZeroOne() - 0.5f) * 5.0f) + this->actor.world.pos.y,
                      ((Rand_ZeroOne() - 0.5f) * 15.0f) + this->actor.world.pos.z };
        EffectSsDeadDb_Spawn(play, &pos, &velAccel, &velAccel, &sPrimColor, &sEnvColor, 100, 0, 9);
    }
}

static void EnTp_SpawnFragment(EnTp* this, PlayState* play) {
    EnTp* fragment = (EnTp*)Actor_Spawn(&play->actorCtx, play, sTailId, this->actor.world.pos.x,
                                        this->actor.world.pos.y, this->actor.world.pos.z, 0, 0, 0, TP_TYPE_FRAGMENT);

    if (fragment != NULL) {
        Actor_SetScale(&fragment->actor, this->actor.scale.z * 0.5f);
        fragment->red = this->red;
    }
}

// Each piece breaks in turn, passing the death down the tail.
static void EnTp_Die(EnTp* this, PlayState* play) {
    this->timer--;
    if (this->timer > 0) {
        return;
    }
    if (this->actor.params == TP_TYPE_HEAD_DYING) {
        EnTp_SpawnHeadDeathEffects(this, play);
        Item_DropCollectibleRandom(play, &this->actor, &this->actor.world.pos, 0x50);
    } else {
        EnTp_SpawnFragment(this, play);
    }
    if (this->actor.child != NULL) {
        this->actor.child->parent = NULL;
        this->actor.child->params = TP_TYPE_TAIL_DYING;
        this->actor.child->colChkInfo.health = 0;
    }
    this->state = TP_STATE_REMOVED;
    Actor_Kill(&this->actor);
}

static void EnTp_Fragment_SetupFade(EnTp* this) {
    this->actor.world.pos.x += ((Rand_ZeroOne() - 0.5f) * 5.0f);
    this->actor.world.pos.y += ((Rand_ZeroOne() - 0.5f) * 5.0f);
    this->actor.world.pos.z += ((Rand_ZeroOne() - 0.5f) * 5.0f);
    this->actor.velocity.x = (Rand_ZeroOne() - 0.5f) * 1.5f;
    this->actor.velocity.y = (Rand_ZeroOne() - 0.5f) * 1.5f;
    this->actor.velocity.z = (Rand_ZeroOne() - 0.5f) * 1.5f;
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    EnTp_SetAction(this, TP_ACTION_FRAGMENT_FADE, EnTp_Fragment_Fade);
}

static void EnTp_Fragment_Fade(EnTp* this, PlayState* play) {
    Actor_UpdatePos(&this->actor);
    this->alpha -= 20;
    if (this->alpha < 20) {
        this->alpha = 0;
        Actor_Kill(&this->actor);
    }
}

static void EnTp_Head_SetupTakeOff(EnTp* this) {
    this->timer = (Rand_ZeroOne() * 15.0f) + 40.0f;
    EnTp_SetAction(this, TP_ACTION_HEAD_TAKE_OFF, EnTp_Head_TakeOff);
}

// Flies up and loops around its home until it makes for the player.
static void EnTp_Head_TakeOff(EnTp* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    Math_SmoothStepToF(&this->actor.speed, 2.5f, 0.1f, 0.2f, 0.0f);
    Math_SmoothStepToF(&this->actor.world.pos.y, player->actor.world.pos.y + 85.0f + this->horizontalVariation, 1.0f,
                       this->actor.speed * 0.25f, 0.0f);
    EnTp_PlayFlySfx(this);
    if (this->collider.base.atFlags & AT_HIT) {
        this->collider.base.atFlags &= ~AT_HIT;
        if (&player->actor == this->collider.base.at) {
            this->wavePhase = 1;
        }
    }
    if (this->red != 0) {
        this->red -= 15;
    }
    if (Math_CosF(this->heightPhase) == 0.0f) {
        this->extraHeightVariation = Rand_ZeroOne() * 4.0f;
    }
    this->actor.world.pos.y +=
        Math_CosF(this->heightPhase) * ((this->actor.speed * 0.25f) + this->extraHeightVariation);
    this->heightPhase += 0.2f;
    if (this->timer != 0) {
        this->timer--;
    }
    Math_SmoothStepToS(&this->actor.world.rot.y, Math_Vec3f_Yaw(&this->actor.world.pos, &this->actor.home.pos), 1, 750,
                       0);
    if (this->timer == 0) {
        this->timer = 200;
        EnTp_SetAction(this, TP_ACTION_HEAD_APPROACH_PLAYER, EnTp_Head_ApproachPlayer);
    }
    this->actor.shape.rot.y = this->actor.world.rot.y;
}

static void EnTp_Head_SetupWait(EnTp* this) {
    this->state = TP_STATE_RESTING;
    this->actor.shape.rot.x = -0x4000;
    this->timer = 60;
    this->wavePhase = 0;
    this->actor.speed = 0.0f;
    EnTp_SetAction(this, TP_ACTION_HEAD_WAIT, EnTp_Head_Wait);
}

static void EnTp_Head_WaveAroundHome(EnTp* this, s16 yaw, f32 amplitude) {
    this->actor.world.pos.x =
        this->actor.home.pos.x + (Math_SinS(2000 * this->wavePhase) * (Math_SinS(yaw) * amplitude));
    this->actor.world.pos.z =
        this->actor.home.pos.z + (Math_SinS(2000 * this->wavePhase) * (Math_CosS(yaw) * amplitude));
}

// Rises from the ground while the player is near, then takes off; sinks back when they leave.
static void EnTp_Head_Wait(EnTp* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    this->wavePhase--;
    if (this->actor.xzDistToPlayer >= TP_WAKE_RANGE) {
        Math_SmoothStepToS(&this->actor.shape.rot.x, -0x4000, 1, 500, 0);
        if (Math_SmoothStepToF(&this->actor.world.pos.y, this->actor.home.pos.y, 0.3f, 1.5f, 0.3f) == 0.0f) {
            this->timer = 60;
        } else {
            EnTp_Head_WaveAroundHome(this, Math_Vec3f_Yaw(&this->actor.home.pos, &player->actor.world.pos), 6.0f);
        }
    } else {
        if (this->collider.base.atFlags & AT_HIT) {
            this->collider.base.atFlags &= ~AT_HIT;
            if (&player->actor == this->collider.base.at) {
                this->timer = 0;
            }
        }
        if (this->timer != 0) {
            this->timer--;
            Math_SmoothStepToS(&this->actor.shape.rot.x, 0, 1, 500, 0);
            Math_SmoothStepToS(&this->actor.world.rot.y, this->actor.yawTowardsPlayer, 1, 1500, 0);
            Math_SmoothStepToF(&this->actor.world.pos.y, this->actor.home.pos.y + 30.0f, 0.3f, 1.0f, 0.3f);
            s16 yaw = Math_Vec3f_Yaw(&this->actor.home.pos, &player->actor.world.pos) + 0x4000;
            EnTp_Head_WaveAroundHome(this, yaw, this->horizontalVariation);
        } else {
            this->actor.shape.rot.x = 0;
            this->state = TP_STATE_AIRBORNE;
            EnTp_Head_SetupTakeOff(this);
        }
    }
    this->actor.shape.rot.y = this->actor.world.rot.y;
    if (this->actor.world.pos.y != this->actor.home.pos.y) {
        EnTp_PlayFlySfx(this);
    }
}

static void EnTp_Head_SnapHome(EnTp* this) {
    this->timer = 80;
    this->actor.velocity.y = 0.0f;
    this->actor.speed = 0.0f;
    this->actor.world.pos = this->actor.home.pos;
    this->actor.shape.rot.x = -0x4000;
    for (Actor* tail = this->actor.child; tail != NULL; tail = tail->child) {
        tail->velocity.y = 0.0f;
        tail->speed = 0.0f;
        tail->world.pos = this->actor.home.pos;
        tail->world.pos.y = this->actor.home.pos.y - 80.0f;
    }
}

static void EnTp_Head_SpawnBurrowBubbles(EnTp* this, PlayState* play) {
    static Vec3f sBubbleAccel = { 0.0f, -0.5f, 0.0f };
    static Color_RGBA8 sBubblePrimColor = { 255, 255, 255, 255 };
    static Color_RGBA8 sBubbleEnvColor = { 150, 150, 150, 0 };
    Vec3f pos = { this->actor.world.pos.x, this->actor.floorHeight, this->actor.world.pos.z };
    Vec3f velocity = { Rand_CenteredFloat(5.0f), (Rand_ZeroOne() * 3.5f) + 1.5f, Rand_CenteredFloat(5.0f) };

    EffectSsDtBubble_SpawnCustomColor(play, &pos, &velocity, &sBubbleAccel, &sBubblePrimColor, &sBubbleEnvColor,
                                      Rand_S16Offset(100, 50), 20, 0);
}

// Dives into the floor, then rises at home below ground and waits there again.
static void EnTp_Head_BurrowReturnHome(EnTp* this, PlayState* play) {
    this->wavePhase--;
    if ((this->timer != 0) || ((this->actor.home.pos.y - this->actor.world.pos.y) > 60.0f)) {
        this->timer--;
        if (this->timer == 0) {
            EnTp_Head_SetupWait(this);
            for (Actor* tail = this->actor.child; tail != NULL; tail = tail->child) {
                ((EnTp*)tail)->wavePhase = ((EnTp*)tail)->timer;
            }
            return;
        }
        if (this->actor.shape.rot.x != -0x4000) {
            EnTp_Head_SnapHome(this);
        }
        this->actor.world.pos.y = this->actor.home.pos.y - this->timer;
        return;
    }
    if (this->actor.shape.rot.x != 0x4000) {
        this->actor.shape.rot.x -= 0x400;
    }
    if (this->red != 0) {
        this->red -= 15;
    }
    this->actor.speed = 2.0f * Math_CosS(this->actor.shape.rot.x);
    this->actor.velocity.y = Math_SinS(this->actor.shape.rot.x) * -2.0f;
    if (this->actor.world.pos.y != this->actor.home.pos.y) {
        EnTp_PlayFlySfx(this);
    }
    bool isCloseToFloor = (this->actor.world.pos.y - this->actor.floorHeight) < 20.0f;
    if (isCloseToFloor && ((play->gameplayFrames & 1) != 0)) {
        EnTp_Head_SpawnBurrowBubbles(this, play);
    }
}

static void EnTp_StunPiece(EnTp* piece, bool isHead) {
    piece->actor.freezeTimer = TP_STUN_DURATION;
    Actor_SetColorFilter(&piece->actor, COLORFILTER_COLORFLAG_BLUE, 255,
                         isHead ? COLORFILTER_BUFFLAG_OPA : COLORFILTER_BUFFLAG_XLU, TP_STUN_DURATION);
}

// A Deku Nut freezes the whole chain and absorbs its pending hits. Unlike vanilla, a bounce off the armoured head
// leaves them alone, so one blow over head and tail still lands on the tail.
static void EnTp_StunChain(EnTp* this, bool isHead) {
    EnTp_StunPiece(this, isHead);
    Actor_PlaySfx(&this->actor, TP_SFX_FREEZE);
    for (EnTp* piece = (EnTp*)this->actor.parent; piece != NULL; piece = (EnTp*)piece->actor.parent) {
        piece->collider.base.acFlags &= ~AC_HIT;
        EnTp_StunPiece(piece, isHead);
        Actor_PlaySfx(&this->actor, TP_SFX_FREEZE);
    }
    for (EnTp* piece = (EnTp*)this->actor.child; piece != NULL; piece = (EnTp*)piece->actor.child) {
        piece->collider.base.acFlags &= ~AC_HIT;
        EnTp_StunPiece(piece, isHead);
    }
}

static void EnTp_UpdateDamage(EnTp* this, PlayState* play) {
    if (!(this->collider.base.acFlags & AC_HIT) || (this->actionIndex < TP_ACTION_TAIL_FOLLOW_HEAD)) {
        return;
    }
    bool isHead = EnTp_IsHead(this);
    this->collider.base.acFlags &= ~AC_HIT;
    Actor_SetDropFlagJntSph(&this->actor, &this->collider);
    this->damageEffect = this->actor.colChkInfo.damageEffect;
    if (this->actor.colChkInfo.damageEffect == TP_DMGEFF_NONE) {
        return;
    }
    if (!isHead) {
        Actor_ApplyDamage(&this->actor);
    }
    if (this->actor.colChkInfo.health != 0) {
        if (this->actor.colChkInfo.damageEffect == TP_DMGEFF_DEKU_NUT) {
            EnTp_StunChain(this, isHead);
        }
        return;
    }
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    EnTp* head = this->head;
    if ((head != NULL) && EnTp_IsHead(head)) {
        Enemy_StartFinishingBlow(play, &head->actor);
        EnTp_SetupDie(head);
        head->damageEffect = this->actor.colChkInfo.damageEffect;
        head->actor.params = TP_TYPE_HEAD_DYING;
    }
}

static void EnTp_UpdateHeadMovement(EnTp* this, PlayState* play) {
    Actor_MoveWithGravity(&this->actor);
    if (this->actionIndex != TP_ACTION_HEAD_BURROW_RETURN_HOME) {
        Actor_UpdateBgCheckInfo(play, &this->actor, 0.0f, 15.0f, 10.0f, TP_BGCHECK_FLAGS);
    }
    if ((this->actor.speed != 0.0f) && (this->actor.bgCheckFlags & BGCHECKFLAG_WALL)) {
        s16 yawToWall = this->actor.wallYaw - this->actor.world.rot.y;
        if (ABS(yawToWall) > 0x4000) {
            this->actor.world.rot.y += (yawToWall >= 0) ? -500 : 500;
            this->actor.shape.rot.y = this->actor.world.rot.y;
        }
    }
    this->actor.shape.rot.z += 0x800;
    if (this->actor.shape.rot.z == 0) {
        Actor_PlaySfx(&this->actor, NA_SE_EN_TAIL_CRY);
    }
    if (this->actionIndex >= TP_ACTION_TAIL_FOLLOW_HEAD) {
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
    }
}

static void EnTp_SpawnSpark(EnTp* this, PlayState* play) {
    Vec3f velocity = { 0.0f, 0.0f, 0.0f };
    Vec3f accel = { -this->actor.velocity.x * 0.25f, -this->actor.velocity.y * 0.25f, -this->actor.velocity.z * 0.25f };
    Vec3f pos = { ((Rand_ZeroOne() - 0.5f) * 25.0f) + this->actor.world.pos.x,
                  ((Rand_ZeroOne() - 0.5f) * 20.0f) + this->actor.world.pos.y,
                  ((Rand_ZeroOne() - 0.5f) * 25.0f) + this->actor.world.pos.z };
    Color_RGBA8 primColor = { this->red, 0, 255, 255 };
    Color_RGBA8 envColor = { 0, 0, 0, 0 };

    EffectSsKirakira_SpawnSmall(play, &pos, &velocity, &accel, &primColor, &envColor);
}

// Culled pieces are not drawn, so the sphere follows the body here with Draw's transform instead.
static void EnTp_UpdateColliderSphere(EnTp* this) {
    bool hasSphere = (this->actor.params <= TP_TYPE_TAIL) || (this->actor.params == TP_TYPE_TAIL_DYING);
    if (!hasSphere) {
        return;
    }
    Matrix_Push();
    Matrix_SetTranslateRotateYXZ(this->actor.world.pos.x, this->actor.world.pos.y, this->actor.world.pos.z,
                                 &this->actor.shape.rot);
    Matrix_Scale(this->actor.scale.x, this->actor.scale.y, this->actor.scale.z, MTXMODE_APPLY);
    if (EnTp_IsHead(this)) {
        Matrix_Translate(0.0f, 0.0f, 8.0f, MTXMODE_APPLY);
    }
    Collider_UpdateSpheres(0, &this->collider);
    Matrix_Pop();
}

static void EnTp_Update(Actor* thisx, PlayState* play) {
    EnTp* this = (EnTp*)thisx;

    if (GET_PLAYER(play)->stateFlags1 & TP_PLAYER_STATE1_DAMAGED) {
        this->damageEffect = TP_DMGEFF_NONE;
    }
    if (this->actor.colChkInfo.health != 0) {
        EnTp_UpdateDamage(this, play);
    }
    this->actionFunc(this, play);
    if (EnTp_IsHead(this)) {
        EnTp_UpdateHeadMovement(this, play);
    }
    EnTp_UpdateColliderSphere(this);
    if (this->actor.params != TP_TYPE_TAIL_DYING) {
        this->kiraSpawnTimer = (this->kiraSpawnTimer - 1) & 7;
    }
    this->actor.focus.pos = this->actor.world.pos;
    // A melee blow leaves the struck piece live: touching it shocks the player.
    if (this->damageEffect == TP_DMGEFF_SHOCKING) {
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
    }
    if ((this->kiraSpawnTimer & 7) == 0) {
        EnTp_SpawnSpark(this, play);
    }
    if ((this->actionIndex >= TP_ACTION_TAIL_FOLLOW_HEAD) && (this->actor.colChkInfo.health != 0)) {
        CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
    }
}

static void EnTp_SpawnTail(EnTp* this, PlayState* play) {
    EnTp* previous = this;

    for (s32 i = 0; i < TP_TAIL_SEGMENTS; i++) {
        EnTp* segment = (EnTp*)Actor_Spawn(&play->actorCtx, play, sTailId, this->actor.world.pos.x,
                                           this->actor.world.pos.y, this->actor.world.pos.z, 0, 0, 0, TP_TYPE_TAIL);
        if (segment == NULL) {
            continue;
        }
        previous->actor.child = &segment->actor;
        segment->actor.parent = &previous->actor;
        segment->kiraSpawnTimer = i + 1;
        segment->head = this;
        Actor_SetScale(&segment->actor, 0.3f);
        if (i == TP_TARGETABLE_SEGMENT) {
            segment->actor.flags |= TP_HEAD_FLAGS;
            segment->state = TP_STATE_AIRBORNE;
        }
        segment->timer = i * -5;
        segment->wavePhase = i * -5;
        segment->horizontalVariation = 6.0f - (i * 0.75f);
        previous = segment;
    }
}

static void EnTp_Init(Actor* thisx, PlayState* play) {
    EnTp* this = (EnTp*)thisx;

    this->actor.lockOnArrowOffset = 10.0f;
    this->actor.attentionRangeType = ATTENTION_RANGE_3;
    this->actor.colChkInfo.damageTable = &sDamageTable;
    ActorShape_Init(&this->actor.shape, 0.0f, ActorShadow_DrawCircle, 0.14f);
    this->state = TP_STATE_RESTING;
    this->actor.colChkInfo.health = 1;
    this->alpha = 255;
    Collider_InitJntSph(play, &this->collider);
    Collider_SetJntSph(play, &this->collider, &this->actor, &sJntSphInit, this->colliderItems);
    if (EnTp_IsHead(this)) {
        this->timer = 0;
        this->collider.base.acFlags |= AC_HARD;
        this->collider.elements->dim.modelSphere.radius = 8;
        this->collider.elements->dim.worldSphere.radius = 8;
        EnTp_Head_SetupWait(this);
        this->actor.focus.pos = this->actor.world.pos;
        Actor_SetScale(&this->actor, 1.5f);
        EnTp_SpawnTail(this, play);
    } else if (this->actor.params == TP_TYPE_TAIL) {
        EnTp_SetAction(this, TP_ACTION_TAIL_FOLLOW_HEAD, EnTp_Tail_FollowHead);
    } else {
        EnTp_Fragment_SetupFade(this);
    }
}

// The chain holds raw pointers both ways; unhook this piece so no neighbour reads freed memory.
static void EnTp_Destroy(Actor* thisx, PlayState* play) {
    EnTp* this = (EnTp*)thisx;

    if ((this->actor.parent != NULL) && (this->actor.parent->child == &this->actor)) {
        this->actor.parent->child = NULL;
    }
    if ((this->actor.child != NULL) && (this->actor.child->parent == &this->actor)) {
        this->actor.child->parent = NULL;
    }
    if (EnTp_IsHead(this) || (this->actor.params == TP_TYPE_HEAD_DYING)) {
        for (Actor* tail = this->actor.child; tail != NULL; tail = tail->child) {
            ((EnTp*)tail)->head = NULL;
        }
    }
    Collider_DestroyJntSph(play, &this->collider);
}

static void EnTp_DrawHead(PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sHeadDL);
    Matrix_Translate(0.0f, 0.0f, 8.0f, MTXMODE_APPLY);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void EnTp_DrawTailSegment(EnTp* this, PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, this->red, 0, 255, this->alpha);
    gDPPipeSync(POLY_XLU_DISP++);
    gDPSetCombineLERP(POLY_XLU_DISP++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, PRIMITIVE, ENVIRONMENT, TEXEL0,
                      ENVIRONMENT, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, PRIMITIVE, ENVIRONMENT, TEXEL0,
                      ENVIRONMENT);
    gDPPipeSync(POLY_XLU_DISP++);
    gSPSegment(POLY_XLU_DISP++, 0x08, (uintptr_t)sTailSegmentTex);
    gDPPipeSync(POLY_XLU_DISP++);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sTailSegmentDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void EnTp_Draw(Actor* thisx, PlayState* play) {
    EnTp* this = (EnTp*)thisx;

    if (this->state != TP_STATE_REMOVED) {
        if (EnTp_IsHead(this) || (this->actor.params == TP_TYPE_HEAD_DYING)) {
            EnTp_DrawHead(play);
        } else {
            EnTp_DrawTailSegment(this, play);
        }
    }
}

// Ship has no kill rule for it; a sword, a stick, Zora's boomerang or a Goron reaches the tail.
static bool CanKillTailpasaran(void* userData) {
    return sApi->LogicHasCapability("CanUseSword") || sApi->LogicCanUseItem(RI_DEKU_STICK) ||
           sApi->LogicHasCapability("CanBeZora") || sApi->LogicHasCapability("CanBeGoron");
}

static S2HActorDefinition DefineTailpasaranPiece(const char* key, const char* description, u32 flags) {
    S2HActorDefinition definition = { 0 };

    definition.structSize = sizeof(definition);
    definition.key = key;
    definition.description = description;
    definition.category = ACTORCAT_ENEMY;
    definition.actorFlags = flags;
    definition.objectId = GAMEPLAY_KEEP;
    definition.instanceSize = sizeof(EnTp);
    definition.init = EnTp_Init;
    definition.destroy = EnTp_Destroy;
    definition.update = EnTp_Update;
    definition.draw = EnTp_Draw;
    return definition;
}

static s16 RegisterTail(void) {
    // Unlike vanilla the tail keeps following a culled head, so its vulnerable segments stay hittable.
    S2HActorDefinition definition =
        DefineTailpasaranPiece(TAILPASARAN_TAIL_KEY, "Tailpasaran tail", ACTOR_FLAG_UPDATE_CULLING_DISABLED);

    return sApi->RegisterActor(&definition);
}

static s16 RegisterTailpasaran(void) {
    S2HActorDefinition definition = DefineTailpasaranPiece(TAILPASARAN_KEY, "Tailpasaran", TP_HEAD_FLAGS);

    definition.enemyFlags = S2H_ACTOR_ENEMY | S2H_ACTOR_ENEMY_NOT_IN_WATER;
    definition.enemyParams = TP_TYPE_HEAD;
    definition.naviHint = "Tailpasaran&Its head is armoured; hit the tail. A sword&gets shocked, so use a stick!";
    // Ship's catalog gives it no soul.
    definition.soulKey = NULL;
    definition.logicKill = CanKillTailpasaran;
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
    if (!RequireOotAsset(TP_OBJECT "gTailpasaranHeadDL")) {
        sApi->Log(TAILPASARAN_KEY ": OoT Asset Loader resources are missing; extract them to enable this enemy.");
        return;
    }
    sTailId = RegisterTail();
    if (sTailId < 0) {
        sApi->Log("Could not register " TAILPASARAN_TAIL_KEY);
        return;
    }
    if (RegisterTailpasaran() < 0) {
        sApi->Log("Could not register " TAILPASARAN_KEY);
    }
}
