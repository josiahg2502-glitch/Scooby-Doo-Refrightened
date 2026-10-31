#pragma once

#include <array>
#include <string>

namespace aurora::ButtonIcons {
inline constexpr std::array<const char*, 6> kPresets{
    "xbox", "playstation", "nintendo", "xbox_original", "playstation_xbox", "nintendo_xbox"};
inline constexpr int kStyleCount = 6 + 3 * 256;

// Two bits per GameCube A/B/X/Y slot: physical south/east/west/north.
constexpr int Style(int family, const std::array<unsigned, 4>& buttons) {
    if (family < 0 || family > 2) return -1;
    unsigned mapping = 0;
    for (unsigned i = 0; i < buttons.size(); ++i) {
        if (buttons[i] > 3) return -1;
        mapping |= buttons[i] << (i * 2);
    }
    if (mapping == 0xE4) return family;
    if (mapping == 0xD8) return family + 3;
    return 6 + family * 256 + static_cast<int>(mapping);
}

inline std::string Name(int style) {
    if (style < 0 || style >= kStyleCount) return "stock";
    if (style < 6) return kPresets[style];
    const unsigned value = static_cast<unsigned>(style - 6);
    std::string name = std::string(kPresets[value / 256]) + "_";
    for (unsigned i = 0; i < 4; ++i) name += "ABXY"[(value >> (i * 2)) & 3];
    return name;
}
} // namespace aurora::ButtonIcons
