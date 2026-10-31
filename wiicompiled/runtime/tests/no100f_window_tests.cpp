#include "hle/no100f_window_contract.h"

#include <array>
#include <iostream>
#include <stdexcept>
#include <unordered_map>

using namespace No100fWindow;

static void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
    try {
        std::array<uint8_t, 0x3000> ram{};
        std::unordered_map<uint32_t, Asset> assets{
            {kMapperId, {0x100, 8}}, {kWorldId, {0x200, 0x100}},
            {kWindowTextureId, {0x600, 2232}}, {kSurfaceId, {0x700, 208}},
            {kGroupId, {0x800, 84}},
        };
        auto write32 = [&](uint32_t address, uint32_t value) {
            BigEndian::Write32(ram.data() + address, value);
        };
        write32(0x100, kMapperId);
        write32(0x210, 0x300);
        write32(0x214, kMaterialCount);
        write32(0x300 + kMaterialIndex * 4, 0x500);
        write32(0x500, 0x600);
        write32(0x700, kSurfaceId);
        write32(0x734, 1);
        write32(0x73C, kGroupId);
        write32(0x740, 0x40A00000);
        ram[0x7CC] = 1;
        write32(0x800, kGroupId);
        BigEndian::Write16(ram.data() + 0x808, 18);
        for (std::size_t i = 0; i < kFrames.size(); ++i) {
            write32(0x80C + static_cast<uint32_t>(i) * 4, kFrames[i]);
            assets[kFrames[i]] = {0x1000 + static_cast<uint32_t>(i) * 0x60, 2232};
        }
        auto contains = [&](uint32_t address, std::size_t size) {
            return address && address < ram.size() && size <= ram.size() - address;
        };
        auto inspect = [&](uint32_t id = kMapperId, uint32_t world = 0x200) {
            return Inspect(id, assets[kMapperId], world, contains,
                [&](uint32_t a) { return ram.at(a); },
                [&](uint32_t a) { return BigEndian::Read16(ram.data() + a); },
                [&](uint32_t a) { return BigEndian::Read32(ram.data() + a); },
                [&](uint32_t key) { return assets[key]; });
        };
        const auto original = ram;
        Require(inspect() == Match::Restore, "Match the verified courtyard and retained frames");
        Require(ram == original, "Inspect without mutating the eight-byte disc mapper or other assets");
        Require(inspect(123) == Match::Unrelated, "Ignore unrelated mapper IDs");
        Require(inspect(kMapperId, 0) == Match::Incompatible, "Reject a missing scene world");
        Require(inspect(kMapperId, 0x201) == Match::Incompatible, "Do not restore in another world");
        write32(0x104, 1);
        Require(inspect() == Match::ExistingMapping, "Preserve mod-supplied mappings and prevent duplicates");
        write32(0x104, 0);
        for (uint32_t address : {0x100u, 0x214u, 0x500u, 0x700u, 0x734u, 0x73Cu,
                                 0x740u, 0x800u, 0x80Cu}) {
            const uint32_t saved = BigEndian::Read32(ram.data() + address);
            write32(address, 0);
            Require(inspect() == Match::Incompatible, "Reject an incompatible asset signature");
            write32(address, saved);
        }
        ram[0x7CC] = 0;
        Require(inspect() == Match::Incompatible, "Preserve a disabled animation");
        ram[0x7CC] = 1;
        BigEndian::Write16(ram.data() + 0x808, 17);
        Require(inspect() == Match::Incompatible, "Require the complete ordered animation group");
        BigEndian::Write16(ram.data() + 0x808, 18);
        BigEndian::Write16(ram.data() + 0x73A, 1);
        Require(inspect() == Match::Incompatible, "Do not replace a mod's changed animation mode");
        BigEndian::Write16(ram.data() + 0x73A, 0);
        for (uint32_t id : {kMapperId, kSurfaceId, kGroupId}) {
            const uint32_t size = assets[id].size;
            assets[id].size = size - 1;
            Require(inspect() == Match::Incompatible, "Reject truncated asset payloads");
            assets[id].size = size;
        }
        write32(0x210, 0x2FF0);
        Require(inspect() == Match::Incompatible, "Bound the complete world material-pointer array");
        write32(0x210, 0x300);
        const Asset baseTexture = assets[kWindowTextureId];
        assets[kWindowTextureId] = {};
        Require(inspect() == Match::Incompatible, "Reject a missing original window material texture");
        assets[kWindowTextureId] = baseTexture;
        for (std::size_t i = 0; i < kFrames.size(); ++i) {
            const uint32_t address = 0x80C + static_cast<uint32_t>(i) * 4;
            write32(address, kFrames[(i + 1) % kFrames.size()]);
            Require(inspect() == Match::Incompatible, "Require the original order for every animation frame");
            write32(address, kFrames[i]);
        }
        const Asset frame = assets[kFrames.back()];
        assets[kFrames.back()] = {};
        Require(inspect() == Match::Incompatible, "Reject a missing texture frame");
        assets[kFrames.back()] = frame;
        for (uint32_t id : {kMapperId, kWorldId, kSurfaceId, kGroupId, kFrames.back()}) {
            const Asset saved = assets[id];
            assets[id].address = 0x2FF0;
            Require(inspect() == Match::Incompatible, "Bound all guest memory reads");
            assets[id] = saved;
        }
        const auto encoded = EncodeMapper();
        Require(BigEndian::Read32(encoded.data()) == kMapperId &&
                BigEndian::Read32(encoded.data() + 4) == 1 &&
                BigEndian::Read32(encoded.data() + 8) == kSurfaceId &&
                BigEndian::Read32(encoded.data() + 12) == 71,
                "Encode one complete big-endian mapper entry, not the Xbox material index");
        Require(inspect() == Match::Restore, "Allow restoration on a fresh scene load");
        std::cout << "Courtyard material matching, asset guards and mapper encoding passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
