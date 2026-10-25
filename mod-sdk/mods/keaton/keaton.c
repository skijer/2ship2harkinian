#include <stdlib.h>
#include <string.h>

#include "z64items.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "2s2h/Rando/Types.h"
#include "objects/gameplay_keep/gameplay_keep.h"

#define KEATON_MASK_KEY "nei.keaton_form_mask"
#define KEATON_FORM_KEY "nei.keaton"
#define KEATON_MODEL_PATH "objects/forms/keaton"
// Medido: este rig tiene las piernas cortas a propósito, así que el cuerpo cuelga 1131 bajo la raíz donde
// el de Link cuelga 2336. Sin escalar la posición de la raíz, la animación lo deja flotando media altura.
// De niño la animación lleva la raíz a ~2376 en vez de a 3377, así que el mismo 1131 pide otra escala.
#define KEATON_ROOT_SCALE_ADULT 0.335f
#define KEATON_ROOT_SCALE_CHILD 0.476f
#define KEATON_ROOT_DROP 350.0f
// Rejilla de MM: Keaton ocupa la primera celda de la segunda fila.
#define KEATON_MASK_PAGE 1
#define KEATON_MASK_SLOT 6

#define TAIL_SEGMENTS 9
#define TAIL_MATRIX_SEGMENT 0x0B
#define TAIL_CLIP_AMOUNT 0.39f

#define FIST_DMG_FLAGS DMG_SWORD
#define FIST_DAMAGE 4
#define FIST_REACH 1200.0f
#define PUNCH_GHOSTS 4
#define LINK_VOICE_ACTIONS 0x20
// Lo que MM le quita de alpha al rastro del puñetazo cada frame.
#define PUNCH_GHOST_FADE (255 / 3)
#define CHAIN_OPENS 0.55f
#define COMBO_STEPS 3

#define CHARGE_LOOP_START 13.0f
#define CHARGE_LOOP_END 17.0f
#define CHARGE_LEVEL1_FRAMES 20
#define CHARGE_LEVEL2_FRAMES 60
#define CHARGE_MAGIC_LEVEL1 8
#define CHARGE_MAGIC_LEVEL2 16

// Filas de la tabla de En_Light, elegidas con params & 0xF.
#define FLAME_BLUE 2
#define FLAME_ORANGE 0
// En_Light se escala solo por su fila × 0.0001f; esto lo multiplica. Medido en juego.
#define BALL_SCALE_LEVEL1 (0.0075f * 0.16f)
#define BALL_SCALE_LEVEL2 (0.0075f * 0.33f)
#define BALL_RADIUS_LEVEL1 34
#define BALL_RADIUS_LEVEL2 60

#define SHOT_SPEED 4.5f
#define SHOT_RANGE_LEVEL1 600.0f
#define SHOT_RANGE_LEVEL2 1000.0f
#define PULL_RADIUS_LEVEL1 280.0f
#define PULL_RADIUS_LEVEL2 400.0f
#define PULL_STEP 7.0f

#define LONG_JUMP_SPEED 16.0f
#define LONG_JUMP_LIFT 8.0f
// El salto cambia de animación en cuanto Link deja el suelo, así que la nuestra tiene que entrar después de ese
// cambio o se pierde. La misma espera de dos frames que usa la Roc's Feather.
#define LONG_JUMP_ANIM_DELAY 2

#define AIR_KICK_SPEED 14.0f
// func_808401F4 usa este empuje para un espadazo bloqueado.
#define RECOIL_SPEED -18.0f
#define KICK_BLOCK_DAMAGE 8
#define KICK_BLOCK_INVINCIBILITY 20
// Respuesta de daño 2 de func_80833B18: el retroceso corto que lanza al jugador de espaldas.
#define KICK_KNOCKBACK_RESPONSE 2

// El escudo de Keaton: un hexágono que se abre de golpe y respira, y que devuelve lo que le llega.
#define REFLECTOR_HEIGHT 20.0f
#define REFLECT_RADIUS 90.0f
#define BURST_RADIUS 70
#define BURST_HEIGHT 90
#define BURST_DAMAGE 8
#define POP_STEP 0.28f
#define PULSE_MIN 0.95f
#define PULSE_PERIOD 24
#define AIR_DRAG 0.35f
#define PROJECTILE_SPEED 6.0f
#define REFLECTED_MAX 8
#define REFLECTED_LIFE 40

#define CLIMB_RATE 2.0f
#define CLIMB_DRAIN_INTERVAL 10
#define CLIMB_MAGIC_COST 1

// MM leaves these bits unnamed: dead, talking, loading, getting an item, carrying, swimming, first person, ledge
// climb, ladder, horse, damage knockback and the textbox freeze.
#define BUSY_STATE1                                                                                          \
    (PLAYER_STATE1_DEAD | PLAYER_STATE1_TALKING | PLAYER_STATE1_200 | PLAYER_STATE1_400 |                    \
     PLAYER_STATE1_CARRYING_ACTOR | PLAYER_STATE1_8000000 | PLAYER_STATE1_100000 | PLAYER_STATE1_4 |         \
     PLAYER_STATE1_200000 | PLAYER_STATE1_800000 | PLAYER_STATE1_4000000 | PLAYER_STATE1_20000000)
#define STATE1_SWIMMING PLAYER_STATE1_8000000
#define STATE1_FIRST_PERSON PLAYER_STATE1_100000
#define STATE1_DAMAGED PLAYER_STATE1_4000000
#define STATE1_CLIMBING PLAYER_STATE1_200000
// Shape yaw stops following anything.
#define STATE2_NO_ROTATION PLAYER_STATE2_40

#define ANIM_SS_DIR "__OTR__misc/link_animetion/gPlayerAnim_mhr_ss_"
#define ANIM_FIELD_DIR "__OTR__misc/link_animetion/gPlayerAnim_mhr_field_"

static const ALIGN_ASSET(2) char sIconTex[] = "__OTR__icon_item_static_yar/gItemIconKeatonMaskTex";
static const ALIGN_ASSET(2) char sNameTex[] = "__OTR__item_name_static/gItemNameKeatonMaskENGTex";
static const ALIGN_ASSET(2) char sGetItemMaskDL[] = "__OTR__objects/object_gi_ki_tan_mask/gGiKeatonMaskDL";
// La máscara que Link se pone en la cara durante la cutscene: la nativa de MM.
static const ALIGN_ASSET(2) char sMaskDL[] = "__OTR__objects/object_mask_ki_tan/object_mask_ki_tan_DL_0004A0";
static const ALIGN_ASSET(2) char sTailSkelPath[] = "__OTR__objects/forms/keaton/object_link_boy/gLinkAdultTails";
static const ALIGN_ASSET(2) char sFluteHandDL[] = "__OTR__" KEATON_MODEL_PATH "/object_link_boy/gKeatonHandAndFluteDL";
static const ALIGN_ASSET(2) char sLeftHandClosedDL[] = "__OTR__" KEATON_MODEL_PATH
                                                       "/object_link_boy/gLinkAdultLeftHandClosedNearDL";
static const ALIGN_ASSET(2) char sLeftHandOpenDL[] = "__OTR__" KEATON_MODEL_PATH
                                                     "/object_link_boy/gLinkAdultLeftHandNearDL";
static const ALIGN_ASSET(2) char sRightHandClosedDL[] = "__OTR__" KEATON_MODEL_PATH
                                                        "/object_link_boy/gLinkAdultRightHandClosedNearDL";
static const ALIGN_ASSET(2) char sRightHandOpenDL[] = "__OTR__" KEATON_MODEL_PATH
                                                      "/object_link_boy/gLinkAdultRightHandNearDL";

// Los dos primeros golpes son los puñetazos Goron de MM; el tercero y el resto del moveset son clips de MHR que
// viajan en este mod.
static const ALIGN_ASSET(2) char sComboThirdPath[] = ANIM_SS_DIR "dash_attack09";
static const f32 sComboHitStart[COMBO_STEPS] = { 4.0f, 6.0f, 14.0f };
static const f32 sComboHitEnd[COMBO_STEPS] = { 9.0f, 13.0f, 26.0f };

static const ALIGN_ASSET(2) char sChargePath[] = ANIM_SS_DIR "attack13";
static const ALIGN_ASSET(2) char sLongJumpPath[] = ANIM_FIELD_DIR "charge_attack01";
static const ALIGN_ASSET(2) char sAirKickPath[] = ANIM_FIELD_DIR "wirebug_dash02";
static const ALIGN_ASSET(2) char sReflectPath[] = "__OTR__misc/link_animetion/gPlayerAnim_mhr_damage_idle03_loop";
// El destello del puñetazo Goron de MM, que es el mismo golpe.
static const ALIGN_ASSET(2) char sPunchEffectDL[] = "__OTR__objects/object_link_goron/gLinkGoronGoronPunchEffectDL";

#include "keaton_tail_anim.inc.c"

static const char* const sRequiredHooks[] = { "OnPlayerPostLimbDraw",
                                              "OnPlayerActionHandler",
                                              "OnActorUpdate",
                                              "OnSceneInit",
                                              "OnActorPlaySfx",
                                              "OnPlayerFilterInput",
                                              "OnBgCheckResolveWallFlags",
                                              "OnPlayerResolveLimbDraw" };
static const S2HModRequirements sRequirements = { sizeof(S2HModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

typedef enum {
    KEATON_IDLE,
    KEATON_COMBO,
    KEATON_CHARGE,
    KEATON_THROW,
    KEATON_LONG_JUMP,
    KEATON_AIR_KICK,
    KEATON_RECOIL,
} KeatonState;

static const S2HModApi* sApi;

static PlayerAnimationHeader* sCombo[COMBO_STEPS];
static PlayerAnimationHeader* sCharge;
static PlayerAnimationHeader* sLongJump;
static PlayerAnimationHeader* sAirKick;
static bool sAnimsLoaded;

// El rastro rojo del puñetazo: MM guarda la matriz del golpe en un anillo de 4 y la vuelve a dibujar con el
// alpha cayendo. La rampa es la suya para pg_punchA, sin los frames en los que todavía no hay destello.
typedef struct {
    MtxF matrix;
    u8 alpha;
} PunchGhost;

static const u8 kPunchAlpha[] = { 100, 200, 255, 255, 255, 200, 100 };
static Gfx* sPunchEffect;
static PunchGhost sPunchGhost[2][PUNCH_GHOSTS];
static s32 sPunchGhostNext[2];
static s32 sPunchFrames;

static u8 sState;
static s32 sStep;
static s16 sChargeTimer;
static u8 sChargeLevel;
static bool sChargeReversing;
static s16 sJumpDelay;
static bool sKickBuffered;
static bool sFistsBlocked;
static s16 sAirKickYaw;

static Vec3f sShotPos;
static Vec3f sShotStep;
static f32 sShotTravelled;
static f32 sShotRange;
static f32 sShotPullRadius;
static u8 sShotLevel;
static bool sShotActive;
static Actor* sFlame;
static u8 sFlameCategory;
static ColliderCylinder sShotCylinder;
static bool sShotCylinderReady;

static s32 sFistTrail[2] = { -1, -1 };
static bool sClimbLent;
static bool sReadingNaturalWall;
static s16 sClimbDrain;

typedef enum {
    REFLECTOR_OFF,
    REFLECTOR_POPPING,
    REFLECTOR_HELD,
} ReflectorState;

typedef struct {
    Actor* actor;
    s16 timer;
} ReflectedProjectile;

static PlayerAnimationHeader* sReflectAnim;
static u8 sReflectorState;
static f32 sReflectorScale;
static s16 sReflectorPulse;
static ColliderCylinder sGuard;
static ColliderCylinder sBurst;
static bool sGuardReady;
static bool sBurstReady;
static ReflectedProjectile sReflected[REFLECTED_MAX];

static Vec3s sTail[TAIL_SEGMENTS];
static u32 sTailFrame;
static u8 sTailPose;
static SkeletonHeader* sTailSkel;
static u8 sTriedTailSkel;

s32 Player_SetAction(PlayState* play, Player* this, PlayerActionFunc actionFunc, s32 arg3);
void func_8083B1A0(Player* player, PlayState* play);
s32 func_808401F4(PlayState* play, Player* player);
void func_80833728(Player* player, s32 index, u32 dmgFlags, s32 damage);
void func_80833B18(PlayState* play, Player* player, s32 hitResponse, f32 speed, f32 velocityY, s16 yaw,
                   s32 invincibilityTimer);
void func_80834D50(PlayState* play, Player* player, PlayerAnimationHeader* anim, f32 speed, u16 sfxId);
void func_80836B3C(PlayState* play, Player* player, f32 arg2);
void func_8082DC38(Player* player);
s32 func_80126440(PlayState* play, ColliderQuad* collider, WeaponInfo* weaponInfo, Vec3f* newTip, Vec3f* newBase);
s32 Player_ActionHandler_10(Player* player, PlayState* play);
bool Player_IsZTargeting(Player* player);
void Player_RequestRumble(PlayState* play, Player* player, s32 sourceIntensity, s32 decayTimer, s32 decayStep,
                          s32 distSq);
extern PlayerAnimationHeader* ResourceMgr_LoadPlayerAnimAsHeader(const char* animPath);
extern Gfx* ResourceMgr_LoadGfxByName(const char* path);
extern SkeletonHeader* ResourceMgr_LoadSkeletonByName(const char* path, SkelAnime* skelAnime);
extern HOST_DATA PlayerAgeProperties sPlayerAgeProperties[PLAYER_FORM_MAX];

static ColliderCylinderInit sShotCylinderInit = {
    { COL_MATERIAL_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_TYPE_PLAYER, COLSHAPE_CYLINDER },
    { ELEM_MATERIAL_UNK2,
      { DMG_SWORD, 0x00, 0x02 },
      { 0x00000000, 0x00, 0x00 },
      ATELEM_ON | ATELEM_NEAREST | ATELEM_SFX_NORMAL,
      ACELEM_NONE,
      OCELEM_NONE },
    { BALL_RADIUS_LEVEL1, 46, -24, { 0, 0, 0 } },
};

static bool IsOnGround(Player* player) {
    return (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) != 0;
}

static void LoadAnims(void) {
    if (sAnimsLoaded) {
        return;
    }
    sAnimsLoaded = true;
    sCombo[0] = (PlayerAnimationHeader*)gPlayerAnim_pg_punchA;
    sCombo[1] = (PlayerAnimationHeader*)gPlayerAnim_pg_punchB;
    sCombo[2] = ResourceMgr_LoadPlayerAnimAsHeader(sComboThirdPath);
    sCharge = ResourceMgr_LoadPlayerAnimAsHeader(sChargePath);
    sLongJump = ResourceMgr_LoadPlayerAnimAsHeader(sLongJumpPath);
    sAirKick = ResourceMgr_LoadPlayerAnimAsHeader(sAirKickPath);
    sReflectAnim = ResourceMgr_LoadPlayerAnimAsHeader(sReflectPath);
    sPunchEffect = ResourceMgr_LoadGfxByName(sPunchEffectDL);
}

// ---- bola de fuego ----

// Actor_Kill solo anula update(); el actor se libera al final del frame, así que un puntero guardado queda
// colgando. Recorrer la lista es lo que hace seguro seguirlo entre frames.
static Actor* ResolveFlame(PlayState* play) {
    if (sFlame == NULL) {
        return NULL;
    }
    for (Actor* it = play->actorCtx.actorLists[sFlameCategory].first; it != NULL; it = it->next) {
        if (it == sFlame) {
            return it->update != NULL ? it : NULL;
        }
    }
    sFlame = NULL;
    return NULL;
}

static void KillFlame(PlayState* play) {
    Actor* flame = ResolveFlame(play);

    if (flame != NULL) {
        Actor_Kill(flame);
    }
    sFlame = NULL;
}

static f32 BallScale(u8 level) {
    return level >= 2 ? BALL_SCALE_LEVEL2 : BALL_SCALE_LEVEL1;
}

// El color va horneado en los params de En_Light al spawnear, así que subir a naranja es reemplazar el
// actor, no recolorearlo.
static void SpawnFlame(PlayState* play, Vec3f* at, u8 level) {
    KillFlame(play);
    Actor* flame = Actor_Spawn(&play->actorCtx, play, ACTOR_EN_LIGHT, at->x, at->y, at->z, 0, 0, 0x4000,
                               level >= 2 ? FLAME_ORANGE : FLAME_BLUE);
    sFlame = flame;
    if (flame != NULL) {
        sFlameCategory = flame->category;
    }
}

static void PlaceFlame(PlayState* play, Vec3f* at, u8 level) {
    Actor* flame = ResolveFlame(play);

    if (flame == NULL) {
        return;
    }
    flame->world.pos = *at;
    Actor_SetScale(flame, BallScale(level));
}

// La bola es su propia aspiradora: a los enemigos los arrastra en vez de empujarlos, así que un grupo se
// junta en el punto que cubre el cilindro.
static void PullEnemies(PlayState* play) {
    for (Actor* it = play->actorCtx.actorLists[ACTORCAT_ENEMY].first; it != NULL; it = it->next) {
        if (it->update == NULL) {
            continue;
        }
        Vec3f toBall;
        f32 distance = Math_Vec3f_DistXYZAndStoreDiff(&it->world.pos, &sShotPos, &toBall);
        if (distance > sShotPullRadius || distance < 1.0f) {
            continue;
        }
        f32 step = PULL_STEP / distance;
        it->world.pos.x += toBall.x * step;
        it->world.pos.y += toBall.y * step;
        it->world.pos.z += toBall.z * step;
    }
}

static void SubmitShotCollider(PlayState* play, Player* player) {
    if (!sShotCylinderReady) {
        Collider_InitCylinder(play, &sShotCylinder);
        Collider_SetCylinder(play, &sShotCylinder, &player->actor, &sShotCylinderInit);
        sShotCylinderReady = true;
    }
    sShotCylinder.dim.radius = sShotLevel >= 2 ? BALL_RADIUS_LEVEL2 : BALL_RADIUS_LEVEL1;
    sShotCylinder.base.atFlags |= AT_ON;
    sShotCylinder.dim.pos.x = (s16)sShotPos.x;
    sShotCylinder.dim.pos.y = (s16)sShotPos.y;
    sShotCylinder.dim.pos.z = (s16)sShotPos.z;
    CollisionCheck_SetAT(play, &play->colChkCtx, &sShotCylinder.base);
}

// Recto a lo que Keaton tenga fijado, y si no, adonde mira. La cámara no vota: se va de Link mientras
// aguanta la carga.
static void AimShot(Player* player) {
    Actor* target = player->focusActor;
    bool lockedOnFoe = target != NULL && target->update != NULL &&
                       (target->category == ACTORCAT_ENEMY || target->category == ACTORCAT_BOSS);

    if (lockedOnFoe) {
        Vec3f toFoe;
        f32 distance = Math_Vec3f_DistXYZAndStoreDiff(&sShotPos, &target->focus.pos, &toFoe);
        if (distance > 1.0f) {
            f32 scale = SHOT_SPEED / distance;
            sShotStep.x = toFoe.x * scale;
            sShotStep.y = toFoe.y * scale;
            sShotStep.z = toFoe.z * scale;
            return;
        }
    }

    s16 yaw = player->actor.shape.rot.y;
    sShotStep.x = Math_SinS(yaw) * SHOT_SPEED;
    sShotStep.y = 0.0f;
    sShotStep.z = Math_CosS(yaw) * SHOT_SPEED;
}

static void LaunchShot(PlayState* play, Player* player, u8 level) {
    sShotPos = player->bodyPartsPos[PLAYER_BODYPART_RIGHT_HAND];
    AimShot(player);
    sShotTravelled = 0.0f;
    sShotRange = level >= 2 ? SHOT_RANGE_LEVEL2 : SHOT_RANGE_LEVEL1;
    sShotPullRadius = level >= 2 ? PULL_RADIUS_LEVEL2 : PULL_RADIUS_LEVEL1;
    sShotLevel = level;
    sShotActive = true;

    Player_PlaySfx(player, NA_SE_VO_LI_MAGIC_ATTACK);
    Player_PlaySfx(player, NA_SE_IT_MAGIC_ARROW_SHOT);
}

static void UpdateShot(PlayState* play, Player* player) {
    if (!sShotActive) {
        return;
    }

    sShotPos.x += sShotStep.x;
    sShotPos.y += sShotStep.y;
    sShotPos.z += sShotStep.z;
    sShotTravelled += SHOT_SPEED;

    PlaceFlame(play, &sShotPos, sShotLevel);
    PullEnemies(play);
    SubmitShotCollider(play, player);
    Audio_PlaySfx_AtPos(&sShotPos, NA_SE_EV_FIRE_PILLAR);

    if (sShotTravelled >= sShotRange || (sShotCylinder.base.atFlags & AT_HIT)) {
        sShotCylinder.base.atFlags &= ~(AT_ON | AT_HIT);
        sShotActive = false;
        KillFlame(play);
    }
}

// ---- trepar ----

static void LendClimbEverything(bool shouldLend) {
    sClimbLent = shouldLend;
}

static void AllowKeatonWall(CollisionContext* colCtx, CollisionPoly* poly, s32 bgId, u32* flags) {
    if (!sClimbLent || sReadingNaturalWall || gPlayState == NULL || !sApi->IsFormActive(KEATON_FORM_KEY)) {
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    if (colCtx == &gPlayState->colCtx && poly == player->actor.wallPoly && bgId == player->actor.wallBgId) {
        *flags |= WALL_FLAG_3;
    }
}

static void StopClimbLend(void) {
    LendClimbEverything(false);
    sClimbDrain = 0;
}

// Keaton trepa lo que sea mientras A esté pulsado, y trepar cuesta magia. El contacto con la pared solo
// decide si PUEDE empezar: volver a preguntarlo una vez está subido es lo que lo tiraba, porque
// el contacto desaparece entre asideros.
static void UpdateClimb(PlayState* play, Player* player) {
    bool climbing = (player->stateFlags1 & STATE1_CLIMBING) != 0;
    bool holdingA = CHECK_BTN_ALL(play->state.input[0].cur.button, BTN_A);

    if (climbing) {
        if (player->skelAnime.playSpeed > 0.0f) {
            player->skelAnime.playSpeed = CLIMB_RATE;
        } else if (player->skelAnime.playSpeed < 0.0f) {
            player->skelAnime.playSpeed = -CLIMB_RATE;
        }
    }

    bool canHold = climbing || ((player->actor.bgCheckFlags & BGCHECKFLAG_WALL) && player->actor.wallPoly != NULL);
    if (!holdingA || !canHold) {
        StopClimbLend();
        return;
    }

    // Query the surface without our own permission, preserving the user's cheats and other mods.
    if (player->actor.wallPoly != NULL) {
        sReadingNaturalWall = true;
        bool freeSurface =
            (SurfaceType_GetWallFlags(&play->colCtx, player->actor.wallPoly, player->actor.wallBgId) &
             WALL_FLAG_3) != 0;
        sReadingNaturalWall = false;
        if (freeSurface) {
            StopClimbLend();
            return;
        }
    }
    if (!gSaveContext.save.saveInfo.playerData.isMagicAcquired ||
        gSaveContext.save.saveInfo.playerData.magic < CLIMB_MAGIC_COST) {
        StopClimbLend();
        return;
    }

    sClimbDrain++;
    if (sClimbDrain >= CLIMB_DRAIN_INTERVAL) {
        sClimbDrain = 0;
        if (!Magic_Consume(play, CLIMB_MAGIC_COST, MAGIC_CONSUME_NOW)) {
            LendClimbEverything(false);
            return;
        }
    }
    LendClimbEverything(true);
}

// ---- reflector: el escudo de Keaton ----

// Un anillo hexagonal hueco, cuatro aros concéntricos de seis. El color va en los vértices: sin G_LIGHTING
// el sombreado ES el color del vértice, y G_CC_SHADE lo pasa tal cual sin textura.
static Vtx sHexVtx[24] = {
    VTX(22, 0, 0, 0, 0, 74, 148, 240, 255),     VTX(11, 19, 0, 0, 0, 74, 148, 240, 255),
    VTX(-11, 19, 0, 0, 0, 74, 148, 240, 255),   VTX(-22, 0, 0, 0, 0, 74, 148, 240, 255),
    VTX(-11, -19, 0, 0, 0, 74, 148, 240, 255),  VTX(11, -19, 0, 0, 0, 74, 148, 240, 255),

    VTX(20, 0, 0, 0, 0, 168, 218, 251, 255),    VTX(10, 17, 0, 0, 0, 168, 218, 251, 255),
    VTX(-10, 17, 0, 0, 0, 168, 218, 251, 255),  VTX(-20, 0, 0, 0, 0, 168, 218, 251, 255),
    VTX(-10, -17, 0, 0, 0, 168, 218, 251, 255), VTX(10, -17, 0, 0, 0, 168, 218, 251, 255),

    VTX(14, 0, 0, 0, 0, 150, 206, 250, 255),    VTX(7, 12, 0, 0, 0, 150, 206, 250, 255),
    VTX(-7, 12, 0, 0, 0, 150, 206, 250, 255),   VTX(-14, 0, 0, 0, 0, 150, 206, 250, 255),
    VTX(-7, -12, 0, 0, 0, 150, 206, 250, 255),  VTX(7, -12, 0, 0, 0, 150, 206, 250, 255),

    VTX(11, 0, 0, 0, 0, 120, 190, 248, 0),      VTX(6, 10, 0, 0, 0, 120, 190, 248, 0),
    VTX(-6, 10, 0, 0, 0, 120, 190, 248, 0),     VTX(-11, 0, 0, 0, 0, 120, 190, 248, 0),
    VTX(-6, -10, 0, 0, 0, 120, 190, 248, 0),    VTX(6, -10, 0, 0, 0, 120, 190, 248, 0),
};

static Gfx sHexDL[] = {
    gsDPPipeSync(),
    gsSPTexture(0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_OFF),
    gsDPSetCombineMode(G_CC_SHADE, G_CC_SHADE),
    gsSPClearGeometryMode(G_CULL_BOTH | G_FOG | G_LIGHTING | G_TEXTURE_GEN | G_TEXTURE_GEN_LINEAR),
    // G_SHADE es lo que hace que el color de vértice exista: sin él el combinador se come lo que quedara.
    gsSPSetGeometryMode(G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH),
    gsSPVertex(sHexVtx, 24, 0),

    gsSP2Triangles(0, 6, 7, 0, 0, 7, 1, 0),   gsSP2Triangles(1, 7, 8, 0, 1, 8, 2, 0),
    gsSP2Triangles(2, 8, 9, 0, 2, 9, 3, 0),   gsSP2Triangles(3, 9, 10, 0, 3, 10, 4, 0),
    gsSP2Triangles(4, 10, 11, 0, 4, 11, 5, 0), gsSP2Triangles(5, 11, 6, 0, 5, 6, 0, 0),

    gsSP2Triangles(6, 12, 13, 0, 6, 13, 7, 0), gsSP2Triangles(7, 13, 14, 0, 7, 14, 8, 0),
    gsSP2Triangles(8, 14, 15, 0, 8, 15, 9, 0), gsSP2Triangles(9, 15, 16, 0, 9, 16, 10, 0),
    gsSP2Triangles(10, 16, 17, 0, 10, 17, 11, 0), gsSP2Triangles(11, 17, 12, 0, 11, 12, 6, 0),

    gsSP2Triangles(12, 18, 19, 0, 12, 19, 13, 0), gsSP2Triangles(13, 19, 20, 0, 13, 20, 14, 0),
    gsSP2Triangles(14, 20, 21, 0, 14, 21, 15, 0), gsSP2Triangles(15, 21, 22, 0, 15, 22, 16, 0),
    gsSP2Triangles(16, 22, 23, 0, 16, 23, 17, 0), gsSP2Triangles(17, 23, 18, 0, 17, 18, 12, 0),

    gsSPEndDisplayList(),
};

static ColliderCylinderInit sGuardInit = {
    { COL_MATERIAL_METAL, AT_NONE, AC_ON | AC_HARD | AC_TYPE_ENEMY, OC1_NONE, OC2_TYPE_PLAYER, COLSHAPE_CYLINDER },
    { ELEM_MATERIAL_UNK0, { 0x00000000, 0x00, 0x00 }, { 0xFFCFFFFF, 0x00, 0x00 }, ATELEM_NONE, ACELEM_ON, OCELEM_NONE },
    { BURST_RADIUS, BURST_HEIGHT, -40, { 0, 0, 0 } },
};

static ColliderCylinderInit sBurstInit = {
    { COL_MATERIAL_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_TYPE_PLAYER, COLSHAPE_CYLINDER },
    { ELEM_MATERIAL_UNK2,
      { DMG_LIGHT_ARROW, 0x00, 0x02 },
      { 0x00000000, 0x00, 0x00 },
      ATELEM_ON | ATELEM_NEAREST | ATELEM_SFX_NORMAL,
      ACELEM_NONE,
      OCELEM_NONE },
    { BURST_RADIUS, BURST_HEIGHT, -40, { 0, 0, 0 } },
};

// Devuelve el ataque a quien lo lanzó, a la altura de su focus: el ojo en un Beamos y la cabeza en todo lo
// demás, que es donde tiene que llegar un rayo devuelto. Lleva el bit de nuez Deku, así que aturde.
static void PunishAttacker(PlayState* play, Player* player, Actor* attacker) {
    if (attacker == NULL || attacker->update == NULL || !sBurstReady) {
        return;
    }
    sBurst.elem.atDmgInfo.dmgFlags = DMG_DEKU_NUT | DMG_LIGHT_ARROW;
    sBurst.elem.atDmgInfo.damage = BURST_DAMAGE;
    sBurst.base.atFlags |= AT_ON;
    sBurst.dim.pos.x = (s16)attacker->focus.pos.x;
    sBurst.dim.pos.y = (s16)attacker->focus.pos.y;
    sBurst.dim.pos.z = (s16)attacker->focus.pos.z;
    CollisionCheck_SetAT(play, &play->colChkCtx, &sBurst.base);
    Player_PlaySfx(player, NA_SE_IT_SHIELD_REFLECT_MG2);
}

// El reflector tiene que SER GOLPEADO para devolver un rayo: un Beamos no dispara nada, su láser son dos
// quads suyos estirados hasta Link, así que no hay proyectil al que dar la vuelta. Un colisionador AC sí lo
// recoge, y base.ac dice quién fue.
static void SubmitGuard(PlayState* play, Player* player) {
    if (!sGuardReady) {
        Collider_InitCylinder(play, &sGuard);
        Collider_SetCylinder(play, &sGuard, &player->actor, &sGuardInit);
        sGuardReady = true;
    }
    if (sGuard.base.acFlags & AC_HIT) {
        sGuard.base.acFlags &= ~AC_HIT;
        PunishAttacker(play, player, sGuard.base.ac);
        sGuard.base.ac = NULL;
    }

    // El cilindro de Link está dentro del guard, así que un rayo que llega le da a los DOS: detectar el
    // ataque no vale de nada si además lo recibe. Bajarle el AC ese frame es lo que hace que bloquee.
    player->cylinder.base.acFlags &= ~(AC_ON | AC_HIT);

    sGuard.base.acFlags |= AC_ON;
    sGuard.dim.pos.x = (s16)player->actor.world.pos.x;
    sGuard.dim.pos.y = (s16)player->actor.world.pos.y;
    sGuard.dim.pos.z = (s16)player->actor.world.pos.z;
    CollisionCheck_SetAC(play, &play->colChkCtx, &sGuard.base);
}

static void SubmitBurst(PlayState* play, Player* player) {
    if (!sBurstReady) {
        Collider_InitCylinder(play, &sBurst);
        Collider_SetCylinder(play, &sBurst, &player->actor, &sBurstInit);
        sBurstReady = true;
    }
    sBurst.elem.atDmgInfo.dmgFlags = DMG_LIGHT_ARROW;
    sBurst.elem.atDmgInfo.damage = BURST_DAMAGE;
    sBurst.base.atFlags |= AT_ON;
    sBurst.dim.pos.x = (s16)player->actor.world.pos.x;
    sBurst.dim.pos.y = (s16)player->actor.world.pos.y;
    sBurst.dim.pos.z = (s16)player->actor.world.pos.z;
    CollisionCheck_SetAT(play, &play->colChkCtx, &sBurst.base);
}

static void RememberReflected(Actor* actor) {
    for (s32 i = 0; i < REFLECTED_MAX; i++) {
        if (sReflected[i].actor == actor) {
            sReflected[i].timer = REFLECTED_LIFE;
            return;
        }
    }
    for (s32 i = 0; i < REFLECTED_MAX; i++) {
        if (sReflected[i].timer <= 0) {
            sReflected[i].actor = actor;
            sReflected[i].timer = REFLECTED_LIFE;
            return;
        }
    }
}

static void TickReflected(void) {
    for (s32 i = 0; i < REFLECTED_MAX; i++) {
        if (sReflected[i].timer > 0) {
            sReflected[i].timer--;
            if (sReflected[i].timer == 0) {
                sReflected[i].actor = NULL;
            }
        }
    }
}

// El colisionador de un proyectil vive en su struct privada, pero cada actor se lo entrega al sistema de
// colisiones cada frame: colChkCtx.colAT ES la forma genérica de alcanzarlo. Pasarlo de daño-a-enemigo a
// daño-a-jugador es lo que hace que un tiro devuelto mate a quien lo lanzó, y este hook corre justo en la
// ventana entre que el proyectil se registra y el siguiente CollisionCheck_AT.
static void RealignReflectedProjectile(Actor* actor) {
    PlayState* play = gPlayState;

    if (actor == NULL || play == NULL) {
        return;
    }
    bool known = false;
    for (s32 i = 0; i < REFLECTED_MAX; i++) {
        if (sReflected[i].timer > 0 && sReflected[i].actor == actor) {
            known = true;
            break;
        }
    }
    if (!known) {
        return;
    }
    for (s32 i = 0; i < play->colChkCtx.colATCount; i++) {
        Collider* collider = play->colChkCtx.colAT[i];

        if (collider != NULL && collider->actor == actor) {
            collider->atFlags &= ~AT_TYPE_ENEMY;
            collider->atFlags |= AT_TYPE_PLAYER;
        }
    }
}

static void ReflectOne(Actor* actor) {
    actor->world.rot.y += 0x8000;
    actor->shape.rot.y = actor->world.rot.y;
    actor->world.rot.x = -actor->world.rot.x;
    actor->velocity.x = -actor->velocity.x;
    actor->velocity.z = -actor->velocity.z;

    if (actor->parent != NULL) {
        actor->world.rot.y = Actor_WorldYawTowardActor(actor, actor->parent);
        actor->shape.rot.y = actor->world.rot.y;
    }
}

// Los proyectiles están repartidos en cuatro categorías: flechas y magia son ITEMACTION, las bombas
// EXPLOSIVES, las nueces PROP, y la piedra de un Octorok es ENEMY como el propio Octorok. Las dos últimas
// listas están llenas de cosas que NO hay que voltear, así que ahí se exige que vuelen y rápido.
typedef struct {
    s32 category;
    bool flyingOnly;
} SweepRule;

static const SweepRule sSweep[] = {
    { ACTORCAT_ITEMACTION, false },
    { ACTORCAT_EXPLOSIVES, false },
    { ACTORCAT_PROP, true },
    { ACTORCAT_ENEMY, true },
};

static void ReflectSweep(PlayState* play, Player* player) {
    for (u32 rule = 0; rule < ARRAY_COUNT(sSweep); rule++) {
        for (Actor* it = play->actorCtx.actorLists[sSweep[rule].category].first; it != NULL; it = it->next) {
            if (it->update == NULL || it == &player->actor) {
                continue;
            }
            if (sSweep[rule].flyingOnly) {
                if ((it->bgCheckFlags & BGCHECKFLAG_GROUND) || it->speed < PROJECTILE_SPEED) {
                    continue;
                }
            } else if (it->speed <= 0.0f) {
                continue;
            }

            Vec3f toPlayer;
            f32 distance = Math_Vec3f_DistXYZAndStoreDiff(&it->world.pos, &player->actor.world.pos, &toPlayer);
            if (distance > REFLECT_RADIUS || distance < 0.1f) {
                continue;
            }

            // Solo se voltea lo que viene hacia aquí. Eso es además lo que impide que un tiro ya devuelto
            // se vuelva a girar cada frame que siga dentro del radio: al alejarse la prueba falla sola.
            f32 approach = (it->velocity.x * toPlayer.x) + (it->velocity.y * toPlayer.y) +
                           (it->velocity.z * toPlayer.z);
            if (approach <= 0.0f) {
                continue;
            }
            ReflectOne(it);
            RememberReflected(it);
            Player_PlaySfx(player, NA_SE_IT_SHIELD_REFLECT_MG);
        }
    }
}

static void OpenReflector(PlayState* play, Player* player) {
    sReflectorState = REFLECTOR_POPPING;
    sReflectorScale = 0.0f;
    sReflectorPulse = 0;

    if (sReflectAnim != NULL) {
        PlayerAnimation_Change(play, &player->skelAnime, sReflectAnim, 1.0f, 0.0f,
                               Animation_GetLastFrame(sReflectAnim), ANIMMODE_LOOP, -6.0f);
    }
    // En el aire el escudo se come el impulso en vez de dejarlo correr.
    if (!IsOnGround(player)) {
        player->speedXZ *= AIR_DRAG;
        player->actor.velocity.y *= AIR_DRAG;
    }
    SubmitBurst(play, player);
    Player_PlaySfx(player, NA_SE_VO_LI_SWORD_N);
    Player_PlaySfx(player, NA_SE_IT_SHIELD_REFLECT_MG);
}

static void CloseReflector(Player* player) {
    sReflectorState = REFLECTOR_OFF;
    sReflectorScale = 0.0f;
    if (sBurstReady) {
        sBurst.base.atFlags &= ~(AT_ON | AT_HIT);
    }
    if (sGuardReady) {
        sGuard.base.acFlags &= ~(AC_ON | AC_HIT);
        sGuard.base.ac = NULL;
    }
}

static void ReflectorAction(Player* player, PlayState* play) {
    TickReflected();

    if (!CHECK_BTN_ALL(play->state.input[0].cur.button, BTN_R) || (player->stateFlags1 & STATE1_DAMAGED)) {
        CloseReflector(player);
        func_8083B1A0(player, play);
        return;
    }

    if (sReflectorState == REFLECTOR_POPPING) {
        sReflectorScale += POP_STEP;
        if (sReflectorScale >= 1.0f) {
            sReflectorScale = 1.0f;
            sReflectorState = REFLECTOR_HELD;
        }
    } else {
        sReflectorPulse = (sReflectorPulse + 1) % PULSE_PERIOD;
        sReflectorScale =
            PULSE_MIN + ((1.0f - PULSE_MIN) * (0.5f + (0.5f * Math_CosS(sReflectorPulse * (0x10000 / PULSE_PERIOD)))));
    }

    SubmitGuard(play, player);
    ReflectSweep(play, player);
    PlayerAnimation_Update(play, &player->skelAnime);

    // damage_idle03 camina su raíz, y jointTable[0] ES la traslación que usa el dibujo: hay que clavar las
    // dos en la base del esqueleto o el modelo se va andando.
    if (player->skelAnime.jointTable != NULL) {
        player->skelAnime.jointTable[0] = player->skelAnime.baseTransl;
        player->skelAnime.prevTransl = player->skelAnime.baseTransl;
    }
    player->speedXZ = 0.0f;
}

static void DrawReflector(PlayState* play, Player* player) {
    if (sReflectorState == REFLECTOR_OFF) {
        return;
    }

    OPEN_DISPS(play->state.gfxCtx);

    // Construido desde la posición del mundo y no desde la matriz del limb que está puesta: el espacio de
    // limb va en cientos de unidades, y un hexágono de 40 dibujado ahí sale del tamaño de un hueso.
    Matrix_Push();
    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    Matrix_Translate(player->actor.world.pos.x, player->actor.world.pos.y + REFLECTOR_HEIGHT,
                     player->actor.world.pos.z, MTXMODE_NEW);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    Matrix_Scale(sReflectorScale, sReflectorScale, sReflectorScale, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, sHexDL);
    Matrix_Pop();

    CLOSE_DISPS(play->state.gfxCtx);
}

// ---- moveset ----

static void ClearState(void) {
    sState = KEATON_IDLE;
    sStep = 0;
    sChargeTimer = 0;
    sChargeLevel = 0;
    sChargeReversing = false;
    sJumpDelay = 0;
    sKickBuffered = false;
    sFistsBlocked = false;
}

static void PlayMove(PlayState* play, Player* player, PlayerAnimationHeader* anim, f32 start, f32 end) {
    f32 speed = end >= start ? 1.0f : -1.0f;

    PlayerAnimation_Change(play, &player->skelAnime, anim, speed, start, end, ANIMMODE_ONCE, -6.0f);
}

// El rastro de los golpes. Link lo lleva en la espada, y Keaton pega con las manos: es el EffectBlure de un
// espadazo, uno por puño, encendido mientras los quads pueden dañar, y en el rojo del puñetazo goron de MM.
static void StartFistTrails(PlayState* play) {
    EffectBlureInit2 blure = {
        0, EFFECT_BLURE_ELEMENT_FLAG_8, 0, { 255, 0, 0, 255 }, { 255, 0, 0, 64 }, { 255, 128, 0, 0 },
        { 255, 128, 0, 0 }, 4, 0, EFF_BLURE_DRAW_MODE_SMOOTH, 0, { 255, 0, 0, 255 }, { 255, 0, 0, 64 },
    };

    for (s32 hand = 0; hand < 2; hand++) {
        if (sFistTrail[hand] >= 0) {
            continue;
        }
        Effect_Add(play, &sFistTrail[hand], EFFECT_BLURE2, 0, 0, &blure);
    }
}

static void StopFistTrails(PlayState* play) {
    for (s32 hand = 0; hand < 2; hand++) {
        if (sFistTrail[hand] < 0) {
            continue;
        }
        Effect_Destroy(play, sFistTrail[hand]);
        sFistTrail[hand] = -1;
    }
}

static void ArmFists(PlayState* play, Player* player) {
    func_80833728(player, 0, FIST_DMG_FLAGS, FIST_DAMAGE);
    func_80833728(player, 1, FIST_DMG_FLAGS, FIST_DAMAGE);
    StartFistTrails(play);
}

static void EndMove(PlayState* play, Player* player) {
    player->meleeWeaponState = PLAYER_MELEE_WEAPON_STATE_0;
    StopFistTrails(play);
    ClearState();
    func_8083B1A0(player, play);
}

static void EnterCharge(PlayState* play, Player* player) {
    sState = KEATON_CHARGE;
    sChargeTimer = 0;
    sChargeLevel = 0;
    sChargeReversing = false;
    player->meleeWeaponState = PLAYER_MELEE_WEAPON_STATE_0;
    PlayMove(play, player, sCharge, 0.0f, CHARGE_LOOP_END);
    Player_PlaySfx(player, NA_SE_IT_SWORD_CHARGE);
}

// PlayerAnimation tiene ONCE y LOOP pero no vaivén, así que cada tramo se relanza con el signo cambiado.
static void StepChargeLoop(PlayState* play, Player* player) {
    sChargeReversing = !sChargeReversing;
    if (sChargeReversing) {
        PlayMove(play, player, sCharge, CHARGE_LOOP_END, CHARGE_LOOP_START);
    } else {
        PlayMove(play, player, sCharge, CHARGE_LOOP_START, CHARGE_LOOP_END);
    }
}

static void UpdateCombo(PlayState* play, Player* player, Input* input) {
    f32 last = Animation_GetLastFrame(sCombo[sStep]);
    f32 frame = player->skelAnime.curFrame;

    if (sStep + 1 < COMBO_STEPS && frame >= last * CHAIN_OPENS && CHECK_BTN_ALL(input->press.button, BTN_B)) {
        sStep++;
        PlayMove(play, player, sCombo[sStep], 0.0f, Animation_GetLastFrame(sCombo[sStep]));
        return;
    }
    player->meleeWeaponState =
        (frame >= sComboHitStart[sStep] && frame <= sComboHitEnd[sStep]) ? PLAYER_MELEE_WEAPON_STATE_1
                                                                           : PLAYER_MELEE_WEAPON_STATE_0;

    // La carga solo se abre cuando el puñetazo ha terminado, como el spin con su espadazo: un toque siempre
    // compra el golpe entero, y encadenar suelta B de todas formas.
    if (PlayerAnimation_Update(play, &player->skelAnime)) {
        player->meleeWeaponState = PLAYER_MELEE_WEAPON_STATE_0;
        if (CHECK_BTN_ALL(input->cur.button, BTN_B) && sCharge != NULL) {
            EnterCharge(play, player);
            return;
        }
        EndMove(play, player);
    }
}

static void UpdateCharge(PlayState* play, Player* player, Input* input) {
    sChargeTimer++;
    u8 level = sChargeTimer >= CHARGE_LEVEL2_FRAMES ? 2 : (sChargeTimer >= CHARGE_LEVEL1_FRAMES ? 1 : 0);

    if (level != sChargeLevel) {
        sChargeLevel = level;
        SpawnFlame(play, &player->bodyPartsPos[PLAYER_BODYPART_RIGHT_HAND], level);
        Player_PlaySfx(player, NA_SE_IT_SWORD_CHARGE);
    }
    PlaceFlame(play, &player->bodyPartsPos[PLAYER_BODYPART_RIGHT_HAND], level);

    if (!CHECK_BTN_ALL(input->cur.button, BTN_B)) {
        s16 cost = level >= 2 ? CHARGE_MAGIC_LEVEL2 : CHARGE_MAGIC_LEVEL1;
        if (level == 0 || !Magic_Consume(play, cost, MAGIC_CONSUME_NOW)) {
            KillFlame(play);
            EndMove(play, player);
            return;
        }
        LaunchShot(play, player, level);
        sState = KEATON_THROW;
        PlayMove(play, player, sCharge, CHARGE_LOOP_END, Animation_GetLastFrame(sCharge));
        return;
    }

    if (PlayerAnimation_Update(play, &player->skelAnime)) {
        StepChargeLoop(play, player);
    }
}

// Un escudo para a Keaton en seco. En el aire además duele: la patada no tiene defensa propia, así que
// tirársela a un guardia cuesta salud y no solo distancia.
static void StartRecoil(PlayState* play, Player* player) {
    player->meleeWeaponState = PLAYER_MELEE_WEAPON_STATE_0;
    player->meleeWeaponQuads[0].base.atFlags &= ~(AT_ON | AT_BOUNCED);
    player->meleeWeaponQuads[1].base.atFlags &= ~(AT_ON | AT_BOUNCED);
    Player_RequestRumble(play, player, 180, 20, 100, 0);

    if (!IsOnGround(player)) {
        ClearState();
        player->actor.colChkInfo.damage = KICK_BLOCK_DAMAGE;
        func_80833B18(play, player, KICK_KNOCKBACK_RESPONSE, 4.0f, 5.0f, player->actor.shape.rot.y + 0x8000,
                      KICK_BLOCK_INVINCIBILITY);
        return;
    }

    sState = KEATON_RECOIL;
    PlayerAnimationHeader* rebound = (PlayerAnimationHeader*)gPlayerAnim_link_fighter_rebound;
    PlayMove(play, player, rebound, 0.0f, Animation_GetLastFrame(rebound));
    player->speedXZ = RECOIL_SPEED;
}

static void StartAirKick(PlayState* play, Player* player);

static void KeatonAction(Player* player, PlayState* play) {
    Input* input = &play->state.input[0];

    if ((sState == KEATON_COMBO || sState == KEATON_AIR_KICK) &&
        (sFistsBlocked || (player->meleeWeaponQuads[0].base.atFlags & AT_BOUNCED) ||
         (player->meleeWeaponQuads[1].base.atFlags & AT_BOUNCED))) {
        sFistsBlocked = false;
        StopFistTrails(play);
        StartRecoil(play, player);
        return;
    }

    // Lo mismo que hacen las acciones de espada de vanilla: de aquí salen el rebote contra un escudo, las
    // chispas contra la pared y la reacción al daño, sin duplicar nada.
    if (func_808401F4(play, player)) {
        ClearState();
        StopFistTrails(play);
        return;
    }

    switch (sState) {
        case KEATON_LONG_JUMP:
            if (IsOnGround(player) && player->actor.velocity.y <= 0.0f) {
                EndMove(play, player);
            } else if ((player->actor.bgCheckFlags & BGCHECKFLAG_WALL) && CHECK_BTN_ALL(input->cur.button, BTN_A)) {
                // Hand wall grabbing back to Link instead of freezing him in our jump clip.
                EndMove(play, player);
            } else if (sAirKick != NULL && (sKickBuffered || CHECK_BTN_ALL(input->press.button, BTN_B))) {
                StartAirKick(play, player);
            } else if (PlayerAnimation_Update(play, &player->skelAnime)) {
                EndMove(play, player);
            }
            break;

        case KEATON_COMBO:
            player->speedXZ = 0.0f;
            UpdateCombo(play, player, input);
            break;

        case KEATON_CHARGE:
            player->speedXZ = 0.0f;
            UpdateCharge(play, player, input);
            break;

        case KEATON_AIR_KICK: {
            // Como en NEI: tocar suelo solo cuenta al caer. Justo al arrancar la patada el jugador todavía
            // roza el suelo del que salta, y sin esto la patada muere en su primer frame.
            const bool landed = IsOnGround(player) && player->actor.velocity.y <= 0.0f;

            player->meleeWeaponState = PLAYER_MELEE_WEAPON_STATE_1;
            // func_808401F4 deja la velocidad en negativo cuando el puño da contra una pared: ahí la patada
            // acaba con el rebote de vanilla en vez de seguir empujando contra ella.
            if (player->speedXZ < 0.0f) {
                EndMove(play, player);
                break;
            }
            if (landed || PlayerAnimation_Update(play, &player->skelAnime)) {
                EndMove(play, player);
            } else {
                // The kick owns propulsion, independent of stick input or a lock-on target.
                player->speedXZ = AIR_KICK_SPEED;
                player->yaw = sAirKickYaw;
                player->actor.shape.rot.y = sAirKickYaw;
                player->stateFlags2 |= STATE2_NO_ROTATION;
            }
            break;
        }

        case KEATON_RECOIL:
            if (PlayerAnimation_Update(play, &player->skelAnime)) {
                EndMove(play, player);
            }
            break;

        default:
            player->speedXZ = 0.0f;
            if (PlayerAnimation_Update(play, &player->skelAnime)) {
                EndMove(play, player);
            }
            break;
    }
}

static bool IsKeatonBusy(Player* player) {
    return player->actionFunc == KeatonAction || player->actionFunc == ReflectorAction;
}

static bool CanAct(Player* player) {
    return !(player->stateFlags1 & BUSY_STATE1) && player->csAction == PLAYER_CSACTION_NONE;
}

static void StartAirKick(PlayState* play, Player* player) {
    sState = KEATON_AIR_KICK;
    sJumpDelay = 0;
    sKickBuffered = false;
    Player_SetAction(play, player, KeatonAction, 0);
    // Long jump and kick share an action, so SetAction can return before clearing old root motion.
    func_8082DC38(player);
    ArmFists(play, player);
    sAirKickYaw = player->actor.shape.rot.y;
    if (player->focusActor != NULL && player->focusActor->update != NULL) {
        sAirKickYaw = Math_Vec3f_Yaw(&player->actor.world.pos, &player->focusActor->world.pos);
    }
    player->yaw = sAirKickYaw;
    player->actor.shape.rot.y = sAirKickYaw;
    player->actor.world.rot.y = sAirKickYaw;
    player->speedXZ = AIR_KICK_SPEED;
    player->actor.speed = AIR_KICK_SPEED;
    player->meleeWeaponState = PLAYER_MELEE_WEAPON_STATE_1;
    PlayMove(play, player, sAirKick, 0.0f, Animation_GetLastFrame(sAirKick));
    Player_PlaySfx(player, NA_SE_IT_SWORD_SWING_HARD);
}

// B en el suelo: el combo. Es la acción de melee de Link, así que instalar aquí la action func es
// exactamente lo que hace vanilla con su espadazo.
static int32_t OnMeleeAction(PlayState* play, Player* player) {
    if (sCombo[0] == NULL || IsKeatonBusy(player) || !CanAct(player) ||
        !CHECK_BTN_ALL(play->state.input[0].press.button, BTN_B) || !IsOnGround(player)) {
        return S2H_FORM_ACTION_VANILLA;
    }

    sState = KEATON_COMBO;
    sStep = 0;
    ArmFists(play, player);
    Player_SetAction(play, player, KeatonAction, 0);
    PlayMove(play, player, sCombo[0], 0.0f, Animation_GetLastFrame(sCombo[0]));
    return S2H_FORM_ACTION_STARTED;
}

// Install the real vanilla airborne action; changing velocity alone leaves the grounded handler running.
static bool CanLongJump(PlayState* play, Player* player) {
    return sLongJump != NULL && !IsKeatonBusy(player) && CanAct(player) &&
           CHECK_BTN_ALL(play->state.input[0].press.button, BTN_A) && IsOnGround(player) &&
           !(player->actor.bgCheckFlags & BGCHECKFLAG_WALL);
}

static int32_t StartLongJump(PlayState* play, Player* player) {
    func_80834D50(play, player, sLongJump, LONG_JUMP_LIFT, NA_SE_VO_LI_AUTO_JUMP);
    sState = KEATON_LONG_JUMP;
    sJumpDelay = LONG_JUMP_ANIM_DELAY;
    sKickBuffered = false;
    player->speedXZ = LONG_JUMP_SPEED;
    player->meleeWeaponState = PLAYER_MELEE_WEAPON_STATE_0;
    player->actor.velocity.y = LONG_JUMP_LIFT;
    player->actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND;
    return S2H_FORM_ACTION_STARTED;
}

static int32_t OnRollAction(PlayState* play, Player* player) {
    const s8 stick = player->controlStickDirections[player->controlStickDataIndex];
    if (Player_IsZTargeting(player) && stick > PLAYER_STICK_DIR_FORWARD) {
        // Some vanilla action lists reach ROLL without visiting ZTARGET_A first.
        if (!IsKeatonBusy(player) && CanAct(player) && IsOnGround(player) &&
            Player_ActionHandler_10(player, play)) {
            return S2H_FORM_ACTION_STARTED;
        }
        return S2H_FORM_ACTION_VANILLA;
    }
    return CanLongJump(play, player) ? StartLongJump(play, player) : S2H_FORM_ACTION_VANILLA;
}

// R: el hexágono. Keaton no lleva escudo, así que la acción de escudo de Link es suya entera.
static int32_t OnShieldAction(PlayState* play, Player* player) {
    if (sReflectAnim == NULL || IsKeatonBusy(player) || !CanAct(player) ||
        !CHECK_BTN_ALL(play->state.input[0].cur.button, BTN_R)) {
        return S2H_FORM_ACTION_VANILLA;
    }
    Player_SetAction(play, player, ReflectorAction, 0);
    OpenReflector(play, player);
    return S2H_FORM_ACTION_STARTED;
}

// Con Z, un salto hacia un lado o hacia atrás es de Link: Keaton esquiva igual. Hacia delante Link daría
// su espadazo en salto, y Keaton no lleva espada: ahí salta en largo, como en NEI.
static int32_t OnZTargetAction(PlayState* play, Player* player) {
    const s8 stick = player->controlStickDirections[player->controlStickDataIndex];

    if (stick > PLAYER_STICK_DIR_FORWARD || !CanLongJump(play, player)) {
        return S2H_FORM_ACTION_VANILLA;
    }
    return StartLongJump(play, player);
}

static const S2HFormActionOverride sActions[] = {
    { SOH_PLAYER_ACTION_MELEE, OnMeleeAction },
    { SOH_PLAYER_ACTION_ROLL, OnRollAction },
    { SOH_PLAYER_ACTION_SHIELD, OnShieldAction },
    { SOH_PLAYER_ACTION_ZTARGET_A, OnZTargetAction },
};

// En el aire no hay lista de action handlers que valga: el salto lo conduce el motor, así que su animación y la
// patada que lo interrumpe se lanzan desde aquí, al final del frame, que es donde ya nadie las pisa.
static void UpdateAirMoves(PlayState* play, Player* player) {
    if (!CanAct(player)) {
        if (sState == KEATON_LONG_JUMP) {
            ClearState();
        }
        return;
    }
    if (sState == KEATON_LONG_JUMP) {
        sKickBuffered |= CHECK_BTN_ALL(play->state.input[0].press.button, BTN_B);
        if (sJumpDelay > 0) {
            sJumpDelay--;
            if (sJumpDelay == 0) {
                Player_SetAction(play, player, KeatonAction, 0);
                PlayMove(play, player, sLongJump, 0.0f, Animation_GetLastFrame(sLongJump));
                player->speedXZ = LONG_JUMP_SPEED;
                if (sKickBuffered && sAirKick != NULL && !IsOnGround(player)) {
                    StartAirKick(play, player);
                }
            }
            return;
        }
        if (IsOnGround(player) && player->actor.velocity.y <= 0.0f) {
            ClearState();
            return;
        }
        if (player->actionFunc == KeatonAction) {
            return;
        }
    }
    if (!sKickBuffered && !CHECK_BTN_ALL(play->state.input[0].press.button, BTN_B)) {
        return;
    }
    if (IsKeatonBusy(player) || IsOnGround(player) || sAirKick == NULL) {
        return;
    }
    StartAirKick(play, player);
}

// Preserve collision results before the player's common update clears/re-registers the attack quads.
static void CaptureFistBlock(Player* player, Input* input) {
    sFistsBlocked = sApi->IsFormActive(KEATON_FORM_KEY) && (sState == KEATON_COMBO || sState == KEATON_AIR_KICK) &&
                    ((player->meleeWeaponQuads[0].base.atFlags | player->meleeWeaponQuads[1].base.atFlags) &
                     AT_BOUNCED);
}

// ---- colas ----

static SkeletonHeader* TailSkeleton(void) {
    if (!sTriedTailSkel) {
        sTriedTailSkel = 1;
        SkeletonHeader* header = ResourceMgr_LoadSkeletonByName(sTailSkelPath, NULL);
        // Un recurso que falta devuelve un puntero a otra cosa, y caminarlo revienta.
        if (header != NULL && header->limbCount > 0 && header->limbCount <= TAIL_SEGMENTS &&
            header->segment != NULL) {
            sTailSkel = header;
        }
    }
    return sTailSkel;
}

// Player_DrawImpl lee la cara que pone la animación en el joint 22, nibble bajo el ojo y en base 1.
static u8 TailPoseFor(Player* player) {
    s32 eye = (player->skelAnime.jointTable[PLAYER_LIMB_MAX].x & 0xF) - 1;

    if (eye < 0) {
        return sTailPose;
    }
    if (eye == 5) {
        return 2;
    }
    if (eye >= 6) {
        return 1;
    }
    return 0;
}

static void UpdateTails(Player* player) {
    SkeletonHeader* skeleton = TailSkeleton();

    if (skeleton == NULL) {
        return;
    }
    u8 pose = TailPoseFor(player);
    if (pose != sTailPose) {
        sTailPose = pose;
        sTailFrame = 0;
    }
    sTailFrame = (sTailFrame + 1) % (u32)kTailClipFrames[sTailPose];
    const s16(*clip)[3] = kTailClip[sTailPose][sTailFrame];

    // Suavizado, no asignado: al cambiar de clip las colas darían un salto, porque los tres arrancan de
    // una postura distinta.
    for (s32 i = 0; i < skeleton->limbCount && i < TAIL_SEGMENTS; i++) {
        s16 x = (s16)(clip[i][0] * TAIL_CLIP_AMOUNT);
        s16 y = (s16)(clip[i][1] * TAIL_CLIP_AMOUNT);
        s16 z = (s16)(clip[i][2] * TAIL_CLIP_AMOUNT);

        sTail[i].x += (x - sTail[i].x) >> 2;
        sTail[i].y += (y - sTail[i].y) >> 2;
        sTail[i].z += (z - sTail[i].z) >> 2;
    }
}

static void DrawTailChain(PlayState* play, StandardLimb** limbs, s32 count, s32 index, Mtx* matrices) {
    while (index != 0xFF && index < count) {
        StandardLimb* limb = limbs[index];

        Matrix_Push();
        Matrix_Translate(limb->jointPos.x, limb->jointPos.y, limb->jointPos.z, MTXMODE_APPLY);
        Matrix_RotateZYX(sTail[index].x, sTail[index].y, sTail[index].z, MTXMODE_APPLY);
        Matrix_ToMtx(&matrices[index]);
        if (limb->dList != NULL) {
            OPEN_DISPS(play->state.gfxCtx);
            gSPDisplayList(POLY_OPA_DISP++, limb->dList);
            CLOSE_DISPS(play->state.gfxCtx);
        }
        if (limb->child != 0xFF) {
            DrawTailChain(play, limbs, count, limb->child, matrices);
        }
        Matrix_Pop();
        index = limb->sibling;
    }
}

// Las tres colas cuelgan de la cintura. No son limbs (21 es el techo del rig del jugador): son su propia
// cadena con sus propias matrices, y este es el único momento en que la cintura es la matriz actual.
static void DrawTails(PlayState* play) {
    SkeletonHeader* skeleton = TailSkeleton();

    if (skeleton == NULL) {
        return;
    }
    Mtx* matrices = (Mtx*)GRAPH_ALLOC(play->state.gfxCtx, skeleton->limbCount * sizeof(Mtx));
    if (matrices == NULL) {
        return;
    }

    OPEN_DISPS(play->state.gfxCtx);
    // 0x0B: el 0x0D se queda con el esqueleto del jugador durante todo su dibujo.
    gSPSegment(POLY_OPA_DISP++, TAIL_MATRIX_SEGMENT, (uintptr_t)matrices);
    CLOSE_DISPS(play->state.gfxCtx);

    DrawTailChain(play, (StandardLimb**)skeleton->segment, skeleton->limbCount, 0, matrices);
}

// Keaton pega con las manos vacías, así que la colocación de quads de z_player_lib —que cuelga de un arma
// empuñada— nunca corre para él: van desde las matrices de las manos, con alcance de puño.
static void UpdateFistQuads(PlayState* play, Player* player, s32 limbIndex) {
    static const Vec3f sFistBases[3] = { { 0.0f, 400.0f, 0.0f }, { 0.0f, 1400.0f, -1000.0f },
                                         { 0.0f, -400.0f, 1000.0f } };
    s32 quadIndex = limbIndex == PLAYER_LIMB_LEFT_HAND ? 0 : 1;
    bool fistsLive = (sState == KEATON_COMBO || sState == KEATON_AIR_KICK) &&
                     player->meleeWeaponState != PLAYER_MELEE_WEAPON_STATE_0;

    if (!fistsLive) {
        player->meleeWeaponQuads[quadIndex].base.atFlags &= ~AT_ON;
        return;
    }

    Vec3f tipLocal = { FIST_REACH, sFistBases[quadIndex + 1].y, sFistBases[quadIndex + 1].z };
    Vec3f baseLocal = sFistBases[quadIndex + 1];
    Vec3f tip;
    Vec3f base;

    Matrix_MultVec3f(&tipLocal, &tip);
    Matrix_MultVec3f(&baseLocal, &base);
    func_80833728(player, quadIndex, FIST_DMG_FLAGS, FIST_DAMAGE);
    player->meleeWeaponQuads[quadIndex].base.atFlags |= AT_ON;

    WeaponInfo* fist = &player->meleeWeaponInfo[quadIndex + 1];
    if (func_80126440(play, &player->meleeWeaponQuads[quadIndex], fist, &tip, &base) && sFistTrail[quadIndex] >= 0) {
        EffectBlure_AddVertex((EffectBlure*)Effect_GetByIndex(sFistTrail[quadIndex]), &fist->tip, &fist->base);
    }
    // func_808401F4 busca la pared con meleeWeaponInfo[0], la punta de la espada, que Keaton nunca mueve: sin
    // esto prueba donde quedó el último espadazo de Link y, si ahí hay geometría, frena cada golpe.
    if (quadIndex == 0) {
        player->meleeWeaponInfo[0] = *fist;
    }
}

// sPunchFrames lo lleva el update, que corre antes del dibujo: el primer frame golpeado ya vale 1.
static bool ArePunchEffectsLive(void) {
    return sPunchEffect != NULL && sPunchFrames >= 1 && sPunchFrames <= (s32)ARRAY_COUNT(kPunchAlpha);
}

// Se dibuja con la matriz de la mano puesta, que es donde MM lo dibuja, y esa misma matriz entra en el
// anillo: el rastro no es más que este destello repintado donde estaba el puño en los frames anteriores.
static void DrawPunchEffect(PlayState* play, s32 hand) {
    if (!ArePunchEffectsLive()) {
        return;
    }
    const u8 alpha = kPunchAlpha[sPunchFrames - 1];

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gDPSetEnvColor(POLY_XLU_DISP++, 255, 0, 0, alpha);
    gSPDisplayList(POLY_XLU_DISP++, sPunchEffect);
    CLOSE_DISPS(play->state.gfxCtx);

    PunchGhost* ghost = &sPunchGhost[hand][sPunchGhostNext[hand]];

    Matrix_Get(&ghost->matrix);
    ghost->alpha = alpha;
    sPunchGhostNext[hand] = (sPunchGhostNext[hand] + 1) % PUNCH_GHOSTS;
}

// El que todavía vale 255 es el destello vivo, que ya se dibujó en su sitio: repintarlo solo lo ensucia.
static void DrawPunchGhosts(PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);

    for (s32 hand = 0; hand < 2; hand++) {
        for (s32 i = 0; i < PUNCH_GHOSTS; i++) {
            PunchGhost* ghost = &sPunchGhost[hand][i];

            if (sPunchEffect == NULL || ghost->alpha == 0 || ghost->alpha == 255) {
                continue;
            }
            Matrix_Push();
            Matrix_Put(&ghost->matrix);
            Gfx_SetupDL25_Xlu(play->state.gfxCtx);
            MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
            gDPSetEnvColor(POLY_XLU_DISP++, 255, 0, 0, ghost->alpha);
            gSPDisplayList(POLY_XLU_DISP++, sPunchEffect);
            Matrix_Pop();
        }
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

static void UpdatePunchEffect(Player* player) {
    const bool striking = (sState == KEATON_COMBO || sState == KEATON_AIR_KICK) &&
                          player->meleeWeaponState != PLAYER_MELEE_WEAPON_STATE_0;

    sPunchFrames = striking ? sPunchFrames + 1 : 0;

    for (s32 hand = 0; hand < 2; hand++) {
        for (s32 i = 0; i < PUNCH_GHOSTS; i++) {
            u8* alpha = &sPunchGhost[hand][i].alpha;

            *alpha = *alpha <= PUNCH_GHOST_FADE ? 0 : *alpha - PUNCH_GHOST_FADE;
        }
    }
}

static void ClearPunchEffect(void) {
    memset(sPunchGhost, 0, sizeof(sPunchGhost));
    sPunchGhostNext[0] = 0;
    sPunchGhostNext[1] = 0;
    sPunchFrames = 0;
}

// Keaton habla con el banco de voz Deku de MM. La voz de Link es un bloque de 0x20 acciones que empieza en
// NA_SE_VO_LI_SWORD_N, y cada forma suma el desplazamiento de su banco.
static void SpeakWithDekuVoice(Actor* actor, s32 kind, u16* sfxId, bool* handled) {
    if (kind != SOH_ACTOR_SFX_VOICE || !sApi->IsFormActive(KEATON_FORM_KEY)) {
        return;
    }
    const u16 action = (u16)(*sfxId - NA_SE_VO_LI_SWORD_N);
    if (action >= LINK_VOICE_ACTIONS) {
        return;
    }
    Player_PlaySfx((Player*)actor, sPlayerAgeProperties[PLAYER_FORM_DEKU].voiceSfxIdOffset + *sfxId);
    *handled = true;
}

// Vanilla resolves these limbs to loaded Link display lists, not resource names. Keep the
// form's waist and select our own hands explicitly, including the adult rig used by child Link.
static void ResolveKeatonBody(Player* player, s32 limbIndex, Gfx** dList, Gfx* limbDList, Vec3f* pos) {
    if (!sApi->IsFormActive(KEATON_FORM_KEY) || dList == NULL || *dList == NULL) {
        return;
    }
    const char* path = NULL;
    if (limbIndex == PLAYER_LIMB_WAIST) {
        *dList = limbDList;
        return;
    }
    const bool running = player->actor.speed > 2.0f && !(player->stateFlags1 & STATE1_SWIMMING);
    if (limbIndex == PLAYER_LIMB_LEFT_HAND) {
        path = player->leftHandType == PLAYER_MODELTYPE_LH_CLOSED || running ? sLeftHandClosedDL : sLeftHandOpenDL;
    } else if (limbIndex == PLAYER_LIMB_RIGHT_HAND) {
        if (player->rightHandType == PLAYER_MODELTYPE_RH_INSTRUMENT) {
            path = sFluteHandDL;
        } else {
            path = player->rightHandType == PLAYER_MODELTYPE_RH_CLOSED || running ? sRightHandClosedDL
                                                                                   : sRightHandOpenDL;
        }
    }
    if (path != NULL && sApi->HasResource(path)) {
        *dList = ResourceMgr_LoadGfxByName(path);
    }
}

static void DrawKeatonParts(Player* player, s32 limbIndex) {
    PlayState* play = gPlayState;

    if (!sApi->IsFormActive(KEATON_FORM_KEY)) {
        return;
    }
    if (limbIndex == PLAYER_LIMB_WAIST) {
        DrawTails(play);
        DrawReflector(play, player);
        DrawPunchGhosts(play);
    }
    if (limbIndex == PLAYER_LIMB_LEFT_HAND || limbIndex == PLAYER_LIMB_RIGHT_HAND) {
        UpdateFistQuads(play, player, limbIndex);
        DrawPunchEffect(play, limbIndex == PLAYER_LIMB_LEFT_HAND ? 0 : 1);
    }
}

// Un zorro ágil pero pequeño: arbustos y flores bomba, como el Zora; nada de rocas plateadas.
static void ResolveKeatonStrength(bool* should, va_list args) {
    s32* strength = va_arg(args, s32*);

    if (sApi->IsFormActive(KEATON_FORM_KEY)) {
        *strength = PLAYER_STRENGTH_ZORA;
    }
}

// ---- flauta ----

static bool sIsFluteOut;

static bool IsHoldingOcarina(Player* player) {
    return player->rightHandType == PLAYER_MODELTYPE_RH_INSTRUMENT;
}

// La flauta de Skull Kid suena con el instrumento al que el propio juego cambia en su minijuego. Se
// reescribe cada frame: el sistema de mensajes vuelve a poner el suyo al abrirse, y gana la última escritura.
static void UpdateFluteVoice(PlayState* play, Player* player) {
    bool isSessionOpen = play->msgCtx.msgMode != MSGMODE_NONE;

    if (IsHoldingOcarina(player)) {
        sIsFluteOut = true;
    } else if (!isSessionOpen) {
        if (sIsFluteOut) {
            AudioOcarina_SetInstrument(OCARINA_INSTRUMENT_OFF);
        }
        sIsFluteOut = false;
    }
    if (sIsFluteOut && isSessionOpen) {
        AudioOcarina_SetInstrument(OCARINA_INSTRUMENT_TATL);
    }
}

static void PutFluteAway(void) {
    if (sIsFluteOut) {
        AudioOcarina_SetInstrument(OCARINA_INSTRUMENT_OFF);
        sIsFluteOut = false;
    }
}

// ---- forma ----

static void EnterKeaton(PlayState* play, Player* player) {
    ClearPunchEffect();
    memset(sTail, 0, sizeof(sTail));
    sTailFrame = 0;
    sTailPose = 0;
    ClearState();
    LoadAnims();
}

static void ExitKeaton(PlayState* play, Player* player) {
    ClearPunchEffect();
    LendClimbEverything(false);
    KillFlame(play);
    StopFistTrails(play);
    CloseReflector(player);
    sShotActive = false;
    PutFluteAway();
    ClearState();
}

// Los colisionadores guardan el actor al que se ataron, y una escena nueva trae un jugador nuevo: hay que
// volver a montarlos o se está apuntando a memoria de la escena anterior.
static void ForgetScene(s16 sceneId, s8 spawnNum) {
    LendClimbEverything(false);
    sClimbDrain = 0;
    ClearPunchEffect();
    sGuardReady = false;
    sBurstReady = false;
    sShotCylinderReady = false;
    sShotActive = false;
    sFlame = NULL;
    sReflectorState = REFLECTOR_OFF;
    // Los efectos son de la escena que se va: sus índices ya no valen.
    sFistTrail[0] = -1;
    sFistTrail[1] = -1;
    memset(sReflected, 0, sizeof(sReflected));
    ClearState();
}

static void UpdateKeaton(PlayState* play, Player* player) {
    if (sState != KEATON_IDLE && sState != KEATON_LONG_JUMP && !IsKeatonBusy(player)) {
        // Damage/cutscenes may replace the action without going through EndMove.
        StopFistTrails(play);
        ClearState();
    }
    UpdatePunchEffect(player);
    UpdateShot(play, player);
    UpdateTails(player);
    UpdateClimb(play, player);
    UpdateAirMoves(play, player);
    UpdateFluteVoice(play, player);
}

// ---- máscara ----

static bool CanWearMask(Player* player, PlayState* play) {
    return IsOnGround(player) && !(player->stateFlags1 & (STATE1_SWIMMING | STATE1_FIRST_PERSON | PLAYER_STATE1_20000000));
}

static void WearMask(PlayState* play, Player* player) {
    sApi->ToggleForm(KEATON_FORM_KEY);
}

// Shuffled once by the randomizer; no vanilla logic needs it.
static const S2HCustomItemRandomizer sRandomizerItem = {
    sizeof(S2HCustomItemRandomizer), 0, S2H_CUSTOM_ITEM_TYPE_ITEM, 0, 1, KEATON_MASK_KEY, KEATON_MASK_KEY, "",
};

static void RegisterMask(void) {
    S2HCustomItemDefinition mask = Z64Items_Define(KEATON_MASK_KEY, sIconTex, sNameTex);

    Z64Items_SetButtons(&mask, S2H_CUSTOM_ITEM_C_BUTTON | S2H_CUSTOM_ITEM_DPAD);
    mask.flags |= S2H_CUSTOM_ITEM_INSTANT | S2H_CUSTOM_ITEM_WEARABLE;
    Z64Items_SetPlacement(&mask, KEATON_MASK_PAGE, KEATON_MASK_SLOT, 0);
    Z64Items_SetReplaces(&mask, ITEM_MASK_KEATON);
    Z64Items_SetVanillaMode(&mask, S2H_VANILLA_ITEM_REPLACE, RI_MASK_KEATON);
    Z64Items_SetGetItemModel(&mask, sGetItemMaskDL);
    Z64Items_SetTextbox(&mask, "You got the %rKeaton Mask%w!&The gate guard paid you for a&look at it and let you "
                               "keep it.^"
                               "Wear it with %y\xA1%w and you don't just&look like Keaton: you become him.");
    Z64Items_SetPauseText(&mask, "%rKeaton Mask&%wPress %y\xA1%w to become Keaton.&As Keaton: %y\xA1%w punches, held "
                                 "%y\xA1%w&throws fire, %y\xA0%w leaps and climbs.");
    Z64Items_SetCanUse(&mask, CanWearMask);
    Z64Items_SetAction(&mask, WearMask, NULL);

    Z64Items_SetLogic(&mask, &sRandomizerItem);
    Z64Items_Register(sApi, &mask);
}

// Keaton pelea con las manos: ni espada ni escudo. La túnica y las botas se quedan como las lleve Link.
static uint16_t ResolveKeatonEquipment(int32_t equipType, uint16_t value) {
    if (equipType == EQUIP_TYPE_SWORD) {
        return EQUIP_VALUE_SWORD_NONE;
    }
    if (equipType == EQUIP_TYPE_SHIELD) {
        return EQUIP_VALUE_SHIELD_NONE;
    }
    return value;
}

// De los items de Link solo toca la ocarina, que en sus manos es la flauta. Su máscara la mantiene el host.
static bool AllowsKeatonItem(uint16_t item, const char* customKey) {
    return item == ITEM_OCARINA_FAIRY || item == ITEM_OCARINA_OF_TIME;
}

static void RegisterForm(void) {
    S2HFormDefinition keaton = { 0 };

    keaton.structSize = sizeof(keaton);
    keaton.key = KEATON_FORM_KEY;
    keaton.label = "Keaton";
    keaton.kind = S2H_FORM_KIND_LINK;
    keaton.item = KEATON_MASK_KEY;
    keaton.modelPath = KEATON_MODEL_PATH;
    keaton.rootScaleAdult = KEATON_ROOT_SCALE_ADULT;
    keaton.rootScaleChild = KEATON_ROOT_SCALE_CHILD;
    keaton.rootDrop = KEATON_ROOT_DROP;
    keaton.resolveEquipment = ResolveKeatonEquipment;
    keaton.allowsButtonItem = AllowsKeatonItem;
    keaton.transformMask = sMaskDL;
    keaton.transformVoiceSfx = NA_SE_VO_LI_FALL_L;
    keaton.actions = sActions;
    keaton.actionCount = ARRAY_COUNT(sActions);
    keaton.onEnter = EnterKeaton;
    keaton.onExit = ExitKeaton;
    keaton.update = UpdateKeaton;

    sApi->RegisterForm(&keaton);
}

S2H_MOD_EXPORT void ModSetApi(const S2HModApi* api) {
    sApi = api;
}

S2H_MOD_EXPORT const S2HModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

S2H_MOD_EXPORT void ModInit(void) {
    LoadAnims();
    RegisterMask();
    RegisterForm();
    S2H_REGISTER_HOOK(sApi, OnPlayerPostLimbDraw, DrawKeatonParts);
    S2H_REGISTER_HOOK(sApi, OnPlayerResolveLimbDraw, ResolveKeatonBody);
    // El id de este hook es el del actor, no el tipo de sonido: el tipo se mira dentro.
    S2H_REGISTER_HOOK_FOR_ID(sApi, OnActorPlaySfx, ACTOR_PLAYER, SpeakWithDekuVoice);
    S2H_REGISTER_HOOK(sApi, OnActorUpdate, RealignReflectedProjectile);
    S2H_REGISTER_HOOK(sApi, OnSceneInit, ForgetScene);
    S2H_REGISTER_HOOK(sApi, OnPlayerFilterInput, CaptureFistBlock);
    S2H_REGISTER_HOOK(sApi, OnBgCheckResolveWallFlags, AllowKeatonWall);
    sApi->RegisterVB(VB_USE_STRENGTH_UPGRADE, ResolveKeatonStrength);
}
