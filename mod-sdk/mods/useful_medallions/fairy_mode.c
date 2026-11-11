// Spirit possession: Ivan controls/movement from NEI, with a mod-owned MM actor.
#include "sw97_mod.h"
#include "overlays/actors/ovl_En_Elf/z_en_elf.h"
#include <string.h>

void EnElf_Init(Actor* actor, PlayState* play);
void EnElf_Destroy(Actor* actor, PlayState* play);
void EnElf_Draw(Actor* actor, PlayState* play);
void func_8083B1A0(Player* player, PlayState* play);
void Player_SetAction_PreserveItemAction(PlayState* play, Player* player, PlayerActionFunc action, s32 flags);

typedef struct { EnElf fairy; f32 yVelocity; } SpiritFairy;

static struct {
    bool active;
    s16 timer;
    Actor* fairy;
    ActorFunc draw;
    PlayerActionFunc action;
    u32 stateFlags;
    s8 previousInvincibility;
} sFairy;
static s16 sFairyId = -1;

bool Sw97_IsFairyModeActive(void) { return sFairy.active; }

static void HoldBody(Player* player, PlayState* play) {
    player->stateFlags1 |= PLAYER_STATE1_20;
    player->speedXZ = player->actor.speed = 0.0f;
    player->actor.velocity = (Vec3f){ 0, 0, 0 };
    player->actor.draw = NULL;
    if (sFairy.fairy != NULL) {
        player->actor.world.pos = sFairy.fairy->world.pos;
        player->actor.focus.pos = sFairy.fairy->world.pos;
    }
}

static void FairyInit(Actor* actor, PlayState* play) {
    actor->params = FAIRY_TYPE_5;
    EnElf_Init(actor, play);
    actor->room = -1;
    EnElf* fairy=(EnElf*)actor;
    static const ALIGN_ASSET(2) char skel[]="__OTR__objects/oot_gameplay_keep/gFairySkel";
    static const ALIGN_ASSET(2) char anim[]="__OTR__objects/oot_gameplay_keep/gFairyAnim";
    if(gSw97Api->HasResource(skel) && gSw97Api->HasResource(anim))
        SkelAnime_Init(play,&fairy->skelAnime,(SkeletonHeader*)skel,(AnimationHeader*)anim,
                       fairy->jointTable,fairy->morphTable,FAIRY_LIMB_MAX);
    Actor_SetScale(actor,0.008f);
    actor->shape.yOffset=1000.0f;
    fairy->innerColor=(Color_RGBAf){255,255,255,255};
    fairy->outerColor=(Color_RGBAf){0,255,0,255};
}

// Ivan's orb transform/material, independent of Tatl's flicker and visibility rules.
static s32 SpiritFairyLimb(PlayState* play,s32 limb,Gfx** dl,Vec3f* pos,Vec3s* rot,Actor* actor,Gfx** gfx){
    if(limb==8){
        Vec3f zero={0,0,0},world;
        f32 scale=((Math_SinS(4096)*0.1f)+1.0f)*0.012f*(actor->scale.x*124.99999f);
        Matrix_MultVec3f(&zero,&world);
        Matrix_Translate(world.x,world.y,world.z,MTXMODE_NEW);
        Matrix_Scale(scale,scale,scale,MTXMODE_APPLY);
    }
    return false;
}
static void FairyDraw(Actor* actor,PlayState* play){
    EnElf* fairy=(EnElf*)actor;
    Gfx* material=GRAPH_ALLOC(play->state.gfxCtx,4*sizeof(Gfx));
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL27_Xlu(play->state.gfxCtx);
    gSPSegment(POLY_XLU_DISP++,0x08,(uintptr_t)material);
    gDPPipeSync(material++);
    gDPSetPrimColor(material++,0,1,255,255,255,255);
    gDPSetRenderMode(material++,G_RM_PASS,G_RM_ZB_CLD_SURF2);
    gSPEndDisplayList(material++);
    gDPSetEnvColor(POLY_XLU_DISP++,0,255,0,50);
    POLY_XLU_DISP=SkelAnime_Draw(play,fairy->skelAnime.skeleton,fairy->skelAnime.jointTable,SpiritFairyLimb,NULL,actor,POLY_XLU_DISP);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void FairyUpdate(Actor* actor, PlayState* play) {
    EnElf* fairy = (EnElf*)actor;
    Input* input = &play->state.input[0];
    // EnPartner_Update: camera-relative stick /10, A rises, B descends.
    Vec3f forward={GET_ACTIVE_CAM(play)->at.x-GET_ACTIVE_CAM(play)->eye.x,0,
                   GET_ACTIVE_CAM(play)->at.z-GET_ACTIVE_CAM(play)->eye.z};
    f32 length=sqrtf(SQ(forward.x)+SQ(forward.z));
    if(length>0){forward.x/=length;forward.z/=length;}
    f32 x=input->cur.stick_x/10.0f, y=input->cur.stick_y/10.0f;
    f32 vx=-forward.z*x+forward.x*y, vz=forward.x*x+forward.z*y;
    if(vx!=0 || vz!=0){
        s16 direction=Math_Atan2S(-vx,vz)-0x4000;
        Math_SmoothStepToS(&actor->world.rot.y,direction,2,10000,0);
        Math_SmoothStepToS(&actor->shape.rot.y,direction,2,10000,0);
    }
    Math_SmoothStepToF(&actor->speed,sqrtf(x*x+y*y),1.0f,1.3f,0);
    SpiritFairy* spirit=(SpiritFairy*)actor;
    f32 target=input->cur.button&BTN_A?6.0f:input->cur.button&BTN_B?-6.0f:0.0f;
    Math_SmoothStepToF(&spirit->yVelocity,target,1.0f,1.5f,0);
    actor->world.pos.x+=Math_SinS(actor->world.rot.y)*actor->speed;
    actor->world.pos.z+=Math_CosS(actor->world.rot.y)*actor->speed;
    actor->world.pos.y+=spirit->yVelocity;
    fairy->timer++;
    SkelAnime_Update(&fairy->skelAnime);
    Vec3f velocity={0,-0.05f,0},accel={0,-0.025f,0};
    Vec3f pos={actor->world.pos.x+Rand_CenteredFloat(6),actor->world.pos.y+Rand_ZeroOne()*6+5,
               actor->world.pos.z+Rand_CenteredFloat(6)};
    Color_RGBA8 primary={255,255,255,255},secondary={0,255,0,255};
    EffectSsKirakira_SpawnDispersed(play,&pos,&velocity,&accel,&primary,&secondary,1500,12);
    Lights_PointGlowSetInfo(&fairy->lightInfoGlow,actor->world.pos.x,actor->world.pos.y+9,actor->world.pos.z,200,255,200,100);

}

static void EndFlight(Player* player, PlayState* play) {
    if (player != NULL) {
        player->actor.draw = sFairy.draw;
        player->stateFlags1 = (player->stateFlags1 & ~PLAYER_STATE1_20) | (sFairy.stateFlags & PLAYER_STATE1_20);
        Player_SetAction_PreserveItemAction(play, player, func_8083B1A0, 0);
        player->stateFlags1 &= ~(PLAYER_STATE1_10000000 | PLAYER_STATE1_20000000);
        player->stateFlags2 &= ~0x100000;
        player->invincibilityTimer = 20;
        Camera_SetFinishedFlag(Play_GetCamera(play,CAM_ID_MAIN));
        Magic_Reset(play);
        play->envCtx.adjLightSettings.fogNear=0;
        memset(play->envCtx.adjLightSettings.fogColor,0,sizeof(play->envCtx.adjLightSettings.fogColor));
        Rumble_Request(200.0f,150,20,80);
        Actor_PlaySfx(&player->actor,NA_SE_EV_TRIFORCE_FLASH);
    }
    if (sFairy.fairy != NULL) Actor_Kill(sFairy.fairy);
    sFairy.fairy = NULL;
    sFairy.active = false;
}

void Sw97_StartFairyMode(void) {
    PlayState* play = gPlayState;
    Player* player = play == NULL ? NULL : GET_PLAYER(play);
    if (sFairy.active || player == NULL || sFairyId < 0) return;
    Actor* fairy = Actor_Spawn(&play->actorCtx, play, sFairyId, player->actor.world.pos.x,
        player->actor.world.pos.y + Player_GetHeight(player) + 5.0f, player->actor.world.pos.z, 0, 0, 0, FAIRY_TYPE_5);
    if (fairy == NULL) return;
    memset(&sFairy, 0, sizeof(sFairy));
    sFairy.fairy = fairy;
    sFairy.draw = player->actor.draw;
    sFairy.action = player->actionFunc;
    sFairy.stateFlags = player->stateFlags1;
    sFairy.previousInvincibility=player->invincibilityTimer;
    Rumble_Request(200.0f,150,20,80);
    Actor_PlaySfx(&player->actor,NA_SE_EV_TRIFORCE_FLASH);
    sFairy.active = true;
    Player_SetAction_PreserveItemAction(play, player, HoldBody, 0);
}

static void TickFlight(Player* player) {
    if (!sFairy.active || gPlayState == NULL) return;
    u32 buttons = 0;
    static const u32 masks[S2H_ITEM_BUTTON_COUNT] = { BTN_B, BTN_CLEFT, BTN_CDOWN, BTN_CRIGHT, 0, BTN_DRIGHT, BTN_DLEFT, BTN_DDOWN, BTN_DUP };
    for (u8 i = 0; i < S2H_ITEM_BUTTON_COUNT; i++) {
        const char* key = gSw97Api->GetEquippedCustomItem(i);
        if (key != NULL && strcmp(key, "nei.medallion.spirit") == 0) buttons |= masks[i];
    }
    bool hit=player->invincibilityTimer>0 && sFairy.previousInvincibility==0;
    sFairy.previousInvincibility=player->invincibilityTimer;
    if (sFairy.fairy == NULL || hit || (player->stateFlags1 & (PLAYER_STATE1_DEAD|PLAYER_STATE1_8000000)) ||
        (++sFairy.timer > 10 && (gPlayState->state.input[0].press.button & buttons))) {
        EndFlight(player, gPlayState);
        return;
    }
    HoldBody(player, gPlayState);
}

static void ForgetFairy(Actor* actor) {
    if (actor == sFairy.fairy) sFairy.fairy = NULL;
}
static void ForgetScene(void) { memset(&sFairy, 0, sizeof(sFairy)); }

void Sw97_RegisterFairyMode(void) {
    S2HActorDefinition actor = { 0 };
    actor.structSize = sizeof(actor);
    actor.key = "nei.medallion.spirit_fairy";
    actor.description = "Spirit Medallion fairy";
    actor.category = ACTORCAT_ITEMACTION;
    actor.actorFlags = ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
    actor.objectId = GAMEPLAY_KEEP;
    actor.instanceSize = sizeof(SpiritFairy);
    actor.init = FairyInit;
    actor.destroy = EnElf_Destroy;
    actor.update = FairyUpdate;
    actor.draw = FairyDraw;
    sFairyId = gSw97Api->RegisterActor(&actor);
    S2H_REGISTER_HOOK(gSw97Api, OnPlayerUpdate, TickFlight);
    S2H_REGISTER_HOOK(gSw97Api, OnActorDestroy, ForgetFairy);
    S2H_REGISTER_HOOK(gSw97Api, OnPlayDestroy, ForgetScene);
}
