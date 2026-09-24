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
    int ObjectsTotal = 0;
    int ObjectsVisible = 0;
    int ObjectsCulled = 0;
    int DrawsSubmitted = 0;
    int TrianglesSubmitted = 0;
    int PlanarCulled = 0;
    float ShadowMs = 0.0f;
    float PlanarMs = 0.0f;
    float ColorMs = 0.0f;
    float SsaoMs = 0.0f;
    float PostMs = 0.0f;
};

/// Options for a single submesh draw (shared lit textures may already be bound).
struct FDrawOptions {
    bool bLitPass = true;
    bool bReceiveShadows = true;
    bool bUseNormalMaps = true;
    bool bBindSharedLitTextures = true; // shadow map unit; env is bound once per pass
};

/// Forward renderer: directional shadow map (light 0), optional half-res planar mirror,
/// opaque / skybox / transparent, then optional post (SSAO → tonemap → FXAA).
class FSceneRenderer {
public:
    static constexpr unsigned int CameraUboBinding = 0;
    static constexpr unsigned int LightsUboBinding = 1;
    /// Planar mirror FBO scale vs framebuffer (0.5 = half-res).
    static constexpr float PlanarReflectionScale = 0.5f;
    static constexpr int MaxAoSamples = 64;

    bool Initialize(const std::string& InShaderDirectory);
    void Shutdown();

    /// Reload shaders from disk if file timestamps changed (or force). Rebinds lit UBOs.
    [[nodiscard]] EShaderReloadResult ReloadShaders(bool bForce = false);

    /// When non-zero, BeginFrame / shadow / planar restore bind this FBO (editor viewport).
    void SetDrawFramebuffer(FRHIFramebufferId Fbo) { DrawTargetFbo = Fbo; }
    [[nodiscard]] FRHIFramebufferId GetDrawFramebuffer() const { return DrawTargetFbo; }

    void BeginFrame(int FramebufferWidth, int FramebufferHeight);
    void DrawScene(const ULevel& Level, const UCameraComponent& Camera);

    /// Queue a skinned mesh draw for the next `DrawScene` (cleared after DrawScene).
    void SubmitSkeletalDraw(const USkeletalMesh& InMesh, const glm::mat4& InModel,
                            const std::vector<glm::mat4>& InBoneMatrices);
    void SubmitSkeletalDraw(const USkeletalMesh& InMesh, const FTransform& Transform,
                            const std::vector<glm::mat4>& InBoneMatrices);

    /// Queue a rigid static mesh with an explicit model matrix (attachments, etc.).
    void SubmitStaticDraw(const UStaticMesh& InMesh, const glm::mat4& InModel, const FMaterial& InMaterial);

    /// World-space lines flushed at end of DrawScene (independent of F1 AABB overlay).
    void ClearDebugOverlay();
    void AddDebugLine(const glm::vec3& A, const glm::vec3& B, const glm::vec3& Color);
    void AddDebugArrow(const glm::vec3& From, const glm::vec3& To, const glm::vec3& Color);
    void AddDebugAabb(const glm::vec3& WorldMin, const glm::vec3& WorldMax, const glm::vec3& Color);

    /// Gameplay / physics debug lines (flushed with the scene overlay pass).
    [[nodiscard]] FDebugDraw& GetDebugOverlay() { return OverlayDebugDraw; }

    void SetDebugDrawEnabled(bool bEnabled) { bDebugDrawEnabled = bEnabled; }
    void ToggleDebugDraw() { bDebugDrawEnabled = !bDebugDrawEnabled; }
    [[nodiscard]] bool IsDebugDrawEnabled() const { return bDebugDrawEnabled; }

    /// When false, DrawScene skips lit geometry / shadows / post (debug overlay still flushes).
    void SetSceneGeometryEnabled(bool bEnabled) { bSceneGeometryEnabled = bEnabled; }
    [[nodiscard]] bool IsSceneGeometryEnabled() const { return bSceneGeometryEnabled; }

    // --- Post-process API (Unreal-like PascalCase) ---
    void SetPostProcessEnabled(bool bEnabled) { Post.bEnabled = bEnabled; }
    [[nodiscard]] bool IsPostProcessEnabled() const { return Post.bEnabled; }

    void SetAmbientOcclusionEnabled(bool bEnabled) { Post.bAmbientOcclusion = bEnabled; }
    [[nodiscard]] bool IsAmbientOcclusionEnabled() const { return Post.bAmbientOcclusion; }

    void SetFxaaEnabled(bool bEnabled) { Post.bFxaa = bEnabled; }
    [[nodiscard]] bool IsFxaaEnabled() const { return Post.bFxaa; }

    void SetEarlyZEnabled(bool bEnabled) { Post.bEarlyZ = bEnabled; }
    [[nodiscard]] bool IsEarlyZEnabled() const { return Post.bEarlyZ; }

    void SetPostProcessQuality(EPostProcessQuality Quality) {
        ApplyPostProcessQuality(Post, Quality);
    }
    [[nodiscard]] EPostProcessQuality GetPostProcessQuality() const { return Post.Quality; }

    void SetPostProcessSettings(const FPostProcessSettings& Settings) { Post = Settings; }
    [[nodiscard]] const FPostProcessSettings& GetPostProcessSettings() const { return Post; }
    [[nodiscard]] FPostProcessSettings& GetPostProcessSettings() { return Post; }

    [[nodiscard]] const FFrameStats& GetFrameStats() const { return FrameStats; }
    [[nodiscard]] const std::string& GetShaderDirectory() const { return ShaderDirectory; }

private:
    bool BindLitUbos() const;
    void UpdateCameraUbo(const UCameraComponent& Camera) const;
    void UpdateCameraUbo(const glm::mat4& InView, const glm::mat4& InProjection,
                         const glm::vec3& InCameraPos) const;
    void UpdateLightsUbo(const ULevel& Level) const;
    void BindEnvironment(const ULevel& Level) const;
    void BindShadowResources(bool bInReceiveShadows,
                             float SourceAngleDegrees = kDefaultLightSourceAngleDegrees) const;
    void BindPlanarReflection(bool bEnabled, const glm::mat4& ReflectionViewProj) const;
    void SetClipPlane(bool bEnabled, const glm::vec4& Plane) const;
    void EnsureShadowMapSize();
    [[nodiscard]] FRHIFramebufferId ColorRestoreFbo() const;
    void DrawFullscreenTriangle() const;
    void RenderPostStack(const ULevel& Level, const UCameraComponent& Camera);
    void RenderShadowPass(const ULevel& Level, const glm::mat4& LightSpace);
    void RenderPlanarReflectionPass(const ULevel& Level, const UCameraComponent& Camera, float PlaneY);
    void DrawSkybox(const ULevel& Level, const glm::mat4& InView, const glm::mat4& InProjection) const;
    void DrawDebug(const ULevel& Level, const UCameraComponent& Camera, const glm::mat4& LightSpace,
                   bool bHasLightSpace);
    void DrawSubMesh(const FShader& Shader, const UStaticMeshComponent& Object,
                     std::size_t InSubMeshIndex, const FMaterial& InMaterial, const glm::mat4& InView,
                     const glm::mat4& InProjection, const glm::mat4& LightSpace,
                     const FDrawOptions& Options) const;
    void DrawQueuedSkeletal(const ULevel& Level, const glm::mat4& InView, const glm::mat4& InProjection,
                            const glm::mat4& LightSpace, bool bInReceiveShadows,
                            float ShadowSourceAngle, const FFrustum* CameraFrustum,
                            bool bUseWorldClipPlane = false);
    void DrawQueuedStatic(const ULevel& Level, const glm::mat4& InView, const glm::mat4& InProjection,
                          const glm::mat4& LightSpace, bool bInReceiveShadows,
                          float ShadowSourceAngle);

    struct FSkeletalDrawItem {
        const USkeletalMesh* Mesh = nullptr;
        glm::mat4 Model{1.0f};
        std::vector<glm::mat4> BoneMatrices;
    };

    struct FStaticDrawItem {
        const UStaticMesh* Mesh = nullptr;
        glm::mat4 Model{1.0f};
        FMaterial Material{};
    };

    std::string ShaderDirectory;
    FShader LitShader;
    FShader SkinnedLitShader;
    FShader UnlitShader;
    FShader ShadowShader;
    FShader SkinnedShadowShader;
    FShader SkyboxShader;
    FShader SsaoShader;
    FShader SsaoBlurShader;
    FShader PostCompositeShader;
    FShader FxaaShader;
    FShadowMap ShadowMap;
    FPlanarReflection PlanarReflection;
    FSceneColorTarget SceneColor;
    FSSAOTarget SsaoTarget;
    FLDRColorTarget LdrColor;
    FGPUPassTimer PassTimers;
    FDebugDraw DebugDraw;
    FDebugDraw OverlayDebugDraw; // gameplay vectors, etc. (always drawn)
    FUniformBuffer CameraUbo;
    FUniformBuffer LightsUbo;
    std::shared_ptr<UTexture2D> WhiteTexture;
    std::shared_ptr<UTexture2D> FlatNormalTexture;
    std::shared_ptr<UStaticMesh> SkyboxMesh;
    std::vector<FSkeletalDrawItem> SkeletalDraws;
    std::vector<FStaticDrawItem> StaticDraws;
    FFrameStats FrameStats{};
    FPostProcessSettings Post{};

    FRHIVertexArrayId FullscreenVao = InvalidVertexArray;
    FRHITextureId AoNoiseTexture = InvalidTexture;
    std::array<glm::vec3, MaxAoSamples> AoKernel{};

    int FbWidth = 0;
    int FbHeight = 0;
    FRHIFramebufferId DrawTargetFbo = InvalidFramebuffer;
    bool bDebugDrawEnabled = false;
    bool bSceneGeometryEnabled = true;
};

