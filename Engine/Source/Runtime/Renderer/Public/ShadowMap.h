#pragma once

#include "RHIHandles.h"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace leon {

/// Depth-only shadow map for directional light 0 (orthographic + manual PCF in the lit shader).
/// Depth texture uses GL_NEAREST so PCF samples discrete texels (not hardware-filtered depth).
class ShadowMap {
public:
    static constexpr int kDefaultSize = 2048;

    ShadowMap() = default;
    ~ShadowMap();

    ShadowMap(const ShadowMap&) = delete;
    ShadowMap& operator=(const ShadowMap&) = delete;

    bool Create(int size = kDefaultSize);
    void Destroy();

    void Begin() const;
    /// Restore draw target to `restoreFbo` (0 = default framebuffer).
    void End(int framebufferWidth, int framebufferHeight, rhi::RHIFramebufferId restoreFbo = rhi::kInvalidFramebuffer) const;

    void BindDepthTexture(unsigned int unit) const;
    [[nodiscard]] bool Valid() const { return fbo_ != 0 && depthTexture_ != 0; }
    [[nodiscard]] int Size() const { return size_; }

    /// Ortho light matrix tightly fitted to a world-space AABB of shadow casters.
    [[nodiscard]] static glm::mat4 FitLightSpaceMatrix(const glm::vec3& lightDirection,
                                                       const glm::vec3& worldMin,
                                                       const glm::vec3& worldMax,
                                                       float padding = 0.5f);

private:
    rhi::RHIFramebufferId fbo_ = rhi::kInvalidFramebuffer;
    rhi::RHITextureId depthTexture_ = rhi::kInvalidTexture;
    int size_ = 0;
};

} // namespace leon
