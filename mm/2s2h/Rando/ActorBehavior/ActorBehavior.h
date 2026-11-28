#ifndef RANDO_ACTOR_BEHAVIOR_H
#define RANDO_ACTOR_BEHAVIOR_H

#include "Rando/Rando.h"

namespace Rando {

namespace ActorBehavior {

void Init();
void OnFileLoad();

bool ShouldMatchContainerStyle();
RandoItemType GetContainerItemType(RandoCheckId randoCheckId);
RandoCheckId GetObjectRandoCheckId(void* actor);
void SetObjectRandoCheckId(const void* object, RandoCheckId rc);

void SpawnWonderItemSparkle(Vec3f* pos);
void DrawWonderItemSparkle(Actor* actor, PlayState* play);

void InitDmChar01Behavior();
void InitDmChar05Behavior();
void InitDmChar08Behavior();
void InitDmHinaBehavior();
void InitDmStkBehavior();
void InitDoorWarp1VBehavior();
void InitEnAkindonutsBehavior();
void InitEnAlBehavior();
void InitEnAnBehavior();
void InitEnAob01Behavior();
void InitEnAzBehavior();
void InitEnBabaBehavior();
void InitEnBalBehavior();
void InitEnBjtBehavior();
void InitEnBomBowlManBehavior();
void InitEnBoxBehavior();
void InitEnButteBehavior();
void InitEnCowBehavior();
void InitEnDaiBehavior();
void InitEnDnhBehavior();
void InitEnElfgrpBehavior();
void InitEnElforgBehavior();
void InitEnFish2Behavior();
void InitEnemyDropBehavior();
// The drop check an actor awards when killed, or RC_UNKNOWN; and the actors that award a given drop check.
RandoCheckId GetEnemyDropCheck(int16_t actorId);
std::vector<int16_t> GetEnemyDropActors(RandoCheckId check);
// A custom enemy's drop, awarded when it is killed like the native KILL drops.
void RegisterEnemyDrop(int16_t actorId, RandoCheckId check);
void InitEnFsnBehavior();
void InitEnFuBehavior();
void InitEnGamelupyBehavior();
void InitEnGb2Behavior();
void InitEnGegBehavior();
void InitEnGgBehavior();
void InitEnGinkoBehavior();
void InitEnGirlABehavior();
void InitEnGKBehavior();
void InitEnGoBehavior();
void InitEnGsBehavior();
void InitEnHgBehavior();
void InitEnInBehavior();
void InitEnInvadepohBehavior();
void InitEnItem00Behavior();
void InitEnJgameTsnBehavior();
void InitEnJgBehavior();
void InitEnJsBehavior();
void InitEnKgyBehavior();
void InitEnKitanBehavior();
void InitEnKnightBehavior();
void InitEnKujiyaBehavior();
void InitEnLiftNutsBehavior();
void InitEnMa4Behavior();
void InitEnMaYtoBehavior();
void InitEnMinifrogBehavior();
void InitEnMnkBehavior();
void InitEnNbBehavior();
void InitEnOsnBehavior();
void InitEnOtBehavior();
void InitEnOwlBehavior();
void InitEnPmBehavior();
void InitEnRuppecrowBehavior();
void InitEnRzBehavior();
void InitEnScopenutsBehavior();
void InitEnSellnutsBehavior();
void InitEnShnBehavior();
void InitEnSiBehavior();
void InitEnSob1Behavior();
void InitEnSshBehavior();
void InitEnStoneheishiBehavior();
void InitEnSyatekiManBehavior();
void InitEnTabBehavior();
void InitEnTakarayaBehavior();
void InitEnTalkBehavior();
void InitEnTimeTagBehavior();
void InitEnTotoBehavior();
void InitEnTrtBehavior();
void InitEnYbBehavior();
void InitEnZogBehavior();
void InitEnZotBehavior();
void InitEnZowBehavior();
void InitItemBHeartBehavior();
void InitItemGetBehavior();
void InitObjCombBehavior();
void InitObjKibakoBehavior();
void InitObjGrassBehavior();
void InitObjMoonStoneBehavior();
void InitObjSnowballBehavior();
void InitObjTaruBehavior();
void InitObjTreeBehavior();
void InitObjTsuboBehavior();
void InitObjWarpstoneBehavior();
void InitPlayerBehavior();
void InitSoulsBehavior();
void InitTrapsBehavior();
void InitWonderItemsBehavior();

} // namespace ActorBehavior

} // namespace Rando

#endif
