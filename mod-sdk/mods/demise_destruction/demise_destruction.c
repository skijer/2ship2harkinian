#include "z64items.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "z64quake.h"
#include "z64rumble.h"

// Keeps the player from reading input and acting while the spell winds up.
#define DEMISE_INPUT_LOCK PLAYER_STATE1_20

#define DEMISE_KEY "nei.demise_destruction"
#define DEMISE_MAGIC_COST 12
#define DEMISE_WINDUP_DURATION 70
#define DEMISE_FINISH_DURATION 5
#define DEMISE_COLLISION_RADIUS 400
#define DEMISE_COLLISION_HEIGHT 200
#define DEMISE_DAMAGE 40
#define DEMISE_DAMAGE_FLAGS (DMG_GORON_POUND | DMG_EXPLOSIVES | DMG_ZORA_BOOMERANG)
#define DEMISE_DARKNESS 0.8f
#define DEMISE_DARKEN_FRAMES 20
#define DEMISE_STORM_AFTERMATH 60
#define DEMISE_STORM_RAIN 60
#define DEMISE_STORM_LIGHTNING_DELAY 501.0f
#define DEMISE_STORM_FADE_STEP_FRAMES 4
#define DEMISE_STORM_AMBIENCE_FADE_RAIN 8

typedef enum {
    DEMISE_IDLE,
    DEMISE_WINDUP,
    DEMISE_FINISH,
} DemisePhase;

static const ALIGN_ASSET(2) char sIconTex[] = "__OTR__textures/icon_item_custom/gItemIconDemiseDestructionTex";
static const ALIGN_ASSET(2) char sNameTex[] = "__OTR__textures/item_name_custom/gDemiseDestructionNameTex";
static const ALIGN_ASSET(2) char sCastAnim[] = "__OTR__misc/link_animetion/gPlayerAnim_nei_demise_destruction";
static const ALIGN_ASSET(2) char sGiveDL[] = "__OTR__objects/object_nei_magic_spell/gDemiseDestructionGiveDL";

static ColliderCylinderInit sColliderInit = {
    {
        COL_MATERIAL_NONE,
        AT_ON | AT_TYPE_PLAYER,
        AC_NONE,
        OC1_NONE,
        OC2_NONE,
        COLSHAPE_CYLINDER,
    },
    {
        ELEM_MATERIAL_UNK2,
        { DEMISE_DAMAGE_FLAGS, 0x00, DEMISE_DAMAGE },
        { 0, 0, 0 },
        ATELEM_ON | ATELEM_SFX_NORMAL,
        ACELEM_NONE,
        OCELEM_NONE,
    },
    { 100, 80, 0, { 0, 0, 0 } },
};

static Color_RGBA8 sDustColor = { 60, 0, 0, 255 };

static const char* const sRequiredHooks[] = { "OnPlayerUpdate", "OnPlayerResolveItemActionInit" };
static const S2HModRequirements sRequirements = { sizeof(S2HModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const S2HModApi* sApi;
static ColliderCylinder sCollider;
static bool sIsColliderReady;
static u8 sPhase;
static s16 sTimer;
static s8 sPreviousInvincibility;
static f32 sDarkness;
static s16 sStormTimer;
static bool sOwnsStorm;
static s16 sSceneId = -1;
static u32 sLastFrame;

extern PlayerAnimationHeader* ResourceMgr_LoadPlayerAnimAsHeader(const char* animPath);

static void SpawnExplosionAt(PlayState* play, Vec3f* pos, f32 scale) {
    Vec3f velocity = { 0.0f, 8.0f * scale, 0.0f };
    Vec3f accel = { 0.0f, -0.3f, 0.0f };

    EffectSsBomb2_SpawnLayered(play, pos, &velocity, &accel, (s16)(120 * scale), (s16)(30 * scale));
}

static void SpawnExplosionRing(PlayState* play, Vec3f* center, f32 radius, u8 count) {
    Vec3f zero = { 0.0f, 0.0f, 0.0f };

    for (u8 i = 0; i < count; i++) {
        s16 angle = (s16)(i * (0x10000 / count));
        Vec3f pos = { center->x + Math_SinS(angle) * radius, center->y, center->z + Math_CosS(angle) * radius };

        EffectSsBomb2_SpawnLayered(play, &pos, &zero, &zero, 80, 20);
    }
}

static void SpawnLightningRing(PlayState* play, Vec3f* center, u8 count) {
    Color_RGBA8 prim = { 255, 255, 255, 255 };
    Color_RGBA8 env = { 180, 0, 0, 255 };
    Vec3f zero = { 0.0f, 0.0f, 0.0f };

    for (u8 i = 0; i < count; i++) {
        s16 angle = (s16)(Rand_ZeroOne() * 0xFFFF);
        f32 radius = 200.0f + Rand_ZeroOne() * 360.0f;
        Vec3f pos = { center->x + Math_SinS(angle) * radius, center->y, center->z + Math_CosS(angle) * radius };

        EffectSsLightning_Spawn(play, &pos, &prim, &env, 80, (s16)(Rand_ZeroOne() * 0xFFFF), 6, 2);
        EffectSsBomb2_SpawnLayered(play, &pos, &zero, &zero, 15, 5);
    }
}

// Dust crawling outward along the ground, the tell that the blast is still growing.
static void SpawnGroundDust(PlayState* play, Vec3f* center, u8 count) {
    Color_RGBA8 env = { 0, 0, 0, 255 };

    for (u8 i = 0; i < count; i++) {
        s16 angle = (s16)(i * (0x10000 / count));
        f32 radius = 80.0f + Rand_ZeroOne() * 240.0f;
        Vec3f pos = { center->x + Math_SinS(angle) * radius, center->y + 2.0f,
                      center->z + Math_CosS(angle) * radius };
        Vec3f velocity = { Math_SinS(angle) * 3.0f, 1.0f + Rand_ZeroOne() * 2.0f, Math_CosS(angle) * 3.0f };
        Vec3f accel = { -velocity.x * 0.1f, -0.1f, -velocity.z * 0.1f };

        EffectSsDust_Spawn(play, 0, &pos, &velocity, &accel, &sDustColor, &env, 300, 10, 10, 0);
    }
}

static void RequestQuake(PlayState* play, s16 speed, s16 strength, s16 duration) {
    s16 quake = Quake_Request(Play_GetCamera(play, CAM_ID_MAIN), QUAKE_TYPE_3);

    Quake_SetSpeed(quake, speed);
    Quake_SetPerturbations(quake, strength, 0, 0, 0);
    Quake_SetDuration(quake, duration);
}

// En_Okarina_Effect's storm: the spell brings the weather, not Song of Storms' puzzles.
static void SummonStorm(PlayState* play) {
    sStormTimer = DEMISE_WINDUP_DURATION + DEMISE_STORM_AFTERMATH;
    if (sOwnsStorm || play->envCtx.precipitation[PRECIP_RAIN_CUR] != 0 ||
        play->envCtx.precipitation[PRECIP_SOS_MAX] != 0) {
        return;
    }
    play->envCtx.precipitation[PRECIP_SOS_MAX] = DEMISE_STORM_RAIN;
    gLightningStrike.delayTimer = DEMISE_STORM_LIGHTNING_DELAY;
    play->envCtx.lightningState = LIGHTNING_LAST;
    Environment_PlayStormNatureAmbience(play);
    sOwnsStorm = true;
}

// The rain drains away a drop count at a time, as Song of Storms' own storm does.
static void FadeStorm(PlayState* play) {
    if ((play->state.frames % DEMISE_STORM_FADE_STEP_FRAMES) != 0) {
        return;
    }
    play->envCtx.precipitation[PRECIP_SOS_MAX]--;
    if (play->envCtx.precipitation[PRECIP_SOS_MAX] == DEMISE_STORM_AMBIENCE_FADE_RAIN) {
        Environment_StopStormNatureAmbience(play);
    }
}

static void UpdateStorm(PlayState* play) {
    if (sStormTimer > 0) {
        sStormTimer--;
        return;
    }
    if (!sOwnsStorm) {
        return;
    }
    if (play->envCtx.precipitation[PRECIP_SOS_MAX] == 0) {
        sOwnsStorm = false;
        return;
    }
    FadeStorm(play);
}

// Same dimming the Great Fairy's magic applies around a spin; it has to be re-sent every frame until it is back at 0.
static void UpdateDarkness(PlayState* play) {
    f32 target = (sPhase == DEMISE_WINDUP) ? DEMISE_DARKNESS : 0.0f;

    if ((sDarkness == 0.0f) && (target == 0.0f)) {
        return;
    }
    Math_StepToF(&sDarkness, target, DEMISE_DARKNESS / DEMISE_DARKEN_FRAMES);
    Environment_AdjustLights(play, sDarkness, 850.0f, 0.2f, 0.0f);
}

// A scene load rebuilds the lights, so nothing the spell set up belongs to the new scene.
static void ForgetPreviousScene(PlayState* play) {
    bool isNewScene = (play->sceneId != sSceneId) || (play->state.frames < sLastFrame);

    sSceneId = play->sceneId;
    sLastFrame = play->state.frames;
    if (!isNewScene) {
        return;
    }
    if (sPhase != DEMISE_IDLE) {
        Magic_Reset(play);
    }
    sApi->ReleaseTimeControl("demise.cast");
    sPhase = DEMISE_IDLE;
    sTimer = 0;
    sDarkness = 0.0f;
    sStormTimer = 0;
    sOwnsStorm = false;
}

static void Stop(Player* player, PlayState* play) {
    if (sPhase == DEMISE_IDLE) {
        return;
    }
    // A wind-up cut short must hand the player back, or Link stays frozen with no input.
    if (sPhase == DEMISE_WINDUP) {
        player->stateFlags1 &= ~DEMISE_INPUT_LOCK;
    }
    sCollider.base.atFlags &= ~(AT_ON | AT_HIT);
    sApi->ReleaseTimeControl("demise.cast");
    sPhase = DEMISE_IDLE;
    sTimer = 0;
    // Consuming leaves the meter flashing, and the next cast is refused until the spell resets it.
    Magic_Reset(play);
}

static bool CanCast(Player* player, PlayState* play) {
    return sPhase == DEMISE_IDLE && !(player->stateFlags1 & PLAYER_STATE1_8000000) &&
           (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND);
}

// Two frames early: the player is still finishing its own update and would stomp the camera setting back.
static void Cast(PlayState* play, Player* player) {
    if (!Magic_Consume(play, DEMISE_MAGIC_COST, MAGIC_CONSUME_NOW)) {
        return;
    }
    if (!sIsColliderReady) {
        Collider_InitCylinder(play, &sCollider);
        sIsColliderReady = true;
    }
    Collider_SetCylinder(play, &sCollider, &player->actor, &sColliderInit);
    sPhase = DEMISE_WINDUP;
    sApi->RequestTimeControl("demise.cast", 100, 1.0f, true);
    sTimer = -2;
    sPreviousInvincibility = player->invincibilityTimer;
}

static void PinPlayerDown(Player* player) {
    player->stateFlags1 |= DEMISE_INPUT_LOCK;
    player->speedXZ = 0.0f;
    player->actor.speed = 0.0f;
    player->actor.velocity.x = 0.0f;
    player->actor.velocity.y = 0.0f;
    player->actor.velocity.z = 0.0f;
}

static void UpdateWindupEffects(Player* player, PlayState* play) {
    Vec3f* pos = &player->actor.world.pos;

    // Majora's Mask has no Ganon sting; the thunder is the nearest cue for the wind-up's turn.
    if (sTimer == 20) {
        Audio_PlaySfx_AtPos(pos, NA_SE_EV_LIGHTNING);
    }
    if (sTimer > 20) {
        if (((sTimer - 20) % 8) == 0) {
            SpawnLightningRing(play, pos, 1);
        }
        if ((play->gameplayFrames % 4) == 0) {
            SpawnGroundDust(play, pos, 4);
        }
    }
    if (sTimer <= 50) {
        return;
    }
    if ((sTimer % 4) == 0) {
        SpawnLightningRing(play, pos, 2);
        Audio_PlaySfx_AtPos(pos, NA_SE_EV_LIGHTNING);
    }
    if ((play->gameplayFrames % 3) == 0) {
        SpawnGroundDust(play, pos, 8);
    }
    if ((sTimer % 8) == 0) {
        Audio_PlaySfx_AtPos(pos, NA_SE_EV_EARTHQUAKE);
        RequestQuake(play, 20000, 3, 8);
    }
    if ((sTimer % 2) == 0) {
        Rumble_Request(200.0f, 180, 20, 10);
    }
}

static void Detonate(Player* player, PlayState* play) {
    Vec3f* pos = &player->actor.world.pos;

    SpawnExplosionRing(play, pos, DEMISE_COLLISION_RADIUS, 12);
    SpawnExplosionAt(play, pos, 1.0f);
    Audio_PlaySfx_AtPos(pos, NA_SE_IT_HAMMER_HIT);
    Audio_PlaySfx_AtPos(pos, NA_SE_IT_BOMB_EXPLOSION);
    Rumble_Request(800.0f, 0xFF, 0x28, 0xC8);
    RequestQuake(play, 27767, 12, 30);
}

static void UpdateWindup(Player* player, PlayState* play) {
    sTimer++;

    PinPlayerDown(player);

    if (sTimer == 0) {
        SummonStorm(play);
        PlayerAnimationHeader* anim = ResourceMgr_LoadPlayerAnimAsHeader(sCastAnim);

        if (anim != NULL) {
            PlayerAnimation_Change(play, &player->skelAnime, anim, 0.65f, 0.0f, Animation_GetLastFrame(anim),
                                   ANIMMODE_ONCE, -8.0f);
        }
    }
    if (sTimer >= 0) {
        PlayerAnimation_Update(play, &player->skelAnime);
    }

    UpdateWindupEffects(player, play);

    if (sTimer < DEMISE_WINDUP_DURATION) {
        return;
    }
    Detonate(player, play);
    sApi->ReleaseTimeControl("demise.cast");
    player->stateFlags1 &= ~DEMISE_INPUT_LOCK;
    sPhase = DEMISE_FINISH;
    sTimer = 0;
}

// The blast is one cylinder of explosive-class damage, held live for a few frames after the flash.
static void UpdateFinish(Player* player, PlayState* play) {
    sCollider.dim.pos.x = (s16)player->actor.world.pos.x;
    sCollider.dim.pos.y = (s16)player->actor.world.pos.y;
    sCollider.dim.pos.z = (s16)player->actor.world.pos.z;
    sCollider.dim.radius = DEMISE_COLLISION_RADIUS;
    sCollider.dim.height = DEMISE_COLLISION_HEIGHT;
    sCollider.base.atFlags |= AT_ON | AT_TYPE_PLAYER;

    CollisionCheck_SetAT(play, &play->colChkCtx, &sCollider.base);

    if (sCollider.base.atFlags & AT_HIT) {
        Audio_PlaySfx_AtPos(&player->actor.world.pos, NA_SE_IT_HAMMER_HIT);
    }

    sTimer++;
    if (sTimer >= DEMISE_FINISH_DURATION) {
        Stop(player, play);
    }
}

static bool WasJustHurt(Player* player) {
    bool hurt = player->invincibilityTimer > 0 && sPreviousInvincibility == 0;

    sPreviousInvincibility = player->invincibilityTimer;
    return hurt;
}

static void UpdateSpell(Player* player) {
    PlayState* play = gPlayState;

    if (play == NULL) {
        return;
    }
    ForgetPreviousScene(play);
    UpdateDarkness(play);
    UpdateStorm(play);
    if (sPhase == DEMISE_IDLE) {
        return;
    }
    player->invincibilityTimer = MAX(player->invincibilityTimer, 2);
    if (sPhase == DEMISE_WINDUP) {
        UpdateWindup(player, play);
    } else {
        UpdateFinish(player, play);
    }
}

// Shuffled once by the randomizer; no vanilla logic needs it.
static const S2HCustomItemRandomizer sRandomizerItem = {
    sizeof(S2HCustomItemRandomizer), 0, S2H_CUSTOM_ITEM_TYPE_ITEM, 0, 1, DEMISE_KEY, DEMISE_KEY, "",
};

S2H_MOD_EXPORT void ModSetApi(const S2HModApi* api) {
    sApi = api;
}

S2H_MOD_EXPORT const S2HModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

static void PauseSpellWorld(Actor* actor, bool* should) {
    if (sPhase == DEMISE_WINDUP && actor->id != ACTOR_PLAYER) *should = false;
}

S2H_MOD_EXPORT void ModInit(void) {
    S2HCustomItemDefinition demise = Z64Items_Define(DEMISE_KEY, sIconTex, sNameTex);

    Z64Items_SetButtons(&demise, S2H_CUSTOM_ITEM_C_BUTTON | S2H_CUSTOM_ITEM_DPAD);
    Z64Items_SetPlacement(&demise, 0, 5, 0);
    Z64Items_SetTextbox(&demise, "You got %rDemise Destruction%w!&The Demon King's storm, bound&into a spell you can "
                                 "call.^"
                                 "Press a %yC button%w with both feet on the&ground. The sky darkens, and&then the "
                                 "earth splits around you.^"
                                 "The blast breaks what a %rGoron's pound%w and a %rbomb%w break&and leaves the rest reeling. Uses&%gmagic%w.");
    Z64Items_SetPauseText(&demise, "%rDemise Destruction&%wPress a %yC button%w on the ground to call&down the storm. "
                                   "Uses magic.");
    Z64Items_SetCanUse(&demise, CanCast);
    Z64Items_SetAction(&demise, Cast, NULL);
    demise.flags |= S2H_CUSTOM_ITEM_INSTANT;
    Z64Items_SetGetItemModel(&demise, sGiveDL);
    demise.getItemScale = 1.06666667f;

    Z64Items_SetLogic(&demise, &sRandomizerItem);
    if (!Z64Items_Register(sApi, &demise)) {
        return;
    }
    S2H_REGISTER_HOOK(sApi, OnPlayerUpdate, UpdateSpell);
    S2H_REGISTER_HOOK(sApi, ShouldActorUpdate, PauseSpellWorld);
}
