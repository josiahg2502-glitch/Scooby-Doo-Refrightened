#pragma once

#include "isa/big_endian.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace No100fWindow {

inline constexpr uint32_t kMapperId = 0x120B8722;
inline constexpr uint32_t kWorldId = 0x09B27E09;
inline constexpr uint32_t kSurfaceId = 0x6C86C1C1;
inline constexpr uint32_t kGroupId = 0x472FDC66;
inline constexpr uint32_t kWindowTextureId = 0x36F3752C;
// Matches the Xbox window material's 31 triangles and UVs; its Xbox index (69) differs.
inline constexpr uint32_t kMaterialIndex = 71;
inline constexpr uint32_t kMaterialCount = 76;
inline constexpr std::array<uint32_t, 18> kFrames{
    0x0817059E, 0x0817059F, 0x081705A0, 0x081705A1, 0x081705A2, 0x081705A3,
    0x081705A4, 0x081705A5, 0x081705A6, 0x08170620, 0x08170621, 0x08170622,
    0x08170623, 0x08170624, 0x08170625, 0x08170626, 0x08170627, 0x08170628,
};

struct Asset {
    uint32_t address = 0;
    uint32_t size = 0;
};

enum class Match { Unrelated, ExistingMapping, Incompatible, Restore };

template <typename Contains, typename Read8, typename Read16, typename Read32, typename FindAsset>
Match Inspect(uint32_t assetId, Asset mapper, uint32_t sceneWorld, Contains contains,
              Read8 read8, Read16 read16, Read32 read32, FindAsset findAsset) {
    if (assetId != kMapperId) return Match::Unrelated;
    if (!mapper.address || mapper.size < 8 || !contains(mapper.address, 8) ||
        read32(mapper.address) != kMapperId) return Match::Incompatible;
    if (read32(mapper.address + 4) != 0) return Match::ExistingMapping;
    if (mapper.size != 8) return Match::Incompatible;

    const Asset world = findAsset(kWorldId);
    if (!sceneWorld || world.address != sceneWorld || !contains(sceneWorld, 0x18) ||
        read32(sceneWorld + 0x14) != kMaterialCount) return Match::Incompatible;
    const uint32_t materials = read32(sceneWorld + 0x10);
    if (!materials || !contains(materials, kMaterialCount * 4)) return Match::Incompatible;
    const uint32_t material = read32(materials + kMaterialIndex * 4);
    const Asset windowTexture = findAsset(kWindowTextureId);
    if (!material || !contains(material, 4) || !windowTexture.address ||
        read32(material) != windowTexture.address) return Match::Incompatible;

    const Asset surface = findAsset(kSurfaceId);
    const Asset group = findAsset(kGroupId);
    if (surface.size != 208 || !surface.address || !contains(surface.address, surface.size) ||
        read32(surface.address) != kSurfaceId || read32(surface.address + 0x34) != 1 ||
        read16(surface.address + 0x3A) != 0 || read32(surface.address + 0x3C) != kGroupId ||
        read32(surface.address + 0x40) != 0x40A00000 || read8(surface.address + 0xCC) != 1 ||
        group.size != 84 || !group.address || !contains(group.address, group.size) ||
        read32(group.address) != kGroupId || read16(group.address + 8) != kFrames.size())
        return Match::Incompatible;
    for (std::size_t i = 0; i < kFrames.size(); ++i) {
        if (read32(group.address + 12 + static_cast<uint32_t>(i) * 4) != kFrames[i])
            return Match::Incompatible;
        const Asset frame = findAsset(kFrames[i]);
        if (!frame.address || !contains(frame.address, 0x54)) return Match::Incompatible;
    }
    return Match::Restore;
}

inline std::array<uint8_t, 16> EncodeMapper() {
    std::array<uint8_t, 16> mapper{};
    BigEndian::Write32(mapper.data(), kMapperId);
    BigEndian::Write32(mapper.data() + 4, 1);
    BigEndian::Write32(mapper.data() + 8, kSurfaceId);
    BigEndian::Write32(mapper.data() + 12, kMaterialIndex);
    return mapper;
}

} // namespace No100fWindow
