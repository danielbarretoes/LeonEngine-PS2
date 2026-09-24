#pragma once

#include "RHIHandles.h"


/// Full-res LDR color target for post (composite → FXAA).
class FLDRColorTarget {
public:
    FLDRColorTarget() = default;
    ~FLDRColorTarget();

    FLDRColorTarget(const FLDRColorTarget&) = delete;
    FLDRColorTarget& operator=(const FLDRColorTarget&) = delete;

    [[nodiscard]] bool EnsureSize(int InWidth, int InHeight);
    void Destroy();

    void BindWrite() const;
    void BindColorTexture(unsigned int Unit) const;

    [[nodiscard]] bool Valid() const {
        return Fbo != InvalidFramebuffer && ColorTexture != InvalidTexture;
    }
    [[nodiscard]] FRHIFramebufferId Framebuffer() const { return Fbo; }
    [[nodiscard]] int GetWidth() const { return Width; }
    [[nodiscard]] int GetHeight() const { return Height; }

private:
    FRHIFramebufferId Fbo = InvalidFramebuffer;
    FRHITextureId ColorTexture = InvalidTexture;
    int Width = 0;
    int Height = 0;
};

