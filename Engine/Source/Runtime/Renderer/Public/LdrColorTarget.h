#pragma once

#include "RHIHandles.h"


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
        return fbo_ != kInvalidFramebuffer && colorTexture_ != kInvalidTexture;
    }
    [[nodiscard]] RHIFramebufferId Framebuffer() const { return fbo_; }
    [[nodiscard]] int Width() const { return width_; }
    [[nodiscard]] int Height() const { return height_; }

private:
    RHIFramebufferId fbo_ = kInvalidFramebuffer;
    RHITextureId colorTexture_ = kInvalidTexture;
    int width_ = 0;
    int height_ = 0;
};

