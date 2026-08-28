#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

#include "ModApi.h"

#define START_SPEED 0.9f
#define SPEED_PER_HIT 0.01f

static const S2HModApi* sApi = NULL;
static s16 sLastHealth = -1;
static uint32_t sHits = 0;

static void (*S2H_Notify)(const char* message);
static SaveContext* sSaveContext;

static f32 CurrentSpeed(void) {
    return START_SPEED + (SPEED_PER_HIT * sHits);
}

// These VBs hand over the speed the player is about to move at, which is the only point where
// scaling sticks: anything written outside is recomputed from input next frame. Walk passes just
// that target, while swim passes it fourth, after incrStep, maxSpeed and speed.
static void ScaleWalkSpeed(bool* should, va_list args) {
    f32* speedTarget = va_arg(args, f32*);

    *speedTarget *= CurrentSpeed();
}

static void ScaleSwimSpeed(bool* should, va_list args) {
    va_arg(args, f32*);
    va_arg(args, f32*);
    va_arg(args, f32*);
    f32* speedTarget = va_arg(args, f32*);

    *speedTarget *= CurrentSpeed();
}

static void OnTick(void) {
    s16 health = sSaveContext->save.saveInfo.playerData.health;

    if (sLastHealth < 0) {
        sLastHealth = health;
        return;
    }

    if (health < sLastHealth) {
        sHits++;

        char message[64];
        snprintf(message, sizeof(message), "hit %u - speed %d%%", sHits, (int)(CurrentSpeed() * 100.0f));
        S2H_Notify(message);
    }

    sLastHealth = health;
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

    S2H_Notify("Faster on Damage: starts at 90%, every hit adds 1%");
    S2H_REGISTER_HOOK(sApi, OnGameStateUpdate, OnTick);
    sApi->RegisterVB(VB_SPEED_MODIFIER_WALK, ScaleWalkSpeed);
    sApi->RegisterVB(VB_SPEED_MODIFIER_SWIM, ScaleSwimSpeed);
}
