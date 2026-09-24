#pragma once

#include "RHIHandles.h"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>


/// Depth-only shadow map for directional light 0 (orthographic + manual PCF in the lit shader).
/// Depth texture uses GL_NEAREST so PCF samples discrete texels (not hardware-filtered depth).
class FShadowMap {
public:
    static constexpr int kDefaultSize = 2048;

    FShadowMap() = default;
    ~FShadowMap();

    FShadowMap(const FShadowMap&) = delete;
    FShadowMap& operator=(const FShadowMap&) = delete;

    bool Create(int size = kDefaultSize);
    void Destroy();

    void Begin() const;
    /// Restore draw target to `restoreFbo` (0 = default framebuffer).
    void End(int framebufferWidth, int framebufferHeight, FRHIFramebufferId restoreFbo = kInvalidFramebuffer) const;

    void BindDepthTexture(unsigned int unit) const;
    [[nodiscard]] bool Valid() const { return fbo_ != 0 && depthTexture_ != 0; }
    [[nodiscard]] int Size() const { return size_; }

    /// Ortho light matrix tightly fitted to a world-space AABB of shadow casters.
    [[nodiscard]] static glm::mat4 FitLightSpaceMatrix(const glm::vec3& lightDirection,
                                                       const glm::vec3& worldMin,
                                                       const glm::vec3& worldMax,
                                                       float padding = 0.5f);

private:
    FRHIFramebufferId fbo_ = kInvalidFramebuffer;
    FRHITextureId depthTexture_ = kInvalidTexture;
    int size_ = 0;
};

