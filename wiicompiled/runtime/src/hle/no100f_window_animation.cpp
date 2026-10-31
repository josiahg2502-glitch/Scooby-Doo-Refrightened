#include "abi_bridge.h"
#include "hle/no100f_window_contract.h"
#include "hle_stubs.h"
#include "memory.h"
#include "runtime_log.h"

#include <cstdint>
#include <cstring>

extern "C" void func_8003FF84(CpuContext* ctx); // xSTFindAsset
extern "C" void func_8002FDB0(CpuContext* ctx); // xMemAlloc(heap, size)

namespace {

No100fWindow::Asset FindAsset(CpuContext* ctx, uint32_t id) {
    if (ctx->gpr[1] < 0x20 || !Memory::Contains(ctx->gpr[1] - 0x20, 0x20)) {
        RT_LOG(RT_TAG_RUNTIME) << "[window-creeper] invalid guest stack for asset lookup\n";
        return {};
    }
    CpuContext call = *ctx;
    call.gpr[1] -= 0x20;
    const uint32_t size = call.gpr[1] + 8;
    Memory::Write32(size, 0);
    call.gpr[3] = id;
    call.gpr[4] = size;
    func_8003FF84(&call);
    return {call.gpr[3], Memory::Read32(size)};
}

uint32_t SceneWorld() {
    const uint32_t scene = Memory::Read32(0x80245CF4);
    if (!scene || !Memory::Contains(scene, 0x44)) return 0;
    const uint32_t env = Memory::Read32(scene + 0x40);
    if (!env || !Memory::Contains(env, 4)) return 0;
    const uint32_t geometry = Memory::Read32(env);
    if (!geometry || !Memory::Contains(geometry, 4)) return 0;
    return Memory::Read32(geometry);
}

} // namespace

// GIHE78 rev 1 zSurfaceRegisterMapper. Its single mapper slot is cleared by the game's zSurfaceExit.
extern "C" void No100f_RegisterWindowMapper(CpuContext* ctx) {
    const uint32_t countAddress = ctx->gpr[13] - 29436;
    const uint32_t mapperAddress = ctx->gpr[13] - 32296;
    const uint32_t id = ctx->gpr[3];
    const int32_t count = static_cast<int32_t>(Memory::Read32(countAddress));
    if (count >= 1 || !id) return;
    if (count < 0) {
        RT_LOG(RT_TAG_RUNTIME) << "[window-creeper] invalid mapper registration count\n";
        return;
    }

    auto mapper = FindAsset(ctx, id);
    if (id == No100fWindow::kMapperId) {
        const auto match = No100fWindow::Inspect(id, mapper, SceneWorld(),
            [](uint32_t address, std::size_t size) { return Memory::Contains(address, size); },
            [](uint32_t address) { return Memory::Read8(address); },
            [](uint32_t address) { return Memory::Read16(address); },
            [](uint32_t address) { return Memory::Read32(address); },
            [&](uint32_t asset) { return FindAsset(ctx, asset); });
        if (match == No100fWindow::Match::Restore) {
            CpuContext call = *ctx;
            call.gpr[3] = Memory::Read32(ctx->gpr[13] - 31024);
            call.gpr[4] = 16;
            func_8002FDB0(&call);
            const uint32_t replacement = call.gpr[3];
            if (replacement && Memory::Contains(replacement, 16)) {
                const auto encoded = No100fWindow::EncodeMapper();
                std::memcpy(Memory::GetPointer(replacement, encoded.size()), encoded.data(), encoded.size());
                mapper.address = replacement;
                RT_LOG(RT_TAG_RUNTIME) << "[window-creeper] restored h001 material 71 using the retained 18-frame animation\n";
            } else {
                RT_LOG(RT_TAG_RUNTIME) << "[window-creeper] cannot allocate scene mapper; restoration not applied\n";
            }
        } else if (match == No100fWindow::Match::Incompatible) {
            RT_LOG(RT_TAG_RUNTIME) << "[window-creeper] incompatible courtyard assets; restoration not applied\n";
        }
    }
    Memory::Write32(mapperAddress, mapper.address);
    if (mapper.address) Memory::Write32(countAddress, 1);
    ctx->gpr[3] = mapper.address;
}
PPC_NATIVE_OVERRIDE_VOID(800D2D28, No100f_RegisterWindowMapper, (CpuContext* ctx), (ctx));
