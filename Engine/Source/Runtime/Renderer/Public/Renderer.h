#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "Camera/Camera.h"
#include "Math/Transform.h"
#include "Debug/DebugDraw.h"
#include "Engine/Level.h"
#include "Frustum.h"
#include "GpuPassTimer.h"
#include "LdrColorTarget.h"
#include "PlanarReflection.h"
#include "PostProcess.h"
#include "SceneColorTarget.h"
#include "Shader.h"
#include "ShadowMap.h"
#include "SkeletalMesh.h"
#include "StaticMesh.h"
#include "Texture.h"
#include "UniformBuffer.h"
#include "RHIHandles.h"
#include <array>
#include <memory>
#include <string>
#include <vector>

namespace leon {

/// Per-frame measurable counters (color pass after frustum culling).
struct FrameStats {
    int objectsTotal = 0;
    int objectsVisible = 0;
    int objectsCulled = 0;
    int drawsSubmitted = 0;
    int trianglesSubmitted = 0;
    int planarCulled = 0;
    float shadowMs = 0.0f;
    float planarMs = 0.0f;
    float colorMs = 0.0f;
    float ssaoMs = 0.0f;
    float postMs = 0.0f;
};

/// Options for a single submesh draw (shared lit textures may already be bound).
struct DrawOptions {
    bool litPass = true;
    bool receiveShadows = true;
    bool useNormalMaps = true;
    bool bindSharedLitTextures = true; // shadow map unit; env is bound once per pass
};

/// Forward renderer: directional shadow map (light 0), optional half-res planar mirror,
/// opaque / skybox / transparent, then optional post (SSAO → tonemap → FXAA).
class Renderer {
public:
    static constexpr unsigned int kCameraUboBinding = 0;
    static constexpr unsigned int kLightsUboBinding = 1;
    /// Planar mirror FBO scale vs framebuffer (0.5 = half-res).
    static constexpr float kPlanarReflectionScale = 0.5f;
    static constexpr int kMaxAoSamples = 64;

    bool Initialize(const std::string& shaderDirectory);
    void Shutdown();

    /// Reload shaders from disk if file timestamps changed (or force). Rebinds lit UBOs.
    [[nodiscard]] EShaderReloadResult ReloadShaders(bool force = false);

    /// When non-zero, BeginFrame / shadow / planar restore bind this FBO (editor viewport).
    void SetDrawFramebuffer(rhi::RHIFramebufferId fbo) { drawTargetFbo_ = fbo; }
    [[nodiscard]] rhi::RHIFramebufferId GetDrawFramebuffer() const { return drawTargetFbo_; }

    void BeginFrame(int framebufferWidth, int framebufferHeight);
    void DrawScene(const Level& level, const Camera& camera);

    /// Queue a skinned mesh draw for the next `DrawScene` (cleared after DrawScene).
    void SubmitSkeletalDraw(const SkeletalMesh& mesh, const glm::mat4& model,
                            const std::vector<glm::mat4>& boneMatrices);
    void SubmitSkeletalDraw(const SkeletalMesh& mesh, const Transform& transform,
                            const std::vector<glm::mat4>& boneMatrices);

    /// Queue a rigid static mesh with an explicit model matrix (attachments, etc.).
    void SubmitStaticDraw(const StaticMesh& mesh, const glm::mat4& model, const Material& material);

    /// World-space lines flushed at end of DrawScene (independent of F1 AABB overlay).
    void ClearDebugOverlay();
    void AddDebugLine(const glm::vec3& a, const glm::vec3& b, const glm::vec3& color);
    void AddDebugArrow(const glm::vec3& from, const glm::vec3& to, const glm::vec3& color);
    void AddDebugAabb(const glm::vec3& worldMin, const glm::vec3& worldMax, const glm::vec3& color);

    /// Gameplay / physics debug lines (flushed with the scene overlay pass).
    [[nodiscard]] DebugDraw& GetDebugOverlay() { return overlayDebugDraw_; }

    void SetDebugDrawEnabled(bool enabled) { debugDrawEnabled_ = enabled; }
    void ToggleDebugDraw() { debugDrawEnabled_ = !debugDrawEnabled_; }
    [[nodiscard]] bool IsDebugDrawEnabled() const { return debugDrawEnabled_; }

    /// When false, DrawScene skips lit geometry / shadows / post (debug overlay still flushes).
    void SetSceneGeometryEnabled(bool enabled) { sceneGeometryEnabled_ = enabled; }
    [[nodiscard]] bool IsSceneGeometryEnabled() const { return sceneGeometryEnabled_; }

    // --- Post-process API (Unreal-like PascalCase) ---
    void SetPostProcessEnabled(bool enabled) { post_.enabled = enabled; }
    [[nodiscard]] bool IsPostProcessEnabled() const { return post_.enabled; }

    void SetAmbientOcclusionEnabled(bool enabled) { post_.ambientOcclusion = enabled; }
    [[nodiscard]] bool IsAmbientOcclusionEnabled() const { return post_.ambientOcclusion; }

    void SetFxaaEnabled(bool enabled) { post_.fxaa = enabled; }
    [[nodiscard]] bool IsFxaaEnabled() const { return post_.fxaa; }

    void SetEarlyZEnabled(bool enabled) { post_.earlyZ = enabled; }
    [[nodiscard]] bool IsEarlyZEnabled() const { return post_.earlyZ; }

    void SetPostProcessQuality(EPostProcessQuality quality) {
        ApplyPostProcessQuality(post_, quality);
    }
    [[nodiscard]] EPostProcessQuality GetPostProcessQuality() const { return post_.quality; }

    void SetPostProcessSettings(const PostProcessSettings& settings) { post_ = settings; }
    [[nodiscard]] const PostProcessSettings& GetPostProcessSettings() const { return post_; }
    [[nodiscard]] PostProcessSettings& GetPostProcessSettings() { return post_; }

    [[nodiscard]] const FrameStats& GetFrameStats() const { return frameStats_; }
    [[nodiscard]] const std::string& GetShaderDirectory() const { return shaderDirectory_; }

private:
    bool bindLitUbos() const;
    void updateCameraUbo(const Camera& camera) const;
    void updateCameraUbo(const glm::mat4& view, const glm::mat4& projection,
                         const glm::vec3& cameraPos) const;
    void updateLightsUbo(const Level& level) const;
    void bindEnvironment(const Level& level) const;
    void bindShadowResources(bool receiveShadows,
                             float sourceAngleDegrees = kDefaultLightSourceAngleDegrees) const;
    void bindPlanarReflection(bool enabled, const glm::mat4& reflectionViewProj) const;
    void setClipPlane(bool enabled, const glm::vec4& plane) const;
    void ensureShadowMapSize();
    [[nodiscard]] rhi::RHIFramebufferId colorRestoreFbo() const;
    void drawFullscreenTriangle() const;
    void renderPostStack(const Level& level, const Camera& camera);
    void renderShadowPass(const Level& level, const glm::mat4& lightSpace);
    void renderPlanarReflectionPass(const Level& level, const Camera& camera, float planeY);
    void drawSkybox(const Level& level, const glm::mat4& view, const glm::mat4& projection) const;
    void drawDebug(const Level& level, const Camera& camera, const glm::mat4& lightSpace,
                   bool hasLightSpace);
    void DrawSubMesh(const Shader& shader, const StaticMeshComponent& object,
                     std::size_t subMeshIndex, const Material& material, const glm::mat4& view,
                     const glm::mat4& projection, const glm::mat4& lightSpace,
                     const DrawOptions& options) const;
    void drawQueuedSkeletal(const Level& level, const glm::mat4& view, const glm::mat4& projection,
                            const glm::mat4& lightSpace, bool receiveShadows,
                            float shadowSourceAngle, const Frustum* cameraFrustum,
                            bool useWorldClipPlane = false);
    void drawQueuedStatic(const Level& level, const glm::mat4& view, const glm::mat4& projection,
                          const glm::mat4& lightSpace, bool receiveShadows,
                          float shadowSourceAngle);

    struct SkeletalDrawItem {
        const SkeletalMesh* mesh = nullptr;
        glm::mat4 model{1.0f};
        std::vector<glm::mat4> boneMatrices;
    };

    struct StaticDrawItem {
        const StaticMesh* mesh = nullptr;
        glm::mat4 model{1.0f};
        Material material{};
    };

    std::string shaderDirectory_;
    Shader litShader_;
    Shader skinnedLitShader_;
    Shader unlitShader_;
    Shader shadowShader_;
    Shader skinnedShadowShader_;
    Shader skyboxShader_;
    Shader ssaoShader_;
    Shader ssaoBlurShader_;
    Shader postCompositeShader_;
    Shader fxaaShader_;
    ShadowMap shadowMap_;
    PlanarReflection planarReflection_;
    SceneColorTarget sceneColor_;
    SsaoTarget ssaoTarget_;
    LdrColorTarget ldrColor_;
    GpuPassTimer passTimers_;
    DebugDraw debugDraw_;
    DebugDraw overlayDebugDraw_; // gameplay vectors, etc. (always drawn)
    UniformBuffer cameraUbo_;
    UniformBuffer lightsUbo_;
    std::shared_ptr<Texture> whiteTexture_;
    std::shared_ptr<Texture> flatNormalTexture_;
    std::shared_ptr<StaticMesh> skyboxMesh_;
    std::vector<SkeletalDrawItem> skeletalDraws_;
    std::vector<StaticDrawItem> staticDraws_;
    FrameStats frameStats_{};
    PostProcessSettings post_{};

    rhi::RHIVertexArrayId fullscreenVao_ = rhi::kInvalidVertexArray;
    rhi::RHITextureId aoNoiseTexture_ = rhi::kInvalidTexture;
    std::array<glm::vec3, kMaxAoSamples> aoKernel_{};

    int fbWidth_ = 0;
    int fbHeight_ = 0;
    rhi::RHIFramebufferId drawTargetFbo_ = rhi::kInvalidFramebuffer;
    bool debugDrawEnabled_ = false;
    bool sceneGeometryEnabled_ = true;
};

} // namespace leon
