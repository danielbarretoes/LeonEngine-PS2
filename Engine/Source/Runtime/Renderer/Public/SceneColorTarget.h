#pragma once

#include "RHIHandles.h"


/// HDR scene color + sampleable depth for post-process (SSAO / tonemap).
class FSceneColorTarget {
public:
    FSceneColorTarget() = default;
    ~FSceneColorTarget();

    FSceneColorTarget(const FSceneColorTarget&) = delete;
    FSceneColorTarget& operator=(const FSceneColorTarget&) = delete;

    /// Allocate or resize color (RGB16F) + depth texture.
    [[nodiscard]] bool EnsureSize(int width, int height);
    void Destroy();

    void Begin() const;
    /// Restore draw target to `restoreFbo` (0 = default framebuffer).
    void End(int framebufferWidth, int framebufferHeight, FRHIFramebufferId restoreFbo = kInvalidFramebuffer) const;

    void BindColorTexture(unsigned int unit) const;
    void BindDepthTexture(unsigned int unit) const;

    [[nodiscard]] bool Valid() const { return fbo_ != 0 && colorTexture_ != 0 && depthTexture_ != 0; }
    [[nodiscard]] FRHIFramebufferId Framebuffer() const { return fbo_; }
    [[nodiscard]] int Width() const { return width_; }
    [[nodiscard]] int Height() const { return height_; }

private:
    FRHIFramebufferId fbo_ = kInvalidFramebuffer;
    FRHITextureId colorTexture_ = kInvalidTexture;
    FRHITextureId depthTexture_ = kInvalidTexture;
    int width_ = 0;
    int height_ = 0;
};

