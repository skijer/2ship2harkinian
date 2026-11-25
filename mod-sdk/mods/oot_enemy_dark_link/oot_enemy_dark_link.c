/*
 * Ocarina of Time's Dark Link (En_Torch2) on Majora's Mask, drawn as adult Link from the OoT Asset Loader's oot.o2r.
 * Vanilla drives a second Player with fake input; this actor plays the same reactions itself and needs no mirror
 * room: it fades in, shadows Link, and sinks into the floor when struck to rise again behind him.
 */
#include "2s2h/ModApi/ModApi.h"

#include <stdlib.h>
#include <string.h>

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "objects/gameplay_keep/gameplay_keep.h"

#define DARK_LINK_KEY "oot.dark_link"
#define DARK_LINK_SOUL_KEY "soul.oot.dark_link"
#define LINK_BOY_OBJECT "objects/oot_object_link_boy/"
#define ADULT_ASSET(name) "__OTR__" LINK_BOY_OBJECT name
#define PLAYER_ANIM_OBJECT "objects/oot_gameplay_keep/"
#define PLAYER_ANIM(name) "__OTR__" PLAYER_ANIM_OBJECT "gPlayerAnim_" name

static const ALIGN_ASSET(2) char sSkel[] = ADULT_ASSET("gLinkAdultSkel");
static const ALIGN_ASSET(2) char sSwordHandDL[] = ADULT_ASSET("gLinkAdultLeftHandHoldingMasterSwordNearDL");
static const ALIGN_ASSET(2) char sShieldHandDL[] = ADULT_ASSET("gLinkAdultRightHandHoldingHylianShieldNearDL");
static const ALIGN_ASSET(2) char sSheathDL[] = ADULT_ASSET("gLinkAdultSheathNearDL");
static const ALIGN_ASSET(2) char sEyeOpenTex[] = ADULT_ASSET("gLinkAdultEyesOpenTex");
static const ALIGN_ASSET(2) char sEyeHalfTex[] = ADULT_ASSET("gLinkAdultEyesHalfTex");
static const ALIGN_ASSET(2) char sEyeClosedTex[] = ADULT_ASSET("gLinkAdultEyesClosedfTex");
static const ALIGN_ASSET(2) char sMouthTex[] = ADULT_ASSET("gLinkAdultMouth1Tex");

static const ALIGN_ASSET(2) char sFightAnim[] = PLAYER_ANIM("link_anchor_waitR");
static const ALIGN_ASSET(2) char sGuardAnim[] = PLAYER_ANIM("link_anchor_waitR_defense_wait");
static const ALIGN_ASSET(2) char sRunAnim[] = PLAYER_ANIM("link_fighter_run");
static const ALIGN_ASSET(2) char sBackWalkAnim[] = PLAYER_ANIM("link_anchor_back_walk");
static const ALIGN_ASSET(2) char sSideWalkRightAnim[] = PLAYER_ANIM("link_anchor_side_walkR");
static const ALIGN_ASSET(2) char sSideWalkLeftAnim[] = PLAYER_ANIM("link_anchor_side_walkL");
static const ALIGN_ASSET(2) char sSlashAnim[] = PLAYER_ANIM("link_fighter_normal_kiru");
static const ALIGN_ASSET(2) char sSlashEndAnim[] = PLAYER_ANIM("link_fighter_normal_kiru_end");
static const ALIGN_ASSET(2) char sRightSlashAnim[] = PLAYER_ANIM("link_fighter_Rside_kiru");
static const ALIGN_ASSET(2) char sRightSlashEndAnim[] = PLAYER_ANIM("link_fighter_Rside_kiru_end");
static const ALIGN_ASSET(2) char sLeftSlashAnim[] = PLAYER_ANIM("link_fighter_Lside_kiru");
static const ALIGN_ASSET(2) char sLeftSlashEndAnim[] = PLAYER_ANIM("link_fighter_Lside_kiru_end");
static const ALIGN_ASSET(2) char sJumpslashAnim[] = PLAYER_ANIM("link_fighter_jump_kiru");
static const ALIGN_ASSET(2) char sJumpslashLandAnim[] = PLAYER_ANIM("link_fighter_jump_kiru_finsh");
static const ALIGN_ASSET(2) char sJumpslashEndAnim[] = PLAYER_ANIM("link_fighter_jump_kiru_finsh_end");
static const ALIGN_ASSET(2) char sSideJumpRightAnim[] = PLAYER_ANIM("link_fighter_Rside_jump");
static const ALIGN_ASSET(2) char sSideJumpRightEndAnim[] = PLAYER_ANIM("link_fighter_Rside_jump_endR");
static const ALIGN_ASSET(2) char sSideJumpLeftAnim[] = PLAYER_ANIM("link_fighter_Lside_jump");
static const ALIGN_ASSET(2) char sSideJumpLeftEndAnim[] = PLAYER_ANIM("link_fighter_Lside_jump_endL");
static const ALIGN_ASSET(2) char sBackflipAnim[] = PLAYER_ANIM("link_fighter_backturn_jump");
static const ALIGN_ASSET(2) char sBackflipEndAnim[] = PLAYER_ANIM("link_fighter_backturn_jump_endR");
static const ALIGN_ASSET(2) char sRollAnim[] = PLAYER_ANIM("link_normal_landing_roll");
static const ALIGN_ASSET(2) char sBladeJumpAnim[] = PLAYER_ANIM("link_normal_jump");
static const ALIGN_ASSET(2) char sReboundAnim[] = PLAYER_ANIM("link_fighter_rebound");
static const ALIGN_ASSET(2) char sHitAnim[] = PLAYER_ANIM("link_anchor_front_hitR");
static const ALIGN_ASSET(2) char sDownAnim[] = PLAYER_ANIM("link_normal_back_downA");

static const char* const sEyeTextures[] = { sEyeOpenTex, sEyeHalfTex, sEyeClosedTex };

static Gfx sEmptyDL[] = {
    gsSPEndDisplayList(),
};

typedef enum {
    DARK_LINK_EYES_OPEN,
    DARK_LINK_EYES_HALF,
    DARK_LINK_EYES_CLOSED,
} DarkLinkEyes;

typedef enum {
    DARK_LINK_DMGEFF_NONE = 0x0,
    DARK_LINK_DMGEFF_STUN = 0x1,
} DarkLinkDamageEffect;

typedef enum {
    SLASH_FORWARD,
    SLASH_RIGHT,
    SLASH_LEFT,
} SlashKind;

typedef struct {
    const char* swing;
    const char* recover;
    s16 sideYaw;
} SlashMove;

static const SlashMove sSlashMoves[] = {
    { sSlashAnim, sSlashEndAnim, 0 },
    { sRightSlashAnim, sRightSlashEndAnim, -0x4000 },
    { sLeftSlashAnim, sLeftSlashEndAnim, 0x4000 },
};

#define DARK_LINK_FLAGS                                                                       \
    (ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE | ACTOR_FLAG_UPDATE_CULLING_DISABLED | \
     ACTOR_FLAG_DRAW_CULLING_DISABLED)
#define DARK_LINK_BGCHECK_FLAGS \
    (UPDBGCHECKINFO_FLAG_1 | UPDBGCHECKINFO_FLAG_4 | UPDBGCHECKINFO_FLAG_8 | UPDBGCHECKINFO_FLAG_10)
#define DARK_LINK_DODGED_PROJECTILES \
    (DMG_NORMAL_ARROW | DMG_FIRE_ARROW | DMG_ICE_ARROW | DMG_LIGHT_ARROW | DMG_HOOKSHOT)
#define DARK_LINK_START_ALPHA 95
#define DARK_LINK_DEATH_FADE 13
#define DARK_LINK_SWORD_DAMAGE 8
#define DARK_LINK_JUMPSLASH_DAMAGE 16
#define DARK_LINK_WAKE_DISTANCE 120.0f
#define DARK_LINK_REACT_DISTANCE 120.0f
#define DARK_LINK_SINK_DEPTH 160.0f
#define DARK_LINK_REAPPEAR_DISTANCE 120.0f
#define DARK_LINK_LEASH 800.0f
#define DARK_LINK_BLADE_HEIGHT 2630.0f
#define DARK_LINK_BLADE_FRAMES 27
#define DARK_LINK_LOW_PLAYER_HEALTH 0x50
#define DARK_LINK_GUARD_HOLD_FRAMES 8
#define DARK_LINK_STAGGER_FRAMES 30
#define DARK_LINK_STUN_FRAMES 80
#define DARK_LINK_MAX_AIR_FRAMES 60
#define DARK_LINK_RUN_SPEED 6.0f

#define FADING_DL_CACHE_SIZE 32
#define FADING_DL_MAX_COMMANDS 2048
#define RENDER_MODE_COMMAND 0xE200001C
#define CYCLE1_BLENDER_MASK 0xCCCC0000
#define ALPHA_CYCLE2_MASK ((7 << 21) | (7 << 18) | (7 << 3) | 7)

extern Gfx* ResourceMgr_LoadGfxByName(const char* path);
extern int ResourceMgr_OTRSigCheck(char* imgData);

static const S2HModApi* sApi;
static s32 sBgmUsers = 0;

struct EnTorch2;
typedef void (*EnTorch2ActionFunc)(struct EnTorch2*, PlayState*);

typedef struct EnTorch2 {
    Actor actor;
    SkelAnime skelAnime;
    u8 jointTableBuffer[PLAYER_LIMB_BUF_SIZE];
    u8 morphTableBuffer[PLAYER_LIMB_BUF_SIZE];
    EnTorch2ActionFunc actionFunc;
    ColliderCylinder bodyCollider;
    ColliderCylinder shieldCollider;
    ColliderQuad swordCollider;
    Vec3f spawnPos;
    const char* landAnim;
    f32 bladeHeight;
    f32 sinkFloorY;
    s32 effectIndex;
    s16 timer;
    s16 invincibilityTimer;
    s16 staggerTimer;
    s16 jumpslashCooldown;
    s16 blinkTimer;
    s8 meleeWeaponState;
    s8 lastClashAnimation;
    u8 alpha;
    u8 clashCount;
    u8 slashKind;
    u8 eyeIndex;
    u8 hasLanded;
    u8 hasSwungOnBlade;
    u8 isCountering;
    u8 isGuarding;
    u8 isJumpslashCounter;
    u8 isHoldingPlayer;
    u8 isBgmUser;
} EnTorch2;

typedef struct {
    const void* source;
    Gfx* copy;
} FadingDL;

static FadingDL sFadingDLs[FADING_DL_CACHE_SIZE];
static u32 sFadingDLCount = 0;

static ColliderCylinderInit sBodyColliderInit = {
    {
        COL_MATERIAL_HIT5,
        AT_NONE,
        AC_ON | AC_TYPE_PLAYER,
        OC1_ON | OC1_TYPE_ALL,
        OC2_TYPE_1,
        COLSHAPE_CYLINDER,
    },
    {
        ELEM_MATERIAL_UNK1,
        { 0x00000000, 0x00, 0x00 },
        { 0xF7CFFFFF, 0x00, 0x00 },
        ATELEM_NONE,
        ACELEM_ON,
        OCELEM_ON,
    },
    { 18, 60, 0, { 0, 0, 0 } },
};

// The shield never stops fire: Din's Fire and fire arrows burn Dark Link through his guard, as in vanilla.
static ColliderCylinderInit sShieldColliderInit = {
    {
        COL_MATERIAL_METAL,
        AT_NONE,
        AC_ON | AC_HARD | AC_TYPE_PLAYER,
        OC1_NONE,
        OC2_NONE,
        COLSHAPE_CYLINDER,
    },
    {
        ELEM_MATERIAL_UNK0,
        { 0x00000000, 0x00, 0x00 },
        { 0xF7C1FFFF & ~DMG_FIRE_ARROW, 0x00, 0x00 },
        ATELEM_NONE,
        ACELEM_ON,
        OCELEM_NONE,
    },
    { 20, 55, 0, { 0, 0, 0 } },
};

static ColliderQuadInit sSwordColliderInit = {
    {
        COL_MATERIAL_METAL,
        AT_ON | AT_TYPE_ENEMY,
        AC_ON | AC_HARD | AC_TYPE_PLAYER,
        OC1_NONE,
        OC2_NONE,
        COLSHAPE_QUAD,
    },
    {
        ELEM_MATERIAL_UNK0,
        { 0xF7CFFFFF, 0x00, DARK_LINK_SWORD_DAMAGE },
        { 0xF7CFFFFF, 0x00, 0x00 },
        ATELEM_ON | ATELEM_SFX_NORMAL,
        ACELEM_ON,
        OCELEM_NONE,
    },
    { { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } } },
};

static DamageTable sDamageTable = {
    /* Deku Nut       */ DMG_ENTRY(0, DARK_LINK_DMGEFF_STUN),
    /* Deku Stick     */ DMG_ENTRY(2, DARK_LINK_DMGEFF_NONE),
    /* Horse trample  */ DMG_ENTRY(0, DARK_LINK_DMGEFF_NONE),
    /* Explosives     */ DMG_ENTRY(2, DARK_LINK_DMGEFF_NONE),
    /* Zora boomerang */ DMG_ENTRY(0, DARK_LINK_DMGEFF_STUN),
    /* Normal arrow   */ DMG_ENTRY(2, DARK_LINK_DMGEFF_NONE),
    /* UNK_DMG_0x06   */ DMG_ENTRY(0, DARK_LINK_DMGEFF_NONE),
    /* Hookshot       */ DMG_ENTRY(0, DARK_LINK_DMGEFF_STUN),
    /* Goron punch    */ DMG_ENTRY(2, DARK_LINK_DMGEFF_NONE),
    /* Sword          */ DMG_ENTRY(1, DARK_LINK_DMGEFF_NONE),
    /* Goron pound    */ DMG_ENTRY(2, DARK_LINK_DMGEFF_NONE),
    /* Fire arrow     */ DMG_ENTRY(2, DARK_LINK_DMGEFF_NONE),
    /* Ice arrow      */ DMG_ENTRY(2, DARK_LINK_DMGEFF_NONE),
    /* Light arrow    */ DMG_ENTRY(2, DARK_LINK_DMGEFF_NONE),
    /* Goron spikes   */ DMG_ENTRY(1, DARK_LINK_DMGEFF_NONE),
    /* Deku spin      */ DMG_ENTRY(0, DARK_LINK_DMGEFF_STUN),
    /* Deku bubble    */ DMG_ENTRY(1, DARK_LINK_DMGEFF_NONE),
    /* Deku launch    */ DMG_ENTRY(1, DARK_LINK_DMGEFF_NONE),
    /* UNK_DMG_0x12   */ DMG_ENTRY(0, DARK_LINK_DMGEFF_NONE),
    /* Zora barrier   */ DMG_ENTRY(0, DARK_LINK_DMGEFF_STUN),
    /* Normal shield  */ DMG_ENTRY(0, DARK_LINK_DMGEFF_NONE),
    /* Light ray      */ DMG_ENTRY(0, DARK_LINK_DMGEFF_NONE),
    /* Thrown object  */ DMG_ENTRY(1, DARK_LINK_DMGEFF_NONE),
    /* Zora punch     */ DMG_ENTRY(1, DARK_LINK_DMGEFF_NONE),
    /* Spin attack    */ DMG_ENTRY(1, DARK_LINK_DMGEFF_NONE),
    /* Sword beam     */ DMG_ENTRY(1, DARK_LINK_DMGEFF_NONE),
    /* Normal Roll    */ DMG_ENTRY(0, DARK_LINK_DMGEFF_NONE),
    /* UNK_DMG_0x1B   */ DMG_ENTRY(0, DARK_LINK_DMGEFF_NONE),
    /* UNK_DMG_0x1C   */ DMG_ENTRY(0, DARK_LINK_DMGEFF_NONE),
    /* Unblockable    */ DMG_ENTRY(0, DARK_LINK_DMGEFF_NONE),
    /* UNK_DMG_0x1E   */ DMG_ENTRY(0, DARK_LINK_DMGEFF_NONE),
    /* Powder Keg     */ DMG_ENTRY(2, DARK_LINK_DMGEFF_NONE),
};

static InitChainEntry sInitChain[] = {
    ICHAIN_F32(lockOnArrowOffset, 500, ICHAIN_CONTINUE),
    ICHAIN_VEC3F_DIV1000(scale, 10, ICHAIN_CONTINUE),
    ICHAIN_F32_DIV1000(gravity, -1200, ICHAIN_STOP),
};

static void EnTorch2_Wait(struct EnTorch2* this, PlayState* play);
static void EnTorch2_Fight(struct EnTorch2* this, PlayState* play);
static void EnTorch2_Stunned(struct EnTorch2* this, PlayState* play);
static void EnTorch2_BladeJump(struct EnTorch2* this, PlayState* play);
static void EnTorch2_Sink(struct EnTorch2* this, PlayState* play);
static void EnTorch2_Die(struct EnTorch2* this, PlayState* play);
static void EnTorch2_SetupFight(EnTorch2* this, PlayState* play);
static void EnTorch2_SetupBackflip(EnTorch2* this, PlayState* play);
static void EnTorch2_SetupRoll(EnTorch2* this, PlayState* play);
static void EnTorch2_SetupJumpslash(EnTorch2* this, PlayState* play);
static void EnTorch2_SetupRecoil(EnTorch2* this, PlayState* play, bool isStagger);

static bool IsTwoWordCommand(u8 opcode) {
    return opcode == 0x20 || opcode == 0x24 || opcode == 0x25 || opcode == 0x27 || opcode == 0x31 || opcode == 0x32 ||
           opcode == 0x33 || opcode == 0x35 || opcode == 0x36 || opcode == 0x42;
}

static u8 GetOpcode(const Gfx* command) {
    return (u8)(command->words.w0 >> 24);
}

static u32 CountCommands(const Gfx* dList) {
    u32 count = 0;

    while (count < FADING_DL_MAX_COMMANDS) {
        u8 opcode = GetOpcode(&dList[count]);

        count += IsTwoWordCommand(opcode) ? 2 : 1;
        if (opcode == G_ENDDL) {
            return count;
        }
    }
    return 0;
}

// OoT's adult Link sets an opaque render mode and a texel-only alpha inline; the copy blends by the env alpha.
static void MakeCommandFade(Gfx* command) {
    if (GetOpcode(command) == G_SETCOMBINE) {
        uintptr_t envAlpha = (G_ACMUX_COMBINED << 21) | (G_ACMUX_ENVIRONMENT << 18) | (G_ACMUX_0 << 3) | G_ACMUX_0;
        command->words.w1 = (command->words.w1 & ~(uintptr_t)ALPHA_CYCLE2_MASK) | envAlpha;
        return;
    }
    if ((u32)command->words.w0 == RENDER_MODE_COMMAND) {
        command->words.w1 = (command->words.w1 & CYCLE1_BLENDER_MASK) | G_RM_AA_ZB_XLU_SURF2;
    }
}

static Gfx* BuildFadingDL(const void* source) {
    if (!ResourceMgr_OTRSigCheck((char*)source)) {
        return NULL;
    }
    Gfx* original = ResourceMgr_LoadGfxByName((const char*)source);
    if (original == NULL) {
        return NULL;
    }
    u32 count = CountCommands(original);
    Gfx* copy = (count != 0) ? malloc(count * sizeof(Gfx)) : NULL;
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, original, count * sizeof(Gfx));
    for (u32 i = 0; i < count; i++) {
        MakeCommandFade(&copy[i]);
        if (IsTwoWordCommand(GetOpcode(&copy[i]))) {
            i++;
        }
    }
    return copy;
}

static Gfx* GetFadingDL(const void* source) {
    for (u32 i = 0; i < sFadingDLCount; i++) {
        if (sFadingDLs[i].source == source) {
            return sFadingDLs[i].copy;
        }
    }
    if (sFadingDLCount == FADING_DL_CACHE_SIZE) {
        return NULL;
    }
    Gfx* copy = BuildFadingDL(source);
    sFadingDLs[sFadingDLCount].source = source;
    sFadingDLs[sFadingDLCount].copy = copy;
    sFadingDLCount++;
    return copy;
}

static s32 AbsYawDiff(s16 a, s16 b) {
    s16 diff = a - b;

    return ABS((s32)diff);
}

static bool IsPlayerShielding(Player* player) {
    return (player->stateFlags1 & PLAYER_STATE1_400000) != 0;
}

static bool IsPlayerSpinning(Player* player) {
    return (player->meleeWeaponState != 0) && (player->meleeWeaponAnimation >= PLAYER_MWA_SPIN_ATTACK_1H) &&
           (player->meleeWeaponAnimation <= PLAYER_MWA_BIG_SPIN_2H);
}

static bool IsPlayerJumpslashing(Player* player) {
    bool isAirborne = !(player->actor.bgCheckFlags & BGCHECKFLAG_GROUND);

    return (player->meleeWeaponAnimation == PLAYER_MWA_JUMPSLASH_START) && isAirborne &&
           ((player->meleeWeaponState != 0) || (player->actor.velocity.y > -3.0f));
}

static bool IsPlayerCastingMagic(Player* player) {
    return (gSaveContext.magicState == MAGIC_STATE_METER_FLASH_1) && !IsPlayerSpinning(player);
}

static bool IsPlayerLowOnHealth(void) {
    return gSaveContext.save.saveInfo.playerData.health < DARK_LINK_LOW_PLAYER_HEALTH;
}

static void EnTorch2_StartBgm(EnTorch2* this) {
    if (this->isBgmUser) {
        return;
    }
    if (sBgmUsers == 0) {
        Audio_PlayBgm_StorePrevBgm(NA_BGM_MINI_BOSS);
    }
    sBgmUsers++;
    this->isBgmUser = true;
}

static void EnTorch2_StopBgm(EnTorch2* this) {
    if (!this->isBgmUser) {
        return;
    }
    this->isBgmUser = false;
    sBgmUsers--;
    if (sBgmUsers == 0) {
        Audio_RestorePrevBgm();
    }
}

// Releases Link from the pause the blade jump put on his action.
static void EnTorch2_ReleasePlayer(EnTorch2* this, PlayState* play) {
    if (!this->isHoldingPlayer) {
        return;
    }
    this->isHoldingPlayer = false;
    GET_PLAYER(play)->stateFlags3 &= ~PLAYER_STATE3_4;
}

static void EnTorch2_PlayAnim(EnTorch2* this, PlayState* play, const char* anim, u8 mode, f32 playSpeed,
                              f32 morphFrames) {
    PlayerAnimation_Change(play, &this->skelAnime, (PlayerAnimationHeader*)anim, playSpeed, 0.0f,
                           Animation_GetLastFrame((void*)anim), mode, morphFrames);
}

static void EnTorch2_LoopAnim(EnTorch2* this, PlayState* play, const char* anim) {
    if ((this->skelAnime.animation == (void*)anim) && (this->skelAnime.playSpeed != 0.0f)) {
        return;
    }
    EnTorch2_PlayAnim(this, play, anim, ANIMMODE_LOOP, 1.0f, -6.0f);
}

static void EnTorch2_FacePlayer(EnTorch2* this) {
    Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 1, 0x2000, 0x100);
}

static void EnTorch2_TurnToPlayerNow(EnTorch2* this) {
    this->actor.shape.rot.y = this->actor.yawTowardsPlayer;
    this->actor.world.rot.y = this->actor.yawTowardsPlayer;
}

static void EnTorch2_EndStance(EnTorch2* this) {
    this->isGuarding = false;
    this->isCountering = false;
    if (this->meleeWeaponState > 0) {
        this->meleeWeaponState = 0;
    }
}

static void EnTorch2_StartSwing(EnTorch2* this, u8 damage) {
    this->swordCollider.base.atFlags &= ~(AT_HIT | AT_BOUNCED);
    this->swordCollider.elem.atDmgInfo.damage = damage;
    this->meleeWeaponState = 1;
}

static bool EnTorch2_IsProjectileComing(EnTorch2* this, PlayState* play) {
    if (func_800BC270(play, &this->actor, 300.0f, DARK_LINK_DODGED_PROJECTILES) != NULL) {
        return true;
    }
    for (Actor* explosive = play->actorCtx.actorLists[ACTORCAT_EXPLOSIVES].first; explosive != NULL;
         explosive = explosive->next) {
        if (Actor_WorldDistXYZToActor(&this->actor, explosive) < 150.0f) {
            return true;
        }
    }
    return false;
}

static bool EnTorch2_WantsToSwing(EnTorch2* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    bool isLowHealth = IsPlayerLowOnHealth();
    s32 attackDelay = isLowHealth ? 15 : 7;
    f32 noAttackChance = isLowHealth ? 0.3f : 0.0f;

    if ((this->actor.speed < 0.0f) || (player->speedXZ < 0.0f)) {
        return false;
    }
    // Still fading in: vanilla's shadow never strikes first.
    if (this->alpha != 255) {
        noAttackChance += 2.0f;
    }
    return ((play->gameplayFrames & attackDelay) == 0) && (noAttackChance <= Rand_ZeroOne());
}

static SlashKind EnTorch2_ChooseSlash(void) {
    switch ((s32)(Rand_ZeroOne() * 7.0f)) {
        case 1:
        case 5:
            return SLASH_RIGHT;
        case 2:
        case 6:
            return SLASH_LEFT;
        default:
            return SLASH_FORWARD;
    }
}

static void EnTorch2_Move(EnTorch2* this, PlayState* play, s16 moveYaw, f32 targetSpeed) {
    s16 relativeYaw = moveYaw - this->actor.shape.rot.y;

    this->actor.world.rot.y = moveYaw;
    Math_StepToF(&this->actor.speed, targetSpeed, 1.0f);
    if (targetSpeed == 0.0f) {
        EnTorch2_LoopAnim(this, play, sFightAnim);
    } else if (ABS(relativeYaw) < 0x2000) {
        EnTorch2_LoopAnim(this, play, sRunAnim);
    } else if (ABS(relativeYaw) > 0x6000) {
        EnTorch2_LoopAnim(this, play, sBackWalkAnim);
    } else {
        EnTorch2_LoopAnim(this, play, (relativeYaw > 0) ? sSideWalkLeftAnim : sSideWalkRightAnim);
    }
}

// Vanilla's stick: toward Link, bent toward his reversed facing while he does not lock on, so the shadow mirrors him.
static s16 EnTorch2_GetMirrorYaw(EnTorch2* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    s16 yaw = this->actor.yawTowardsPlayer;

    if (!this->actor.isLockedOn) {
        Math_SmoothStepToS(&yaw, player->actor.shape.rot.y + 0x7FFF, 1, 0x2328, 0);
    }
    return yaw;
}

static void EnTorch2_Slash(EnTorch2* this, PlayState* play) {
    const SlashMove* move = &sSlashMoves[this->slashKind];
    bool isAnimDone = PlayerAnimation_Update(play, &this->skelAnime);

    Math_StepToF(&this->actor.speed, 0.0f, 0.5f);
    if (this->timer == 0) {
        if (!isAnimDone) {
            return;
        }
        EnTorch2_EndStance(this);
        EnTorch2_PlayAnim(this, play, move->recover, ANIMMODE_ONCE, 1.0f, 0.0f);
        this->timer = 1;
        return;
    }
    if (isAnimDone) {
        EnTorch2_SetupFight(this, play);
    }
}

static void EnTorch2_SetupSlash(EnTorch2* this, PlayState* play, SlashKind kind, bool isCounter) {
    const SlashMove* move = &sSlashMoves[kind];

    EnTorch2_EndStance(this);
    EnTorch2_TurnToPlayerNow(this);
    EnTorch2_PlayAnim(this, play, move->swing, ANIMMODE_ONCE, 1.0f, 0.0f);
    this->slashKind = kind;
    this->isCountering = isCounter;
    this->timer = 0;
    this->actor.world.rot.y = this->actor.shape.rot.y + move->sideYaw;
    this->actor.speed = (move->sideYaw != 0) ? 4.0f : 0.0f;
    EnTorch2_StartSwing(this, DARK_LINK_SWORD_DAMAGE);
    Actor_PlaySfx(&this->actor, NA_SE_IT_SWORD_SWING);
    Actor_PlaySfx(&this->actor, NA_SE_VO_LI_SWORD_N);
    this->actionFunc = EnTorch2_Slash;
}

static void EnTorch2_Guard(EnTorch2* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    bool isPlayerSwingingNear =
        (player->meleeWeaponState != 0) && (this->actor.xzDistToPlayer <= DARK_LINK_REACT_DISTANCE);

    EnTorch2_FacePlayer(this);
    PlayerAnimation_Update(play, &this->skelAnime);
    Math_StepToF(&this->actor.speed, 0.0f, 1.0f);
    if (EnTorch2_IsProjectileComing(this, play)) {
        EnTorch2_SetupRoll(this, play);
        return;
    }
    if (isPlayerSwingingNear && IsPlayerSpinning(player)) {
        EnTorch2_SetupBackflip(this, play);
        return;
    }
    if (isPlayerSwingingNear || IsPlayerCastingMagic(player)) {
        this->timer = DARK_LINK_GUARD_HOLD_FRAMES;
        return;
    }
    if (DECR(this->timer) == 0) {
        EnTorch2_SetupFight(this, play);
    }
}

static void EnTorch2_SetupGuard(EnTorch2* this, PlayState* play) {
    EnTorch2_EndStance(this);
    EnTorch2_PlayAnim(this, play, sGuardAnim, ANIMMODE_LOOP, 1.0f, -4.0f);
    this->isGuarding = true;
    this->timer = DARK_LINK_GUARD_HOLD_FRAMES;
    this->actor.world.rot.y = this->actor.shape.rot.y;
    this->actionFunc = EnTorch2_Guard;
}

static void EnTorch2_Leap(EnTorch2* this, PlayState* play) {
    bool isAnimDone = PlayerAnimation_Update(play, &this->skelAnime);

    if (!this->hasLanded) {
        this->timer++;
        bool isGrounded = (this->timer > 2) && (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND);
        if (isGrounded || (this->timer > DARK_LINK_MAX_AIR_FRAMES)) {
            this->hasLanded = true;
            this->actor.speed = 0.0f;
            EnTorch2_PlayAnim(this, play, this->landAnim, ANIMMODE_ONCE, 1.0f, 0.0f);
            Actor_PlaySfx(&this->actor, NA_SE_PL_LAND_GROUND);
        }
        return;
    }
    if (!isAnimDone) {
        return;
    }
    if (!this->isJumpslashCounter) {
        EnTorch2_SetupFight(this, play);
        return;
    }
    this->isJumpslashCounter = false;
    if (this->alpha == 255) {
        EnTorch2_SetupJumpslash(this, play);
    } else {
        EnTorch2_SetupBackflip(this, play);
    }
}

static void EnTorch2_StartLeap(EnTorch2* this, PlayState* play, const char* anim, const char* landAnim, s16 moveYaw,
                               f32 velocityY) {
    EnTorch2_EndStance(this);
    EnTorch2_PlayAnim(this, play, anim, ANIMMODE_ONCE, 1.0f, 0.0f);
    this->landAnim = landAnim;
    this->actor.world.rot.y = moveYaw;
    this->actor.speed = 8.0f;
    this->actor.velocity.y = velocityY;
    this->invincibilityTimer = 10;
    this->timer = 0;
    this->hasLanded = false;
    Actor_PlaySfx(&this->actor, NA_SE_VO_LI_AUTO_JUMP);
    this->actionFunc = EnTorch2_Leap;
}

static void EnTorch2_SetupBackflip(EnTorch2* this, PlayState* play) {
    EnTorch2_TurnToPlayerNow(this);
    this->isJumpslashCounter = false;
    EnTorch2_StartLeap(this, play, sBackflipAnim, sBackflipEndAnim, this->actor.shape.rot.y + 0x8000, 6.5f);
}

static void EnTorch2_SetupSideJump(EnTorch2* this, PlayState* play, bool isRight, bool isJumpslashCounter) {
    EnTorch2_TurnToPlayerNow(this);
    this->isJumpslashCounter = isJumpslashCounter;
    if (isRight) {
        EnTorch2_StartLeap(this, play, sSideJumpRightAnim, sSideJumpRightEndAnim, this->actor.shape.rot.y - 0x4000,
                           5.0f);
    } else {
        EnTorch2_StartLeap(this, play, sSideJumpLeftAnim, sSideJumpLeftEndAnim, this->actor.shape.rot.y + 0x4000, 5.0f);
    }
}

static void EnTorch2_Roll(EnTorch2* this, PlayState* play) {
    if (this->actor.xzDistToPlayer < 50.0f) {
        Math_StepToF(&this->actor.speed, 0.0f, 2.0f);
    }
    if (PlayerAnimation_Update(play, &this->skelAnime)) {
        EnTorch2_SetupFight(this, play);
    }
}

static void EnTorch2_SetupRoll(EnTorch2* this, PlayState* play) {
    EnTorch2_EndStance(this);
    EnTorch2_TurnToPlayerNow(this);
    EnTorch2_PlayAnim(this, play, sRollAnim, ANIMMODE_ONCE, 1.0f, 0.0f);
    this->actor.speed = 9.0f;
    this->invincibilityTimer = 20;
    Actor_PlaySfx(&this->actor, NA_SE_PL_ROLL);
    this->actionFunc = EnTorch2_Roll;
}

static void EnTorch2_Jumpslash(EnTorch2* this, PlayState* play) {
    bool isAnimDone = PlayerAnimation_Update(play, &this->skelAnime);

    if (!this->hasLanded) {
        this->timer++;
        bool isGrounded = (this->timer > 2) && (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND);
        if (isGrounded || (this->timer > DARK_LINK_MAX_AIR_FRAMES)) {
            this->hasLanded = true;
            this->actor.speed = 0.0f;
            EnTorch2_PlayAnim(this, play, sJumpslashLandAnim, ANIMMODE_ONCE, 1.0f, 0.0f);
            Actor_PlaySfx(&this->actor, NA_SE_IT_SWORD_SWING_HARD);
        }
        return;
    }
    if (this->skelAnime.animation == (void*)sJumpslashLandAnim) {
        if ((this->skelAnime.curFrame >= 4.0f) && (this->meleeWeaponState > 0)) {
            this->meleeWeaponState = 0;
        }
        if (isAnimDone) {
            EnTorch2_PlayAnim(this, play, sJumpslashEndAnim, ANIMMODE_ONCE, 1.0f, 0.0f);
        }
        return;
    }
    if (isAnimDone) {
        EnTorch2_SetupFight(this, play);
    }
}

static void EnTorch2_SetupJumpslash(EnTorch2* this, PlayState* play) {
    EnTorch2_EndStance(this);
    EnTorch2_TurnToPlayerNow(this);
    EnTorch2_PlayAnim(this, play, sJumpslashAnim, ANIMMODE_ONCE, 1.0f, 0.0f);
    this->actor.speed = 6.0f;
    this->actor.velocity.y = 8.0f;
    this->invincibilityTimer = 10;
    this->timer = 0;
    this->hasLanded = false;
    EnTorch2_StartSwing(this, DARK_LINK_JUMPSLASH_DAMAGE);
    Actor_PlaySfx(&this->actor, NA_SE_VO_LI_SWORD_L);
    Actor_PlaySfx(&this->actor, NA_SE_IT_SWORD_SWING_HARD);
    this->actionFunc = EnTorch2_Jumpslash;
}

static void EnTorch2_LeaveBlade(EnTorch2* this, PlayState* play) {
    EnTorch2_ReleasePlayer(this, play);
    this->actor.world.pos.y += this->bladeHeight * this->actor.scale.y;
    this->bladeHeight = 0.0f;
    this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;
    EnTorch2_SetupBackflip(this, play);
}

// Standing on Link's thrust blade: Link is held still while the shadow slashes down at him.
static void EnTorch2_BladeJump(EnTorch2* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    s16 bladeYaw = player->actor.shape.rot.y - 0x3E8;
    bool isAnimDone = PlayerAnimation_Update(play, &this->skelAnime);

    Math_SmoothStepToF(&this->actor.world.pos.x, player->actor.world.pos.x + Math_SinS(bladeYaw) * 45.0f, 1.0f, 5.0f,
                       0.0f);
    Math_SmoothStepToF(&this->actor.world.pos.z, player->actor.world.pos.z + Math_CosS(bladeYaw) * 45.0f, 1.0f, 5.0f,
                       0.0f);
    Math_SmoothStepToF(&this->bladeHeight, DARK_LINK_BLADE_HEIGHT, 1.0f, 2000.0f, 0.0f);
    this->timer--;
    bool isPlayerHurt = (player->invincibilityTimer > 0) && (this->meleeWeaponState <= 0);
    if ((this->timer <= 0) || (this->hasSwungOnBlade && isPlayerHurt)) {
        EnTorch2_LeaveBlade(this, play);
        return;
    }
    if (this->timer == 19) {
        Actor_PlaySfx(&this->actor, NA_SE_VO_LI_AUTO_JUMP);
    }
    if (!this->hasSwungOnBlade && (this->timer < 16)) {
        this->hasSwungOnBlade = true;
        EnTorch2_PlayAnim(this, play, sSlashAnim, ANIMMODE_ONCE, 1.0f, 0.0f);
        EnTorch2_StartSwing(this, DARK_LINK_SWORD_DAMAGE);
        Actor_PlaySfx(&this->actor, NA_SE_IT_SWORD_SWING);
        Actor_PlaySfx(&this->actor, NA_SE_VO_LI_SWORD_N);
        return;
    }
    if (this->hasSwungOnBlade && isAnimDone && (this->meleeWeaponState > 0)) {
        this->meleeWeaponState = 0;
    }
}

static void EnTorch2_SetupBladeJump(EnTorch2* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    EnTorch2_EndStance(this);
    EnTorch2_TurnToPlayerNow(this);
    EnTorch2_PlayAnim(this, play, sBladeJumpAnim, ANIMMODE_ONCE, 1.0f, 0.0f);
    player->stateFlags3 |= PLAYER_STATE3_4;
    player->meleeWeaponState = 0;
    player->speedXZ = 0.0f;
    this->isHoldingPlayer = true;
    this->hasSwungOnBlade = false;
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    this->actor.speed = 0.0f;
    this->timer = DARK_LINK_BLADE_FRAMES;
    this->actionFunc = EnTorch2_BladeJump;
}

static void EnTorch2_Recoil(EnTorch2* this, PlayState* play) {
    Math_StepToF(&this->actor.speed, 0.0f, 0.5f);
    if (PlayerAnimation_Update(play, &this->skelAnime)) {
        EnTorch2_SetupFight(this, play);
    }
}

static void EnTorch2_SetupRecoil(EnTorch2* this, PlayState* play, bool isStagger) {
    EnTorch2_EndStance(this);
    EnTorch2_PlayAnim(this, play, sReboundAnim, ANIMMODE_ONCE, isStagger ? 0.6f : 1.0f, 0.0f);
    this->actor.world.rot.y = this->actor.shape.rot.y;
    this->actor.speed = -3.0f;
    if (isStagger) {
        this->staggerTimer = DARK_LINK_STAGGER_FRAMES;
    }
    this->actionFunc = EnTorch2_Recoil;
}

// Clashing blades: enough different swings parried in a row leave the shadow staggered and open.
static void EnTorch2_RegisterClash(EnTorch2* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    s32 threshold = (IsPlayerLowOnHealth() ? 3 : 6) + (s32)Rand_CenteredFloat(2.0f);

    if (this->lastClashAnimation != player->meleeWeaponAnimation) {
        this->clashCount++;
        this->lastClashAnimation = player->meleeWeaponAnimation;
    }
    if (this->clashCount <= threshold) {
        return;
    }
    this->clashCount = 0;
    Actor_PlaySfx(&this->actor, NA_SE_PL_DAMAGE);
    EnTorch2_SetupRecoil(this, play, true);
}

// Vanilla's reactions to what Link is doing, in its order of priority.
static bool EnTorch2_React(EnTorch2* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    f32 distance = this->actor.xzDistToPlayer;

    if (EnTorch2_IsProjectileComing(this, play)) {
        EnTorch2_SetupRoll(this, play);
        return true;
    }
    if (IsPlayerCastingMagic(player)) {
        EnTorch2_SetupGuard(this, play);
        return true;
    }
    if ((this->jumpslashCooldown == 0) && IsPlayerJumpslashing(player)) {
        this->jumpslashCooldown = 15;
        EnTorch2_SetupSideJump(this, play, (play->gameplayFrames % 2) != 0, true);
        return true;
    }
    if ((player->meleeWeaponState == 0) || (distance > DARK_LINK_REACT_DISTANCE) || (this->staggerTimer != 0)) {
        return false;
    }
    s8 animation = player->meleeWeaponAnimation;
    if ((animation == PLAYER_MWA_STAB_1H) && (distance < 90.0f)) {
        bool canJumpOnBlade =
            (distance <= 85.0f) && (player->invincibilityTimer == 0) && Actor_IsTargeted(play, &this->actor);
        if (canJumpOnBlade) {
            EnTorch2_SetupBladeJump(this, play);
        } else {
            EnTorch2_SetupBackflip(this, play);
        }
    } else if (animation <= PLAYER_MWA_FORWARD_COMBO_2H) {
        EnTorch2_SetupSlash(this, play, SLASH_FORWARD, true);
    } else if (animation <= PLAYER_MWA_RIGHT_COMBO_2H) {
        EnTorch2_SetupSlash(this, play, SLASH_RIGHT, true);
    } else if (animation <= PLAYER_MWA_LEFT_COMBO_2H) {
        EnTorch2_SetupSlash(this, play, SLASH_LEFT, true);
    } else if (animation < PLAYER_MWA_SPIN_ATTACK_1H) {
        EnTorch2_SetupGuard(this, play);
    } else {
        EnTorch2_SetupBackflip(this, play);
    }
    return true;
}

static void EnTorch2_SwingOrBackflip(EnTorch2* this, PlayState* play) {
    if (EnTorch2_WantsToSwing(this, play)) {
        EnTorch2_SetupSlash(this, play, EnTorch2_ChooseSlash(), false);
    } else {
        EnTorch2_SetupBackflip(this, play);
    }
}

// Vanilla's neutral game: close in, keep 70-100 units, strike face to face and flip away when crowded.
static void EnTorch2_Fight(EnTorch2* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    f32 distance = this->actor.xzDistToPlayer;
    s32 facingDiff = AbsYawDiff(player->actor.shape.rot.y, this->actor.shape.rot.y);

    EnTorch2_FacePlayer(this);
    PlayerAnimation_Update(play, &this->skelAnime);
    if (EnTorch2_React(this, play)) {
        return;
    }
    bool isFaceToFace = facingDiff >= 0x7800;
    if ((distance > 70.0f) && (distance <= 90.0f) && isFaceToFace &&
        (this->actor.isLockedOn || !IsPlayerShielding(player))) {
        if (EnTorch2_WantsToSwing(this, play)) {
            EnTorch2_SetupSlash(this, play, EnTorch2_ChooseSlash(), false);
        } else {
            EnTorch2_Move(this, play, this->actor.yawTowardsPlayer, 0.0f);
        }
        return;
    }
    if ((distance <= 70.0f) || ((distance <= 80.0f) && (player->meleeWeaponState != 0))) {
        EnTorch2_SwingOrBackflip(this, play);
        return;
    }
    if (distance > 100.0f) {
        if (IsPlayerSpinning(player) && (distance < 280.0f)) {
            EnTorch2_SetupBackflip(this, play);
        } else {
            EnTorch2_Move(this, play, EnTorch2_GetMirrorYaw(this, play), DARK_LINK_RUN_SPEED);
        }
        return;
    }
    bool isSideOn = (facingDiff >= 0x3000) && !isFaceToFace;
    if (isSideOn || !EnTorch2_WantsToSwing(this, play)) {
        EnTorch2_Move(this, play, EnTorch2_GetMirrorYaw(this, play), DARK_LINK_RUN_SPEED);
        return;
    }
    EnTorch2_SetupSlash(this, play, EnTorch2_ChooseSlash(), false);
}

static void EnTorch2_SetupFight(EnTorch2* this, PlayState* play) {
    EnTorch2_EndStance(this);
    EnTorch2_LoopAnim(this, play, sFightAnim);
    this->actionFunc = EnTorch2_Fight;
}

static void EnTorch2_Stunned(EnTorch2* this, PlayState* play) {
    Math_StepToF(&this->actor.speed, 0.0f, 1.0f);
    if (this->actor.colorFilterTimer == 0) {
        EnTorch2_SetupFight(this, play);
    }
}

static u16 EnTorch2_GetFilterBuffer(EnTorch2* this) {
    return (this->alpha == 255) ? COLORFILTER_BUFFLAG_OPA : COLORFILTER_BUFFLAG_XLU;
}

static void EnTorch2_SetupStunned(EnTorch2* this, PlayState* play) {
    EnTorch2_EndStance(this);
    EnTorch2_ReleasePlayer(this, play);
    this->skelAnime.playSpeed = 0.0f;
    this->actor.speed = 0.0f;
    Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_BLUE, 255, EnTorch2_GetFilterBuffer(this),
                         DARK_LINK_STUN_FRAMES);
    Actor_PlaySfx(&this->actor, NA_SE_EN_COMMON_FREEZE);
    this->actionFunc = EnTorch2_Stunned;
}

static bool EnTorch2_FindFloor(EnTorch2* this, PlayState* play, Vec3f* spot) {
    CollisionPoly* poly;
    s32 bgId;
    f32 floorY = BgCheck_EntityRaycastFloor5(&play->colCtx, &poly, &bgId, &this->actor, spot);

    if ((floorY <= BGCHECK_Y_MIN) || ((spot->y - floorY) > 300.0f)) {
        return false;
    }
    spot->y = floorY;
    return true;
}

// Rises behind Link, or back near its spawn when that spot strays too far or has no floor.
static void EnTorch2_Reappear(EnTorch2* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    Vec3f spot;

    spot.x = player->actor.world.pos.x - Math_SinS(player->actor.shape.rot.y) * DARK_LINK_REAPPEAR_DISTANCE;
    spot.y = this->spawnPos.y + 40.0f;
    spot.z = player->actor.world.pos.z - Math_CosS(player->actor.shape.rot.y) * DARK_LINK_REAPPEAR_DISTANCE;
    if (Math_Vec3f_DistXYZ(&spot, &this->spawnPos) > DARK_LINK_LEASH) {
        s16 yaw = Math_Vec3f_Yaw(&this->spawnPos, &player->actor.world.pos) + (s16)Rand_CenteredFloat(4000.0f);
        f32 radius = 25.0f + (Rand_ZeroOne() * 20.0f);

        spot.x = this->spawnPos.x + Math_SinS(yaw) * radius;
        spot.z = this->spawnPos.z + Math_CosS(yaw) * radius;
    }
    if (!EnTorch2_FindFloor(this, play, &spot)) {
        spot = this->spawnPos;
    }
    this->actor.world.pos = spot;
    this->actor.prevPos = spot;
    this->actor.home.pos = spot;
    this->actor.velocity.y = 0.0f;
    this->actor.speed = 0.0f;
    this->actor.shape.rot.y = Actor_WorldYawTowardActor(&this->actor, &player->actor);
    this->actor.world.rot.y = this->actor.shape.rot.y;
}

static void EnTorch2_Sink(EnTorch2* this, PlayState* play) {
    PlayerAnimation_Update(play, &this->skelAnime);
    if (this->actor.world.pos.y >= (this->sinkFloorY - DARK_LINK_SINK_DEPTH)) {
        return;
    }
    EnTorch2_Reappear(this, play);
    this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;
    if (this->alpha != 255) {
        this->clashCount = 0;
        this->staggerTimer = 0;
    }
    EnTorch2_SetupFight(this, play);
}

static void EnTorch2_SetupSink(EnTorch2* this, PlayState* play) {
    EnTorch2_EndStance(this);
    EnTorch2_ReleasePlayer(this, play);
    EnTorch2_PlayAnim(this, play, sHitAnim, ANIMMODE_ONCE, 1.0f, 0.0f);
    this->actor.world.rot.y = this->actor.yawTowardsPlayer;
    this->actor.speed = -8.0f;
    this->actor.velocity.y = 6.0f;
    this->sinkFloorY = this->actor.floorHeight;
    this->bladeHeight = 0.0f;
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_RED, 255, EnTorch2_GetFilterBuffer(this), 12);
    Actor_PlaySfx(&this->actor, NA_SE_VO_LI_DAMAGE_S);
    this->actionFunc = EnTorch2_Sink;
}

static void EnTorch2_Die(EnTorch2* this, PlayState* play) {
    PlayerAnimation_Update(play, &this->skelAnime);
    Math_StepToF(&this->actor.speed, 0.0f, 0.5f);
    if (this->alpha <= DARK_LINK_DEATH_FADE) {
        this->alpha = 0;
        Actor_Kill(&this->actor);
        return;
    }
    this->alpha -= DARK_LINK_DEATH_FADE;
}

static void EnTorch2_SetupDie(EnTorch2* this, PlayState* play) {
    EnTorch2_EndStance(this);
    EnTorch2_ReleasePlayer(this, play);
    EnTorch2_StopBgm(this);
    Enemy_StartFinishingBlow(play, &this->actor);
    Item_DropCollectibleRandom(play, &this->actor, &this->actor.world.pos, 0xC0);
    EnTorch2_PlayAnim(this, play, sDownAnim, ANIMMODE_ONCE, 1.0f, 0.0f);
    this->actor.flags &= ~(ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE);
    this->actor.world.rot.y = this->actor.yawTowardsPlayer;
    this->actor.speed = -6.0f;
    this->actor.velocity.y = 6.0f;
    this->bladeHeight = 0.0f;
    Actor_PlaySfx(&this->actor, NA_SE_VO_LI_DOWN);
    this->actionFunc = EnTorch2_Die;
}

static void EnTorch2_Wake(EnTorch2* this, PlayState* play) {
    EnTorch2_StartBgm(this);
    if (EnTorch2_IsProjectileComing(this, play)) {
        EnTorch2_SetupRoll(this, play);
    } else {
        EnTorch2_SetupFight(this, play);
    }
}

// Waits frozen and translucent until Link comes close, locks on or shoots.
static void EnTorch2_Wait(EnTorch2* this, PlayState* play) {
    EnTorch2_TurnToPlayerNow(this);
    if ((this->actor.xzDistToPlayer <= DARK_LINK_WAKE_DISTANCE) || Actor_IsTargeted(play, &this->actor) ||
        EnTorch2_IsProjectileComing(this, play)) {
        EnTorch2_Wake(this, play);
    }
}

static void EnTorch2_SetupWait(EnTorch2* this, PlayState* play) {
    EnTorch2_PlayAnim(this, play, sFightAnim, ANIMMODE_LOOP, 0.0f, 0.0f);
    this->alpha = DARK_LINK_START_ALPHA;
    this->actionFunc = EnTorch2_Wait;
}

static bool EnTorch2_IsHitByFire(EnTorch2* this) {
    ColliderElement* hitter = this->bodyCollider.elem.acHitElem;

    return (hitter != NULL) && (hitter->atDmgInfo.dmgFlags & DMG_FIRE_ARROW);
}

static bool EnTorch2_IsHitFromFront(EnTorch2* this, PlayState* play) {
    Actor* attacker = this->bodyCollider.base.ac;

    if (attacker == NULL) {
        attacker = &GET_PLAYER(play)->actor;
    }
    return AbsYawDiff(Actor_WorldYawTowardActor(&this->actor, attacker), this->actor.shape.rot.y) < 0x4000;
}

static void EnTorch2_UpdateDamage(EnTorch2* this, PlayState* play) {
    if (this->shieldCollider.base.acFlags & AC_BOUNCED) {
        this->shieldCollider.base.acFlags &= ~AC_BOUNCED;
        this->bodyCollider.base.acFlags &= ~AC_HIT;
        this->actor.world.rot.y = this->actor.shape.rot.y;
        this->actor.speed = -3.0f;
        return;
    }
    if (this->swordCollider.base.acFlags & AC_BOUNCED) {
        this->swordCollider.base.acFlags &= ~AC_BOUNCED;
        this->bodyCollider.base.acFlags &= ~AC_HIT;
        EnTorch2_RegisterClash(this, play);
        return;
    }
    if (!(this->bodyCollider.base.acFlags & AC_HIT)) {
        return;
    }
    this->bodyCollider.base.acFlags &= ~AC_HIT;
    if (this->actor.colChkInfo.health == 0) {
        return;
    }
    if (this->isGuarding && !EnTorch2_IsHitByFire(this) && EnTorch2_IsHitFromFront(this, play)) {
        Actor_PlaySfx(&this->actor, NA_SE_IT_SHIELD_REFLECT_SW);
        return;
    }
    if (this->actionFunc == EnTorch2_Wait) {
        this->alpha = MAX(this->alpha, DARK_LINK_START_ALPHA);
        EnTorch2_StartBgm(this);
    }
    Actor_SetDropFlag(&this->actor, &this->bodyCollider.elem);
    if (this->actor.colChkInfo.damageEffect == DARK_LINK_DMGEFF_STUN) {
        if (this->actionFunc != EnTorch2_Stunned) {
            EnTorch2_SetupStunned(this, play);
        }
        return;
    }
    if (Actor_ApplyDamage(&this->actor) == 0) {
        EnTorch2_SetupDie(this, play);
        return;
    }
    EnTorch2_SetupSink(this, play);
}

static bool EnTorch2_IsVulnerable(EnTorch2* this) {
    return (this->invincibilityTimer == 0) && (this->actionFunc != EnTorch2_Sink) &&
           (this->actionFunc != EnTorch2_Die) && (this->actionFunc != EnTorch2_BladeJump);
}

static void EnTorch2_UpdateCollision(EnTorch2* this, PlayState* play) {
    CollisionCheckContext* colChkCtx = &play->colChkCtx;
    bool isVulnerable = EnTorch2_IsVulnerable(this);
    bool isSolid = (this->actionFunc != EnTorch2_Sink) && (this->actionFunc != EnTorch2_Die) &&
                   (this->actionFunc != EnTorch2_BladeJump);

    Collider_UpdateCylinder(&this->actor, &this->bodyCollider);
    this->bodyCollider.base.colMaterial = isVulnerable ? COL_MATERIAL_HIT5 : COL_MATERIAL_NONE;
    if (isSolid) {
        CollisionCheck_SetOC(play, colChkCtx, &this->bodyCollider.base);
    }
    if (isVulnerable) {
        CollisionCheck_SetAC(play, colChkCtx, &this->bodyCollider.base);
    }
    if (this->isGuarding) {
        this->shieldCollider.dim.pos.x = this->actor.world.pos.x + Math_SinS(this->actor.shape.rot.y) * 20.0f;
        this->shieldCollider.dim.pos.y = this->actor.world.pos.y;
        this->shieldCollider.dim.pos.z = this->actor.world.pos.z + Math_CosS(this->actor.shape.rot.y) * 20.0f;
        CollisionCheck_SetAC(play, colChkCtx, &this->shieldCollider.base);
    }
    if (this->meleeWeaponState < 1) {
        return;
    }
    if (this->swordCollider.base.atFlags & AT_BOUNCED) {
        this->swordCollider.base.atFlags &= ~AT_BOUNCED;
        if (this->actionFunc != EnTorch2_BladeJump) {
            EnTorch2_SetupRecoil(this, play, false);
            return;
        }
    }
    CollisionCheck_SetAT(play, colChkCtx, &this->swordCollider.base);
    if (this->isCountering) {
        CollisionCheck_SetAC(play, colChkCtx, &this->swordCollider.base);
    }
}

static void EnTorch2_UpdateBlink(EnTorch2* this) {
    if (DECR(this->blinkTimer) == 0) {
        this->blinkTimer = 30 + (s16)(Rand_ZeroOne() * 60.0f);
    }
    if (this->blinkTimer == 2) {
        this->eyeIndex = DARK_LINK_EYES_CLOSED;
    } else if ((this->blinkTimer == 1) || (this->blinkTimer == 3)) {
        this->eyeIndex = DARK_LINK_EYES_HALF;
    } else {
        this->eyeIndex = DARK_LINK_EYES_OPEN;
    }
}

// Fades in by one step every 8 frames while fighting, as vanilla's shadow does.
static void EnTorch2_UpdateAlpha(EnTorch2* this, PlayState* play) {
    bool isFighting = (this->actionFunc != EnTorch2_Wait) && (this->actionFunc != EnTorch2_Sink) &&
                      (this->actionFunc != EnTorch2_Die);

    if (isFighting && (this->alpha != 255) && ((play->gameplayFrames % 8) == 0)) {
        this->alpha++;
    }
}

static void EnTorch2_UpdateTimers(EnTorch2* this) {
    if (this->invincibilityTimer > 0) {
        this->invincibilityTimer--;
    }
    DECR(this->staggerTimer);
    DECR(this->jumpslashCooldown);
}

static bool EnTorch2_IsOverVoid(EnTorch2* this, PlayState* play) {
    if (!(this->actor.bgCheckFlags & BGCHECKFLAG_GROUND_TOUCH)) {
        return false;
    }
    FloorProperty property = SurfaceType_GetFloorProperty(&play->colCtx, this->actor.floorPoly, this->actor.floorBgId);
    return (property == FLOOR_PROPERTY_5) || (property == FLOOR_PROPERTY_12);
}

static void EnTorch2_Init(Actor* thisx, PlayState* play) {
    static const Vec3s sSkeletonBaseTransl = { -57, 3377, 0 };
    EnTorch2* this = (EnTorch2*)thisx;
    EffectBlureInit1 slashBlure;

    Actor_ProcessInitChain(&this->actor, sInitChain);
    SkelAnime_InitPlayer(play, &this->skelAnime, (FlexSkeletonHeader*)sSkel, (PlayerAnimationHeader*)sFightAnim, 1 | 8,
                         this->jointTableBuffer, this->morphTableBuffer, PLAYER_LIMB_MAX);
    this->skelAnime.baseTransl = sSkeletonBaseTransl;
    ActorShape_Init(&this->actor.shape, 0.0f, ActorShadow_DrawCircle, 30.0f);
    this->actor.colChkInfo.cylRadius = 60;
    this->actor.colChkInfo.cylHeight = 100;
    this->actor.colChkInfo.damageTable = &sDamageTable;
    this->actor.colChkInfo.mass = 50;
    this->actor.colChkInfo.health = MAX(gSaveContext.save.saveInfo.playerData.healthCapacity >> 3, 1);
    Collider_InitCylinder(play, &this->bodyCollider);
    Collider_SetCylinder(play, &this->bodyCollider, &this->actor, &sBodyColliderInit);
    Collider_InitCylinder(play, &this->shieldCollider);
    Collider_SetCylinder(play, &this->shieldCollider, &this->actor, &sShieldColliderInit);
    Collider_InitQuad(play, &this->swordCollider);
    Collider_SetQuad(play, &this->swordCollider, &this->actor, &sSwordColliderInit);
    memset(&slashBlure, 0, sizeof(slashBlure));
    for (s32 i = 0; i < 4; i++) {
        slashBlure.p1StartColor[i] = 255;
        slashBlure.p2StartColor[i] = 255;
        slashBlure.p1EndColor[i] = 255;
        slashBlure.p2EndColor[i] = 255;
    }
    slashBlure.p1EndColor[3] = 0;
    slashBlure.p2EndColor[3] = 0;
    slashBlure.p2StartColor[3] = 64;
    slashBlure.elemDuration = 4;
    slashBlure.unkFlag = 0;
    slashBlure.calcMode = 2;
    Effect_Add(play, &this->effectIndex, EFFECT_BLURE1, 0, 0, &slashBlure);
    this->spawnPos = this->actor.world.pos;
    this->lastClashAnimation = -1;
    this->meleeWeaponState = -1;
    this->blinkTimer = 40;
    EnTorch2_SetupWait(this, play);
}

static void EnTorch2_Destroy(Actor* thisx, PlayState* play) {
    EnTorch2* this = (EnTorch2*)thisx;

    EnTorch2_ReleasePlayer(this, play);
    EnTorch2_StopBgm(this);
    Effect_Destroy(play, this->effectIndex);
    Collider_DestroyCylinder(play, &this->bodyCollider);
    Collider_DestroyCylinder(play, &this->shieldCollider);
    Collider_DestroyQuad(play, &this->swordCollider);
}

static void EnTorch2_Update(Actor* thisx, PlayState* play) {
    EnTorch2* this = (EnTorch2*)thisx;

    EnTorch2_UpdateDamage(this, play);
    this->actionFunc(this, play);
    if (this->actor.update == NULL) {
        return;
    }
    Actor_MoveWithGravity(&this->actor);
    if (this->actionFunc != EnTorch2_Sink) {
        Actor_UpdateBgCheckInfo(play, &this->actor, 35.0f, 15.0f, 60.0f, DARK_LINK_BGCHECK_FLAGS);
        if (EnTorch2_IsOverVoid(this, play)) {
            EnTorch2_ReleasePlayer(this, play);
            EnTorch2_StopBgm(this);
            Actor_Kill(&this->actor);
            return;
        }
    }
    EnTorch2_UpdateTimers(this);
    EnTorch2_UpdateAlpha(this, play);
    EnTorch2_UpdateBlink(this);
    this->actor.focus.pos = this->actor.world.pos;
    this->actor.focus.pos.y += 45.0f;
    this->actor.shape.yOffset = this->bladeHeight;
    this->actor.shape.shadowAlpha = this->alpha;
    EnTorch2_UpdateCollision(this, play);
}

static void EnTorch2_UpdateSword(EnTorch2* this) {
    static Vec3f sBladeBaseNear = { 0.0f, 1400.0f, -1000.0f };
    static Vec3f sBladeTipNear = { 4000.0f, 1400.0f, -1000.0f };
    static Vec3f sBladeBaseFar = { 0.0f, -400.0f, 1000.0f };
    static Vec3f sBladeTipFar = { 4000.0f, -400.0f, 1000.0f };
    static Vec3f sTrailTip = { 4000.0f, 400.0f, 0.0f };
    static Vec3f sTrailBase = { 1000.0f, 400.0f, 0.0f };
    Vec3f quad[4];
    Vec3f tip;
    Vec3f base;

    Matrix_MultVec3f(&sBladeBaseNear, &quad[0]);
    Matrix_MultVec3f(&sBladeTipNear, &quad[1]);
    Matrix_MultVec3f(&sBladeBaseFar, &quad[2]);
    Matrix_MultVec3f(&sBladeTipFar, &quad[3]);
    Collider_SetQuadVertices(&this->swordCollider, &quad[0], &quad[1], &quad[2], &quad[3]);
    Matrix_MultVec3f(&sTrailTip, &tip);
    Matrix_MultVec3f(&sTrailBase, &base);
    if (this->meleeWeaponState >= 1) {
        EffectBlure_AddVertex(Effect_GetByIndex(this->effectIndex), &tip, &base);
    } else if (this->meleeWeaponState >= 0) {
        EffectBlure_AddSpace(Effect_GetByIndex(this->effectIndex));
        this->meleeWeaponState = -1;
    }
}

static s32 EnTorch2_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, Actor* thisx,
                                     Gfx** gfx) {
    EnTorch2* this = (EnTorch2*)thisx;

    // The root keeps its animated height only: the actor itself carries every horizontal move.
    if (limbIndex == PLAYER_LIMB_ROOT) {
        pos->x = this->skelAnime.baseTransl.x;
        pos->z = this->skelAnime.baseTransl.z;
    } else if (limbIndex == PLAYER_LIMB_LEFT_HAND) {
        *dList = (Gfx*)sSwordHandDL;
    } else if (limbIndex == PLAYER_LIMB_RIGHT_HAND) {
        *dList = (Gfx*)sShieldHandDL;
    } else if (limbIndex == PLAYER_LIMB_SHEATH) {
        *dList = (Gfx*)sSheathDL;
    }
    if ((this->alpha != 255) && (*dList != NULL)) {
        Gfx* fadingDL = GetFadingDL(*dList);
        if (fadingDL != NULL) {
            *dList = fadingDL;
        }
    }
    return false;
}

static void EnTorch2_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, Actor* thisx, Gfx** gfx) {
    if (limbIndex == PLAYER_LIMB_LEFT_HAND) {
        EnTorch2_UpdateSword((EnTorch2*)thisx);
    }
}

// Vanilla's red environment colour and alpha, over adult Link turned to a dark silhouette.
static Gfx* EnTorch2_DrawBody(EnTorch2* this, PlayState* play, Gfx* gfx) {
    gDPSetEnvColor(gfx++, 255, 0, 0, this->alpha);
    gSPSegment(gfx++, 0x08, (uintptr_t)sEyeTextures[this->eyeIndex]);
    gSPSegment(gfx++, 0x09, (uintptr_t)sMouthTex);
    gSPSegment(gfx++, 0x0C, (uintptr_t)sEmptyDL);
    gDPSetGrayscaleColor(gfx++, 56, 48, 72, 255);
    gSPGrayscale(gfx++, true);
    gfx = SkelAnime_DrawFlex(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount,
                             EnTorch2_OverrideLimbDraw, EnTorch2_PostLimbDraw, &this->actor, gfx);
    gSPGrayscale(gfx++, false);
    return gfx;
}

static void EnTorch2_Draw(Actor* thisx, PlayState* play) {
    EnTorch2* this = (EnTorch2*)thisx;

    OPEN_DISPS(play->state.gfxCtx);
    if (this->alpha == 255) {
        Gfx_SetupDL25_Opa(play->state.gfxCtx);
        POLY_OPA_DISP = EnTorch2_DrawBody(this, play, POLY_OPA_DISP);
    } else {
        Gfx_SetupDL25_Xlu(play->state.gfxCtx);
        POLY_XLU_DISP = EnTorch2_DrawBody(this, play, POLY_XLU_DISP);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawSoul(PlayState* play, S2HGetItemEntry* entry) {
    u32 frames = play->state.frames;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               (uintptr_t)Gfx_TwoTexScroll(play->state.gfxCtx, 0, 0, 0, 0x10, 0x20, 1, (frames * 2) & 0x3F,
                                           (frames * -6) & 0x7F, 0x10, 0x20));
    gDPSetPrimColor(POLY_XLU_DISP++, 0xC0, 0xC0, 90, 70, 120, 0);
    gDPSetEnvColor(POLY_XLU_DISP++, 200, 30, 40, 0);
    Matrix_Scale(0.13333333f, 0.13333333f, 0.13333333f, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gameplay_keep_DL_01ACF0);
    CLOSE_DISPS(play->state.gfxCtx);
}

// Ship's rule: any sword trades blows; otherwise Zora's boomerang or nuts stun him for heavier hits.
static bool CanKillDarkLink(void* userData) {
    bool canUseStick = sApi->LogicCanUseItem(RI_DEKU_STICK);
    bool canSmash = canUseStick || sApi->LogicHasCapability("CanBeGoron");
    bool canHitHard = canSmash || sApi->LogicCanUseItem(RI_BOW) || sApi->LogicHasCapability("HasExplosives");

    return sApi->LogicHasCapability("CanUseSword") || (sApi->LogicHasCapability("CanBeZora") && canHitHard) ||
           (sApi->LogicCanUseItem(RI_DEKU_NUT) && canSmash);
}

static void RegisterSoul(void) {
    static S2HCustomItemRandomizer sSoulLogic = { 0 };
    S2HCustomItemDefinition soul = { 0 };

    // Pool membership follows the host's soul shuffle and soul pool; the mod only declares the item.
    sSoulLogic.structSize = sizeof(sSoulLogic);
    sSoulLogic.flags = S2H_CUSTOM_ITEM_RANDO_ADVANCEMENT;
    sSoulLogic.type = S2H_CUSTOM_ITEM_TYPE_ITEM;
    sSoulLogic.logicKey = DARK_LINK_SOUL_KEY;
    sSoulLogic.hintKey = DARK_LINK_SOUL_KEY;
    soul.structSize = sizeof(soul);
    soul.key = DARK_LINK_SOUL_KEY;
    soul.presentationFlags = S2H_ITEM_HIDE_FROM_KALEIDO;
    soul.getItemText = "You got the %rSoul of Dark Link%w!&Your shadow can be defeated now.";
    soul.getItemEntry.drawFunc = DrawSoul;
    soul.randomizer = &sSoulLogic;
    if (!sApi->RegisterCustomItem(&soul)) {
        sApi->Log("Could not register " DARK_LINK_SOUL_KEY);
    }
}

static s16 RegisterDarkLink(void) {
    S2HActorDefinition definition = { 0 };

    definition.structSize = sizeof(definition);
    definition.key = DARK_LINK_KEY;
    definition.description = "Dark Link";
    definition.category = ACTORCAT_ENEMY;
    definition.actorFlags = DARK_LINK_FLAGS;
    definition.objectId = GAMEPLAY_KEEP;
    definition.instanceSize = sizeof(EnTorch2);
    definition.init = EnTorch2_Init;
    definition.destroy = EnTorch2_Destroy;
    definition.update = EnTorch2_Update;
    definition.draw = EnTorch2_Draw;
    definition.enemyFlags =
        S2H_ACTOR_ENEMY | S2H_ACTOR_ENEMY_MINIBOSS | S2H_ACTOR_ENEMY_NOT_IN_TIMED_ROOMS | S2H_ACTOR_ENEMY_NOT_IN_WATER;
    definition.naviHint = "Dark Link&Your own shadow. It copies every move&you make, so outwit it!";
    // The host finds the soul item as "soul." + soulKey.
    definition.soulKey = DARK_LINK_KEY;
    definition.logicKill = CanKillDarkLink;
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
    if (!RequireOotAsset(LINK_BOY_OBJECT "gLinkAdultSkel") ||
        !RequireOotAsset(PLAYER_ANIM_OBJECT "gPlayerAnim_link_fighter_backturn_jump")) {
        sApi->Log(DARK_LINK_KEY ": OoT Asset Loader resources are missing; extract them to enable this enemy.");
        return;
    }
    if (RegisterDarkLink() < 0) {
        sApi->Log("Could not register " DARK_LINK_KEY);
        return;
    }
    RegisterSoul();
}
