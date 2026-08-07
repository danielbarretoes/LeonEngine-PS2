#include <leon/core/MemoryStats.h>
#include <leon/rhi/IRHIDevice.h>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <dxgi1_4.h>
#include <psapi.h>
#include <Windows.h>
#else
#include <fstream>
#include <unistd.h>
#endif

namespace leon {
namespace {

#ifdef _WIN32

void queryProcessRam(MemorySnapshot& out) {
    PROCESS_MEMORY_COUNTERS_EX counters{};
    counters.cb = sizeof(counters);
    if (!GetProcessMemoryInfo(GetCurrentProcess(),
                              reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
                              sizeof(counters))) {
        return;
    }
    out.processWorkingSetBytes = static_cast<std::size_t>(counters.WorkingSetSize);
    out.processPrivateBytes = static_cast<std::size_t>(counters.PrivateUsage);
}

bool queryGpuDxgi(MemorySnapshot& out) {
    IDXGIFactory4* factory = nullptr;
    if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory4), reinterpret_cast<void**>(&factory))) ||
        factory == nullptr) {
        return false;
    }

    IDXGIAdapter1* adapter1 = nullptr;
    if (FAILED(factory->EnumAdapters1(0, &adapter1)) || adapter1 == nullptr) {
        factory->Release();
        return false;
    }

    IDXGIAdapter3* adapter3 = nullptr;
    const HRESULT qi =
        adapter1->QueryInterface(__uuidof(IDXGIAdapter3), reinterpret_cast<void**>(&adapter3));
    adapter1->Release();
    factory->Release();
    if (FAILED(qi) || adapter3 == nullptr) {
        return false;
    }

    DXGI_QUERY_VIDEO_MEMORY_INFO info{};
    const HRESULT q = adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &info);
    adapter3->Release();
    if (FAILED(q)) {
        return false;
    }

    out.gpuValid = true;
    out.gpuUsedBytes = static_cast<std::size_t>(info.CurrentUsage);
    out.gpuBudgetBytes = static_cast<std::size_t>(info.Budget);
    out.gpuReportsUsage = true;
    return true;
}

#else

void queryProcessRam(MemorySnapshot& out) {
    std::ifstream statm("/proc/self/statm");
    std::size_t sizePages = 0;
    std::size_t residentPages = 0;
    if (!(statm >> sizePages >> residentPages)) {
        return;
    }
    const long pageSize = sysconf(_SC_PAGESIZE);
    if (pageSize <= 0) {
        return;
    }
    const auto page = static_cast<std::size_t>(pageSize);
    out.processWorkingSetBytes = residentPages * page;
    out.processPrivateBytes = sizePages * page;
}

#endif

bool queryGpuViaRhi(MemorySnapshot& out) {
    rhi::IRHIDevice* device = rhi::GetActiveDevice();
    if (device == nullptr) {
        return false;
    }
    const rhi::GpuMemoryInfo info = device->QueryGpuMemory();
    if (!info.valid) {
        return false;
    }
    out.gpuValid = true;
    out.gpuReportsUsage = info.reportsUsage;
    out.gpuBudgetBytes = info.budgetBytes;
    out.gpuUsedBytes = info.usedBytes;
    return true;
}

} // namespace

MemorySnapshot queryMemorySnapshot() {
    MemorySnapshot snap{};

#ifdef _WIN32
    queryProcessRam(snap);
    if (!queryGpuDxgi(snap)) {
        (void)queryGpuViaRhi(snap);
    }
#else
    queryProcessRam(snap);
    (void)queryGpuViaRhi(snap);
#endif

    return snap;
}

} // namespace leon
