#include <leon/core/MemoryStats.h>
#include <leon/rhi/IRHIDevice.h>

#include <cstdint>

#if defined(LEON_PLATFORM_PS2)
#include <cstddef>
// newlib provides sbrk, but <unistd.h> hides it under strict -std=c++17 (no GNU extensions).
extern "C" void* sbrk(std::ptrdiff_t increment);
#endif

namespace leon {
namespace {
// EE: 32 MB main RAM; the kernel owns the first 1 MB and ELFs load at 0x00100000.
constexpr std::uintptr_t kEeUserBase = 0x00100000u;
constexpr std::size_t kEeRamBytes = 32u * 1024u * 1024u;
} // namespace

MemorySnapshot queryMemorySnapshot() {
    MemorySnapshot snap{};
#if defined(LEON_PLATFORM_PS2)
    // Program image + heap high-water (newlib break). Stack at the top of RAM is not counted.
    const auto brk = reinterpret_cast<std::uintptr_t>(sbrk(0));
    snap.processWorkingSetBytes = brk > kEeUserBase ? brk - kEeUserBase : 0;
    snap.processPrivateBytes = snap.processWorkingSetBytes;
    snap.ramBudgetBytes = kEeRamBytes;
#endif
    if (const rhi::IRHIDevice* device = rhi::GetActiveDevice()) {
        const rhi::GpuMemoryInfo info = device->QueryGpuMemory();
        snap.gpuValid = info.valid;
        snap.gpuReportsUsage = info.reportsUsage;
        snap.gpuBudgetBytes = info.budgetBytes;
        snap.gpuUsedBytes = info.usedBytes;
    }
    return snap;
}

} // namespace leon
