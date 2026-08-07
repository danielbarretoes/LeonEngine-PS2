#pragma once

#include <leon/rhi/RHIHandles.h>

namespace leon {

/// Full-res LDR color target for post (composite → FXAA).
class LdrColorTarget {
public:
    LdrColorTarget() = default;
    ~LdrColorTarget();

    LdrColorTarget(const LdrColorTarget&) = delete;
    LdrColorTarget& operator=(const LdrColorTarget&) = delete;

    [[nodiscard]] bool EnsureSize(int width, int height);
    void Destroy();

    void BindWrite() const;
    void BindColorTexture(unsigned int unit) const;

    [[nodiscard]] bool Valid() const {
        return fbo_ != rhi::kInvalidFramebuffer && colorTexture_ != rhi::kInvalidTexture;
    }
    [[nodiscard]] rhi::RHIFramebufferId Framebuffer() const { return fbo_; }
    [[nodiscard]] int Width() const { return width_; }
    [[nodiscard]] int Height() const { return height_; }

private:
    rhi::RHIFramebufferId fbo_ = rhi::kInvalidFramebuffer;
    rhi::RHITextureId colorTexture_ = rhi::kInvalidTexture;
    int width_ = 0;
    int height_ = 0;
};

} // namespace leon
