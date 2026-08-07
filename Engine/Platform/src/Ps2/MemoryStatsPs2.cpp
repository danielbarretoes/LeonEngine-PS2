#include <leon/core/MemoryStats.h>
#include <leon/rhi/IRHIDevice.h>

namespace leon {

MemorySnapshot queryMemorySnapshot() {
    MemorySnapshot snap{};
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
