#pragma once

#include <array>
#include <cstdint>

namespace No100fXboxLayout {

inline constexpr std::array<const char*, 12> kBindings{
    "south", "west", "east", "north", "start", "disabled",
    "left_trigger@50", "right_trigger@50",
    "dpad_up", "dpad_down", "dpad_left", "dpad_right",
};

// Add the original secondary actions after GC normalization, without GC Z's
// global trigger modifier: gum + sneak and run + soap must remain independent.
inline uint32_t AddActions(uint32_t buttons, bool black, bool white) {
    return buttons | (black ? 0x102000u : 0u) | (white ? 0x200u : 0u);
}

inline uint8_t Trigger(uint8_t value) {
    return value > 128 ? value : 0;
}

// Match PADBlockInput: releasing a button held across F10 must precede a new action.
inline bool ReadAction(bool held, bool blocked, bool& suppressed) {
    if (!held) suppressed = false;
    else if (blocked) suppressed = true;
    return held && !blocked && !suppressed;
}

} // namespace No100fXboxLayout
