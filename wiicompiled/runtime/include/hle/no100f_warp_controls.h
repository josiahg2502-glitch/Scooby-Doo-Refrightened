#pragma once

#include <cstdint>

namespace No100fWarpControls {
// Pinned GIHE78: mode 7 is entered by event 0x9B; 0x9C is its confirmation state.
inline constexpr uint32_t kModeAddress = 0x80270E18;
inline constexpr uint32_t kWarpMode = 7;
inline constexpr uint16_t kFaces = 0x0600;

constexpr uint16_t SwapFaces(uint16_t buttons) {
    return static_cast<uint16_t>((buttons & ~kFaces) |
        ((buttons & 0x0200) << 1) | ((buttons & 0x0400) >> 1));
}

struct State {
    uint32_t device = 0;
    bool menu = false;
    uint16_t suppressed = 0;

    uint16_t Filter(uint16_t buttons, bool enabled, bool inMenu, bool blocked, uint32_t id) {
        if (!enabled || !id) {
            *this = {};
            return buttons;
        }
        if (device != id) {
            *this = {id, inMenu, static_cast<uint16_t>(inMenu ? buttons & kFaces : 0)};
        }
        if (menu != inMenu || blocked) suppressed |= buttons & kFaces;
        menu = inMenu;
        suppressed &= buttons;
        buttons &= static_cast<uint16_t>(~suppressed);
        return inMenu ? SwapFaces(buttons) : buttons;
    }
};
} // namespace No100fWarpControls
