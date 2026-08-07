#include <glad/glad.h>
#include <leon/rhi/IRHIDevice.h>

#include <array>
#include <iostream>
#include <string>

namespace leon::rhi {
namespace {

class OpenGLDevice final : public IRHIDevice {
public:
    bool LoadProcedures(void* (*loader)(const char*)) override {
        if (loader == nullptr) {
            return false;
        }
        if (gladLoadGLLoader(reinterpret_cast<GLADloadproc>(loader)) == 0) {
            std::cerr << "Failed to initialize GLAD (OpenGL RHI)\n";
            return false;
        }
        const char* ver = reinterpret_cast<const char*>(glGetString(GL_VERSION));
        version_ = ver != nullptr ? ver : "unknown";
        std::cout << "OpenGL " << version_ << '\n';
        return true;
    }

    void SetViewport(int x, int y, int width, int height) override {
        glViewport(x, y, width, height);
    }

    [[nodiscard]] GpuMemoryInfo QueryGpuMemory() const override {
        GpuMemoryInfo out{};
        if (GLAD_GL_NVX_gpu_memory_info != 0) {
            GLint totalKb = 0;
            GLint availableKb = 0;
            glGetIntegerv(GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX, &totalKb);
            glGetIntegerv(GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX, &availableKb);
            if (totalKb > 0 && availableKb >= 0) {
                out.valid = true;
                out.reportsUsage = true;
                out.budgetBytes = static_cast<std::size_t>(totalKb) * 1024u;
                const GLint usedKb = totalKb > availableKb ? (totalKb - availableKb) : 0;
                out.usedBytes = static_cast<std::size_t>(usedKb) * 1024u;
                return out;
            }
        }
        if (GLAD_GL_ATI_meminfo != 0) {
            std::array<GLint, 4> texFree{};
            glGetIntegerv(GL_TEXTURE_FREE_MEMORY_ATI, texFree.data());
            if (texFree[0] > 0) {
                out.valid = true;
                out.reportsUsage = false;
                out.budgetBytes = static_cast<std::size_t>(texFree[0]) * 1024u;
                out.usedBytes = 0;
                return out;
            }
        }
        return out;
    }

    [[nodiscard]] const char* GetName() const override { return "OpenGL"; }

    [[nodiscard]] const char* GetApiVersionString() const override { return version_.c_str(); }

private:
    std::string version_;
};

[[nodiscard]] IRHIDevice*& ActiveDeviceSlot() {
    static IRHIDevice* active = nullptr;
    return active;
}

} // namespace

std::unique_ptr<IRHIDevice> CreateDefaultRHIDevice() {
    return std::make_unique<OpenGLDevice>();
}

void SetActiveDevice(IRHIDevice* device) {
    ActiveDeviceSlot() = device;
}

IRHIDevice* GetActiveDevice() {
    return ActiveDeviceSlot();
}

} // namespace leon::rhi
