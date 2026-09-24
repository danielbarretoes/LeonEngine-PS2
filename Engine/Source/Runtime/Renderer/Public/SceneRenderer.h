#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "Camera/CameraComponent.h"
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
#include "Texture2D.h"
#include "UniformBuffer.h"
#include "RHIHandles.h"
#include <array>
#include <memory>
#include <string>
#include <vector>


/// Per-frame measurable counters (color pass after frustum culling).
struct FFrameStats {
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
struct FDrawOptions {
    bool litPass = true;
    bool receiveShadows = true;
    bool useNormalMaps = true;
    bool bindSharedLitTextures = true; // shadow map unit; env is bound once per pass
};

/// Forward renderer: directional shadow map (light 0), optional half-res planar mirror,
/// opaque / skybox / transparent, then optional post (SSAO → tonemap → FXAA).
class FSceneRenderer {
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
    void SetDrawFramebuffer(FRHIFramebufferId fbo) { drawTargetFbo_ = fbo; }
    [[nodiscard]] FRHIFramebufferId GetDrawFramebuffer() const { return drawTargetFbo_; }

    void BeginFrame(int framebufferWidth, int framebufferHeight);
    void DrawScene(const ULevel& level, const UCameraComponent& camera);

    /// Queue a skinned mesh draw for the next `DrawScene` (cleared after DrawScene).
    void SubmitSkeletalDraw(const USkeletalMesh& mesh, const glm::mat4& model,
                            const std::vector<glm::mat4>& boneMatrices);
    void SubmitSkeletalDraw(const USkeletalMesh& mesh, const FTransform& transform,
                            const std::vector<glm::mat4>& boneMatrices);

    /// Queue a rigid static mesh with an explicit model matrix (attachments, etc.).
    void SubmitStaticDraw(const UStaticMesh& mesh, const glm::mat4& model, const FMaterial& material);

    /// World-space lines flushed at end of DrawScene (independent of F1 AABB overlay).
    void ClearDebugOverlay();
    void AddDebugLine(const glm::vec3& a, const glm::vec3& b, const glm::vec3& color);
    void AddDebugArrow(const glm::vec3& from, const glm::vec3& to, const glm::vec3& color);
    void AddDebugAabb(const glm::vec3& worldMin, const glm::vec3& worldMax, const glm::vec3& color);

    /// Gameplay / physics debug lines (flushed with the scene overlay pass).
    [[nodiscard]] FDebugDraw& GetDebugOverlay() { return overlayDebugDraw_; }

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

    void SetPostProcessSettings(const FPostProcessSettings& settings) { post_ = settings; }
    [[nodiscard]] const FPostProcessSettings& GetPostProcessSettings() const { return post_; }
    [[nodiscard]] FPostProcessSettings& GetPostProcessSettings() { return post_; }

    [[nodiscard]] const FFrameStats& GetFrameStats() const { return frameStats_; }
    [[nodiscard]] const std::string& GetShaderDirectory() const { return shaderDirectory_; }

private:
    bool bindLitUbos() const;
    void updateCameraUbo(const UCameraComponent& camera) const;
    void updateCameraUbo(const glm::mat4& view, const glm::mat4& projection,
                         const glm::vec3& cameraPos) const;
    void updateLightsUbo(const ULevel& level) const;
    void bindEnvironment(const ULevel& level) const;
    void bindShadowResources(bool receiveShadows,
                             float sourceAngleDegrees = kDefaultLightSourceAngleDegrees) const;
    void bindPlanarReflection(bool enabled, const glm::mat4& reflectionViewProj) const;
    void setClipPlane(bool enabled, const glm::vec4& plane) const;
    void ensureShadowMapSize();
    [[nodiscard]] FRHIFramebufferId colorRestoreFbo() const;
    void drawFullscreenTriangle() const;
    void renderPostStack(const ULevel& level, const UCameraComponent& camera);
    void renderShadowPass(const ULevel& level, const glm::mat4& lightSpace);
    void renderPlanarReflectionPass(const ULevel& level, const UCameraComponent& camera, float planeY);
    void drawSkybox(const ULevel& level, const glm::mat4& view, const glm::mat4& projection) const;
    void drawDebug(const ULevel& level, const UCameraComponent& camera, const glm::mat4& lightSpace,
                   bool hasLightSpace);
    void DrawSubMesh(const FShader& shader, const UStaticMeshComponent& object,
                     std::size_t subMeshIndex, const FMaterial& material, const glm::mat4& view,
                     const glm::mat4& projection, const glm::mat4& lightSpace,
                     const FDrawOptions& options) const;
    void drawQueuedSkeletal(const ULevel& level, const glm::mat4& view, const glm::mat4& projection,
                            const glm::mat4& lightSpace, bool receiveShadows,
                            float shadowSourceAngle, const FFrustum* cameraFrustum,
                            bool useWorldClipPlane = false);
    void drawQueuedStatic(const ULevel& level, const glm::mat4& view, const glm::mat4& projection,
                          const glm::mat4& lightSpace, bool receiveShadows,
                          float shadowSourceAngle);

    struct FSkeletalDrawItem {
        const USkeletalMesh* mesh = nullptr;
        glm::mat4 model{1.0f};
        std::vector<glm::mat4> boneMatrices;
    };

    struct FStaticDrawItem {
        const UStaticMesh* mesh = nullptr;
        glm::mat4 model{1.0f};
        FMaterial material{};
    };

    std::string shaderDirectory_;
    FShader litShader_;
    FShader skinnedLitShader_;
    FShader unlitShader_;
    FShader shadowShader_;
    FShader skinnedShadowShader_;
    FShader skyboxShader_;
    FShader ssaoShader_;
    FShader ssaoBlurShader_;
    FShader postCompositeShader_;
    FShader fxaaShader_;
    FShadowMap shadowMap_;
    FPlanarReflection planarReflection_;
    FSceneColorTarget sceneColor_;
    FSSAOTarget ssaoTarget_;
    FLDRColorTarget ldrColor_;
    FGPUPassTimer passTimers_;
    FDebugDraw debugDraw_;
    FDebugDraw overlayDebugDraw_; // gameplay vectors, etc. (always drawn)
    FUniformBuffer cameraUbo_;
    FUniformBuffer lightsUbo_;
    std::shared_ptr<UTexture2D> whiteTexture_;
    std::shared_ptr<UTexture2D> flatNormalTexture_;
    std::shared_ptr<UStaticMesh> skyboxMesh_;
    std::vector<FSkeletalDrawItem> skeletalDraws_;
    std::vector<FStaticDrawItem> staticDraws_;
    FFrameStats frameStats_{};
    FPostProcessSettings post_{};

    FRHIVertexArrayId fullscreenVao_ = kInvalidVertexArray;
    FRHITextureId aoNoiseTexture_ = kInvalidTexture;
    std::array<glm::vec3, kMaxAoSamples> aoKernel_{};

    int fbWidth_ = 0;
    int fbHeight_ = 0;
    FRHIFramebufferId drawTargetFbo_ = kInvalidFramebuffer;
    bool debugDrawEnabled_ = false;
    bool sceneGeometryEnabled_ = true;
};

