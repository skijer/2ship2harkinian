#pragma once
#include "UnboundJson.h"
#include "UnboundSchema.h"
namespace S2H::Unbound {
struct PackedField {
    const char* key;
    u32 word;
    u32 shift;
    u32 mask;
};

constexpr PackedField kSurfaceFields[] = {
    { Schema::kCamera, 0, 0, 0xFF },          { Schema::kExit, 0, 8, 0x1F },
    { Schema::kFloorType, 0, 13, 0x1F },      { Schema::kWallFlags, 0, 18, 0x07 },
    { Schema::kWallType, 0, 21, 0x1F },       { Schema::kFloorProperty, 0, 26, 0x0F },
    { Schema::kIsSoft, 0, 30, 1 },            { Schema::kIsHorseBlocked, 0, 31, 1 },
    { Schema::kMaterial, 1, 0, 0x0F },        { Schema::kFloorEffect, 1, 4, 0x03 },
    { Schema::kLightSetting, 1, 6, 0x1F },    { Schema::kEcho, 1, 11, 0x3F },
    { Schema::kCanHookshot, 1, 17, 1 },       { Schema::kConveyorSpeed, 1, 18, 0x07 },
    { Schema::kConveyorDirection, 1, 21, 0x3F }, { Schema::kIsWallDamage, 1, 27, 1 },
};
}
