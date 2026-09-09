/**
 * The smallest useful 2ship mod: say hello when the player picks up a recovery heart.
 *
 * A mod exports two functions. The host calls ModSetApi first with the table of everything it can
 * do, then ModInit to let it bind the symbols it needs and register its hooks.
 */

#include <stddef.h>

#include "ModApi.h"

static const S2HModApi* sApi = NULL;

static void (*S2H_Notify)(const char* message);
static void (*S2H_Log)(const char* message);

// Runs on every Item_Give, which is what a heart pickup goes through.
static void OnItemGive(u8 item) {
    if (item != ITEM_RECOVERY_HEART) {
        return;
    }

    S2H_Notify("Hello, world!");
    S2H_Log("hello_mod: hello, world!");
}

S2H_MOD_EXPORT void ModSetApi(const S2HModApi* api) {
    sApi = api;
}

S2H_MOD_EXPORT void ModInit(void) {
    if (!S2H_MOD_API_MATCHES(sApi)) {
        return;
    }

    if (S2H_BIND(sApi, S2H_Notify) == NULL || S2H_BIND(sApi, S2H_Log) == NULL) {
        return;
    }

    S2H_REGISTER_HOOK(sApi, OnItemGive, OnItemGive);
}
