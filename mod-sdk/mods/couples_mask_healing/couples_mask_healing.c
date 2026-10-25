#include "global.h"
#include "2s2h/ModApi/ModApi.h"

static const S2HModApi* sApi;
static s32 sTimer;
static const char* const sHooks[] = { "OnPlayerUpdate" };
static const S2HModRequirements sRequirements = { sizeof(S2HModRequirements), sHooks, ARRAY_COUNT(sHooks) };

static void Update(Player* player) {
    if (gPlayState == NULL || player != GET_PLAYER(gPlayState) || player->currentMask != PLAYER_MASK_COUPLE ||
        gSaveContext.save.saveInfo.playerData.health <= 0) {
        sTimer = 0;
        return;
    }
    if (--sTimer > 0) return;
    if (gSaveContext.save.isNight == 0) {
        sTimer = 4;
        if (gSaveContext.save.saveInfo.playerData.health < gSaveContext.save.saveInfo.playerData.healthCapacity)
            ++gSaveContext.save.saveInfo.playerData.health;
    } else {
        sTimer = 7;
        if (gSaveContext.save.saveInfo.playerData.magic < gSaveContext.magicCapacity)
            ++gSaveContext.save.saveInfo.playerData.magic;
    }
}

S2H_MOD_EXPORT void ModSetApi(const S2HModApi* api) { sApi = api; }
S2H_MOD_EXPORT const S2HModRequirements* ModGetRequirements(void) { return &sRequirements; }
S2H_MOD_EXPORT void ModInit(void) { S2H_REGISTER_HOOK(sApi, OnPlayerUpdate, Update); }
