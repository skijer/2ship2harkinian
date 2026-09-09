#include <stddef.h>

#include "ModApi.h"

// timeSpeedOffset is added to R_TIME_SPEED, which is 3 in normal play: 0 is default speed, -2 is
// what the Inverted Song of Time sets, and -3 freezes the clock. Anything below that makes the
// sum negative and the clock walks backwards.
#define REVERSE_TIME_SPEED_OFFSET -4

static const S2HModApi* sApi = NULL;

static void (*S2H_Notify)(const char* message);
static SaveContext* sSaveContext;

// The game restores this on save load and on some cutscenes, so it is reasserted every tick.
static void OnTick(void) {
    if (sSaveContext->save.timeSpeedOffset != REVERSE_TIME_SPEED_OFFSET) {
        sSaveContext->save.timeSpeedOffset = REVERSE_TIME_SPEED_OFFSET;
    }
}

S2H_MOD_EXPORT void ModSetApi(const S2HModApi* api) {
    sApi = api;
}

S2H_MOD_EXPORT void ModInit(void) {
    if (!S2H_MOD_API_MATCHES(sApi)) {
        return;
    }

    sSaveContext = sApi->GetSymbol("gSaveContext");
    if (S2H_BIND(sApi, S2H_Notify) == NULL || sSaveContext == NULL) {
        return;
    }

    S2H_Notify("Reverse Time: the clock runs backwards");
    S2H_REGISTER_HOOK(sApi, OnGameStateUpdate, OnTick);
}
