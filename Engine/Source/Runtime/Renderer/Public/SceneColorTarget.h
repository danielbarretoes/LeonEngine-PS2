#pragma once

#include "RHIHandles.h"

namespace leon {

/// HDR scene color + sampleable depth for post-process (SSAO / tonemap).
class SceneColorTarget {
public:
    SceneColorTarget() = default;
    ~SceneColorTarget();

    SceneColorTarget(const SceneColorTarget&) = delete;
    SceneColorTarget& operator=(const SceneColorTarget&) = delete;

    /// Allocate or resize color (RGB16F) + depth texture.
    [[nodiscard]] bool EnsureSize(int width, int height);
    void Destroy();

    void Begin() const;
    /// Restore draw target to `restoreFbo` (0 = default framebuffer).
    void End(int framebufferWidth, int framebufferHeight, rhi::RHIFramebufferId restoreFbo = rhi::kInvalidFramebuffer) const;

    void BindColorTexture(unsigned int unit) const;
    void BindDepthTexture(unsigned int unit) const;

    [[nodiscard]] bool Valid() const { return fbo_ != 0 && colorTexture_ != 0 && depthTexture_ != 0; }
    [[nodiscard]] rhi::RHIFramebufferId Framebuffer() const { return fbo_; }
    [[nodiscard]] int Width() const { return width_; }
    [[nodiscard]] int Height() const { return height_; }

private:
    rhi::RHIFramebufferId fbo_ = rhi::kInvalidFramebuffer;
    rhi::RHITextureId colorTexture_ = rhi::kInvalidTexture;
    rhi::RHITextureId depthTexture_ = rhi::kInvalidTexture;
    int width_ = 0;
    int height_ = 0;
};

} // namespace leon
