#ifndef VARIABLES_H
#define VARIABLES_H

#include "z64.h"
#include "segment_symbols.h"
#include "macros.h"
#include "attributes.h"

#ifdef __cplusplus
extern "C"
{
#endif

// data
extern HOST_DATA size_t gDmaMgrDmaBuffSize;
extern HOST_DATA vs32 gIrqMgrResetStatus;
extern HOST_DATA volatile OSTime sIrqMgrResetTime;
extern HOST_DATA volatile OSTime gIrqMgrRetraceTime;
extern HOST_DATA s32 sIrqMgrRetraceCount;

// extern UNK_TYPE1 sGfxPrintFontTLUT;
// extern UNK_TYPE1 sGfxPrintRainbowTLUT;
// extern UNK_TYPE1 sGfxPrintRainbowData;
// extern UNK_TYPE1 sGfxPrintFontData;
// extern UNK_TYPE4 D_80097524;
// extern u32 sRandInt;

extern HOST_DATA u8 sYaz0DataBuffer[0x400];
extern HOST_DATA u8* sYaz0CurDataEnd;
extern HOST_DATA uintptr_t sYaz0CurRomStart;
extern HOST_DATA u32 sYaz0CurSize;
extern HOST_DATA u8* sYaz0MaxPtr;
extern HOST_DATA void* gYaz0DecompressDstEnd;

// extern UNK_TYPE4 D_8009CD10;
// extern UNK_TYPE4 sArenaLockMsg;

extern HOST_DATA DmaEntry dmadata[1568];

extern HOST_DATA u8 sDropTable[DROP_TABLE_SIZE * DROP_TABLE_NUMBER];
extern HOST_DATA u8 sDropTableAmounts[DROP_TABLE_SIZE * DROP_TABLE_NUMBER];
extern HOST_DATA s32 D_801AE194[32];
extern HOST_DATA u8 D_801AE214[32];

// extern s32 sEntryIndex;
// extern u32 sCurrentBit;
// extern s32 sTimer;

extern HOST_DATA ActorOverlay gActorOverlayTable[ACTOR_ID_MAX];
extern HOST_DATA ActorId gMaxActorId;
extern HOST_DATA BgCheckSceneSubdivisionEntry sSceneSubdivisionList[];
extern HOST_DATA BgSpecialSceneMaxObjects sCustomDynapolyMem[];

// extern UNK_TYPE4 D_801BDAC0;
// extern UNK_TYPE4 D_801BDAC4;
// extern UNK_TYPE4 D_801BDAC8;
// extern UNK_TYPE4 D_801BDACC;
// extern UNK_TYPE4 D_801BDAF0;
// extern UNK_TYPE4 D_801BDAF8;

extern HOST_DATA u8 kanfontOrdering[92];
// extern actor_init_var_func sInitChainHandlers[11];

extern HOST_DATA UNK_PTR D_801BF5C0;
// extern UNK_TYPE4 D_801BEAD4;
// extern UNK_TYPE4 D_801BEAD8;
// extern UNK_TYPE1 D_801BEAE0;
// extern UNK_TYPE4 D_801BEAF4;
// extern UNK_TYPE4 D_801BEAF8;
// extern UNK_TYPE4 D_801BEB04;
// extern UNK_TYPE4 D_801BEB08;
// extern UNK_TYPE4 D_801BEB14;
// extern UNK_TYPE4 D_801BEB18;
// extern UNK_TYPE4 D_801BEB24;
// extern UNK_TYPE4 D_801BEB28;
// extern UNK_TYPE1 D_801BEB38;
// extern UNK_TYPE4 D_801BEBB8;
// extern UNK_TYPE4 D_801BEBD8;
// extern UNK_TYPE2 D_801BEBF8;
// extern UNK_TYPE2 D_801BEBFA;
// extern UNK_TYPE2 D_801BEBFC;
// extern UNK_TYPE2 D_801BEC10;
extern HOST_DATA UNK_PTR D_801BEC14;
// extern UNK_TYPE4 D_801BEC1C;
// extern UNK_TYPE4 D_801BEC20;
// extern UNK_TYPE1 D_801BEC24;
// extern UNK_TYPE1 D_801BEC2C;
// extern UNK_TYPE2 D_801BEC5C;
// extern UNK_TYPE2 D_801BEC5E;
// extern UNK_TYPE4 D_801BEC70;
// extern UNK_TYPE1 D_801BEC84;
// extern UNK_TYPE1 D_801BECA4;
// extern UNK_TYPE1 D_801BECC4;
// extern UNK_TYPE1 D_801BECE4;
// extern UNK_TYPE1 D_801BED00;
// extern UNK_TYPE2 D_801BED24;
// extern UNK_TYPE1 D_801BED3C;
// extern UNK_TYPE1 D_801BED40;
// extern UNK_TYPE1 D_801BED4C;
// extern UNK_TYPE1 D_801BED54;
// extern UNK_TYPE1 D_801BED55;
// extern UNK_TYPE1 D_801BED56;
// extern UNK_TYPE1 D_801BED88;
// extern UNK_TYPE1 D_801BEFC8;
// extern UNK_TYPE1 D_801BF15C;
// extern UNK_TYPE1 D_801BF170;
// extern UNK_TYPE1 D_801BF176;
// extern UNK_TYPE1 D_801BF177;
// extern UNK_TYPE1 D_801BF178;
// extern UNK_TYPE1 D_801BF3B4;
// extern UNK_TYPE2 D_801BF550;
// extern UNK_TYPE2 D_801BF554;
// extern UNK_TYPE2 D_801BF558;
// extern UNK_TYPE4 D_801BF55C;
// extern UNK_TYPE4 D_801BF580;
// extern UNK_TYPE4 D_801BF594;
// extern UNK_TYPE4 D_801BF5A4;
// extern UNK_TYPE2 D_801BF5B0;
// extern UNK_TYPE1 D_801BF68C;

extern HOST_DATA FlexSkeletonHeader* gPlayerSkeletons[PLAYER_FORM_MAX];
extern HOST_DATA PlayerModelIndices gPlayerModelTypes[];
extern HOST_DATA struct_80124618 D_801C03A0[];
extern HOST_DATA struct_80124618 D_801C0490[];
extern HOST_DATA Gfx gCullBackDList[];
extern HOST_DATA Gfx gCullFrontDList[];

extern HOST_DATA u32 gBitFlags[32];
extern HOST_DATA u16 gEquipMasks[];
extern HOST_DATA u16 gEquipNegMasks[];
extern HOST_DATA u32 gUpgradeMasks[8];
extern HOST_DATA u32 gUpgradeNegMasks[];
extern HOST_DATA u8 gEquipShifts[];
extern HOST_DATA u8 gUpgradeShifts[8];
extern HOST_DATA u16 gUpgradeCapacities[][4];
extern HOST_DATA u32 gGsFlagsMask[];
extern HOST_DATA u32 gGsFlagsShift[];
extern HOST_DATA TexturePtr gItemIcons[131];
extern HOST_DATA u8 gItemSlots[77];
extern HOST_DATA s16 gItemPrices[];
extern HOST_DATA u16 gSceneIdsPerRegion[11][27];
extern HOST_DATA u8 gPlayerFormItemRestrictions[PLAYER_FORM_MAX][114];

extern HOST_DATA s16 gPlayerFormObjectIds[PLAYER_FORM_MAX];
extern HOST_DATA ObjectId gObjectTableSize;
extern HOST_DATA RomFile gObjectTable[OBJECT_ID_MAX];

extern HOST_DATA SceneTableEntry gSceneTable[SCENE_MAX];
extern HOST_DATA UNK_PTR D_801C5C50;
// extern UNK_TYPE1 D_801C5C9C;
extern HOST_DATA UNK_PTR D_801C5CB0;

// extern UNK_TYPE1 D_801D0D54;
// extern UNK_TYPE2 sQuakeIndex;
// extern UNK_TYPE2 sIsCameraUnderwater;
extern HOST_DATA Input* D_801D0D60;
// extern UNK_TYPE2 sPlayerCsIdToCsCamId;
// extern UNK_TYPE1 D_801D0D7A;

extern HOST_DATA u32 retryCount;
extern HOST_DATA u32 cfbIdx[3];

extern HOST_DATA s16 gLowPassFilterData[];
extern HOST_DATA s16 gHighPassFilterData[];
extern HOST_DATA s16 gBandStopFilterData[];
extern HOST_DATA s16 gBandPassFilterData[];
extern HOST_DATA s16* gWaveSamples[9];
extern HOST_DATA f32 gBendPitchOneOctaveFrequencies[];
extern HOST_DATA f32 gBendPitchTwoSemitonesFrequencies[];
extern HOST_DATA f32 gPitchFrequencies[];
extern HOST_DATA u8 gDefaultShortNoteVelocityTable[];
extern HOST_DATA u8 gDefaultShortNoteGateTimeTable[];
extern HOST_DATA EnvelopePoint gDefaultEnvelope[];
extern HOST_DATA NoteSampleState gZeroedSampleState;
extern HOST_DATA NoteSampleState gDefaultSampleState;
extern HOST_DATA u16 gHaasEffectDelaySize[];
extern HOST_DATA u16 gHaasEffectDelaySize[];
extern HOST_DATA s16 gInvalidAdpcmCodeBook[];
extern HOST_DATA f32 gHeadsetPanVolume[];
extern HOST_DATA f32 gStereoPanVolume[];
extern HOST_DATA f32 gDefaultPanVolume[];
extern HOST_DATA s32 gAudioCtxInitalized;
extern HOST_DATA u8 D_801D6200[0x400];

extern HOST_DATA u8 gAudioSpecId;
extern HOST_DATA u8 gAudioHeapResetState;
extern HOST_DATA AudioSpec gAudioSpecs[21];

// rodata
extern HOST_DATA const u16 gAudioEnvironmentalSfx[];
// extern UNK_TYPE1 D_801E0BFC;
extern HOST_DATA f32 D_801E0CEC;
extern HOST_DATA f32 D_801E0CF0;
extern HOST_DATA f32 D_801E0CF4;
extern HOST_DATA f32 D_801E0CF8;
extern HOST_DATA f32 D_801E0CFC;
extern HOST_DATA f32 D_801E0D20;
extern HOST_DATA f32 D_801E0D24;
extern HOST_DATA f32 D_801E0D28;
extern HOST_DATA f32 D_801E0D2C;
extern HOST_DATA f32 D_801E0D30;
extern HOST_DATA f32 D_801E0D34;
extern HOST_DATA f64 D_801E0D58;
extern HOST_DATA f32 D_801E0D60;
extern HOST_DATA f32 D_801E0D64;
extern HOST_DATA f32 D_801E0D68;
extern HOST_DATA f32 D_801E0D8C;
extern HOST_DATA f32 D_801E0D90;
extern HOST_DATA f32 D_801E0D94;
extern HOST_DATA f32 D_801E0D98;
extern HOST_DATA f32 D_801E0D9C;
extern HOST_DATA f32 D_801E0DBC;
extern HOST_DATA f32 D_801E0DC0;
extern HOST_DATA f32 D_801E0DC4;
extern HOST_DATA f32 D_801E0DC8;
extern HOST_DATA f32 D_801E0DCC;
extern HOST_DATA f32 D_801E0DD0;
extern HOST_DATA f32 D_801E0DD4;
extern HOST_DATA f64 D_801E0DD8;
extern HOST_DATA f64 D_801E0DE0;
extern HOST_DATA f32 D_801E0DE8;
extern HOST_DATA f32 D_801E0DEC;
extern HOST_DATA f32 D_801E0DF0;
extern HOST_DATA f32 D_801E0DF4;
extern HOST_DATA f32 D_801E0DF8;
extern HOST_DATA f32 D_801E0DFC;
extern HOST_DATA f32 D_801E0E00;
extern HOST_DATA f32 D_801E0E04;
extern HOST_DATA f32 D_801E0E08;
extern HOST_DATA f32 D_801E0E0C;
extern HOST_DATA f32 D_801E0E10;
extern HOST_DATA f32 D_801E0E14;
extern HOST_DATA f32 D_801E0E18;
extern HOST_DATA f32 D_801E0E1C;
extern HOST_DATA f32 D_801E0E20;
extern HOST_DATA f32 D_801E0E24;
extern HOST_DATA f64 D_801E0EB0;
// extern UNK_TYPE4 D_801E1068;
extern HOST_DATA UNK_PTR D_801E10B0;
extern HOST_DATA const s16 gAudioTatumInit[];
extern HOST_DATA const AudioHeapInitSizes gAudioHeapInitSizes;
// extern UNK_TYPE4 D_801E1108;
// extern UNK_TYPE4 D_801E110C;
extern HOST_DATA u8 gSoundFontTable[];
extern HOST_DATA u8 gSequenceFontTable[];
extern HOST_DATA u8 gSequenceTable[];
extern HOST_DATA u8 gSampleBankTable[];

// bss
// extern UNK_TYPE1 D_801ED894;

// extern UNK_TYPE1 D_801F4E20;
// extern UNK_TYPE1 sBeatingHeartsDDPrim;
// extern UNK_TYPE1 sBeatingHeartsDDEnv;
// extern UNK_TYPE1 sHeartsDDPrim;
// extern UNK_TYPE1 D_801F4F56;
// extern UNK_TYPE1 D_801F4F58;
// extern UNK_TYPE1 D_801F4F5A;
// extern UNK_TYPE1 D_801F4F60;
// extern UNK_TYPE1 D_801F4F66;
// extern UNK_TYPE1 D_801F4F68;
// extern UNK_TYPE1 D_801F4F6A;

// extern UNK_TYPE1 sSkyboxDrawMatrix;
// extern UNK_TYPE1 D_801F6AF0;
// extern UNK_TYPE1 D_801F6AF2;
// extern UNK_TYPE4 D_801F6B00;
// extern UNK_TYPE4 D_801F6B04;
// extern UNK_TYPE4 D_801F6B08;

extern HOST_DATA void (*sKaleidoScopeUpdateFunc)(PlayState* play);
extern HOST_DATA void (*sKaleidoScopeDrawFunc)(PlayState* play);

extern HOST_DATA GfxMasterList* gGfxMasterDL;

extern HOST_DATA u64* gAudioSPDataPtr;
extern HOST_DATA u32 gAudioSPDataSize;

extern HOST_DATA MtxF* sMatrixStack;
extern HOST_DATA MtxF* sCurrentMatrix;

extern HOST_DATA Color_RGBA8_u32 gVisMonoColor;

extern HOST_DATA s32 D_801FD120;
extern HOST_DATA u8 sResetAudioHeapTimer;
extern HOST_DATA u16 sResetAudioHeapFadeReverbVolume;
extern HOST_DATA u16 sResetAudioHeapFadeReverbVolumeStep;
extern HOST_DATA AudioContext gAudioCtx; // at 0x80200C70
extern HOST_DATA AudioCustomUpdateFunction gAudioCustomUpdateFunction;
extern HOST_DATA AudioCustomSeqFunction gAudioCustomSeqFunction;
extern HOST_DATA AudioCustomReverbFunction gAudioCustomReverbFunction;
extern HOST_DATA AudioCustomSynthFunction gAudioCustomSynthFunction;

// other segments
extern HOST_DATA Mtx D_01000000;
extern HOST_DATA u16 D_0F000000[];

// #region 2S2H [General]
extern HOST_DATA PlayState* gPlayState;
extern HOST_DATA GameState* gGameState;
// #endregion

#ifdef __cplusplus
};
#endif

#endif
