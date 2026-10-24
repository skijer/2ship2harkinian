#ifndef GAME_INTERACTOR_MOD_HOOKS_H
#define GAME_INTERACTOR_MOD_HOOKS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "z64.h"
#include "2s2h/ModApi/Player/PlayerHookTypes.h"

void GameInteractor_ExecuteOnPlayerUpdate(Player* player);
void GameInteractor_ExecuteOnInterfaceDrawEnd(PlayState* play);
void GameInteractor_ExecuteOnActorResolveMotionScale(Actor* actor, f32* scale);
void GameInteractor_ExecuteOnCollisionRegisterAC(Collider* collider);
void GameInteractor_ExecuteOnOcarinaInput(void);

void GameInteractor_ExecuteOnPlayerFilterInput(Player* player, Input* input);
void GameInteractor_ExecuteOnPlayerResolveItemAction(s32 item, s8* itemAction);
void GameInteractor_ExecuteOnPlayerResolveItemActionInit(s8 itemAction, PlayerItemActionInitFunc* init);
void GameInteractor_ExecuteOnPlayerResolveItemActionUpdate(s8 itemAction, PlayerUpperActionFunc* update);
void GameInteractor_ExecuteOnPlayerResolveModelGroup(Player* player, s32 itemAction, s32* modelGroup);
bool GameInteractor_ExecuteOnPlayerActionHandler(PlayState* play, Player* player, s32 action, bool* startedAction);
void GameInteractor_ExecuteOnPlayerResolveAnim(s32 group, s32 animType, PlayerAnimationHeader** anim);
void GameInteractor_ExecuteOnPlayerResolveAgeProperties(Player* player, PlayerAgeProperties** properties);
void GameInteractor_ExecuteOnPlayerResolveMotionScale(Player* player, s32 kind, f32* scale);
void GameInteractor_ExecuteOnPlayerResolveHeight(Player* player, f32* height);
void GameInteractor_ExecuteOnPlayerResolveFaceTextures(const char** eyes, const char** mouth);
void GameInteractor_ExecuteOnPlayerResolveLimbDraw(Player* player, s32 limbIndex, Gfx** dList, Gfx* limbDList,
                                                   Vec3f* pos);
void GameInteractor_ExecuteOnPlayerHealthChange(s16 amount);
void GameInteractor_ExecuteOnPlayerShieldBlocked(PlayState* play, Player* player, Actor* attacker);
void GameInteractor_ExecuteOnCollisionResolveDamage(Actor* victim, ColliderElement* attack, f32* damage);
void GameInteractor_ExecuteOnResolveEnvHazard(PlayState* play, s16* hazard);
void GameInteractor_ExecuteOnMagicResolveCost(s16* amount);
void GameInteractor_ExecuteOnPlayDrawBegin(void);
void GameInteractor_ExecuteOnPlayDrawEnd(void);
void GameInteractor_ExecuteOnActorDrawBegin(Actor* actor, PlayState* play);
void GameInteractor_ExecuteOnActorDrawEnd(Actor* actor, PlayState* play);
bool GameInteractor_ExecuteOnActorPlaySfx(Actor* actor, s32 kind, u16* sfxId);
void GameInteractor_ExecuteOnActorResolveGrayscale(Actor* actor, PlayState* play, Color_RGBA8* grayscale);
void GameInteractor_ExecuteOnRoomResolveGrayscale(PlayState* play, Room* room, Color_RGBA8* grayscale);
void GameInteractor_ExecuteOnActorResolveBgCheckFlags(Actor* actor, s32* flags);
void GameInteractor_ExecuteOnActorResolvePlayerRelation(Actor* actor, f32* xzDist, f32* yDist, s16* yawTowards);
void GameInteractor_ExecuteOnBgCheckRaycastFloor(CollisionContext* colCtx, Vec3f* pos, Actor* actor,
                                                 CollisionPoly** poly, s32* bgId, f32* floorY);
void GameInteractor_ExecuteOnBgCheckResolveWallFlags(CollisionContext* colCtx, CollisionPoly* poly, s32 bgId,
                                                     u32* flags);
void GameInteractor_ExecuteOnCameraResolveView(Camera* camera, Vec3f* eye, Vec3f* at, Vec3f* up);
void GameInteractor_ExecuteOnSceneSpawnActors(void);
void GameInteractor_ExecuteOnLoadFile(s32 fileNum);
void GameInteractor_ExecuteOnLoadGame(s32 fileNum);
void GameInteractor_ExecuteOnGameFrameUpdate(void);

bool GameInteractor_ExecuteOnKaleidoInput(PlayState* play, Input* input);
void GameInteractor_ExecuteOnKaleidoItemCursor(PlayState* play, u16* item, u16* slot);
void GameInteractor_ExecuteOnKaleidoItemDraw(PlayState* play, s32 slot, s32 item, Vtx* vertices);
bool GameInteractor_ExecuteOnKaleidoItemEquip(PlayState* play, u8 button, u16 item);
const char* GameInteractor_ExecuteOnKaleidoResolveItemIcon(PlayState* play, u16 item, const char* iconPath);
const char* GameInteractor_ExecuteOnKaleidoResolveName(PlayState* play, u16 item, const char* namePath);
bool GameInteractor_ExecuteOnKaleidoDebugEditor(PlayState* play);
const char* GameInteractor_ExecuteOnInterfaceResolveButtonIcon(PlayState* play, u8 button, u16 item,
                                                               const char* iconPath);
const char* GameInteractor_ExecuteOnMessageResolveItemIcon(u16 itemId, const char* iconPath);
const char* GameInteractor_ExecuteOnResolveCustomGetItem(Actor* actor, PlayState* play, const char* key);
bool GameInteractor_ExecuteOnResolveItemGive(PlayState* play, u8* item);
bool GameInteractor_ExecuteOnGetItemDraw(PlayState* play, s16* drawId);
void GameInteractor_ExecuteOnGetItemDrawPost(PlayState* play, s16 drawId);
PlayerAnimationHeader* GameInteractor_ExecuteOnPlayerResolveAnimSite(s32 site, s32 index, PlayerAnimationHeader* anim);
void GameInteractor_ExecuteOnResolveSwordDamage(PlayState* play, s32 dmgFlags, u8* damage);
bool GameInteractor_ExecuteOnBgCheckLineTest(CollisionContext* colCtx, Vec3f* posA, Vec3f* posB, Vec3f* hitPos,
                                             CollisionPoly** poly, s32* bgId, Actor* actor, u32 flags, bool result);
bool GameInteractor_ExecuteOnActorTalk(Actor* actor, PlayState* play);
void GameInteractor_ExecuteOnAudioTablesReady(void);

// Called from the audio thread; the hooks run on the game thread at the start of the next frame.
void GameInteractor_QueueOcarinaNote(u8 note, f32 modulator, s8 bend);
void GameInteractor_QueueOcarinaPlaybackNote(u8 note, f32 modulator);
void GameInteractor_ExecuteQueuedOcarinaNotes(void);
#ifdef __cplusplus
}
#endif

#endif // GAME_INTERACTOR_MOD_HOOKS_H
