#pragma once

#include <cstdint>
#include <vector>
#include <ship/resource/Resource.h>
#include <libultraship/libultra.h>
#include "z64math.h"

namespace SOH {

typedef struct {
    /* 0x00 */ u16 type;
    union {
        u32 vtxData[3];
        struct {
            /* 0x02 */ u32 flags_vIA; // upper three bits are exclusion flags
            /* 0x04 */ u32 flags_vIB; // upper three bits are flags
                                      // bit 29 = conveyor surface
            /* 0x06 */ u32 vIC;
        };
    };
    /* 0x08 */ Vec3s normal; // Unit normal vector
                             // Value ranges from -0x7FFF to 0x7FFF, representing -1.0 to 1.0; 0x8000 is invalid

    // 2S2H [Unbound] s32 (world extent); every mirror here must match z64bgcheck.h (static_asserts in the factory)
    /* 0x10 */ s32 dist; // Plane distance from origin along the normal
} CollisionPoly;         // size = 0x14

typedef struct {
    /* 0x00 */ s32 xMin;
    /* 0x04 */ s32 ySurface;
    /* 0x08 */ s32 zMin;
    /* 0x0C */ s32 xLength;
    /* 0x10 */ s32 zLength;
    /* 0x14 */ u32 properties;
    s32 room; // -1 applies to every room

    // 0x0008_0000 = ?
    // 0x0007_E000 = Room Index, 0x3F = all rooms
    // 0x0000_1F00 = Lighting Settings Index
    // 0x0000_00FF = CamData index
} WaterBox; // size = 0x18

typedef struct {
    /* 0x00 */ u16 cameraSType;
    /* 0x02 */ s16 numCameras;
    /* 0x04 */ Vec3s* camPosData;
} CamData;

typedef struct {
    u32 data[2];

    // Type 1
    // 0x0800_0000 = wall damage
} SurfaceType;

typedef struct {
    /* 0x00 */ Vec3i minBounds; // minimum coordinates of poly bounding box
    /* 0x0C */ Vec3i maxBounds; // maximum coordinates of poly bounding box
    /* 0x18 */ u32 numVertices;
    /* 0x1C */ Vec3i* vtxList;
    /* 0x14 */ u32 numPolygons;
    /* 0x18 */ CollisionPoly* polyList;
    /* 0x1C */ SurfaceType* surfaceTypeList;
    /* 0x20 */ CamData* cameraDataList;
    /* 0x24 */ u32 numWaterBoxes;
    /* 0x28 */ WaterBox* waterBoxes;
    size_t cameraDataListLen; // OTRTODO: Added to allow for bounds checking the cameraDataList.
} CollisionHeaderData;        // original name: BGDataInfo

class CollisionHeader : public Ship::Resource<CollisionHeaderData> {
  public:
    using Resource::Resource;

    CollisionHeader() : Resource(std::shared_ptr<Ship::ResourceInitData>()) {
    }

    CollisionHeaderData* GetPointer();
    size_t GetPointerSize();

    CollisionHeaderData collisionHeaderData;

    std::vector<Vec3i> vertices;

    std::vector<CollisionPoly> polygons;

    uint32_t surfaceTypesCount;
    std::vector<SurfaceType> surfaceTypes;

    uint32_t camDataCount;
    std::vector<CamData> camData;
    std::vector<int32_t> camPosDataIndices;

    int32_t camPosCount;
    Vec3s camPosDataZero;
    std::vector<Vec3s> camPosData;

    std::vector<WaterBox> waterBoxes;
};
}; // namespace SOH
