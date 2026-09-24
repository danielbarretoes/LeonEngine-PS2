#pragma once

#include "RHIHandles.h"


/// Color+depth FBO for a horizontal planar mirror pass (typically half-res).
class FPlanarReflection {
public:
    FPlanarReflection() = default;
    ~FPlanarReflection();

    FPlanarReflection(const FPlanarReflection&) = delete;
    FPlanarReflection& operator=(const FPlanarReflection&) = delete;

    /// Allocate or resize color/depth to match the viewport.
    [[nodiscard]] bool EnsureSize(int width, int height);
    void Destroy();

    void Begin() const;
    /// Restore draw target to `restoreFbo` (0 = default framebuffer).
    void End(int framebufferWidth, int framebufferHeight,
             FRHIFramebufferId restoreFbo = kInvalidFramebuffer) const;

    void BindColorTexture(unsigned int unit) const;
    [[nodiscard]] bool Valid() const {
        return fbo_ != kInvalidFramebuffer && colorTexture_ != kInvalidTexture;
    }
    [[nodiscard]] int width() const { return width_; }
    [[nodiscard]] int height() const { return height_; }

private:
    FRHIFramebufferId fbo_ = kInvalidFramebuffer;
    FRHITextureId colorTexture_ = kInvalidTexture;
    FRHIRenderbufferId depthRbo_ = kInvalidRenderbuffer;
    int width_ = 0;
    int height_ = 0;
};

