#pragma once

#include <cstdint>
#include <cstring>
#include <vector>
#include "SceneCommand.h"
#include <libultraship/libultra/types.h>
#include "z64math.h"

namespace SOH {
// 2S2H [Unbound] mirrors z64light.h: position is f32 (world extent)
typedef struct {
    /* 0x00 */ f32 x;
    /* 0x04 */ f32 y;
    /* 0x08 */ f32 z;
    /* 0x0C */ u8 color[3];
    /* 0x0F */ u8 drawGlow;
    /* 0x10 */ s16 radius;
} LightPoint; // size = 0x14

typedef struct {
    /* 0x0 */ s8 x;
    /* 0x1 */ s8 y;
    /* 0x2 */ s8 z;
    /* 0x3 */ u8 color[3];
} LightDirectional; // size = 0x6

typedef union {
    LightPoint point;
    LightDirectional dir;
} LightParams; // size = 0x14

typedef struct {
    /* 0x0 */ u8 type;
    /* 0x4 */ LightParams params;
} LightInfo; // size = 0x18

constexpr u8 LIGHT_INFO_TYPE_DIRECTIONAL = 1;

// Archives store every light as three s16; a directional light's bytes are its packed dir fields, kept verbatim
inline void LightInfo_SetCoords(LightInfo& light, f32 x, f32 y, f32 z) {
    if (light.type == LIGHT_INFO_TYPE_DIRECTIONAL) {
        const s16 packed[3] = { (s16)x, (s16)y, (s16)z };
        memcpy(&light.params, packed, sizeof(packed));
        return;
    }
    light.params.point.x = x;
    light.params.point.y = y;
    light.params.point.z = z;
}

inline Vec3f LightInfo_GetCoords(const LightInfo& light) {
    if (light.type == LIGHT_INFO_TYPE_DIRECTIONAL) {
        s16 packed[3];
        memcpy(packed, &light.params, sizeof(packed));
        return { (f32)packed[0], (f32)packed[1], (f32)packed[2] };
    }
    return { light.params.point.x, light.params.point.y, light.params.point.z };
}

class SetLightList : public SceneCommand<LightInfo> {
  public:
    using SceneCommand::SceneCommand;

    LightInfo* GetPointer();
    size_t GetPointerSize();

    uint32_t numLights;
    std::vector<LightInfo> lightList;
};
}; // namespace SOH
