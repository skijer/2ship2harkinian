#include "z64items.h"
#include "z64aiming.h"
#include <string.h>
#include "overlays/actors/ovl_En_Arrow/z_en_arrow.h"

#define MOD "oot_items"
#define SLING "oot.slingshot"
#define BOOM "oot.boomerang"
#define HAMMER "oot.hammer"
#define DIN "oot.dins_fire"
#define FARORE "oot.farores_wind"
#define NAYRU "oot.nayrus_love"
// The bow reticle's reach: 341000 hand units at the player's 0.01 scale.
#define SLINGSHOT_RETICLE_RANGE 3410.0f
#define OOT_ICON(name) "__OTR__textures/oot_icon_item_static/" name
#define OOT_NAME(name) "__OTR__textures/oot_item_name_static/" name

void func_8083B1A0(Player* player, PlayState* play);
void func_8088A594(EnArrow* arrow, PlayState* play);
s32 Player_SetAction(PlayState* play, Player* player, PlayerActionFunc action, s32 flags);
bool Player_UpdateUpperBody(Player* player, PlayState* play);
void EffectSsKFire_Spawn(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, s16 scaleMax, u8 type);

typedef struct {
    Actor actor;
    ColliderCylinder collider;
    s16 timer;
    s16 spin;
    bool returning;
} OotEffect;

typedef struct {
    u32 version;
    bool valid;
    s32 entrance;
    s16 scene;
    s8 room;
    s16 yaw;
    Vec3f pos;
    s16 seeds;
} OotSave;

static const S2HModApi* sApi;
static OotSave sSave;
static s16 sBoomId, sFireId;
static Actor* sBoom;
static bool sWaiveVoid, sSlingOut;
static s16 sShieldFrames, sSlingCooldown;
static const u32 sButtons[S2H_ITEM_BUTTON_COUNT] = {
    BTN_B, BTN_CLEFT, BTN_CDOWN, BTN_CRIGHT, 0, BTN_DRIGHT, BTN_DLEFT, BTN_DDOWN, BTN_DUP
};
static const char* const sHooks[] = { "OnPlayerUpdate", "OnSceneInit", "OnLoadFile", "OnActorDestroy",
                                      "OnPlayDrawEnd" };
static const S2HModRequirements sRequirements = { sizeof(S2HModRequirements), sHooks, ARRAY_COUNT(sHooks) };
static const ALIGN_ASSET(2) char sBoomDL[] = "__OTR__objects/oot_object_gi_boomerang/gGiBoomerangDL";
static const ALIGN_ASSET(2) char sHammerDL[] = "__OTR__objects/oot_object_gi_hammer/gGiHammerDL";
static const ALIGN_ASSET(2) char sSlingDL[] = "__OTR__objects/oot_object_link_child/gLinkChildRightHandHoldingSlingshotNearDL";

static ColliderCylinderInit sBoomCollider = {
    { COL_MATERIAL_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_TYPE_PLAYER, COLSHAPE_CYLINDER },
    { ELEM_MATERIAL_UNK2, { DMG_ZORA_BOOMERANG, 0, 1 }, { 0, 0, 0 },
      ATELEM_ON | ATELEM_NEAREST | ATELEM_SFX_NORMAL, ACELEM_NONE, OCELEM_NONE },
    { 18, 24, -12, { 0, 0, 0 } }
};

static void Store(void) { sApi->StorageSet(MOD, "state", &sSave, sizeof(sSave)); }
static void Load(s32 fileNum) {
    memset(&sSave, 0, sizeof(sSave));
    if (sApi->StorageGet(MOD, "state", &sSave, sizeof(sSave)) != sizeof(sSave) || sSave.version != 1) {
        memset(&sSave, 0, sizeof(sSave));
        sSave.version = 1;
        sSave.seeds = 50;
    }
    sSave.seeds = CLAMP(sSave.seeds, 0, 50);
}
static bool CanUse(Player* player, PlayState* play) {
    return play != NULL && player != NULL && player->transformation == PLAYER_FORM_HUMAN &&
           play->pauseCtx.state == PAUSE_STATE_OFF && play->transitionTrigger != TRANS_TRIGGER_START &&
           play->msgCtx.msgLength == 0 && !(player->stateFlags1 & PLAYER_STATE1_20000000);
}
static bool SpendMagic(PlayState* play, s16 cost) {
    if (gSaveContext.save.saveInfo.playerData.magic < cost) {
        Audio_PlaySfx(NA_SE_SY_ERROR);
        return false;
    }
    return Magic_Consume(play, cost, MAGIC_CONSUME_NOW);
}
static bool HasModels(Player* player, PlayState* play) {
    if (!CanUse(player, play)) return false;
    if (sApi->HasResource(sBoomDL)) return true;
    sApi->ShowTextbox(play, "Extract your OoT assets with %rOoT Asset Loader%w, then restart.", true);
    return false;
}

static void BoomInit(Actor* actor, PlayState* play) {
    OotEffect* effect = (OotEffect*)actor;
    Collider_InitCylinder(play, &effect->collider);
    Collider_SetCylinder(play, &effect->collider, actor, &sBoomCollider);
    Actor_SetScale(actor, 0.02f);
    actor->room = -1;
    effect->timer = 36;
    actor->speed = 12.0f;
}
static void EffectDestroy(Actor* actor, PlayState* play) {
    Collider_DestroyCylinder(play, &((OotEffect*)actor)->collider);
    if (sBoom == actor) sBoom = NULL;
}
static void BoomUpdate(Actor* actor, PlayState* play) {
    OotEffect* effect = (OotEffect*)actor;
    Player* player = GET_PLAYER(play);
    effect->spin += 0x2000;
    if (--effect->timer <= 0 || (effect->collider.base.atFlags & AT_HIT) ||
        (actor->bgCheckFlags & BGCHECKFLAG_WALL)) effect->returning = true;
    if (effect->returning) {
        Vec3f target = player->actor.focus.pos;
        Vec3f delta = { target.x - actor->world.pos.x, target.y - actor->world.pos.y, target.z - actor->world.pos.z };
        f32 distance = sqrtf(SQ(delta.x) + SQ(delta.y) + SQ(delta.z));
        if (distance < 20.0f || effect->timer < -180) {
            Actor_Kill(actor);
            return;
        }
        actor->world.pos.x += delta.x * 14.0f / distance;
        actor->world.pos.y += delta.y * 14.0f / distance;
        actor->world.pos.z += delta.z * 14.0f / distance;
    } else Actor_MoveWithoutGravity(actor);
    Actor_UpdateBgCheckInfo(play, actor, 8, 8, 8, UPDBGCHECKINFO_FLAG_1 | UPDBGCHECKINFO_FLAG_4);
    effect->collider.base.atFlags &= ~AT_HIT;
    Collider_UpdateCylinder(actor, &effect->collider);
    CollisionCheck_SetAT(play, &play->colChkCtx, &effect->collider.base);
}
static void BoomDraw(Actor* actor, PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    Matrix_RotateYS(((OotEffect*)actor)->spin, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sBoomDL);
    CLOSE_DISPS(play->state.gfxCtx);
}
static void ThrowBoom(PlayState* play, Player* player) {
    if (sBoom != NULL) { ((OotEffect*)sBoom)->returning = true; return; }
    sBoom = Actor_Spawn(&play->actorCtx, play, sBoomId, player->actor.focus.pos.x,
                       player->actor.focus.pos.y, player->actor.focus.pos.z, 0, player->actor.shape.rot.y, 0, 0);
    if (sBoom != NULL) Audio_PlaySfx(NA_SE_IT_BOOMERANG_THROW);
}
static void FireInit(Actor* actor, PlayState* play) {
    OotEffect* effect = (OotEffect*)actor;
    ColliderCylinderInit init = sBoomCollider;
    init.elem.atDmgInfo.dmgFlags = DMG_FIRE_ARROW;
    init.elem.atDmgInfo.damage = 4;
    init.dim.radius = 20;
    init.dim.height = 160;
    init.dim.yShift = -40;
    Collider_InitCylinder(play, &effect->collider);
    Collider_SetCylinder(play, &effect->collider, actor, &init);
    effect->timer = 40;
}
static void FireUpdate(Actor* actor, PlayState* play) {
    OotEffect* effect = (OotEffect*)actor;
    if (--effect->timer <= 0) { Actor_Kill(actor); return; }
    f32 radius = (40 - effect->timer) * 8.0f;
    effect->collider.dim.radius = radius;
    Collider_UpdateCylinder(actor, &effect->collider);
    CollisionCheck_SetAT(play, &play->colChkCtx, &effect->collider.base);
    if ((effect->timer % 3) == 0) {
        for (s32 i = 0; i < 12; ++i) {
            s16 angle = i * (0x10000 / 12);
            Vec3f pos = { actor->world.pos.x + Math_SinS(angle) * radius, actor->world.pos.y + 15,
                          actor->world.pos.z + Math_CosS(angle) * radius };
            Vec3f velocity = { 0, 1.5f, 0 }, accel = { 0, 0.1f, 0 };
            EffectSsKFire_Spawn(play, &pos, &velocity, &accel, 100, 0);
        }
    }
}
static void CastDin(PlayState* play, Player* player) {
    if (!SpendMagic(play, 12)) return;
    Actor_Spawn(&play->actorCtx, play, sFireId, player->actor.world.pos.x, player->actor.world.pos.y,
                player->actor.world.pos.z, 0, 0, 0, 0);
    Audio_PlaySfx(NA_SE_IT_MAGIC_ARROW_SHOT);
}
static void CastNayru(PlayState* play, Player* player) {
    if (SpendMagic(play, 24)) sShieldFrames = 1200;
}
static void WaiveVoid(bool* should, va_list args) {
    if (sWaiveVoid) { *should = false; sWaiveVoid = false; }
}
static void CastFarore(PlayState* play, Player* player) {
    if (!(player->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) { Audio_PlaySfx(NA_SE_SY_ERROR); return; }
    if (sSave.valid && CHECK_BTN_ALL(play->state.input[0].cur.button, BTN_R)) {
        sSave.valid = false;
        Store();
        Audio_PlaySfx(NA_SE_SY_CANCEL);
        return;
    }
    if (!SpendMagic(play, 6)) return;
    if (!sSave.valid) {
        sSave.valid = true;
        sSave.entrance = gSaveContext.save.entrance;
        sSave.scene = play->sceneId;
        sSave.room = play->roomCtx.curRoom.num;
        sSave.yaw = player->actor.shape.rot.y;
        sSave.pos = player->actor.world.pos;
        Store();
        Audio_PlaySfx(NA_SE_SY_GET_ITEM);
    } else {
        RespawnData* respawn = &gSaveContext.respawn[RESPAWN_MODE_DOWN];
        respawn->entrance = sSave.entrance;
        respawn->roomIndex = sSave.room;
        respawn->pos = sSave.pos;
        respawn->yaw = sSave.yaw;
        respawn->playerParams = 0xDFF;
        play->nextEntrance = sSave.entrance;
        play->transitionTrigger = TRANS_TRIGGER_START;
        play->transitionType = TRANS_TYPE_FADE_BLACK;
        gSaveContext.nextTransitionType = TRANS_TYPE_FADE_BLACK_FAST;
        gSaveContext.respawnFlag = 1;
        sWaiveVoid = true;
    }
}

static u32 SlingButton(Player* player) {
    return player->heldItemButton < S2H_ITEM_BUTTON_COUNT ? sButtons[player->heldItemButton] : 0;
}
static void AimSling(Player* player, PlayState* play) {
    Input* input = &play->state.input[0];
    Z64Items_KeepHeld(sApi);
    if (!sSlingOut || (input->press.button & (BTN_B | BTN_A | BTN_CUP))) {
        Z64Aiming_Release(player, play);
        func_8083B1A0(player, play);
        return;
    }
    Z64Aiming_Update(player, play);
    Player_UpdateUpperBody(player, play);
    if (sSlingCooldown > 0) --sSlingCooldown;
    if (sSlingCooldown == 0 && (input->press.button & SlingButton(player))) {
        if (sSave.seeds <= 0) { Audio_PlaySfx(NA_SE_SY_ERROR); return; }
        s16 yaw, pitch;
        Z64Aiming_GetDirection(player, &yaw, &pitch);
        Actor* shot = Actor_Spawn(&play->actorCtx, play, ACTOR_EN_ARROW, player->actor.focus.pos.x,
                                  player->actor.focus.pos.y, player->actor.focus.pos.z, pitch, yaw, 0,
                                  ARROW_TYPE_SLINGSHOT);
        if (shot != NULL) {
            u8 previous = player->unk_D57;
            player->unk_D57 = 1;
            func_8088A594((EnArrow*)shot, play);
            player->unk_D57 = previous;
            --sSave.seeds;
            Store();
            sSlingCooldown = 12;
        }
    }
}
static void TakeSling(PlayState* play, Player* player) {
    sSlingOut = true;
    sSlingCooldown = 8;
    Z64Aiming_Request(player, play);
    Player_SetAction(play, player, AimSling, 1);
}
static void PutSling(Player* player, PlayState* play) {
    sSlingOut = false;
    Z64Aiming_Release(player, play);
    if (player->actionFunc == AimSling) func_8083B1A0(player, play);
}
static void DrawSlingReticle(void) {
    PlayState* play = gPlayState;

    if (play == NULL || !sSlingOut || !Z64Aiming_IsAiming() || play->pauseCtx.state != PAUSE_STATE_OFF) {
        return;
    }
    Z64Aiming_DrawReticle(play, GET_PLAYER(play), SLINGSHOT_RETICLE_RANGE,
                          (Z64AimingReticleStyle){ Z64AIMING_TOGGLE_BOW, Z64AIMING_TINT_LAUNCH });
}
static s32 HoldItem(Player* player, PlayState* play) { return 0; }
static s32 SeedCount(const char* key) { return sSave.seeds; }
static void RefillSeeds(const char* key) { sSave.seeds = 50; Store(); }

static void Tick(Player* player) {
    PlayState* play = gPlayState;
    if (play == NULL || play->pauseCtx.state != PAUSE_STATE_OFF) return;
    if (sShieldFrames > 0) {
        --sShieldFrames;
        if (player->invincibilityTimer < 20) player->invincibilityTimer = 20;
        if ((play->gameplayFrames & 7) == 0) {
            s16 angle = play->gameplayFrames * 0x800;
            Vec3f pos = { player->actor.world.pos.x + Math_SinS(angle) * 35, player->actor.world.pos.y + 40,
                          player->actor.world.pos.z + Math_CosS(angle) * 35 };
            Vec3f zero = { 0, 0, 0 };
            Color_RGBA8 primary = { 100, 180, 255, 255 }, env = { 0, 40, 255, 0 };
            EffectSsKirakira_SpawnDispersed(play, &pos, &zero, &zero, &primary, &env, 600, 16);
        }
    }
    const char* held = sApi->GetEquippedCustomItem(player->heldItemButton);
    if (held != NULL && strcmp(held, HAMMER) == 0 && player->heldItemAction == PLAYER_IA_CUSTOM) {
        Z64Items_KeepHeld(sApi);
        for (s32 i = 0; i < 2; ++i) {
            player->meleeWeaponQuads[i].elem.atDmgInfo.dmgFlags = DMG_GORON_PUNCH | DMG_GORON_POUND;
            player->meleeWeaponQuads[i].elem.atDmgInfo.damage = 4;
        }
    }
}
static void Scene(s16 scene, s8 spawn) {
    sBoom = NULL;
    sSlingOut = false;
    sShieldFrames = 0;
    Z64Aiming_Drop();
}
static void ActorDestroyed(Actor* actor) { if (actor == sBoom) sBoom = NULL; }

static void RegisterItem(const char* key, const char* icon, const char* name, const char* text,
                         u8 slot, S2HCustomItemInitFunc init) {
    S2HCustomItemDefinition item = Z64Items_Define(key, icon, name);
    Z64Items_SetButtons(&item, S2H_CUSTOM_ITEM_C_BUTTON | S2H_CUSTOM_ITEM_DPAD);
    Z64Items_SetForms(&item, S2H_CUSTOM_ITEM_FORM_BIT(PLAYER_FORM_HUMAN));
    Z64Items_SetPlacement(&item, 2, slot, 0);
    Z64Items_SetPauseText(&item, text);
    Z64Items_SetTextbox(&item, text);
    const S2HCustomItemRandomizer randomizer = {
        sizeof(S2HCustomItemRandomizer), 0, S2H_CUSTOM_ITEM_TYPE_ITEM, 0, 1, key, key, "",
    };
    Z64Items_SetLogic(&item, &randomizer);
    Z64Items_SetCanUse(&item, HasModels);
    Z64Items_SetAction(&item, init, NULL);
    item.flags |= S2H_CUSTOM_ITEM_INSTANT;
    if (strcmp(key, HAMMER) == 0) {
        item.flags &= ~S2H_CUSTOM_ITEM_INSTANT;
        item.update = HoldItem;
        item.meleeWeapon = Z64ITEMS_MELEE_GREAT_FAIRY_SWORD;
        Z64Items_SetHeldModel(&item, sHammerDL, PLAYER_MODELGROUP_TWO_HAND_SWORD);
        Z64Items_SetGetItemModel(&item, sHammerDL);
    } else if (strcmp(key, SLING) == 0) {
        item.flags &= ~S2H_CUSTOM_ITEM_INSTANT;
        item.update = HoldItem;
        item.putAway = PutSling;
        item.onAcquire = RefillSeeds;
        item.getAmmo = SeedCount;
        Z64Items_SetHeldModel(&item, sSlingDL, PLAYER_MODELGROUP_BOW);
        Z64Items_SetFirstPersonModel(&item, sSlingDL);
    } else if (strcmp(key, BOOM) == 0) Z64Items_SetGetItemModel(&item, sBoomDL);
    Z64Items_Register(sApi, &item);
}

S2H_MOD_EXPORT void ModSetApi(const S2HModApi* api) { sApi = api; }
S2H_MOD_EXPORT const S2HModRequirements* ModGetRequirements(void) { return &sRequirements; }
S2H_MOD_EXPORT void ModInit(void) {
    Load(0);
    S2HActorDefinition actor = { 0 };
    actor.structSize = sizeof(actor);
    actor.key = "oot.boomerang.projectile";
    actor.description = "OoT boomerang";
    actor.category = ACTORCAT_ITEMACTION;
    actor.actorFlags = ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
    actor.objectId = GAMEPLAY_KEEP;
    actor.instanceSize = sizeof(OotEffect);
    actor.init = BoomInit;
    actor.destroy = EffectDestroy;
    actor.update = BoomUpdate;
    actor.draw = BoomDraw;
    sBoomId = sApi->RegisterActor(&actor);
    actor.key = "oot.dins_fire.effect";
    actor.description = "Din's Fire";
    actor.init = FireInit;
    actor.update = FireUpdate;
    actor.draw = NULL;
    sFireId = sApi->RegisterActor(&actor);
    if (sBoomId < 0 || sFireId < 0) return;
    RegisterItem(SLING, OOT_ICON("gItemIconSlingshotTex"), OOT_NAME("gFairySlingshotItemNameENGTex"),
                 "%rFairy Slingshot&%wPress its button to aim, press again to fire. A/B leaves aiming. 50 seeds.", 0, TakeSling);
    RegisterItem(BOOM, OOT_ICON("gItemIconBoomerangTex"), OOT_NAME("gBoomerangItemNameENGTex"),
                 "%rBoomerang&%wThrow to stun enemies. Press again to call it back.", 1, ThrowBoom);
    RegisterItem(HAMMER, OOT_ICON("gItemIconHammerTex"), OOT_NAME("gMegatonHammerItemNameENGTex"),
                 "%rMegaton Hammer&%wA heavy two-handed weapon. Its strikes use MM Goron punch/pound damage.", 2, NULL);
    RegisterItem(DIN, OOT_ICON("gItemIconDinsFireTex"), OOT_NAME("gDinsFireItemNameENGTex"),
                 "%rDin's Fire&%wAn expanding fire blast. 12 MP.", 3, CastDin);
    RegisterItem(FARORE, OOT_ICON("gItemIconFaroresWindTex"), OOT_NAME("gFaroresWindItemNameENGTex"),
                 "%gFarore's Wind&%wSet a return point, then warp back. Hold R when casting to dispel it. 6 MP.", 4, CastFarore);
    RegisterItem(NAYRU, OOT_ICON("gItemIconNayrusLoveTex"), OOT_NAME("gNayrusLoveItemNameENGTex"),
                 "%bNayru's Love&%wA protective aura for one minute. 24 MP.", 5, CastNayru);
    S2H_REGISTER_HOOK(sApi, OnPlayerUpdate, Tick);
    S2H_REGISTER_HOOK(sApi, OnSceneInit, Scene);
    S2H_REGISTER_HOOK(sApi, OnPlayDrawEnd, DrawSlingReticle);
    S2H_REGISTER_HOOK(sApi, OnLoadFile, Load);
    S2H_REGISTER_HOOK(sApi, OnActorDestroy, ActorDestroyed);
    sApi->RegisterVB(VB_INFLICT_VOID_DAMAGE, WaiveVoid);
}
