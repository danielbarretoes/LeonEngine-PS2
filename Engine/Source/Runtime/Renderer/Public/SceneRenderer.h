#pragma once

#include "Camera/CameraComponent.h"
#include "CoreMinimal.h"
#include "Debug/DebugDraw.h"
#include "Engine/Level.h"
#include "Frustum.h"
#include "GpuPassTimer.h"
#include "LdrColorTarget.h"
#include "PlanarReflection.h"
#include "PostProcess.h"
#include "RHIHandles.h"
#include "SceneColorTarget.h"
#include "Shader.h"
#include "ShadowMap.h"
#include "SkeletalMesh.h"
#include "StaticMesh.h"
#include "Texture2D.h"
#include "UniformBuffer.h"

/** Per-frame measurable counters (color pass after frustum culling). */
struct RENDERER_API FFrameStats
{
	int32 ObjectsTotal = 0;
	int32 ObjectsVisible = 0;
	int32 ObjectsCulled = 0;
	int32 DrawsSubmitted = 0;
	int32 TrianglesSubmitted = 0;
	int32 PlanarCulled = 0;
	float ShadowMs = 0.0f;
	float PlanarMs = 0.0f;
	float ColorMs = 0.0f;
	float SsaoMs = 0.0f;
	float PostMs = 0.0f;
};

/** Options for a single submesh draw (shared lit textures may already be bound). */
struct RENDERER_API FDrawOptions
{
	bool bLitPass = true;
	bool bReceiveShadows = true;
	bool bUseNormalMaps = true;
	bool bBindSharedLitTextures = true; // shadow map unit; env is bound once per pass
};

/**
 * Forward renderer: directional shadow map (light 0), optional half-res planar mirror,
 * opaque / transparent, then optional post (SSAO → tonemap → FXAA).
 *
 * Views are in UE view space (x right, y up, z forward; ViewMatrices.h). Projections passed between the passes are
 * already in GL clip space (the camera's UE projection through ToGLClipSpace, GLClipSpace.h), so every MVP is
 * Model * View * ProjectionGL.
 */
class RENDERER_API FSceneRenderer
{
public:
	static constexpr uint32 CameraUboBinding = 0;
	static constexpr uint32 LightsUboBinding = 1;
	/** Planar mirror FBO scale vs framebuffer (0.5 = half-res). */
	static constexpr float PlanarReflectionScale = 0.5f;
	static constexpr int32 MaxAoSamples = 64;

	/** World-space mirror about the horizontal plane z = PlaneZ, applied before the view by the planar pass. */
	[[nodiscard]] static FMatrix MakeReflectMatrix(float PlaneZ);

	bool Initialize(const FString& InShaderDirectory);
	void Shutdown();

	/** Reloads shaders from disk if file timestamps changed (or when forced). Rebinds lit UBOs. */
	[[nodiscard]] EShaderReloadResult ReloadShaders(bool bForce = false);

	/** When non-zero, BeginFrame / shadow / planar restore bind this FBO (editor viewport). */
	void SetDrawFramebuffer(FRHIFramebufferId Fbo)
	{
		DrawTargetFbo = Fbo;
	}
	[[nodiscard]] FRHIFramebufferId GetDrawFramebuffer() const
	{
		return DrawTargetFbo;
	}

	void BeginFrame(int32 FramebufferWidth, int32 FramebufferHeight);
	void DrawScene(const ULevel& Level, const UCameraComponent& Camera);
	/** Reads the draw framebuffer as bottom-up BGR rows with no padding (screenshots). */
	void ReadFramebufferBgr(int32 Width, int32 Height, TArray<uint8>& OutBgr) const;

	/** Queues a skinned mesh draw for the next DrawScene (cleared after DrawScene). */
	void SubmitSkeletalDraw(const USkeletalMesh& InMesh, const FMatrix& InModel, const TArray<FMatrix>& InBoneMatrices);
	void SubmitSkeletalDraw(
		const USkeletalMesh& InMesh, const FTransform& Transform, const TArray<FMatrix>& InBoneMatrices);

	/** Queues a rigid static mesh with an explicit model matrix (attachments, etc.). */
	void SubmitStaticDraw(const UStaticMesh& InMesh, const FMatrix& InModel, const FMaterial& InMaterial);

	/** World-space lines flushed at the end of DrawScene (independent of the F1 AABB overlay). */
	void ClearDebugOverlay();
	void AddDebugLine(const FVector& A, const FVector& B, const FLinearColor& Color);
	void AddDebugArrow(const FVector& From, const FVector& To, const FLinearColor& Color);
	void AddDebugAabb(const FVector& WorldMin, const FVector& WorldMax, const FLinearColor& Color);

	/** Gameplay / physics debug lines (flushed with the scene overlay pass). */
	[[nodiscard]] FDebugDraw& GetDebugOverlay()
	{
		return OverlayDebugDraw;
	}

	void SetDebugDrawEnabled(bool bEnabled)
	{
		bDebugDrawEnabled = bEnabled;
	}
	void ToggleDebugDraw()
	{
		bDebugDrawEnabled = !bDebugDrawEnabled;
	}
	[[nodiscard]] bool IsDebugDrawEnabled() const
	{
		return bDebugDrawEnabled;
	}

	/** When false, DrawScene skips lit geometry / shadows / post (debug overlay still flushes). */
	void SetSceneGeometryEnabled(bool bEnabled)
	{
		bSceneGeometryEnabled = bEnabled;
	}
	[[nodiscard]] bool IsSceneGeometryEnabled() const
	{
		return bSceneGeometryEnabled;
	}

	// --- Post-process API (Unreal-like PascalCase) ---
	void SetPostProcessEnabled(bool bEnabled)
	{
		Post.bEnabled = bEnabled;
	}
	[[nodiscard]] bool IsPostProcessEnabled() const
	{
		return Post.bEnabled;
	}

	void SetAmbientOcclusionEnabled(bool bEnabled)
	{
		Post.bAmbientOcclusion = bEnabled;
	}
	[[nodiscard]] bool IsAmbientOcclusionEnabled() const
	{
		return Post.bAmbientOcclusion;
	}

	void SetFxaaEnabled(bool bEnabled)
	{
		Post.bFxaa = bEnabled;
	}
	[[nodiscard]] bool IsFxaaEnabled() const
	{
		return Post.bFxaa;
	}

	void SetEarlyZEnabled(bool bEnabled)
	{
		Post.bEarlyZ = bEnabled;
	}
	[[nodiscard]] bool IsEarlyZEnabled() const
	{
		return Post.bEarlyZ;
	}

	void SetPostProcessQuality(EPostProcessQuality Quality)
	{
		ApplyPostProcessQuality(Post, Quality);
	}
	[[nodiscard]] EPostProcessQuality GetPostProcessQuality() const
	{
		return Post.Quality;
	}

	void SetPostProcessSettings(const FPostProcessSettings& Settings)
	{
		Post = Settings;
	}
	[[nodiscard]] const FPostProcessSettings& GetPostProcessSettings() const
	{
		return Post;
	}
	[[nodiscard]] FPostProcessSettings& GetPostProcessSettings()
	{
		return Post;
	}

	[[nodiscard]] const FFrameStats& GetFrameStats() const
	{
		return FrameStats;
	}
	[[nodiscard]] const FString& GetShaderDirectory() const
	{
		return ShaderDirectory;
	}

private:
	bool BindLitUbos() const;
	void UpdateCameraUbo(const UCameraComponent& Camera) const;
	void UpdateCameraUbo(const FMatrix& InView, const FMatrix& InProjection, const FVector& InCameraPos) const;
	void UpdateLightsUbo(const ULevel& Level) const;
	void BindShadowResources(bool bInReceiveShadows, float SourceAngleDegrees = DefaultLightSourceAngleDegrees) const;
	void BindPlanarReflection(bool bEnabled, const FMatrix& ReflectionViewProj) const;
	void SetClipPlane(bool bEnabled, const FVector4& Plane) const;
	void EnsureShadowMapSize();
	[[nodiscard]] FRHIFramebufferId ColorRestoreFbo() const;
	void DrawFullscreenTriangle() const;
	void RenderPostStack(const UCameraComponent& Camera);
	void RenderShadowPass(const ULevel& Level, const FMatrix& LightSpace);
	void RenderPlanarReflectionPass(const ULevel& Level, const UCameraComponent& Camera, float PlaneZ);
	void DrawDebug(const ULevel& Level, const UCameraComponent& Camera, const FMatrix& LightSpace, bool bHasLightSpace);
	void DrawSubMesh(const FShader& Shader, const UStaticMeshComponent& Object, int32 InSubMeshIndex,
		const FMaterial& InMaterial, const FMatrix& InView, const FMatrix& InProjection, const FMatrix& LightSpace,
		const FDrawOptions& Options) const;
	void DrawQueuedSkeletal(const FMatrix& InView, const FMatrix& InProjection, const FMatrix& LightSpace,
		bool bInReceiveShadows, float ShadowSourceAngle, const FFrustum* CameraFrustum,
		bool bUseWorldClipPlane = false);
	void DrawQueuedStatic(const ULevel& Level, const FMatrix& InView, const FMatrix& InProjection,
		const FMatrix& LightSpace, bool bInReceiveShadows, float ShadowSourceAngle);

	struct FSkeletalDrawItem
	{
		const USkeletalMesh* Mesh = nullptr;
		FMatrix Model = FMatrix::Identity;
		/** Skin matrices in the GL memory layout (uploaded as they are). */
		TArray<FMatrix> BoneMatrices;
	};

	struct FStaticDrawItem
	{
		const UStaticMesh* Mesh = nullptr;
		FMatrix Model = FMatrix::Identity;
		FMaterial Material{};
	};

	FString ShaderDirectory;
	FShader LitShader;
	FShader SkinnedLitShader;
	FShader UnlitShader;
	FShader ShadowShader;
	FShader SkinnedShadowShader;
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
	TSharedPtr<UTexture2D> WhiteTexture;
	TSharedPtr<UTexture2D> FlatNormalTexture;
	TArray<FSkeletalDrawItem> SkeletalDraws;
	TArray<FStaticDrawItem> StaticDraws;
	FFrameStats FrameStats{};
	FPostProcessSettings Post{};

	FRHIVertexArrayId FullscreenVao = InvalidVertexArray;
	FRHITextureId AoNoiseTexture = InvalidTexture;
	FVector AoKernel[MaxAoSamples];

	int32 FbWidth = 0;
	int32 FbHeight = 0;
	FRHIFramebufferId DrawTargetFbo = InvalidFramebuffer;
	bool bDebugDrawEnabled = false;
	bool bSceneGeometryEnabled = true;
};
