#pragma once

#include "shared_asset_archive.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Cojiro is OoT's cucco with blue feathers, built from the player's oot.o2r for cojiro_companion.
namespace CojiroAssets {

using ReadResourceFunc = std::vector<uint8_t> (*)(const char* path);

struct Rgb5 {
    uint32_t red;
    uint32_t green;
    uint32_t blue;
};

struct HashRemap {
    uint64_t source;
    uint64_t target;
};

constexpr const char* kSourceFolder = "objects/oot_object_niw/";
constexpr const char* kTargetFolder = "objects/object_nei_cojiro/";
constexpr std::string_view kEyeTexture = "gCuccoEyeTex";
constexpr std::array<std::string_view, 4> kFeatherTextures = { "gCuccoBaseFeathersTex", "gCuccoHeadFeathersTex",
                                                                "gCuccoTailFeathersTex", kEyeTexture };
constexpr std::array<std::string_view, 7> kDisplayLists = { "gCuccoTailAndUndersideDL", "gCuccoLeftFootDL",
                                                            "gCuccoLeftWingDL",         "gCuccoRightFootDL",
                                                            "gCuccoRightWingDL",        "gCuccoNeckDL",
                                                            "gCuccoHeadDL" };

constexpr Rgb5 kCojiroBlue = { 55, 110, 245 };
constexpr uint32_t kEyeBackgroundMinChannel = 21;
constexpr uint32_t kEyeBackgroundMaxSpread = 2;
constexpr uint32_t kTextureRgba16 = 2;
constexpr size_t kTexelsOffset = SharedAssetArchive::kResourceHeaderSize + SharedAssetArchive::kTextureLayoutSize;

constexpr uint64_t kResourceHashPolynomial = 0x42F0E1EBA9EA3693;
constexpr uint32_t kDisplayListType = 0x4F444C54;
// Resource header, ucode byte, then padding to 8 bytes.
constexpr size_t kFirstCommandOffset = 72;
constexpr size_t kCommandSize = 8;
constexpr uint8_t kEndDisplayListOpcode = 0xDF;
constexpr std::array<uint8_t, 7> kHashedOpcodes = { 0x20, 0x31, 0x32, 0x33, 0x35, 0x36, 0x42 };

// Same CRC-64 as the resource manager, so the result names a resource inside a display list.
inline uint64_t CalculateResourceHash(std::string_view path) {
    uint64_t hash = UINT64_MAX;
    for (unsigned char byte : path) {
        hash ^= static_cast<uint64_t>(byte) << 56;
        for (int bit = 0; bit < 8; bit++) {
            const bool isTopBitSet = (hash >> 63) != 0;
            hash <<= 1;
            if (isTopBitSet) {
                hash ^= kResourceHashPolynomial;
            }
        }
    }
    return hash;
}

inline std::string GetSourcePath(std::string_view name) {
    return std::string(kSourceFolder).append(name);
}

inline std::string GetTargetPath(std::string_view name) {
    return std::string(kTargetFolder).append(name);
}

inline void WriteU32(std::vector<uint8_t>& bytes, size_t offset, uint32_t value) {
    for (size_t i = 0; i < 4; i++) {
        bytes[offset + i] = static_cast<uint8_t>(value >> (i * 8));
    }
}

inline uint64_t ReadHash(const std::vector<uint8_t>& bytes, size_t offset) {
    return static_cast<uint64_t>(SharedAssetArchive::ReadU32(bytes, offset)) << 32 |
           SharedAssetArchive::ReadU32(bytes, offset + 4);
}

inline void WriteHash(std::vector<uint8_t>& bytes, size_t offset, uint64_t hash) {
    WriteU32(bytes, offset, static_cast<uint32_t>(hash >> 32));
    WriteU32(bytes, offset + 4, static_cast<uint32_t>(hash));
}

inline uint16_t ReadTexel(const std::vector<uint8_t>& texture, size_t index) {
    const size_t offset = kTexelsOffset + index * 2;
    return static_cast<uint16_t>(texture[offset] << 8 | texture[offset + 1]);
}

inline void WriteTexel(std::vector<uint8_t>& texture, size_t index, uint16_t texel) {
    const size_t offset = kTexelsOffset + index * 2;
    texture[offset] = static_cast<uint8_t>(texel >> 8);
    texture[offset + 1] = static_cast<uint8_t>(texel);
}

inline Rgb5 UnpackRgba16(uint16_t texel) {
    return { static_cast<uint32_t>(texel >> 11) & 31, static_cast<uint32_t>(texel >> 6) & 31,
             static_cast<uint32_t>(texel >> 1) & 31 };
}

inline uint32_t GetTextureWidth(const std::vector<uint8_t>& texture) {
    return SharedAssetArchive::ReadU32(texture, SharedAssetArchive::kResourceHeaderSize + 4);
}

inline uint32_t GetTextureHeight(const std::vector<uint8_t>& texture) {
    return SharedAssetArchive::ReadU32(texture, SharedAssetArchive::kResourceHeaderSize + 8);
}

inline bool IsRgba16Texture(const std::vector<uint8_t>& texture) {
    if (texture.size() < kTexelsOffset) {
        return false;
    }
    const size_t texelBytes = SharedAssetArchive::ReadU32(texture, SharedAssetArchive::kResourceHeaderSize + 12);
    return SharedAssetArchive::ReadU32(texture, SharedAssetArchive::kResourceHeaderSize) == kTextureRgba16 &&
           texelBytes == static_cast<size_t>(GetTextureWidth(texture)) * GetTextureHeight(texture) * 2 &&
           texture.size() == kTexelsOffset + texelBytes;
}

// Keeps the texel's brightest channel as its light, and its alpha bit.
inline uint16_t TintBlue(uint16_t texel) {
    const Rgb5 rgb = UnpackRgba16(texel);
    const uint32_t light = std::max({ rgb.red, rgb.green, rgb.blue });
    // Rounds light * channel / 255, which never falls on a half.
    const auto scale = [light](uint32_t channel) { return (2 * light * channel + 255) / 510; };
    return static_cast<uint16_t>(scale(kCojiroBlue.red) << 11 | scale(kCojiroBlue.green) << 6 |
                                 scale(kCojiroBlue.blue) << 1 | (texel & 1));
}

inline bool IsEyeBackground(uint16_t texel) {
    const Rgb5 rgb = UnpackRgba16(texel);
    const uint32_t darkest = std::min({ rgb.red, rgb.green, rgb.blue });
    const uint32_t brightest = std::max({ rgb.red, rgb.green, rgb.blue });
    return darkest >= kEyeBackgroundMinChannel && brightest - darkest <= kEyeBackgroundMaxSpread;
}

// The eye's white feather background would be a square on a blue bird; the iris, pupil and highlights stay.
inline std::vector<bool> FindEyeBackground(const std::vector<uint8_t>& eye) {
    const uint32_t width = GetTextureWidth(eye);
    const uint32_t height = GetTextureHeight(eye);
    std::vector<bool> isBackground(static_cast<size_t>(width) * height, false);
    std::vector<size_t> pending;
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            if (x == 0 || y == 0 || x + 1 == width || y + 1 == height) {
                pending.push_back(static_cast<size_t>(y) * width + x);
            }
        }
    }
    while (!pending.empty()) {
        const size_t index = pending.back();
        pending.pop_back();
        if (isBackground[index] || !IsEyeBackground(ReadTexel(eye, index))) {
            continue;
        }
        isBackground[index] = true;
        const size_t x = index % width;
        const size_t y = index / width;
        if (x > 0) {
            pending.push_back(index - 1);
        }
        if (x + 1 < width) {
            pending.push_back(index + 1);
        }
        if (y > 0) {
            pending.push_back(index - width);
        }
        if (y + 1 < height) {
            pending.push_back(index + width);
        }
    }
    return isBackground;
}

// Assumes IsRgba16Texture(texture).
inline std::vector<uint8_t> TintFeathers(std::vector<uint8_t> texture, bool isEye) {
    const size_t texelCount = static_cast<size_t>(GetTextureWidth(texture)) * GetTextureHeight(texture);
    const std::vector<bool> isTinted = isEye ? FindEyeBackground(texture) : std::vector<bool>(texelCount, true);
    for (size_t index = 0; index < texelCount; index++) {
        if (isTinted[index]) {
            WriteTexel(texture, index, TintBlue(ReadTexel(texture, index)));
        }
    }
    return texture;
}

inline bool IsHashedOpcode(uint8_t opcode) {
    return std::find(kHashedOpcodes.begin(), kHashedOpcodes.end(), opcode) != kHashedOpcodes.end();
}

// Returns nothing for a resource that is not a terminated binary display list.
inline std::vector<uint8_t> RemapDisplayList(std::vector<uint8_t> displayList, const std::vector<HashRemap>& remaps) {
    if (displayList.size() < kFirstCommandOffset || SharedAssetArchive::ReadU32(displayList, 4) != kDisplayListType) {
        return {};
    }
    size_t offset = kFirstCommandOffset;
    while (offset + kCommandSize <= displayList.size()) {
        const uint8_t opcode = displayList[offset + 3];
        offset += kCommandSize;
        if (IsHashedOpcode(opcode)) {
            if (offset + kCommandSize > displayList.size()) {
                return {};
            }
            const uint64_t hash = ReadHash(displayList, offset);
            for (const HashRemap& remap : remaps) {
                if (hash == remap.source) {
                    WriteHash(displayList, offset, remap.target);
                }
            }
            offset += kCommandSize;
        }
        if (opcode == kEndDisplayListOpcode) {
            return displayList;
        }
    }
    return {};
}

// The display lists keep OoT's beak, comb, claw and vertices; only the feather textures point to the blue copies.
inline std::vector<SharedAssetArchive::ArchiveFile> MakeCojiroAssets(ReadResourceFunc readResource) {
    std::vector<SharedAssetArchive::ArchiveFile> files;
    std::vector<HashRemap> remaps;
    for (std::string_view name : kFeatherTextures) {
        const std::string source = GetSourcePath(name);
        const std::string target = GetTargetPath(name);
        std::vector<uint8_t> texture = readResource(source.c_str());
        if (!IsRgba16Texture(texture)) {
            return {};
        }
        files.push_back({ target, TintFeathers(std::move(texture), name == kEyeTexture) });
        remaps.push_back({ CalculateResourceHash(source), CalculateResourceHash(target) });
    }
    for (std::string_view name : kDisplayLists) {
        std::vector<uint8_t> displayList = RemapDisplayList(readResource(GetSourcePath(name).c_str()), remaps);
        if (displayList.empty()) {
            return {};
        }
        files.push_back({ GetTargetPath(name), std::move(displayList) });
    }
    return files;
}

} // namespace CojiroAssets
