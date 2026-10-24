// NO100F 60 fps ([video] fps60): particle emitters keep the game's 30 Hz emission rate.
//
// The per-frame emitter update (0x800C4AAC) calls the emit function (0x800C4B04) with the frame time. Emitters with an
// emit interval of 0 (torches, candles) emit their count every call, so at 60 fps they made twice the particles. That
// fills the shared 2000-particle pool, and the allocator (0x80032728) then takes the newest particles of a
// lower-priority system: in "Mind your Manors" the floor fog lost two puffs a frame and refilled them at random spots,
// which flickered. Here each emitter collects the frame time and emits once it reaches about one 30 Hz frame, with
// the collected time, so emitters run exactly as at 30 fps while particles still move every frame.
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

extern "C" void No100f_UpdateParEmitter(CpuContext* ctx) {
    const uint32_t emitter = ctx->gpr[3];
    double dt = ctx->fpr[1].d;
    func_800AD3B4(ctx);
    if (ctx->gpr[3] != 0 || (Memory::Read8(emitter + 0x18) & 1) == 0) return;
    if (static const bool fps60 = RuntimeConfigFile::Fps60(); fps60) {
        // Three quarters of a 30 Hz frame: two 60 fps frames always reach it, a 30 fps frame (slowdown) does alone.
        static std::unordered_map<uint32_t, double> pending;
        double& sum = pending[emitter];
        sum += dt;
        if (sum < 0.75 / 30.0) return;
        dt = sum;
        sum = 0.0;
    }
    ctx->gpr[3] = emitter;
    ctx->fpr[1].d = dt;
    func_800C4B04(ctx);
}
PPC_NATIVE_OVERRIDE_VOID(800C4AAC, No100f_UpdateParEmitter, (CpuContext* ctx), (ctx));
