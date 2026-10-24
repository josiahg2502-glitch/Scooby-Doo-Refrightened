// NO100F 60 fps ([video] fps60): particle emitters keep the game's 30 Hz emission rate.
//
// The per-frame emitter update (0x800C4AAC) calls the emit function (0x800C4B04) with the frame time. Emitters with an
// emit interval of 0 (torches, candles) emit their count every call, so at 60 fps they made twice the particles. That
// fills the shared 2000-particle pool, and the allocator (0x80032728) then takes the newest particles of a
// lower-priority system: in "Mind your Manors" the floor fog lost two puffs a frame and refilled them at random spots,
// which flickered. Here an emitter that emits on every call (its timer +0x1C is 0) emits at most once per 30 Hz step:
// not in the frame right after it emitted, unless that frame was a full 30 Hz step (slowdown). It still gets this
// frame's time: emit sets a particle's velocity to the asset velocity times that time, and particles move by their
// velocity every frame. Emitters with a running timer count it down with the frame time, as before.
//
// Native in full: an override's address is excluded from translation, so the original body cannot be called.
// It is: if the game is not paused (0x800AD3B4 returns 0) and the emitter is enabled (+0x18 bit 0), emit(emitter, dt).
#include <cstdint>
#include <unordered_map>
#include "abi_bridge.h"
#include "memory.h"
#include "hle_stubs.h"
#include "ppc_runtime.h"
#include "runtime_config.h"

extern "C" void func_800AD3B4(CpuContext* ctx);  // nonzero while the game is paused
extern "C" void func_800C4B04(CpuContext* ctx);  // the game's emit(emitter, dt)

namespace {
constexpr uint32_t kFrameTime = 0x80270D84u;   // [r13 - 0x74FC]: measured frame time, clamped to 1/60..0.1 s
constexpr uint32_t kFrameCount = 0x80270890u;  // [r13 - 0x79F0]: main loop frame counter
}  // namespace

extern "C" void No100f_UpdateParEmitter(CpuContext* ctx) {
    const uint32_t emitter = ctx->gpr[3];
    const double dt = ctx->fpr[1].d;
    func_800AD3B4(ctx);
    if (ctx->gpr[3] != 0 || (Memory::Read8(emitter + 0x18) & 1) == 0) return;
    if (static const bool fps60 = RuntimeConfigFile::Fps60(); fps60 && !(Memory::ReadFloat32(emitter + 0x1C) > 0.0f)) {
        static std::unordered_map<uint32_t, uint32_t> lastFrame;
        const uint32_t frame = Memory::Read32(kFrameCount);
        auto [it, fresh] = lastFrame.try_emplace(emitter, frame);
        if (!fresh && it->second != frame) {
            if (frame - it->second == 1 && Memory::ReadFloat32(kFrameTime) < 0.75f / 30.0f) return;
            it->second = frame;
        }
    }
    ctx->gpr[3] = emitter;
    ctx->fpr[1].d = dt;
    func_800C4B04(ctx);
}
PPC_NATIVE_OVERRIDE_VOID(800C4AAC, No100f_UpdateParEmitter, (CpuContext* ctx), (ctx));
