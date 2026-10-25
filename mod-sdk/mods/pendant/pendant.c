/**
 * Pendant of Memories: three borrowed moves on B. From a sheathed stand near an enemy, a Mortal Draw that kills
 * in one cut; in mid air, a ground pound that bounces off what it hits; locked on after three side hops, a
 * parry leap over the enemy. A on its cell turns the moves off.
 */

#include <math.h>

#include "z64items.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "z64quake.h"
#include "z64rumble.h"

#define PENDANT_KEY "nei.equip.pendant"
#define MORTAL_DRAW_RANGE 200.0f
#define MORTAL_DRAW_MIN_RANGE 20.0f
#define MORTAL_DRAW_HITSTOP 10
#define MORTAL_DRAW_RECOVERY 15
#define MORTAL_DRAW_SLASH_FRAMES 12
#define MORTAL_DRAW_DAMAGE 0xFF
#define GPOUND_STALL_FRAMES 4
#define GPOUND_FALL_VELOCITY (-18.0f)
#define GPOUND_FALL_GRAVITY (-2.5f)
#define GPOUND_LANDING_FRAMES 25
#define GPOUND_SHOCKWAVE_FRAMES 6
#define GPOUND_BOUNCE_VELOCITY 8.0f
#define GPOUND_TIMEOUT 60
#define PARRY_ARC_FRAMES 22
#define PARRY_ARC_HEIGHT 80.0f
#define PARRY_LAND_DIST 120.0f
#define PARRY_LANDING_FRAMES 12
#define PARRY_HOPS_TO_ARM 3
#define PARRY_HOP_WINDOW 60
#define DEFAULT_GRAVITY (-1.2f)

// Set by the Z-target side hop and backflip, the same bit Ship names PLAYER_STATE2_HOPPING.
#define PLAYER_STATE2_HOPPING PLAYER_STATE2_80000

static const ALIGN_ASSET(2) char sIcon[] = "__OTR__icon_item_static_yar/gItemIconPendantOfMemoriesTex";
static const ALIGN_ASSET(2) char sName[] = "__OTR__item_name_static/gItemNamePendantOfMemoriesENGTex";
static const ALIGN_ASSET(2) char sGetItemDL[] = "__OTR__objects/object_gi_reserve_c_01/gGiPendantOfMemoriesDL";

static const ALIGN_ASSET(2) char sDrawAnim[] = "__OTR__objects/gameplay_keep/gPlayerAnim_link_fighter_power_kiru_start";
static const ALIGN_ASSET(2) char sPlantAnim[] = "__OTR__objects/gameplay_keep/gPlayerAnim_link_fighter_power_kiru_wait";
static const ALIGN_ASSET(2) char sLandingAnim[] = "__OTR__objects/gameplay_keep/gPlayerAnim_link_normal_landing";
static const ALIGN_ASSET(2) char sRollAnim[] = "__OTR__objects/gameplay_keep/gPlayerAnim_link_normal_landing_roll";

extern PlayerAnimationHeader* ResourceMgr_LoadPlayerAnimAsHeader(const char* animPath);
extern void func_80833864(PlayState* play, Player* player, s32 meleeWeaponAnim);

typedef enum {
    PENDANT_IDLE,
    PENDANT_DRAW_SLASH,
    PENDANT_DRAW_HITSTOP,
    PENDANT_DRAW_RECOVERY,
    PENDANT_GPOUND_STALL,
    PENDANT_GPOUND_FALLING,
    PENDANT_GPOUND_LANDING,
    PENDANT_PARRY_ARC,
    PENDANT_PARRY_LANDING,
} PendantState;

static const S2HModApi* sApi;

static ColliderCylinder sStrikeCollider;

static ColliderCylinderInit sStrikeColliderInit = {
    { COL_MATERIAL_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_CYLINDER },
    { ELEM_MATERIAL_UNK2, { DMG_SWORD, 0x00, 0x08 }, { 0, 0, 0 }, ATELEM_ON | ATELEM_SFX_NORMAL, ACELEM_NONE,
      OCELEM_NONE },
    { 80, 80, 0, { 0, 0, 0 } },
};

static struct {
    PendantState state;
    s16 timer;
    Actor* target;
    Vec3f arcStart;
    Vec3f arcEnd;
    bool hasParryHit;
    bool isColliderReady;
    u8 hops;
    s16 hopTimer;
    bool isLeapArmed;
    bool wasHopping;
} sPendant;

static bool IsActive(void) {
    return sApi->IsEquipmentOwned(PENDANT_KEY) && sApi->IsEquipmentToggleOn(PENDANT_KEY);
}

static bool IsGrounded(Player* player) {
    return (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) != 0;
}

static bool IsPlayerBusy(PlayState* play, Player* player) {
    return Player_InCsMode(play) || (player->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_TALKING)) != 0;
}

static bool IsZTargeting(Player* player) {
    return player->focusActor != NULL && player->focusActor->update != NULL;
}

static bool IsBPressed(PlayState* play) {
    return CHECK_BTN_ALL(play->state.input[0].press.button, BTN_B);
}

static void PlaySfx(Player* player, u16 sfxId) {
    Audio_PlaySfx_AtPos(&player->actor.world.pos, sfxId);
}

static void ChangeAnim(PlayState* play, Player* player, const char* path, f32 playSpeed, f32 startFrame, f32 endFrame,
                       u8 mode, f32 morph) {
    PlayerAnimationHeader* anim = ResourceMgr_LoadPlayerAnimAsHeader(path);

    if (anim != NULL) {
        PlayerAnimation_Change(play, &player->skelAnime, anim, playSpeed, startFrame, endFrame, mode, morph);
    }
}

static f32 GetLastFrame(const char* path) {
    PlayerAnimationHeader* anim = ResourceMgr_LoadPlayerAnimAsHeader(path);

    return anim == NULL ? 0.0f : Animation_GetLastFrame(anim);
}

static bool IsPlayingAnim(Player* player, const char* path) {
    return player->skelAnime.animation == ResourceMgr_LoadPlayerAnimAsHeader(path);
}

// The actor lists are the only proof a remembered pointer still names a live actor.
static bool IsActorAlive(PlayState* play, Actor* actor) {
    if (actor == NULL) {
        return false;
    }
    for (s32 category = 0; category < ACTORCAT_MAX; category++) {
        for (Actor* listed = play->actorCtx.actorLists[category].first; listed != NULL; listed = listed->next) {
            if (listed == actor) {
                return actor->update != NULL;
            }
        }
    }
    return false;
}

static void ReturnToIdle(void) {
    sPendant.state = PENDANT_IDLE;
    sPendant.timer = 0;
    sPendant.target = NULL;
}

static void ResetMoves(void) {
    ReturnToIdle();
    sPendant.hops = 0;
    sPendant.hopTimer = 0;
    sPendant.isLeapArmed = false;
    sPendant.wasHopping = false;
}

static void EnsureStrikeCollider(PlayState* play, Player* player) {
    if (sPendant.isColliderReady) {
        return;
    }
    Collider_InitCylinder(play, &sStrikeCollider);
    Collider_SetCylinder(play, &sStrikeCollider, &player->actor, &sStrikeColliderInit);
    sPendant.isColliderReady = true;
}

static void StrikeAround(PlayState* play, Player* player, Vec3f* center) {
    sStrikeCollider.dim.pos.x = center->x;
    sStrikeCollider.dim.pos.y = center->y;
    sStrikeCollider.dim.pos.z = center->z;
    sStrikeCollider.base.atFlags |= AT_ON;
    CollisionCheck_SetAT(play, &play->colChkCtx, &sStrikeCollider.base);
}

static bool ConsumeStrikeHit(void) {
    if (!(sStrikeCollider.base.atFlags & AT_HIT)) {
        return false;
    }
    sStrikeCollider.base.atFlags &= ~AT_HIT;
    return true;
}

static void ShakeCamera(PlayState* play, s16 speed, s16 y, s16 x, s16 zoom, s16 frames) {
    s16 quake = Quake_Request(Play_GetCamera(play, CAM_ID_MAIN), QUAKE_TYPE_3);

    Quake_SetSpeed(quake, speed);
    Quake_SetPerturbations(quake, y, x, zoom, 0);
    Quake_SetDuration(quake, frames);
}

static Actor* FindMortalDrawTarget(Player* player, PlayState* play) {
    Actor* best = NULL;
    f32 closest = MORTAL_DRAW_RANGE;

    for (Actor* enemy = play->actorCtx.actorLists[ACTORCAT_ENEMY].first; enemy != NULL; enemy = enemy->next) {
        f32 dist = Actor_WorldDistXZToActor(&player->actor, enemy);

        if (dist < closest && dist > MORTAL_DRAW_MIN_RANGE) {
            closest = dist;
            best = enemy;
        }
    }
    return best;
}

static bool CanMortalDraw(Player* player, PlayState* play) {
    return IsBPressed(play) && Player_GetMeleeWeaponHeld(player) == PLAYER_MELEEWEAPON_NONE &&
           !IsZTargeting(player) && IsGrounded(player) && player->speedXZ <= 1.0f;
}

static void StartMortalDraw(Player* player, PlayState* play, Actor* target) {
    s16 yaw = Actor_WorldYawTowardActor(&player->actor, target);

    sPendant.state = PENDANT_DRAW_SLASH;
    sPendant.timer = 0;
    sPendant.target = target;
    player->actor.shape.rot.y = yaw;
    player->yaw = yaw;
    player->speedXZ = 0.0f;
    player->actor.velocity.x = 0.0f;
    player->actor.velocity.z = 0.0f;
    // The swing brings its own clip, so the iaijutsu draw goes on after it.
    func_80833864(play, player, PLAYER_MWA_JUMPSLASH_START);
    ChangeAnim(play, player, sDrawAnim, 2.0f, 0.0f, GetLastFrame(sDrawAnim), ANIMMODE_ONCE, -3.0f);
    player->meleeWeaponQuads[0].elem.atDmgInfo.damage = MORTAL_DRAW_DAMAGE;
    player->meleeWeaponQuads[1].elem.atDmgInfo.damage = MORTAL_DRAW_DAMAGE;
    PlaySfx(player, NA_SE_IT_SWORD_SWING_HARD);
}

static void UpdateMortalDraw(Player* player, PlayState* play) {
    bool hasHit = (player->meleeWeaponQuads[0].base.atFlags & AT_HIT) ||
                  (player->meleeWeaponQuads[1].base.atFlags & AT_HIT);

    sPendant.timer++;
    player->speedXZ = 0.0f;
    if (hasHit) {
        sPendant.state = PENDANT_DRAW_HITSTOP;
        sPendant.timer = 0;
        ShakeCamera(play, 20000, 8, 0, 0, MORTAL_DRAW_HITSTOP);
        Rumble_Request(0.0f, 180, 14, 100);
        PlaySfx(player, NA_SE_IT_SWORD_STRIKE);
        return;
    }
    if (sPendant.timer > MORTAL_DRAW_SLASH_FRAMES) {
        sPendant.state = PENDANT_DRAW_RECOVERY;
        sPendant.timer = 0;
    }
}

// Link freezes for the hitstop; the enemy's own hit reaction freezes it.
static void UpdateHitstop(Player* player) {
    player->skelAnime.playSpeed = 0.0f;
    player->speedXZ = 0.0f;
    player->actor.velocity.x = 0.0f;
    player->actor.velocity.z = 0.0f;
    if (++sPendant.timer >= MORTAL_DRAW_HITSTOP) {
        sPendant.state = PENDANT_DRAW_RECOVERY;
        sPendant.timer = 0;
        player->skelAnime.playSpeed = 1.0f;
    }
}

// The sword stays drawn afterwards, as in Twilight Princess.
static void UpdateDrawRecovery(Player* player) {
    player->speedXZ = 0.0f;
    if (++sPendant.timer >= MORTAL_DRAW_RECOVERY) {
        ReturnToIdle();
    }
}

static bool CanGroundPound(Player* player, PlayState* play) {
    return !IsGrounded(player) && Player_GetMeleeWeaponHeld(player) != PLAYER_MELEEWEAPON_NONE && IsBPressed(play);
}

static void HoldStill(Player* player) {
    player->actor.velocity.x = 0.0f;
    player->actor.velocity.y = 0.0f;
    player->actor.velocity.z = 0.0f;
    player->actor.gravity = 0.0f;
    player->speedXZ = 0.0f;
}

static void PlantSwordPose(Player* player, PlayState* play, f32 playSpeed, f32 morph) {
    ChangeAnim(play, player, sPlantAnim, playSpeed, GetLastFrame(sPlantAnim), 0.0f, ANIMMODE_ONCE, morph);
}

// The sword is raised with the plant pose played backwards and frozen on its last frame.
static void StartGroundPound(Player* player, PlayState* play) {
    sPendant.state = PENDANT_GPOUND_STALL;
    sPendant.timer = 0;
    HoldStill(player);
    PlantSwordPose(player, play, 0.0f, -3.0f);
    EnsureStrikeCollider(play, player);
    PlaySfx(player, NA_SE_IT_SWORD_SWING_HARD);
}

static void UpdateGroundPoundStall(Player* player, PlayState* play) {
    HoldStill(player);
    if (++sPendant.timer < GPOUND_STALL_FRAMES) {
        return;
    }
    sPendant.state = PENDANT_GPOUND_FALLING;
    sPendant.timer = 0;
    player->actor.velocity.y = GPOUND_FALL_VELOCITY;
    player->actor.gravity = GPOUND_FALL_GRAVITY;
    // The swing sets the sword's damage and takes over the animation, so the slam pose goes on after it.
    func_80833864(play, player, PLAYER_MWA_JUMPSLASH_START);
    PlantSwordPose(player, play, -2.0f, 0.0f);
}

static void LandGroundPound(Player* player, PlayState* play) {
    sPendant.state = PENDANT_GPOUND_LANDING;
    sPendant.timer = 0;
    StrikeAround(play, player, &player->actor.world.pos);
    Rumble_Request(0.0f, 255, 20, 150);
    ShakeCamera(play, 28000, 14, 2, 100, 16);
    PlaySfx(player, NA_SE_IT_HAMMER_HIT);
    ChangeAnim(play, player, sLandingAnim, 1.0f, 0.0f, GetLastFrame(sLandingAnim), ANIMMODE_ONCE, -6.0f);
    player->actor.gravity = -1.0f;
}

static void UpdateGroundPoundFall(Player* player, PlayState* play) {
    sPendant.timer++;
    player->actor.gravity = GPOUND_FALL_GRAVITY;
    player->speedXZ = 0.0f;
    // The melee action keeps putting its own clip back.
    if (!IsPlayingAnim(player, sPlantAnim)) {
        PlantSwordPose(player, play, -2.0f, 0.0f);
    }
    // Last frame's hit is read before the collider is registered again: a pogo off whatever it struck.
    if (ConsumeStrikeHit()) {
        player->actor.velocity.y = GPOUND_BOUNCE_VELOCITY;
        player->actor.gravity = DEFAULT_GRAVITY;
        sStrikeCollider.base.atFlags &= ~AT_ON;
        PlaySfx(player, NA_SE_IT_SWORD_STRIKE);
        ReturnToIdle();
        return;
    }
    StrikeAround(play, player, &player->actor.world.pos);
    if (IsGrounded(player)) {
        LandGroundPound(player, play);
    }
    if (sPendant.timer > GPOUND_TIMEOUT) {
        player->actor.gravity = DEFAULT_GRAVITY;
        ReturnToIdle();
    }
}

static void UpdateGroundPoundLanding(Player* player, PlayState* play) {
    player->speedXZ = 0.0f;
    if (++sPendant.timer < GPOUND_SHOCKWAVE_FRAMES) {
        StrikeAround(play, player, &player->actor.world.pos);
        if (ConsumeStrikeHit()) {
            PlaySfx(player, NA_SE_IT_SWORD_STRIKE);
        }
    }
    if (sPendant.timer > GPOUND_LANDING_FRAMES) {
        ReturnToIdle();
    }
}

static void TrackSideHops(Player* player) {
    bool isHopping = (player->stateFlags2 & PLAYER_STATE2_HOPPING) != 0;

    if (!IsZTargeting(player)) {
        sPendant.hops = 0;
        sPendant.isLeapArmed = false;
        sPendant.wasHopping = false;
        return;
    }
    if (isHopping && !sPendant.wasHopping) {
        sPendant.hops++;
        sPendant.hopTimer = 0;
        if (sPendant.hops >= PARRY_HOPS_TO_ARM && !sPendant.isLeapArmed) {
            sPendant.isLeapArmed = true;
            Audio_PlaySfx(NA_SE_SY_LOCK_ON);
        }
    }
    sPendant.wasHopping = isHopping;
    if (++sPendant.hopTimer > PARRY_HOP_WINDOW) {
        sPendant.hops = 0;
        sPendant.isLeapArmed = false;
    }
}

static bool CanParryLeap(Player* player, PlayState* play) {
    if (!sPendant.isLeapArmed || !IsBPressed(play)) {
        return false;
    }
    sPendant.isLeapArmed = false;
    sPendant.hops = 0;
    return IsZTargeting(player);
}

static void PlayRollLoop(Player* player, PlayState* play) {
    ChangeAnim(play, player, sRollAnim, 3.0f, 0.0f, GetLastFrame(sRollAnim), ANIMMODE_LOOP, -3.0f);
}

// A negative invincibility timer is intangibility without the red flash.
static void StartParryLeap(Player* player, PlayState* play) {
    Actor* target = player->focusActor;
    f32 dx = target->world.pos.x - player->actor.world.pos.x;
    f32 dz = target->world.pos.z - player->actor.world.pos.z;
    f32 distXZ = MAX(sqrtf(SQ(dx) + SQ(dz)), 1.0f);
    s16 yaw = Actor_WorldYawTowardActor(&player->actor, target);

    sPendant.state = PENDANT_PARRY_ARC;
    sPendant.timer = 0;
    sPendant.target = target;
    sPendant.hasParryHit = false;
    sPendant.arcStart = player->actor.world.pos;
    sPendant.arcEnd.x = target->world.pos.x + (dx / distXZ) * PARRY_LAND_DIST;
    sPendant.arcEnd.y = sPendant.arcStart.y;
    sPendant.arcEnd.z = target->world.pos.z + (dz / distXZ) * PARRY_LAND_DIST;
    HoldStill(player);
    player->actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND;
    player->invincibilityTimer = -(PARRY_ARC_FRAMES + 5);
    func_80833864(play, player, PLAYER_MWA_JUMPSLASH_START);
    player->actor.shape.rot.y = yaw;
    player->yaw = yaw;
    PlayRollLoop(player, play);
    EnsureStrikeCollider(play, player);
    PlaySfx(player, NA_SE_IT_SWORD_SWING_HARD);
}

static void SilenceSwordQuads(Player* player) {
    player->meleeWeaponQuads[0].base.atFlags &= ~AT_ON;
    player->meleeWeaponQuads[1].base.atFlags &= ~AT_ON;
    player->meleeWeaponState = PLAYER_MELEE_WEAPON_STATE_0;
}

// One hit only, on the enemy itself: the melee system would otherwise land a second one on the way down.
static void StrikeParryTarget(Player* player, PlayState* play) {
    if (sPendant.hasParryHit) {
        SilenceSwordQuads(player);
        return;
    }
    StrikeAround(play, player, &sPendant.target->world.pos);
    if (!ConsumeStrikeHit()) {
        return;
    }
    sPendant.hasParryHit = true;
    sStrikeCollider.base.atFlags &= ~AT_ON;
    SilenceSwordQuads(player);
    PlaySfx(player, NA_SE_IT_SWORD_STRIKE);
}

static void EndParryArc(Player* player, PlayState* play) {
    sPendant.state = PENDANT_PARRY_LANDING;
    sPendant.timer = 0;
    player->actor.gravity = DEFAULT_GRAVITY;
}

static void UpdateParryArc(Player* player, PlayState* play) {
    f32 t;

    if (!IsActorAlive(play, sPendant.target)) {
        EndParryArc(player, play);
        return;
    }
    t = MIN((f32)++sPendant.timer / PARRY_ARC_FRAMES, 1.0f);
    if (!IsPlayingAnim(player, sRollAnim)) {
        PlayRollLoop(player, play);
    }
    player->actor.world.pos.x = sPendant.arcStart.x + (sPendant.arcEnd.x - sPendant.arcStart.x) * t;
    player->actor.world.pos.z = sPendant.arcStart.z + (sPendant.arcEnd.z - sPendant.arcStart.z) * t;
    player->actor.world.pos.y = sPendant.arcStart.y + (sPendant.arcEnd.y - sPendant.arcStart.y) * t +
                                PARRY_ARC_HEIGHT * 4.0f * t * (1.0f - t);
    HoldStill(player);
    player->cylinder.base.ocFlags1 &= ~OC1_ON;
    StrikeParryTarget(player, play);
    player->actor.shape.rot.y = Actor_WorldYawTowardActor(&player->actor, sPendant.target);
    player->yaw = player->actor.shape.rot.y;

    if (sPendant.timer >= PARRY_ARC_FRAMES) {
        EndParryArc(player, play);
        ChangeAnim(play, player, sLandingAnim, 1.5f, 0.0f, GetLastFrame(sLandingAnim), ANIMMODE_ONCE, -3.0f);
        PlaySfx(player, NA_SE_PL_WALK_GROUND);
    } else if (player->actor.bgCheckFlags & BGCHECKFLAG_CEILING) {
        EndParryArc(player, play);
    }
}

static void UpdateParryLanding(Player* player) {
    if (!IsGrounded(player) && ++sPendant.timer <= PARRY_LANDING_FRAMES) {
        return;
    }
    player->speedXZ = 0.0f;
    player->actor.velocity.x = 0.0f;
    player->actor.velocity.y = 0.0f;
    player->actor.velocity.z = 0.0f;
    player->cylinder.base.ocFlags1 |= OC1_ON;
    ReturnToIdle();
}

static void StartMoveFromIdle(Player* player, PlayState* play) {
    Actor* drawTarget = NULL;

    TrackSideHops(player);
    if (CanMortalDraw(player, play)) {
        drawTarget = FindMortalDrawTarget(player, play);
    }
    if (drawTarget != NULL) {
        StartMortalDraw(player, play, drawTarget);
    } else if (CanGroundPound(player, play)) {
        StartGroundPound(player, play);
    } else if (CanParryLeap(player, play)) {
        StartParryLeap(player, play);
    }
}

static void TickPendant(Player* player) {
    PlayState* play = gPlayState;

    if (play == NULL || player == NULL) {
        return;
    }
    if (!IsActive() || IsPlayerBusy(play, player)) {
        if (sPendant.state != PENDANT_IDLE) {
            player->cylinder.base.ocFlags1 |= OC1_ON;
            player->skelAnime.playSpeed = 1.0f;
        }
        ResetMoves();
        return;
    }
    switch (sPendant.state) {
        case PENDANT_IDLE:
            StartMoveFromIdle(player, play);
            break;
        case PENDANT_DRAW_SLASH:
            UpdateMortalDraw(player, play);
            break;
        case PENDANT_DRAW_HITSTOP:
            UpdateHitstop(player);
            break;
        case PENDANT_DRAW_RECOVERY:
            UpdateDrawRecovery(player);
            break;
        case PENDANT_GPOUND_STALL:
            UpdateGroundPoundStall(player, play);
            break;
        case PENDANT_GPOUND_FALLING:
            UpdateGroundPoundFall(player, play);
            break;
        case PENDANT_GPOUND_LANDING:
            UpdateGroundPoundLanding(player, play);
            break;
        case PENDANT_PARRY_ARC:
            UpdateParryArc(player, play);
            break;
        case PENDANT_PARRY_LANDING:
            UpdateParryLanding(player);
            break;
        default:
            ReturnToIdle();
            break;
    }
}

// The collider is bound to the player actor, and a new scene brings a new one.
static void ForgetSceneState(s16 sceneId, s8 spawnNum) {
    ResetMoves();
    sPendant.isColliderReady = false;
}

static void TurnOff(const char* key) {
    ResetMoves();
}

static const char* const sRequiredHooks[] = { "OnPlayerUpdate", "OnSceneInit" };

static const S2HModRequirements sRequirements = { sizeof(S2HModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

S2H_MOD_EXPORT void ModSetApi(const S2HModApi* api) {
    sApi = api;
}

S2H_MOD_EXPORT const S2HModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

S2H_MOD_EXPORT void ModInit(void) {
    S2HCustomEquipDefinition definition = { 0 };

    definition.structSize = sizeof(definition);
    definition.key = PENDANT_KEY;
    definition.iconPath = sIcon;
    definition.namePath = sName;
    definition.pauseText = "%rPendant of Memories\n%w%yB%w near an enemy with your sword sheathed draws and "
                           "strikes. %yB%w in mid air slams down. Locked on, three side hops then %yB%w leap "
                           "over the foe. %yA%w in pause toggles it on or off.";
    definition.getItemText = "You got the %rPendant of Memories%w!\nA keepsake from Kafei, proof of his promise. "
                             "It remembers the moves of heroes from other tales, and they answer to %yB%w.";
    definition.slot = S2H_EQUIP_SLOT_UPGRADE;
    definition.page = 0;
    definition.row = 1;
    definition.column = 0;
    definition.toggles = 1;
    definition.onUnequip = TurnOff;
    definition.getItemModelPath = sGetItemDL;
    if (!sApi->RegisterCustomEquipment(&definition)) {
        return;
    }

    S2H_REGISTER_HOOK(sApi, OnPlayerUpdate, TickPendant);
    S2H_REGISTER_HOOK(sApi, OnSceneInit, ForgetSceneState);
}
