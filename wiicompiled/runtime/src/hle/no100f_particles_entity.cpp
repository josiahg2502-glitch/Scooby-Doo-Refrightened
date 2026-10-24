// NO100F 60 fps ([video] fps60): particles that entity code emits keep the game's 30 Hz emission rate and speed.
//
// Entity code (about 90 call sites) emits through "emit with overrides" (0x800C5C0C): it applies the overrides the
// caller passes (count, interval, position, velocity, ...) to the emitter asset, calls the emit function (0x800C4B04)
// and, if asked, restores the asset. With fps60 the main loop runs twice per 30 Hz frame, which broke this two ways:
// - Emission while the emitter timer is 0 (interval 0, or the "emit now" override) happens on every call, so
//   continuous effects (candles, van exhaust, firework trails) made twice the particles.
// - 36 call sites pass a fixed 1/30 s instead of the frame time. Emit sets a particle's velocity to the asset
//   velocity times that time and the particle moves by its velocity every frame, so at 60 fps those particles moved
//   twice as fast (and interval timers ran twice as fast).
// Here, with fps60, a fixed 1/30 s becomes the frame time, and an emitter that emits on every call emits at most once
// per 30 Hz step: not in the frame right after it emitted, unless that frame was a full 30 Hz step (slowdown). Calls in
// the same frame all emit, as at 30 fps. Without fps60 this is the original code.
//
// Native in full: an override's address is excluded from translation, so the original body cannot be called.
#include <cstdint>
#include <unordered_map>
#include "abi_bridge.h"
#include "memory.h"
#include "hle_stubs.h"
#include "ppc_runtime.h"
#include "runtime_config.h"

extern "C" void func_800C4B04(CpuContext* ctx);  // the game's emit(emitter, dt), returns r3
extern "C" void func_800CD950(CpuContext* ctx);  // looks up the emitter's +0x44 from asset +0x2C

namespace {
constexpr uint32_t kSave = 0x802486A0u;        // the game's static save area for restoring the asset
constexpr uint32_t kFrameTime = 0x80270D84u;   // [r13 - 0x74FC]: measured frame time, clamped to 1/60..0.1 s
constexpr uint32_t kFrameCount = 0x80270890u;  // [r13 - 0x79F0]: main loop frame counter
constexpr uint32_t kZero = 0x80273A10u;        // [r2 - 0x5D70]: 0.0f, the emitter timer value for "emit now"

void Copy8(uint32_t dst, uint32_t src) { Memory::Write8(dst, Memory::Read8(src)); }
void Copy32(uint32_t dst, uint32_t src) { Memory::Write32(dst, Memory::Read32(src)); }

// emitter +0x44 = asset +0x2C ? fn_800CD950(asset +0x2C) : 0
void LookUp44(CpuContext* ctx, uint32_t emitter, uint32_t asset) {
    const uint32_t id = Memory::Read32(asset + 0x2C);
    uint32_t result = 0;
    if (id != 0) {
        ctx->gpr[3] = id;
        func_800CD950(ctx);
        result = ctx->gpr[3];
    }
    Memory::Write32(emitter + 0x44, result);
}

// With fps60: false if this every-call emission would be the second in one 30 Hz step.
bool EmitThisFrame(uint32_t emitter) {
    static std::unordered_map<uint32_t, uint32_t> lastFrame;
    const uint32_t frame = Memory::Read32(kFrameCount);
    auto [it, fresh] = lastFrame.try_emplace(emitter, frame);
    if (fresh || it->second == frame) return true;
    if (frame - it->second == 1 && Memory::ReadFloat32(kFrameTime) < 0.75f / 30.0f) return false;
    it->second = frame;
    return true;
}
}  // namespace

extern "C" void No100f_EmitWithOverrides(CpuContext* ctx) {
    const uint32_t emitter = ctx->gpr[3];
    const uint32_t ov = ctx->gpr[4];
    double dt = ctx->fpr[1].d;
    const uint32_t flags = Memory::Read32(ov);
    const bool restore = (flags & 0x4000) != 0;
    const uint32_t asset = Memory::Read32(emitter + 0x10);

    if (restore) {
        Copy8(kSave + 0x4, asset + 0xA);
        Copy8(kSave + 0x5, asset + 0xB);
        Copy32(kSave + 0x8, asset + 0xC);
        Copy32(kSave + 0xC, asset + 0x2C);
        for (uint32_t i = 0; i < 7; ++i) Copy32(kSave + 0x10 + i * 4, asset + 0x34 + i * 4);
        for (uint32_t i = 0; i < 4; ++i) {
            Copy8(kSave + 0x2C + i, asset + 0x50 + i);
            Copy8(kSave + 0x30 + i, asset + 0x54 + i);
        }
        for (uint32_t i = 0; i < 5; ++i) Copy32(kSave + 0x34 + i * 4, asset + 0x58 + i * 4);
        Copy32(kSave + 0x50, emitter + 0x1C);
        for (uint32_t i = 0; i < 3; ++i) Copy8(kSave + 0x48 + i, emitter + 0x20 + i);
        Copy32(kSave + 0x4C, asset + 0x10);
    }

    if (flags & 0x4) Copy8(asset + 0xA, ov + 0x4);
    if (flags & 0x8) Copy8(asset + 0xB, ov + 0x5);
    if (flags & 0x10) Copy32(asset + 0xC, ov + 0x8);
    if (flags & 0x800) Copy32(emitter + 0x1C, kZero);
    if (flags & 0x8000) {
        Copy32(asset + 0x2C, ov + 0xC);
        LookUp44(ctx, emitter, asset);
    }
    if (flags & 0x20)
        for (uint32_t i = 0; i < 3; ++i) Copy32(asset + 0x34 + i * 4, ov + 0x10 + i * 4);
    if (flags & 0x10000)
        for (uint32_t i = 0; i < 3; ++i) Copy8(emitter + 0x20 + i, ov + 0x48 + i);
    if (flags & 0x20000) Copy32(asset + 0x10, ov + 0x4C);
    if (flags & 0x40)
        for (uint32_t i = 0; i < 3; ++i) Copy32(asset + 0x40 + i * 4, ov + 0x1C + i * 4);
    if (flags & 0x2) Copy32(asset + 0x4C, ov + 0x28);
    if (flags & 0x80)
        for (uint32_t i = 0; i < 4; ++i) Copy8(asset + 0x50 + i, ov + 0x2C + i);
    if (flags & 0x100)
        for (uint32_t i = 0; i < 4; ++i) Copy8(asset + 0x54 + i, ov + 0x30 + i);
    if (flags & 0x200) Copy32(asset + 0x58, ov + 0x34);
    if (flags & 0x2000) Copy32(asset + 0x5C, ov + 0x38);
    if (flags & 0x400) Copy32(asset + 0x60, ov + 0x3C);
    if (flags & 0x1) Copy32(asset + 0x64, ov + 0x40);
    if (flags & 0x1000) Copy32(asset + 0x68, ov + 0x44);

    bool emit = true;
    if (static const bool fps60 = RuntimeConfigFile::Fps60(); fps60) {
        if (static_cast<float>(dt) == 1.0f / 30.0f) dt = Memory::ReadFloat32(kFrameTime);
        // Emit only counts the timer (+0x1C) down while it is above 0; at 0 it emits on every call.
        if (!(Memory::ReadFloat32(emitter + 0x1C) > 0.0f)) emit = EmitThisFrame(emitter);
    }
    uint32_t result = 0;
    if (emit) {
        ctx->gpr[3] = emitter;
        ctx->fpr[1].d = dt;
        func_800C4B04(ctx);
        result = ctx->gpr[3];
    }

    if (restore) {
        Copy8(asset + 0xA, kSave + 0x4);
        Copy8(asset + 0xB, kSave + 0x5);
        Copy32(asset + 0xC, kSave + 0x8);
        Copy32(asset + 0x2C, kSave + 0xC);
        LookUp44(ctx, emitter, asset);
        for (uint32_t i = 0; i < 7; ++i) Copy32(asset + 0x34 + i * 4, kSave + 0x10 + i * 4);
        for (uint32_t i = 0; i < 4; ++i) {
            Copy8(asset + 0x50 + i, kSave + 0x2C + i);
            Copy8(asset + 0x54 + i, kSave + 0x30 + i);
        }
        for (uint32_t i = 0; i < 5; ++i) Copy32(asset + 0x58 + i * 4, kSave + 0x34 + i * 4);
        Copy32(emitter + 0x1C, kSave + 0x50);
        for (uint32_t i = 0; i < 3; ++i) Copy8(emitter + 0x20 + i, kSave + 0x48 + i);
        Copy32(asset + 0x10, kSave + 0x4C);
    }
    ctx->gpr[3] = result;
}
PPC_NATIVE_OVERRIDE_VOID(800C5C0C, No100f_EmitWithOverrides, (CpuContext* ctx), (ctx));
