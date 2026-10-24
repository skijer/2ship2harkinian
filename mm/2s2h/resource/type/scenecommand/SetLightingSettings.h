#pragma once

#include <vector>
#include "SceneCommand.h"
#include <libultraship/libultra/types.h>

namespace SOH {
// 2S2H [Unbound] mirrors WorldFog in z64light.h
typedef struct {
    /* 0x0 */ f32 fogStart;
    /* 0x4 */ f32 fogEnd;
    /* 0x8 */ f32 drawDistance;
    /* 0xC */ f32 nearPlane;
} WorldFog; // size = 0x10

// Mirrors EnvLightSettings in z64environment.h (static_asserts in z_scene_2SH.cpp)
typedef struct {
    /* 0x00 */ u8 ambientColor[3];
    /* 0x03 */ s8 light1Dir[3];
    /* 0x06 */ u8 light1Color[3];
    /* 0x09 */ s8 light2Dir[3];
    /* 0x0C */ u8 light2Color[3];
    /* 0x0F */ u8 fogColor[3];
    /* 0x12 */ s16 fogNear;
    /* 0x14 */ s16 fogFar;
    /* 0x16 */ u8 hasWorldFog;
    /* 0x18 */ WorldFog worldFog;
} EnvLightSettings; // size = 0x28

class SetLightingSettings : public SceneCommand<EnvLightSettings> {
  public:
    using SceneCommand::SceneCommand;

    EnvLightSettings* GetPointer();
    size_t GetPointerSize();

    std::vector<EnvLightSettings> settings;
};
}; // namespace SOH
