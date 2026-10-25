// MM port: OoT's Lizalfos role goes to the Dinolfos, and MM has no Stalfos (its En_Test is the crater marks).
// Each enemy keeps its native damage, stun and death; this mod only swaps in its own action while possessed.
#include <stdbool.h>
#include <math.h>
#include <string.h>
#include "functions.h"
#include "variables.h"
#include "macros.h"
#include "z64items.h"
#include "soh/ResourceManagerHelpers.h"
#include "partner_control.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "overlays/actors/ovl_En_Dinofos/z_en_dinofos.h"
#include "overlays/actors/ovl_En_Tite/z_en_tite.h"
#include "overlays/actors/ovl_En_Firefly/z_en_firefly.h"
#include "overlays/actors/ovl_En_Okuta/z_en_okuta.h"
#include "objects/object_dinofos/object_dinofos.h"
#include "objects/object_tite/object_tite.h"
#include "z64oot_names.h"

void EnDinofos_SetupIdle(EnDinofos*);void EnDinofos_SetupSlash(EnDinofos*);
// MM's Tektite states are unnamed: idle on the ground, and the attack wind-up that leads into its leap.
void func_80893ED4(EnTite*);void func_80893FD0(EnTite*);
#define EnTite_SetupIdle func_80893ED4
#define EnTite_SetupAttack func_80893FD0
void EnFirefly_SetupFlyIdle(EnFirefly*);
void EnOkuta_SetupFloat(EnOkuta*);void EnOkuta_SetupShoot(EnOkuta*,PlayState*);void EnOkuta_SpawnProjectile(EnOkuta*,PlayState*);

#define CAP_KEY "nei.possession_cap"
#define PROVIDER_KEY "nei.enemy_control"
#define OKUTA_SHOT_FRAME 6
static const SOHModApi*sApi;
static Actor*sHost,*sQueued;
static Input sInput;
static s16 sCameraYaw,sAttack,sCooldown,sLastId,sLastParams;
static bool sRegistered,sSpawnProjectile,sActive;
static Vec3f sOrigin;
static const ALIGN_ASSET(2) char sModel[]="__OTR__objects/object_nei_possession/cap_dl";
static const ALIGN_ASSET(2) char sIcon[]="__OTR__textures/icon_item_custom/gPossessionCapIconTex";
static const ALIGN_ASSET(2) char sName[]="__OTR__textures/item_name_custom/gPossessionCapNameTex";
typedef struct {Collider*base;ColliderElement*elem;u8 atFlags,acFlags;u32 damageFlags;u8 damage;}ColliderSnapshot;
static ColliderSnapshot sColliders[12];static u8 sColliderCount;
static const char*sHooks[]={"OnGameFrameUpdate","OnActorUpdate","OnActorInit","OnActorKill","OnActorDrawEnd",
    "OnActorResolvePlayerRelation","OnSceneInit","OnLoadGame","OnConsoleGive"};
static const SOHModRequirements sRequirements={sizeof(SOHModRequirements),sHooks,ARRAY_COUNT(sHooks)};

static bool Live(PlayState*play,const Actor*w){if(!w)return false;for(int c=0;c<ACTORCAT_MAX;++c)for(Actor*a=play->actorCtx.actorLists[c].first;a;a=a->next)if(a==w)return a->update!=NULL;return false;}
static bool Supported(Actor*a){
    if(!a||!a->update||!a->draw||!a->colChkInfo.health||a->parent)return false;
    switch(a->id){case ACTOR_EN_DINOFOS:case ACTOR_EN_TITE:case ACTOR_EN_FIREFLY:return true;
        case ACTOR_EN_OKUTA:return EN_OKUTA_GET_TYPE(a)<EN_OKUTA_TYPE_PROJECTILE_BASE;default:return false;}
}
static SkelAnime* Skeleton(Actor*a){
    switch(a->id){case ACTOR_EN_DINOFOS:return &((EnDinofos*)a)->skelAnime;
        case ACTOR_EN_TITE:return &((EnTite*)a)->skelAnime;case ACTOR_EN_FIREFLY:return &((EnFirefly*)a)->skelAnime;
        case ACTOR_EN_OKUTA:return &((EnOkuta*)a)->skelAnime;default:return NULL;}
}
static void Driver(Actor*,PlayState*);
static void DinofosDriver(EnDinofos*self,PlayState*play){Driver(&self->actor,play);}
static void TiteDriver(EnTite*self,PlayState*play){Driver(&self->actor,play);}
static void FireflyDriver(EnFirefly*self,PlayState*play){Driver(&self->actor,play);}
static void OkutaDriver(EnOkuta*self,PlayState*play){Driver(&self->actor,play);}
static bool OwnAction(Actor*a){switch(a->id){
    case ACTOR_EN_DINOFOS:return ((EnDinofos*)a)->actionFunc==DinofosDriver;
    case ACTOR_EN_TITE:return ((EnTite*)a)->actionFunc==TiteDriver;case ACTOR_EN_FIREFLY:return ((EnFirefly*)a)->actionFunc==FireflyDriver;
    case ACTOR_EN_OKUTA:return ((EnOkuta*)a)->actionFunc==OkutaDriver;default:return false;}}
static void InstallDriver(Actor*a){switch(a->id){
    case ACTOR_EN_DINOFOS:((EnDinofos*)a)->actionFunc=DinofosDriver;break;
    case ACTOR_EN_TITE:((EnTite*)a)->actionFunc=TiteDriver;break;case ACTOR_EN_FIREFLY:((EnFirefly*)a)->actionFunc=FireflyDriver;break;
    case ACTOR_EN_OKUTA:((EnOkuta*)a)->actionFunc=OkutaDriver;break;default:break;}}
static void SetupNativeIdle(Actor*a){switch(a->id){
    case ACTOR_EN_DINOFOS:EnDinofos_SetupIdle((EnDinofos*)a);break;
    case ACTOR_EN_TITE:EnTite_SetupIdle((EnTite*)a);break;
    case ACTOR_EN_FIREFLY:EnFirefly_SetupFlyIdle((EnFirefly*)a);break;
    case ACTOR_EN_OKUTA:EnOkuta_SetupFloat((EnOkuta*)a);((EnOkuta*)a)->headScale=(Vec3f){1,1,1};break;
    default:break;}}
static void Idle(Actor*a,PlayState*play){a->speed=0;sAttack=0;SetupNativeIdle(a);InstallDriver(a);}
static void Snapshot(Collider*base,ColliderElement*elem){
    if(sColliderCount>=ARRAY_COUNT(sColliders))return;
    sColliders[sColliderCount++]=(ColliderSnapshot){base,elem,base->atFlags,base->acFlags,elem->atDmgInfo.dmgFlags,elem->atDmgInfo.damage};
    base->acFlags|=AC_TYPE_ENEMY;
    base->atFlags=(base->atFlags&~AT_TYPE_ENEMY)|AT_TYPE_PLAYER;
    elem->atDmgInfo.dmgFlags=DMG_SWORD;
}
static void TakeColliders(Actor*a){
    sColliderCount=0;
    switch(a->id){
        case ACTOR_EN_DINOFOS:{EnDinofos*t=(EnDinofos*)a;Snapshot(&t->knifeCollider.base,&t->knifeCollider.elem);
            for(int i=0;i<ARRAY_COUNT(t->bodyAndFireColliderElements);++i)Snapshot(&t->bodyAndFireCollider.base,&t->bodyAndFireColliderElements[i].base);break;}
        case ACTOR_EN_TITE:{EnTite*t=(EnTite*)a;Snapshot(&t->collider.base,&t->collider.elem);break;}
        case ACTOR_EN_FIREFLY:{EnFirefly*t=(EnFirefly*)a;Snapshot(&t->collider.base,&t->collider.elem);break;}
        case ACTOR_EN_OKUTA:{EnOkuta*t=(EnOkuta*)a;Snapshot(&t->collider.base,&t->collider.elem);break;}
        default:break;
    }
}
static void RestoreColliders(void){for(int i=0;i<sColliderCount;++i){ColliderSnapshot*c=&sColliders[i];
    c->base->atFlags=(c->base->atFlags&~(AT_TYPE_PLAYER|AT_TYPE_ENEMY))|(c->atFlags&(AT_TYPE_PLAYER|AT_TYPE_ENEMY));
    c->base->acFlags=(c->base->acFlags&~(AC_TYPE_PLAYER|AC_TYPE_ENEMY))|(c->acFlags&(AC_TYPE_PLAYER|AC_TYPE_ENEMY));
    c->elem->atDmgInfo.dmgFlags=c->damageFlags;c->elem->atDmgInfo.damage=c->damage;
}sColliderCount=0;}
static Actor* ProviderActor(PlayState*play){return sActive&&Live(play,sHost)&&sHost->colChkInfo.health?sHost:NULL;}
static bool Begin(PlayState*play){
    if(!Live(play,sQueued)||!Supported(sQueued))return false;
    sHost=sQueued;sQueued=NULL;sActive=true;sLastId=sHost->id;sLastParams=sHost->params;
    sOrigin=GET_PLAYER(play)->actor.world.pos;sAttack=sCooldown=0;memset(&sInput,0,sizeof(sInput));
    TakeColliders(sHost);Idle(sHost,play);return true;
}
static bool Ground(PlayState*play,Vec3f*point){
    CollisionPoly*poly=NULL;s32 bg;Vec3f probe=*point;probe.y+=70;
    f32 y=BgCheck_EntityRaycastFloor5(play,&play->colCtx,&poly,&bg,&GET_PLAYER(play)->actor,&probe);
    if(!poly||y<=BGCHECK_Y_MIN||y>point->y+55||y<point->y-250)return false;point->y=y;return true;
}
static void End(PlayState*play){
    if(!sActive)return;Player*p=GET_PLAYER(play);Vec3f point=p->actor.world.pos;
    if(Live(play,sHost)){
        if(p->actor.parent==sHost){point=sHost->world.pos;point.x-=Math_SinS(sHost->shape.rot.y)*50;point.z-=Math_CosS(sHost->shape.rot.y)*50;}
        RestoreColliders();
        if(OwnAction(sHost)&&sHost->colChkInfo.health)SetupNativeIdle(sHost);
    }else sColliderCount=0;
    if(!Ground(play,&point)){Vec3f origin=sOrigin;if(Ground(play,&origin))point=origin;}
    p->actor.world.pos=p->actor.prevPos=point;p->actor.velocity=(Vec3f){0,0,0};p->speedXZ=0;
    sActive=false;sHost=sQueued=NULL;
}
static void InputPartner(PlayState*play,const Input*input,s16 yaw){if(sActive){sInput=*input;sCameraYaw=yaw;}}
static bool RegisterPartner(void){
    if(sRegistered)return true;if(!SOH_MOD_API_HAS(sApi,FindService))return false;
    NeiRegisterPartnerV1 reg=(NeiRegisterPartnerV1)sApi->FindService("nei.partner.register.v1");
    static const NeiPartnerV1 provider={sizeof(provider),PROVIDER_KEY,"Enemy",ProviderActor,Begin,End,InputPartner,NEI_PARTNER_TRACK_PLAYER};
    if(reg)sRegistered=reg(&provider);return sRegistered;
}
static void StartAttack(Actor*a,PlayState*play){
    sCooldown=15;sAttack=30;
    switch(a->id){
        case ACTOR_EN_DINOFOS:EnDinofos_SetupSlash((EnDinofos*)a);break;
        case ACTOR_EN_TITE:EnTite_SetupAttack((EnTite*)a);a->velocity.y=8;a->speed=6;a->bgCheckFlags&=~BGCHECKFLAG_GROUND;sAttack=18;break;
        case ACTOR_EN_FIREFLY:sAttack=12;break;
        case ACTOR_EN_OKUTA:EnOkuta_SetupShoot((EnOkuta*)a,play);break;
        default:break;
    }
    InstallDriver(a);
}
static void Driver(Actor*a,PlayState*play){
    if(!sActive||a!=sHost)return;
    SkelAnime*skel=Skeleton(a);f32 x=sInput.cur.stick_x,y=sInput.cur.stick_y,m=sqrtf(x*x+y*y);
    if(sCooldown)--sCooldown;
    if(m>10){s16 yaw=sCameraYaw+(s16)(atan2f(x,y)*32768/M_PI);Math_SmoothStepToS(&a->shape.rot.y,yaw,2,0x1600,0x40);}
    a->world.rot.y=a->shape.rot.y;
    if(sInput.press.button&BTN_B){if(!sAttack&&!sCooldown)StartAttack(a,play);}
    if(a->id==ACTOR_EN_FIREFLY){
        f32 vertical=(sInput.cur.button&BTN_A)?-0x3000:(sInput.cur.button&BTN_R)?0x3000:0;
        a->shape.rot.x=0x1554-(s16)vertical;
        a->speed=sAttack?12:(m>10?MIN(7,m*.1f):(vertical?4:0));
        if(sAttack)--sAttack;
        skel->playSpeed=sAttack?2:1.3f;SkelAnime_Update(skel);
    }else if(a->id==ACTOR_EN_TITE){
        bool ground=(a->bgCheckFlags&BGCHECKFLAG_GROUND)!=0;
        if(ground&&!sAttack){a->speed=0;if((m>10&&!sCooldown)||(sInput.press.button&BTN_A)){
            EnTite_SetupAttack((EnTite*)a);InstallDriver(a);a->velocity.y=(sInput.press.button&BTN_A)?9:5;
            a->speed=m>10?4:0;a->bgCheckFlags&=~BGCHECKFLAG_GROUND;sCooldown=10;}
            else if(skel->animation!=(AnimationHeader*)ResourceMgr_LoadAnimByName(object_tite_Anim_0012E4))Idle(a,play);
        }
        if(sAttack)--sAttack;SkelAnime_Update(skel);
    }else if(a->id==ACTOR_EN_OKUTA){
        EnOkuta*t=(EnOkuta*)a;a->gravity=0;a->velocity.y=0;a->world.pos.y=a->home.pos.y;
        a->speed=m>10?MIN(4,m*.065f):0;
        if(sAttack){a->speed=0;if(Animation_OnFrame(skel,OKUTA_SHOT_FRAME)){sSpawnProjectile=true;EnOkuta_SpawnProjectile(t,play);sSpawnProjectile=false;}
            if(SkelAnime_Update(skel)){Idle(a,play);sCooldown=15;}}
        else SkelAnime_Update(skel);
        Actor_MoveWithGravity(a);
    }else{
        if(sAttack){
            a->speed=0;
            if(SkelAnime_Update(skel)){Idle(a,play);sCooldown=10;}
        }
        else{
            f32 speed=m>10?MIN(6,(m-10)*.1f):0;a->speed=speed;
            const char*clip=speed>.1f?gDinolfosWalkAnim:gDinolfosIdleAnim;
            AnimationHeader*resolved=(AnimationHeader*)ResourceMgr_LoadAnimByName(clip);
            if(skel->animation!=resolved)Animation_MorphToLoop(skel,resolved,-3);
            skel->playSpeed=speed>.1f?MAX(.7f,speed*.25f):1;
            if((sInput.press.button&BTN_A)&&(a->bgCheckFlags&BGCHECKFLAG_GROUND)){a->velocity.y=9;a->bgCheckFlags&=~BGCHECKFLAG_GROUND;}
            SkelAnime_Update(skel);
        }
    }
    sInput.press.button=sInput.rel.button=0;
}
static void ActorUpdated(Actor*a){
    PlayState*play=gPlayState;if(!play||!sActive||a!=sHost)return;
    if(!a->update||!a->colChkInfo.health)return;
    // Damage/stun/death remain native; reclaim control only after recovery.
    if(!OwnAction(a)&&a->colorFilterTimer==0)Idle(a,play);
    if(sAttack>0){
        if(a->id==ACTOR_EN_TITE)CollisionCheck_SetAT(play,&play->colChkCtx,&((EnTite*)a)->collider.base);
        if(a->id==ACTOR_EN_FIREFLY)CollisionCheck_SetAT(play,&play->colChkCtx,&((EnFirefly*)a)->collider.base);
    }
}
static void ActorInitialized(Actor*a){if(sSpawnProjectile&&a->id==ACTOR_EN_OKUTA&&EN_OKUTA_GET_TYPE(a)>=EN_OKUTA_TYPE_PROJECTILE_BASE){
    EnOkuta*t=(EnOkuta*)a;t->collider.base.atFlags=(t->collider.base.atFlags&~AT_TYPE_ENEMY)|AT_TYPE_PLAYER;
    t->collider.elem.atDmgInfo.dmgFlags=DMG_DEKU_NUT;t->collider.elem.atDmgInfo.damage=1;
}}
static void Relation(Actor*a,f32*xz,f32*y,s16*yaw){if(sActive&&a==sHost){*xz=200;*y=0;*yaw=a->shape.rot.y;}}
static void Killed(Actor*a){if(a==sQueued)sQueued=NULL;if(a==sHost){sColliderCount=0;}}
static Actor* Selection(PlayState*play){Actor*target=GET_PLAYER(play)->focusActor;if(Live(play,target))return target;
    f32 best=400;target=NULL;for(int c=0;c<ACTORCAT_MAX;++c)for(Actor*a=play->actorCtx.actorLists[c].first;a;a=a->next)
        if(Supported(a)){f32 d=Actor_WorldDistXYZToActor(&GET_PLAYER(play)->actor,a);if(d<best){best=d;target=a;}}return target;
}
static bool QueueActor(Actor*a){if(!gPlayState||!Live(gPlayState,a)||!Supported(a)||sActive)return false;sQueued=a;return true;}
static void Use(PlayState*play,Player*p){
    Actor*target=Selection(play);
    if(!QueueActor(target))sApi->ShowTextbox(play,"This enemy cannot be controlled yet.&Try a Dinolfos, Tektite, Keese&or Octorok.",true);
}
static void Console(PlayState*play,const char*type,const char*key,bool*handled,bool*success){
    if(strcmp(type,"possess"))return;*handled=true;*success=QueueActor(Selection(play));
}
static void Reset(void){sHost=sQueued=NULL;sActive=false;sColliderCount=0;sAttack=sCooldown=0;}
static void Scene(s16 sceneId,s8 spawnNum){Reset();}
static void Load(s32 file){Reset();}
static void Tick(void){
    PlayState*play=gPlayState;if(!play)return;RegisterPartner();
    if(sHost&&!Live(play,sHost)){sHost=NULL;sColliderCount=0;}
    if(sActive&&!ProviderActor(play)){
        void(*stop)(void)=(void(*)(void))sApi->FindService("nei.partner.end.v1");if(stop)stop();else End(play);return;
    }
    if(sQueued&&!Live(play,sQueued))sQueued=NULL;
    if(sQueued&&!sActive&&sRegistered){
        bool(*begin)(const char*)=(bool(*)(const char*))sApi->FindService("nei.partner.begin.v1");
        if(begin)begin(PROVIDER_KEY);
    }
}
static void DrawCap(PlayState*play,Vec3f point,s16 yaw,f32 scale){
    OPEN_DISPS(play->state.gfxCtx);Gfx_SetupDL_25Opa(play->state.gfxCtx);Matrix_Push();
    Matrix_Translate(point.x,point.y,point.z,MTXMODE_NEW);Matrix_RotateZYX(0,yaw,0,MTXMODE_APPLY);Matrix_Scale(scale,scale,scale,MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++,MATRIX_NEWMTX(play->state.gfxCtx),G_MTX_NOPUSH|G_MTX_LOAD|G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++,(Gfx*)sModel);Matrix_Pop();CLOSE_DISPS(play->state.gfxCtx);
}
static void ActorDrawn(Actor*a,PlayState*play){if(!sActive||a!=sHost)return;Vec3f point=a->focus.pos;
    if(Math_Vec3f_DistXYZ(&point,&a->world.pos)>150){point=a->world.pos;point.y+=MAX(20,a->colChkInfo.cylHeight);}
    DrawCap(play,point,a->shape.rot.y,.012f);
}
static void GetDraw(PlayState*play,GetItemEntry*entry){
    OPEN_DISPS(play->state.gfxCtx);Gfx_SetupDL_25Opa(play->state.gfxCtx);Matrix_Scale(.045f,.045f,.045f,MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++,MATRIX_NEWMTX(play->state.gfxCtx),G_MTX_NOPUSH|G_MTX_LOAD|G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++,(Gfx*)sModel);CLOSE_DISPS(play->state.gfxCtx);
}
SOH_MOD_EXPORT void ModSetApi(const SOHModApi*api){sApi=api;}
SOH_MOD_EXPORT const SOHModRequirements*ModGetRequirements(void){return &sRequirements;}
SOH_MOD_EXPORT void ModInit(void){
    SOHCustomItemDefinition item=Z64Items_Define(CAP_KEY,sIcon,sName);
    Z64Items_SetButtons(&item,SOH_CUSTOM_ITEM_C_BUTTON|SOH_CUSTOM_ITEM_DPAD);item.flags|=SOH_CUSTOM_ITEM_INSTANT;
    Z64Items_SetPlacement(&item,4,1,0);item.init=Use;item.getItemEntry.drawFunc=GetDraw;
    Z64Items_SetTextbox(&item,"You got the %rPossession Cap%w!&Target a compatible enemy and&press its %y\xA1%w button.^"
        "Move with the Stick, use %b\x9F%w&and %g\xA0%w for its own actions,&and %y\xA5%w to return to Link.");
    Z64Items_SetPauseText(&item,"%rPossession Cap&%y\xA4%w-target an enemy, then use %y\xA1%w.&%y\xA5%w returns to Link.");
    if(!Z64Items_Register(sApi,&item))return;
    if(SOH_MOD_API_HAS(sApi,PublishService))sApi->PublishService("nei.possession.request.v1",(void*)QueueActor);
    SOH_REGISTER_HOOK(sApi,OnGameFrameUpdate,Tick);SOH_REGISTER_HOOK(sApi,OnActorUpdate,ActorUpdated);
    SOH_REGISTER_HOOK(sApi,OnActorInit,ActorInitialized);SOH_REGISTER_HOOK(sApi,OnActorKill,Killed);
    SOH_REGISTER_HOOK(sApi,OnActorResolvePlayerRelation,Relation);SOH_REGISTER_HOOK(sApi,OnActorDrawEnd,ActorDrawn);
    SOH_REGISTER_HOOK(sApi,OnSceneInit,Scene);SOH_REGISTER_HOOK(sApi,OnLoadGame,Load);SOH_REGISTER_HOOK(sApi,OnConsoleGive,Console);
}
