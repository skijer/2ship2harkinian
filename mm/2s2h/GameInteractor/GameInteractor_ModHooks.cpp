#include "GameInteractor.h"
#include "GameInteractor_ModHooks.h"
#include "2s2h/ModApi/CustomItemRegistry/CustomItemRegistry.h"

#include <array>
#include <atomic>

void GameInteractor_ExecuteOnPlayerUpdate(Player* player) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerUpdate>(player);
}

void GameInteractor_ExecuteOnInterfaceDrawEnd(PlayState* play) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnInterfaceDrawEnd>(play);
}

void GameInteractor_ExecuteOnActorResolveMotionScale(Actor* actor, f32* scale) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnActorResolveMotionScale>(actor, scale);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::OnActorResolveMotionScale>(actor->id, actor, scale);
}

void GameInteractor_ExecuteOnCollisionRegisterAC(Collider* collider) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnCollisionRegisterAC>(collider);
}

void GameInteractor_ExecuteOnOcarinaInput(void) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnOcarinaInput>();
}

void GameInteractor_ExecuteOnPlayerFilterInput(Player* player, Input* input) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerFilterInput>(player, input);
}

void GameInteractor_ExecuteOnPlayerResolveItemAction(s32 item, s8* itemAction) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerResolveItemAction>(item, itemAction);
}

void GameInteractor_ExecuteOnPlayerResolveItemActionInit(s8 itemAction, PlayerItemActionInitFunc* init) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerResolveItemActionInit>(itemAction, init);
}

void GameInteractor_ExecuteOnPlayerResolveItemActionUpdate(s8 itemAction, PlayerUpperActionFunc* update) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerResolveItemActionUpdate>(itemAction, update);
}

void GameInteractor_ExecuteOnPlayerResolveModelGroup(Player* player, s32 itemAction, s32* modelGroup) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerResolveModelGroup>(player, itemAction, modelGroup);
}

bool GameInteractor_ExecuteOnPlayerActionHandler(PlayState* play, Player* player, s32 action, bool* startedAction) {
    bool consumed = false;

    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerActionHandler>(play, player, action, &consumed,
                                                                                  startedAction);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::OnPlayerActionHandler>(action, play, player, action,
                                                                                       &consumed, startedAction);
    return consumed;
}

void GameInteractor_ExecuteOnPlayerResolveAnim(s32 group, s32 animType, PlayerAnimationHeader** anim) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerResolveAnim>(group, animType, anim);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::OnPlayerResolveAnim>(group, group, animType, anim);
}

void GameInteractor_ExecuteOnPlayerResolveAgeProperties(Player* player, PlayerAgeProperties** properties) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerResolveAgeProperties>(player, properties);
}

void GameInteractor_ExecuteOnPlayerResolveMotionScale(Player* player, s32 kind, f32* scale) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerResolveMotionScale>(player, kind, scale);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::OnPlayerResolveMotionScale>(kind, player, kind, scale);
}

void GameInteractor_ExecuteOnPlayerResolveHeight(Player* player, f32* height) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerResolveHeight>(player, height);
}

void GameInteractor_ExecuteOnPlayerResolveFaceTextures(const char** eyes, const char** mouth) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerResolveFaceTextures>(eyes, mouth);
}

void GameInteractor_ExecuteOnPlayerResolveLimbDraw(Player* player, s32 limbIndex, Gfx** dList, Gfx* limbDList,
                                                   Vec3f* pos) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerResolveLimbDraw>(player, limbIndex, dList, limbDList,
                                                                                    pos);
}

void GameInteractor_ExecuteOnPlayerHealthChange(s16 amount) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerHealthChange>(amount);
}

void GameInteractor_ExecuteOnPlayerShieldBlocked(PlayState* play, Player* player, Actor* attacker) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerShieldBlocked>(play, player, attacker);
}

void GameInteractor_ExecuteOnCollisionResolveDamage(Actor* victim, ColliderElement* attack, f32* damage) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnCollisionResolveDamage>(victim, attack, damage);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::OnCollisionResolveDamage>(victim->id, victim, attack,
                                                                                          damage);
}

void GameInteractor_ExecuteOnResolveEnvHazard(PlayState* play, s16* hazard) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnResolveEnvHazard>(play, hazard);
}

void GameInteractor_ExecuteOnMagicResolveCost(s16* amount) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnMagicResolveCost>(amount);
}

void GameInteractor_ExecuteOnPlayDrawBegin(void) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayDrawBegin>();
}

void GameInteractor_ExecuteOnPlayDrawEnd(void) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayDrawEnd>();
}

void GameInteractor_ExecuteOnActorDrawBegin(Actor* actor, PlayState* play) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnActorDrawBegin>(actor, play);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::OnActorDrawBegin>(actor->id, actor, play);
    GameInteractor::Instance->ExecuteHooksForPtr<GameInteractor::OnActorDrawBegin>((uintptr_t)actor, actor, play);
}

void GameInteractor_ExecuteOnActorDrawEnd(Actor* actor, PlayState* play) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnActorDrawEnd>(actor, play);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::OnActorDrawEnd>(actor->id, actor, play);
}

bool GameInteractor_ExecuteOnActorPlaySfx(Actor* actor, s32 kind, u16* sfxId) {
    bool handled = false;

    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnActorPlaySfx>(actor, kind, sfxId, &handled);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::OnActorPlaySfx>(actor->id, actor, kind, sfxId,
                                                                                &handled);
    return handled;
}

void GameInteractor_ExecuteOnActorResolveGrayscale(Actor* actor, PlayState* play, Color_RGBA8* grayscale) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnActorResolveGrayscale>(actor, play, grayscale);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::OnActorResolveGrayscale>(actor->id, actor, play,
                                                                                         grayscale);
}

void GameInteractor_ExecuteOnRoomResolveGrayscale(PlayState* play, Room* room, Color_RGBA8* grayscale) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnRoomResolveGrayscale>(play, room, grayscale);
}

void GameInteractor_ExecuteOnActorResolveBgCheckFlags(Actor* actor, s32* flags) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnActorResolveBgCheckFlags>(actor, flags);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::OnActorResolveBgCheckFlags>(actor->id, actor, flags);
}

void GameInteractor_ExecuteOnActorResolvePlayerRelation(Actor* actor, f32* xzDist, f32* yDist, s16* yawTowards) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnActorResolvePlayerRelation>(actor, xzDist, yDist,
                                                                                         yawTowards);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::OnActorResolvePlayerRelation>(actor->id, actor, xzDist,
                                                                                              yDist, yawTowards);
}

void GameInteractor_ExecuteOnBgCheckRaycastFloor(CollisionContext* colCtx, Vec3f* pos, Actor* actor,
                                                 CollisionPoly** poly, s32* bgId, f32* floorY) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnBgCheckRaycastFloor>(colCtx, pos, actor, poly, bgId,
                                                                                  floorY);
}

void GameInteractor_ExecuteOnBgCheckResolveWallFlags(CollisionContext* colCtx, CollisionPoly* poly, s32 bgId,
                                                     u32* flags) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnBgCheckResolveWallFlags>(colCtx, poly, bgId, flags);
}

void GameInteractor_ExecuteOnCameraResolveView(Camera* camera, Vec3f* eye, Vec3f* at, Vec3f* up) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnCameraResolveView>(camera, eye, at, up);
}

void GameInteractor_ExecuteOnSceneSpawnActors(void) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnSceneSpawnActors>();
}

void GameInteractor_ExecuteOnLoadFile(s32 fileNum) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnLoadFile>(fileNum);
}

void GameInteractor_ExecuteOnLoadGame(s32 fileNum) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnLoadGame>(fileNum);
}

void GameInteractor_ExecuteOnGameFrameUpdate(void) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnGameFrameUpdate>();
}

bool GameInteractor_ExecuteOnKaleidoInput(PlayState* play, Input* input) {
    bool handled = false;
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnKaleidoInput>(play, input, &handled);
    return handled;
}

void GameInteractor_ExecuteOnKaleidoItemCursor(PlayState* play, u16* item, u16* slot) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnKaleidoItemCursor>(play, item, slot);
}

void GameInteractor_ExecuteOnKaleidoItemDraw(PlayState* play, s32 slot, s32 item, Vtx* vertices) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnKaleidoItemDraw>(play, slot, item, vertices);
}

bool GameInteractor_ExecuteOnKaleidoItemEquip(PlayState* play, u8 button, u16 item) {
    bool handled = false;
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnKaleidoItemEquip>(play, button, item, &handled);
    return handled;
}

const char* GameInteractor_ExecuteOnKaleidoResolveItemIcon(PlayState* play, u16 item, const char* iconPath) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnKaleidoResolveItemIcon>(play, item, &iconPath);
    return iconPath;
}

const char* GameInteractor_ExecuteOnKaleidoResolveName(PlayState* play, u16 item, const char* namePath) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnKaleidoResolveName>(play, item, &namePath);
    return namePath;
}

bool GameInteractor_ExecuteOnKaleidoDebugEditor(PlayState* play) {
    bool handled = false;
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnKaleidoDebugEditor>(play, &handled);
    return handled;
}

const char* GameInteractor_ExecuteOnInterfaceResolveButtonIcon(PlayState* play, u8 button, u16 item,
                                                               const char* iconPath) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnInterfaceResolveButtonIcon>(play, button, item,
                                                                                         &iconPath);
    return iconPath;
}

const char* GameInteractor_ExecuteOnMessageResolveItemIcon(u16 itemId, const char* iconPath) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnMessageResolveItemIcon>(itemId, &iconPath);
    return iconPath;
}

const char* GameInteractor_ExecuteOnResolveCustomGetItem(Actor* actor, PlayState* play, const char* key) {
    const S2HGetItemEntry* registered = CustomItemRegistry_FindGetItemEntry(key);
    S2HGetItemEntry entry = registered != nullptr ? *registered : S2HGetItemEntry{};
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnResolveCustomGetItem>(actor, play, &entry, &key);
    return key;
}

bool GameInteractor_ExecuteOnResolveItemGive(PlayState* play, u8* item) {
    bool handled = false;
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnResolveItemGive>(play, item, &handled);
    return handled;
}

bool GameInteractor_ExecuteOnGetItemDraw(PlayState* play, s16* drawId) {
    bool handled = false;
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnGetItemDraw>(play, drawId, &handled);
    return handled;
}

void GameInteractor_ExecuteOnGetItemDrawPost(PlayState* play, s16 drawId) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnGetItemDrawPost>(play, drawId);
}

PlayerAnimationHeader* GameInteractor_ExecuteOnPlayerResolveAnimSite(s32 site, s32 index, PlayerAnimationHeader* anim) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerResolveAnimSite>(site, index, &anim);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::OnPlayerResolveAnimSite>(site, site, index, &anim);
    return anim;
}

void GameInteractor_ExecuteOnResolveSwordDamage(PlayState* play, s32 dmgFlags, u8* damage) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnResolveSwordDamage>(play, dmgFlags, damage);
}

bool GameInteractor_ExecuteOnBgCheckLineTest(CollisionContext* colCtx, Vec3f* posA, Vec3f* posB, Vec3f* hitPos,
                                             CollisionPoly** poly, s32* bgId, Actor* actor, u32 flags, bool result) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnBgCheckLineTest>(colCtx, posA, posB, hitPos, poly, bgId,
                                                                              actor, flags, &result);
    return result;
}

bool GameInteractor_ExecuteOnActorTalk(Actor* actor, PlayState* play) {
    bool continueVanilla = true;
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnActorTalk>(actor, play, &continueVanilla);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::OnActorTalk>(actor->id, actor, play,
                                                                             &continueVanilla);
    GameInteractor::Instance->ExecuteHooksForPtr<GameInteractor::OnActorTalk>((uintptr_t)actor, actor, play,
                                                                              &continueVanilla);
    return continueVanilla;
}

void GameInteractor_ExecuteOnAudioTablesReady(void) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnAudioTablesReady>();
}

namespace {

struct QueuedOcarinaNote {
    bool isPlayback;
    u8 note;
    f32 modulator;
    s8 bend;
};

// Single producer (audio thread), single consumer (game thread).
std::array<QueuedOcarinaNote, 128> sQueuedOcarinaNotes;
std::atomic<uint32_t> sQueuedOcarinaNoteWrite = 0;
std::atomic<uint32_t> sQueuedOcarinaNoteRead = 0;

void QueueOcarinaNote(const QueuedOcarinaNote& note) {
    const uint32_t write = sQueuedOcarinaNoteWrite.load(std::memory_order_relaxed);
    if (write - sQueuedOcarinaNoteRead.load(std::memory_order_acquire) >= sQueuedOcarinaNotes.size()) {
        return;
    }
    sQueuedOcarinaNotes[write % sQueuedOcarinaNotes.size()] = note;
    sQueuedOcarinaNoteWrite.store(write + 1, std::memory_order_release);
}

} // namespace

void GameInteractor_QueueOcarinaNote(u8 note, f32 modulator, s8 bend) {
    QueueOcarinaNote({ false, note, modulator, bend });
}

void GameInteractor_QueueOcarinaPlaybackNote(u8 note, f32 modulator) {
    QueueOcarinaNote({ true, note, modulator, 0 });
}

void GameInteractor_ExecuteQueuedOcarinaNotes(void) {
    const uint32_t write = sQueuedOcarinaNoteWrite.load(std::memory_order_acquire);
    uint32_t read = sQueuedOcarinaNoteRead.load(std::memory_order_relaxed);
    for (; read != write; ++read) {
        const QueuedOcarinaNote queued = sQueuedOcarinaNotes[read % sQueuedOcarinaNotes.size()];
        if (queued.isPlayback) {
            GameInteractor::Instance->ExecuteHooks<GameInteractor::OnOcarinaPlaybackNote>(queued.note,
                                                                                          queued.modulator);
        } else {
            GameInteractor::Instance->ExecuteHooks<GameInteractor::OnOcarinaNote>(queued.note, queued.modulator,
                                                                                  queued.bend);
        }
    }
    sQueuedOcarinaNoteRead.store(read, std::memory_order_release);
}
