#pragma once

#include <leon/rhi/RHIHandles.h>

namespace leon {

/// Color+depth FBO for a horizontal planar mirror pass (typically half-res).
class PlanarReflection {
public:
    PlanarReflection() = default;
    ~PlanarReflection();

    PlanarReflection(const PlanarReflection&) = delete;
    PlanarReflection& operator=(const PlanarReflection&) = delete;

    /// Allocate or resize color/depth to match the viewport.
    [[nodiscard]] bool EnsureSize(int width, int height);
    void Destroy();

    void Begin() const;
    /// Restore draw target to `restoreFbo` (0 = default framebuffer).
    void End(int framebufferWidth, int framebufferHeight,
             rhi::RHIFramebufferId restoreFbo = rhi::kInvalidFramebuffer) const;

    void BindColorTexture(unsigned int unit) const;
    [[nodiscard]] bool Valid() const {
        return fbo_ != rhi::kInvalidFramebuffer && colorTexture_ != rhi::kInvalidTexture;
    }
    [[nodiscard]] int width() const { return width_; }
    [[nodiscard]] int height() const { return height_; }

private:
    rhi::RHIFramebufferId fbo_ = rhi::kInvalidFramebuffer;
    rhi::RHITextureId colorTexture_ = rhi::kInvalidTexture;
    rhi::RHIRenderbufferId depthRbo_ = rhi::kInvalidRenderbuffer;
    int width_ = 0;
    int height_ = 0;
};

} // namespace leon
