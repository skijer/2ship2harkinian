#ifndef RANDO_MISC_BEHAVIOR_H
#define RANDO_MISC_BEHAVIOR_H

#include "Rando/Rando.h"

namespace Rando {

namespace MiscBehavior {

void Init();
void OnFileLoad();

void CheckQueue();
void CheckQueueReset();
void InitFileSelect();
void InitKaleidoItemPage();
void InitOfferGetItemBehavior();
void BeforeEndOfCycleSave();
void AfterEndOfCycleSave();
void OnFileCreate(s16 fileNum);
// Turns the new save into a rando save that starts as Human in South Clock Town.
void PrepareRandoSave();
// Records finalSeed with the options and starting items configured in the menu.
void PersistSeedOptions(uint32_t finalSeed);
void OnFlagSet(FlagType flagType, u32 flag);
void OnSceneFlagSet(s16 sceneId, FlagType flagType, u32 flag);
void OnSceneInit(s16 sceneId, s8 spawnNum);
void OfferTrapItem();
void SariasSongHint();
void BankSignHint();
void InitTycoonWallet();

typedef struct {
    char tex[512];
    uint16_t width;
    uint16_t height;
    uint8_t im_fmt;
    uint8_t im_siz;
    uint8_t id;
} Sprite;
Sprite* GetSeedTexture(const uint8_t index);

} // namespace MiscBehavior

} // namespace Rando

#endif
