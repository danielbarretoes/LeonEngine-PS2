#pragma once

#include "RHIHandles.h"


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
             RHIFramebufferId restoreFbo = kInvalidFramebuffer) const;

    void BindColorTexture(unsigned int unit) const;
    [[nodiscard]] bool Valid() const {
        return fbo_ != kInvalidFramebuffer && colorTexture_ != kInvalidTexture;
    }
    [[nodiscard]] int width() const { return width_; }
    [[nodiscard]] int height() const { return height_; }

private:
    RHIFramebufferId fbo_ = kInvalidFramebuffer;
    RHITextureId colorTexture_ = kInvalidTexture;
    RHIRenderbufferId depthRbo_ = kInvalidRenderbuffer;
    int width_ = 0;
    int height_ = 0;
};

