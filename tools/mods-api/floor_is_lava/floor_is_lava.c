#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>

#include "ModApi.h"

#define BURN_INTERVAL 20
#define BURN_DAMAGE 4

static const S2HModApi* sApi = NULL;
static uint32_t sContactTicks = 0;

static void (*S2H_Notify)(const char* message);
static PlayState** sPlayState;
static SaveContext* sSaveContext;

static Player* GetPlayer(PlayState* play) {
    return (Player*)play->actorCtx.actorLists[ACTORCAT_PLAYER].first;
}

static bool IsGameplayRunning(PlayState* play, Player* player) {
    return play->pauseCtx.state == PAUSE_STATE_OFF && play->msgCtx.msgMode == MSGMODE_NONE &&
           play->gameOverCtx.state == GAMEOVER_INACTIVE && play->transitionTrigger == TRANS_TRIGGER_OFF &&
           !(player->stateFlags1 & PLAYER_STATE1_DEAD);
}

static bool IsTouchingTheFloor(Player* player) {
    if (player->actor.bgCheckFlags & BGCHECKFLAG_WATER) {
        return false;
    }

    return (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) != 0;
}

// The fields the rando's fire trap sets, so this is the game's own burning rather than an imitation.
static void SetPlayerOnFire(Player* player) {
    for (int i = 0; i < PLAYER_BODYPART_MAX; i++) {
        player->bodyFlameTimers[i] = (u8)(rand() % 200);
    }

    player->bodyIsBurning = true;
}

static void BurnPlayer(Player* player) {
    SetPlayerOnFire(player);

    sSaveContext->save.saveInfo.playerData.health -= BURN_DAMAGE;
    if (sSaveContext->save.saveInfo.playerData.health < 0) {
        sSaveContext->save.saveInfo.playerData.health = 0;
    }
}

static void OnTick(void) {
    PlayState* play = *sPlayState;
    if (play == NULL) {
        return;
    }

    Player* player = GetPlayer(play);
    if (player == NULL || !IsGameplayRunning(play, player)) {
        return;
    }

    if (player->currentMask == PLAYER_MASK_CIRCUS_LEADER || !IsTouchingTheFloor(player)) {
        sContactTicks = 0;
        return;
    }

    sContactTicks++;
    if (sContactTicks % BURN_INTERVAL != 0) {
        return;
    }

    if (sSaveContext->save.saveInfo.playerData.health > 0) {
        BurnPlayer(player);
    }
}

// VB_GIVE_ITEM_FROM_ITEM00 passes the collectible actor; see its call site in z_en_item00.c.
static void ShouldGiveItemFromItem00(bool* should, va_list args) {
    Actor* item00 = va_arg(args, Actor*);

    if (item00->params == ITEM00_RECOVERY_HEART) {
        *should = false;
    }
}

S2H_MOD_EXPORT void ModSetApi(const S2HModApi* api) {
    sApi = api;
}

S2H_MOD_EXPORT void ModInit(void) {
    if (!S2H_MOD_API_MATCHES(sApi)) {
        return;
    }

    sPlayState = sApi->GetSymbol("gPlayState");
    sSaveContext = sApi->GetSymbol("gSaveContext");
    if (S2H_BIND(sApi, S2H_Notify) == NULL || sPlayState == NULL || sSaveContext == NULL) {
        return;
    }

    S2H_Notify("Floor is Lava: wear the Circus Leader's Mask to survive");
    S2H_REGISTER_HOOK(sApi, OnGameStateUpdate, OnTick);
    sApi->RegisterVB(VB_GIVE_ITEM_FROM_ITEM00, ShouldGiveItemFromItem00);
}
