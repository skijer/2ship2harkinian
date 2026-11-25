#ifndef SOULS_H
#define SOULS_H

#include "Rando/Rando.h"

#include <vector>

#define SOUL_RI_TO_RANDO_INF(randoItemId) ((randoItemId - RI_SOUL_BOSS_GOHT) + RANDO_INF_OBTAINED_SOUL_OF_BOSS_GOHT)

struct NativeEnemySoul {
    RandoItemId randoItemId;
    const char* key;
};

extern bool HaveEnemySoul(ActorId enemyId);
const std::vector<NativeEnemySoul>& GetNativeEnemySouls();
const char* GetNativeEnemySoulKey(RandoItemId randoItemId);
int32_t GetCustomEnemySoulItem(const char* soulKey);

#endif // SOULS_H
