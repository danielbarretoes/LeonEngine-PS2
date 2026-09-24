#pragma once

#include <cstddef>

namespace leon {

/// Process RAM + approximate GPU VRAM for the HUD (queried a few times per second).
struct MemorySnapshot {
    std::size_t processWorkingSetBytes = 0; // physical RAM resident
    std::size_t processPrivateBytes = 0;    // private commit (approx. process heap/maps)
    std::size_t ramBudgetBytes = 0;         // total RAM for the process (0 = unknown; PS2: EE)

    bool gpuValid = false;
    bool gpuReportsUsage = false; // false → only free/budget known (e.g. ATI_meminfo)
    std::size_t gpuUsedBytes = 0;
    std::size_t gpuBudgetBytes = 0;
};

/// Sample current process + GPU memory. Safe to call from the render thread with a GL context.
[[nodiscard]] MemorySnapshot queryMemorySnapshot();

} // namespace leon
