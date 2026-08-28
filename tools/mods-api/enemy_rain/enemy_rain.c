#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "ModApi.h"

#define TICKS_PER_SECOND 20
#define SPAWN_INTERVAL (60 * TICKS_PER_SECOND)

static const S2HModApi* sApi = NULL;
static uint32_t sTicks = 0;

static void (*S2H_Notify)(const char* message);
static int32_t (*S2H_RunCommand)(const char* command);
static PlayState** sPlayState;

static const s16 sEnemies[] = {
    ACTOR_EN_FIREFLY, ACTOR_EN_TITE,    ACTOR_EN_CROW,    ACTOR_EN_GRASSHOPPER,
    ACTOR_EN_OKUTA,   ACTOR_EN_DODONGO, ACTOR_EN_WALLMAS, ACTOR_EN_ST,
};

#define ENEMY_COUNT (sizeof(sEnemies) / sizeof(sEnemies[0]))

static bool IsGameplayRunning(PlayState* play) {
    return play->pauseCtx.state == PAUSE_STATE_OFF && play->msgCtx.msgMode == MSGMODE_NONE &&
           play->gameOverCtx.state == GAMEOVER_INACTIVE && play->transitionTrigger == TRANS_TRIGGER_OFF;
}

// A scene only loads the objects its own actors need, so most of the list cannot spawn here. The
// command returns non-zero for those, and walking the whole list finds whichever this scene has.
static bool SpawnRandomEnemy(void) {
    uint32_t start = rand() % ENEMY_COUNT;

    for (uint32_t i = 0; i < ENEMY_COUNT; i++) {
        char command[32];
        snprintf(command, sizeof(command), "spawn %d 0", sEnemies[(start + i) % ENEMY_COUNT]);

        if (S2H_RunCommand(command) == 0) {
            return true;
        }
    }

    return false;
}

static void OnTick(void) {
    PlayState* play = *sPlayState;
    if (play == NULL || !IsGameplayRunning(play)) {
        return;
    }

    sTicks++;
    if (sTicks % SPAWN_INTERVAL != 0) {
        return;
    }

    if (SpawnRandomEnemy()) {
        S2H_Notify("Enemy Rain: you are not alone");
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
    if (S2H_BIND(sApi, S2H_Notify) == NULL || S2H_BIND(sApi, S2H_RunCommand) == NULL || sPlayState == NULL) {
        return;
    }

    S2H_Notify("Enemy Rain: one enemy per minute");
    S2H_REGISTER_HOOK(sApi, OnGameStateUpdate, OnTick);
}
