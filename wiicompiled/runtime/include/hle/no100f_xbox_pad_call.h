#pragma once

#include <cstdint>

void No100f_ApplyXboxPadActions(CpuContext* ctx, uint32_t pad, uint32_t buttons);

// Static translated calls bypass the dispatcher. Keep the original iPadUpdate
// intact and apply the same result filter at both kinds of call boundary.
class No100fXboxPadCall {
public:
    No100fXboxPadCall(uint32_t target, CpuContext* ctx)
        : ctx_(ctx), pad_(target == 0x8000F5E8u ? ctx->gpr[3] : 0),
          buttons_(target == 0x8000F5E8u ? ctx->gpr[4] : 0) {}
    ~No100fXboxPadCall() {
        if (pad_) No100f_ApplyXboxPadActions(ctx_, pad_, buttons_);
    }
private:
    CpuContext* ctx_;
    uint32_t pad_;
    uint32_t buttons_;
};
