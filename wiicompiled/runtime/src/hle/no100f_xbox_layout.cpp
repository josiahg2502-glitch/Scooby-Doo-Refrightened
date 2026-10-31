#include "abi_bridge.h"
#include "controller_button_names.h"
#include "hle/no100f_xbox_layout.h"
#include "input_bindings.h"
#include "runtime_config.h"
#include "runtime_log.h"

#include <array>
#include <string>

namespace {
struct XboxPadState {
    SDL_JoystickID controller{};
    std::array<bool, 2> suppressed{};
};
std::array<XboxPadState, PAD_CHANMAX> g_xboxPadStates;
}

void No100f_ApplyXboxPadActions(CpuContext* ctx, uint32_t pad, uint32_t buttons) {
    if (!RuntimeConfigFile::XboxOriginalLayout()) {
        g_xboxPadStates = {};
        return;
    }
    if (ctx->gpr[3] != 1) return;
    if (!buttons || !Memory::Contains(pad, 0x56) || !Memory::Contains(buttons, 4)) {
        RT_LOG(RT_TAG_CONFIG) << "[xbox-layout] invalid pad-update result\n";
        return;
    }
    const uint16_t port = Memory::Read16(pad + 0x54);
    if (port >= PAD_CHANMAX) return;
    const int index = PADGetIndexForPort(port);
    SDL_Gamepad* gamepad = index < 0 ? nullptr : PADGetSDLGamepadForIndex(index);
    auto& state = g_xboxPadStates[port];
    if (!gamepad) {
        state = {};
        return;
    }
    const SDL_JoystickID controller = SDL_GetJoystickID(SDL_GetGamepadJoystick(gamepad));
    if (state.controller != controller) state = {controller, {}};
    const bool blocked = InputBindings::InputBlocked();
    const auto held = [&](size_t action) {
        const std::string token = RuntimeConfigFile::XboxButton(action);
        const auto* native = ControllerNames::FindNativeButton(token);
        if (native && native->nativeButton == PAD_NATIVE_BUTTON_DISABLED) return false;
        if (!native || native->nativeButton >= SDL_GAMEPAD_BUTTON_COUNT) {
            static std::array<std::string, 2> reported;
            if (reported[action] != token) {
                reported[action] = token;
                RT_LOG(RT_TAG_CONFIG) << "[xbox-layout] invalid "
                    << (action == 0 ? "Black" : "White") << " binding '" << token
                    << "'; action disabled\n";
            }
            return false;
        }
        return SDL_GetGamepadButton(gamepad, static_cast<SDL_GamepadButton>(native->nativeButton));
    };
    const bool black = No100fXboxLayout::ReadAction(held(0), blocked, state.suppressed[0]);
    const bool white = No100fXboxLayout::ReadAction(held(1), blocked, state.suppressed[1]);
    Memory::Write32(buttons, No100fXboxLayout::AddActions(Memory::Read32(buttons), black, white));
}
