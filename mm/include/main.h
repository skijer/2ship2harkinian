#ifndef MAIN_H
#define MAIN_H

#include "ultra64.h"
#include "scheduler.h"
#include "padmgr.h"

extern HOST_DATA s32 gScreenWidth;
extern HOST_DATA s32 gScreenHeight;
extern HOST_DATA size_t gSystemHeapSize;

extern HOST_DATA uintptr_t gSegments[NUM_SEGMENTS];
extern HOST_DATA SchedContext gSchedContext;
extern HOST_DATA OSThread gGraphThread;
extern HOST_DATA PadMgr gPadMgr;

void Main(void* arg);

#define SEGMENTED_TO_K0(addr) (addr) // (void*)((gSegments[SEGMENT_NUMBER(addr)] + K0BASE) + SEGMENT_OFFSET(addr))

#endif
