#include "hle/no100f_xbox_layout.h"
#include "hle/no100f_warp_controls.h"
#include <cassert>
#include <cstdint>
#include <string_view>

struct CpuContext { uint32_t gpr[32]{}; };
#include "hle/no100f_xbox_pad_call.h"
static unsigned filtered = 0;
void No100f_ApplyXboxPadActions(CpuContext* ctx, uint32_t pad, uint32_t buttons) {
    assert(ctx->gpr[3] == 1);
    assert(pad == 0x80010000);
    assert(buttons == 0x80020000);
    ++filtered;
}

int main() {
    for (uint32_t mode = 0; mode <= 12; ++mode) {
        const bool menu = No100fWarpControls::UsesMenuFaces(mode);
        assert(menu == (mode == 2 || mode == 4 || mode == 7));
        No100fWarpControls::State state;
        state.Filter(0, true, menu, false, 1);
        assert(state.Filter(0x0400, true, menu, false, 1) == (menu ? 0x0200 : 0x0400));
        assert(state.Filter(0x0200, true, menu, false, 1) == (menu ? 0x0400 : 0x0200));
        assert(state.Filter(0x0100, true, menu, false, 1) == 0x0100);
        assert(state.Filter(0x0400, false, menu, false, 1) == 0x0400);
    }
    using No100fWarpControls::SwapFaces;
    for (unsigned value = 0; value <= 0xffff; ++value) {
        assert(SwapFaces(SwapFaces(value)) == value);
        assert((SwapFaces(value) & ~0x0600) == (value & ~0x0600));
    }
    No100fWarpControls::State warp;
    assert(warp.Filter(0x0400, true, false, false, 1) == 0x0400);
    assert(warp.Filter(0x0400, true, true, false, 1) == 0);
    assert(warp.Filter(0, true, true, false, 1) == 0);
    assert(warp.Filter(0x0400, true, true, false, 1) == 0x0200);
    assert(warp.Filter(0x0400, true, false, false, 1) == 0);
    assert(warp.Filter(0, true, false, false, 1) == 0);
    assert(warp.Filter(0x0200, true, false, false, 1) == 0x0200);
    assert(warp.Filter(0x0200, false, true, false, 1) == 0x0200);
    assert(warp.Filter(0x0400, true, true, false, 2) == 0);
    assert(warp.Filter(0, true, true, false, 2) == 0);
    assert(warp.Filter(0x0200, true, true, true, 2) == 0);
    assert(warp.Filter(0x0200, true, true, false, 2) == 0);
    assert(warp.Filter(0, true, true, false, 2) == 0);
    assert(warp.Filter(0x0200, true, true, false, 2) == 0x0400);
    using namespace No100fXboxLayout;
    assert(std::string_view(kBindings[0]) == "south");
    assert(std::string_view(kBindings[1]) == "west");
    assert(std::string_view(kBindings[2]) == "east");
    assert(std::string_view(kBindings[3]) == "north");
    assert(std::string_view(kBindings[5]) == "disabled");
    for (uint32_t combo = 0; combo < 16; ++combo) {
        const uint32_t primary = ((combo & 1) ? 0x100u : 0u) | ((combo & 2) ? 0x1000u : 0u);
        const auto output = AddActions(primary, combo & 4, combo & 8);
        assert((output & 0x1100u) == primary);
        assert(((output & 0x102000u) != 0) == bool(combo & 4));
        assert(((output & 0x200u) != 0) == bool(combo & 8));
    }
    // Press/release calculation is left to xPadUpdate, after this filter.
    const uint32_t held = AddActions(0, true, true);
    assert((held & ~AddActions(0, false, false)) == held);
    const uint32_t otherInputs = 0xF00F1u;
    assert((AddActions(otherInputs, true, true) & otherInputs) == otherInputs);
    assert(AddActions(otherInputs, false, false) == otherInputs);
    bool suppressed = false;
    assert(ReadAction(true, false, suppressed));
    assert(!ReadAction(true, true, suppressed));
    assert(suppressed);
    assert(!ReadAction(true, false, suppressed));
    assert(!ReadAction(false, false, suppressed));
    assert(!suppressed);
    assert(ReadAction(true, false, suppressed));
    for (unsigned value = 0; value <= 255; ++value)
        assert(Trigger(static_cast<uint8_t>(value)) == (value > 128 ? value : 0));
    CpuContext ctx;
    {
        No100fXboxPadCall unrelated(0x80001234, &ctx);
    }
    assert(filtered == 0);
    ctx.gpr[3] = 0x80010000;
    ctx.gpr[4] = 0x80020000;
    {
        No100fXboxPadCall call(0x8000F5E8, &ctx);
        ctx.gpr[3] = 1;
        ctx.gpr[4] = 0;
    }
    assert(filtered == 1);
}
