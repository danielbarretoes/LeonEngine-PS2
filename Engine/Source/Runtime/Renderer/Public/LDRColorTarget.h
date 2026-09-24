#pragma once

#include "RHIHandles.h"


/// Full-res LDR color target for post (composite → FXAA).
class FLDRColorTarget {
public:
    FLDRColorTarget() = default;
    ~FLDRColorTarget();

    FLDRColorTarget(const FLDRColorTarget&) = delete;
    FLDRColorTarget& operator=(const FLDRColorTarget&) = delete;

    [[nodiscard]] bool EnsureSize(int width, int height);
    void Destroy();

    void BindWrite() const;
    void BindColorTexture(unsigned int unit) const;

    [[nodiscard]] bool Valid() const {
        return fbo_ != kInvalidFramebuffer && colorTexture_ != kInvalidTexture;
    }
    [[nodiscard]] FRHIFramebufferId Framebuffer() const { return fbo_; }
    [[nodiscard]] int Width() const { return width_; }
    [[nodiscard]] int Height() const { return height_; }

private:
    FRHIFramebufferId fbo_ = kInvalidFramebuffer;
    FRHITextureId colorTexture_ = kInvalidTexture;
    int width_ = 0;
    int height_ = 0;
};

