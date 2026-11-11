#include "z64oot_names.h"
#ifdef _WIN32
__declspec(dllimport) void EnIshi_Idle(EnIshi*, PlayState*);
#else
void EnIshi_Idle(EnIshi*, PlayState*);
#endif
void EnIshi_DropItem(EnIshi*, PlayState*);
void EnIshi_DropCollectable(EnIshi*, PlayState*);
void EnIshi_SetupCutsceneExplode(EnIshi*);
void EnIshi_SpawnDebrisBoulder(Actor*, PlayState*);
void EnIshi_SpawnDustBoulder(Actor*, PlayState*);
typedef struct {Actor* actor;u32 swing;u8 hits;} AxeProp;
static AxeProp sProps[32];
static void ResetProps(void){memset(sProps,0,sizeof(sProps));}
static bool PropHit(Actor* actor,int hits,float range){
 Player* p=GET_PLAYER(gPlayState);AdvanceSwing(p,gPlayState);
 if(Weapon(p)!=CH_HAMMER||!p->meleeWeaponState||Actor_WorldDistXYZToActor(&p->actor,actor)>range||
    ABS((s16)(Actor_WorldYawTowardActor(&p->actor,actor)-p->actor.shape.rot.y))>0x3800)return false;
 AxeProp* slot=NULL;
 for(int i=0;i<ARRAY_COUNT(sProps);i++)if(sProps[i].actor==actor){slot=&sProps[i];break;}
 if(!slot)for(int i=0;i<ARRAY_COUNT(sProps);i++)if(!sProps[i].actor||!IsActorAlive(gPlayState,sProps[i].actor)){slot=&sProps[i];*slot=(AxeProp){actor,UINT32_MAX,0};break;}
 if(!slot||slot->swing==sSwingId)return false;
 slot->swing=sSwingId;slot->hits++;Audio_PlayActorSound2(actor,NA_SE_IT_HAMMER_HIT);
 CollisionCheck_SpawnShieldParticlesMetal(gPlayState,&actor->world.pos);
 if(slot->hits<hits)return false;slot->actor=NULL;return true;
}
// NEI's IKAxe_SwingBreakSweep: the Power Keg's obstacles also fall to the axe (heavy blocks 5 hits, rocks 1).
#define HEAVY_BLOCK_PIECE_BIG 2
#define HEAVY_BLOCK_PIECE_SMALL 3

static void BreakBoulder(Actor* actor, bool dropsItem) {
    PlayState* play = gPlayState;

    if (dropsItem) {
        Vec3f dropPos = { actor->world.pos.x, actor->world.pos.y + 30.0f, actor->world.pos.z };
        Item_DropCollectibleRandom(play, NULL, &dropPos, 0);
    }
    SoundSource_PlaySfxAtFixedWorldPos(play, &actor->world.pos, 40, NA_SE_EV_WALL_BROKEN);
    Actor_Kill(actor);
}

static void SmashHeavyBlock(Actor* actor) {
    const s32 type = actor->params & 0xFF;

    if (type != HEAVY_BLOCK_PIECE_BIG && type != HEAVY_BLOCK_PIECE_SMALL && PropHit(actor, 5, 180.0f)) {
        Actor_Kill(actor);
    }
}

static void SmashSilverBoulder(Actor* actor);

static void SmashProp(Actor* actor) {
    if (!actor->update) {
        return;
    }
    switch (actor->id) {
        case ACTOR_EN_ISHI:
            SmashSilverBoulder(actor);
            break;
        case ACTOR_BG_HEAVY_BLOCK:
            SmashHeavyBlock(actor);
            break;
        case ACTOR_OBJ_HAMISHI:
        case ACTOR_OBJ_BOMBIWA:
            if (PropHit(actor, 1, 250.0f)) {
                BreakBoulder(actor, true);
            }
            break;
        case ACTOR_EN_GOROIWA:
            if (PropHit(actor, 1, 250.0f)) {
                BreakBoulder(actor, false);
            }
            break;
        default:
            break;
    }
}

static void SmashSilverBoulder(Actor* actor) {
    EnIshi* rock = (EnIshi*)actor;
    if (ENISHI_GET_SIZE_FLAG(actor) != ISHI_SIZE_SILVER_BOULDER ||
        rock->actionFunc != EnIshi_Idle || !PropHit(actor, 3, 250.0f)) return;
    PlayState* play = gPlayState;
    if (rock->flags & ISHI_FLAG_CUTSCENE_ROCK) {
        EnIshi_DropCollectable(rock, play);
        EnIshi_SetupCutsceneExplode(rock);
        return;
    }
    EnIshi_DropItem(rock, play);
    EnIshi_SpawnDebrisBoulder(actor, play);
    EnIshi_SpawnDustBoulder(actor, play);
    SoundSource_PlaySfxAtFixedWorldPos(play, &actor->world.pos, 20, NA_SE_EV_ROCK_BROKEN);
    Actor_Kill(actor);
}
