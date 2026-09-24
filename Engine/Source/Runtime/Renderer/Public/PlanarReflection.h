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
    [[nodiscard]] bool EnsureSize(int InWidth, int InHeight);
    void Destroy();

    void Begin() const;
    /// Restore draw target to `restoreFbo` (0 = default framebuffer).
    void End(int FramebufferWidth, int FramebufferHeight,
             FRHIFramebufferId RestoreFbo = InvalidFramebuffer) const;

    void BindColorTexture(unsigned int Unit) const;
    [[nodiscard]] bool Valid() const {
        return Fbo != InvalidFramebuffer && ColorTexture != InvalidTexture;
    }
    [[nodiscard]] int GetWidth() const { return Width; }
    [[nodiscard]] int GetHeight() const { return Height; }

private:
    FRHIFramebufferId Fbo = InvalidFramebuffer;
    FRHITextureId ColorTexture = InvalidTexture;
    FRHIRenderbufferId DepthRbo = InvalidRenderbuffer;
    int Width = 0;
    int Height = 0;
};

