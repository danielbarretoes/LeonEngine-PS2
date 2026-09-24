#pragma once

#include <cstddef>
#include <memory>

namespace leon::rhi {

struct GpuMemoryInfo {
    bool valid = false;
    bool reportsUsage = false;
    std::size_t budgetBytes = 0;
    std::size_t usedBytes = 0;
};

/// Graphics API device contract. Implementations live under Plugins/RHI/*.
/// Engine Platform/Utilities talk only to this interface (not glad).
class IRHIDevice {
public:
    virtual ~IRHIDevice() = default;

    /// Load API entry points after a window context exists.
    /// `loader` is typically glfwGetProcAddress (cast to void*).
    [[nodiscard]] virtual bool LoadProcedures(void* (*loader)(const char*)) = 0;

    virtual void SetViewport(int x, int y, int width, int height) = 0;

    [[nodiscard]] virtual GpuMemoryInfo QueryGpuMemory() const = 0;

    [[nodiscard]] virtual const char* GetName() const = 0;
    [[nodiscard]] virtual const char* GetApiVersionString() const = 0;
};

/// Create the compile-time selected RHI (default: OpenGL plugin).
[[nodiscard]] std::unique_ptr<IRHIDevice> CreateDefaultRHIDevice();

/// Active device for the current GL/window context (set by Platform::Window).
void SetActiveDevice(IRHIDevice* device);
[[nodiscard]] IRHIDevice* GetActiveDevice();

} // namespace leon::rhi
