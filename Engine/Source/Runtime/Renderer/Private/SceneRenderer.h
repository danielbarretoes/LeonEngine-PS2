#pragma once

#include "CoreMinimal.h"
#include "Debug/DebugDraw.h"
#include "Frustum.h"
#include "GpuPassTimer.h"
#include "LdrColorTarget.h"
#include "Level/Light.h"
#include "LineBatchRenderer.h"
#include "PlanarReflection.h"
#include "PostProcess.h"
#include "RHIHandles.h"
#include "RenderResourceCache.h"
#include "RendererInterface.h"
#include "SceneColorTarget.h"
#include "SceneView.h"
#include "Shader.h"
#include "ShadowMap.h"
#include "Texture2DResource.h"
#include "UniformBuffer.h"

class FLightSceneProxy;
class FSceneInterface;
class FStaticMeshSceneProxy;
class UObject;
class USkeletalMesh;
class UTexture2D;

/** Options for a single submesh draw (shared lit textures may already be bound). */
struct FDrawOptions
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
 * It draws a view family's scene (FScene's proxies, in the scene's order) through its view (UE: the scene renderer of
 * a view family). Views are in UE view space (x right, y up, z forward; ViewMatrices.h). Projections passed between
 * the passes are already in GL clip space (the view's UE projection through ToGLClipSpace, GLClipSpace.h), so every
 * MVP is Model * View * ProjectionGL. The GPU copies of the engine's meshes and textures come from its resource cache.
 */
class FSceneRenderer
{
public:
	static constexpr uint32 CameraUboBinding = 0;
	static constexpr uint32 LightsUboBinding = 1;
	/** Planar mirror FBO scale vs framebuffer (0.5 = half-res). */
	static constexpr float PlanarReflectionScale = 0.5f;
	static constexpr int32 MaxAoSamples = 64;

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

	/** Frees the GPU copy of an asset, if the resource cache has one (IRendererModule::ReleaseAssetResources). */
	void ReleaseAssetResources(const UObject* Asset)
	{
		Resources.ReleaseResources(Asset);
	}

	void BeginFrame(int32 FramebufferWidth, int32 FramebufferHeight);
	/**
	 * Draws the family's first view of its scene: shadows, the planar mirror, the opaque meshes, the skinned meshes,
	 * the translucent meshes, the bounds debug, post processing, the world's debug lines and the axes gizmo (the show
	 * flags decide the debug parts). A family without a scene draws only the background.
	 */
	void Render(const FSceneViewFamily& ViewFamily);
	/** Reads the draw framebuffer as bottom-up BGR rows with no padding (screenshots). */
	void ReadFramebufferBgr(int32 Width, int32 Height, TArray<uint8>& OutBgr) const;

	/** When false, Render skips lit geometry / shadows / post (debug overlay still flushes). */
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
	void UpdateCameraUbo(const FSceneView& View) const;
	void UpdateCameraUbo(const FMatrix& InView, const FMatrix& InProjection, const FVector& InCameraPos) const;
	void UpdateLightsUbo() const;
	void BindShadowResources(bool bInReceiveShadows, float SourceAngleDegrees = DefaultLightSourceAngleDegrees) const;
	void BindPlanarReflection(bool bEnabled, const FMatrix& ReflectionViewProj) const;
	void SetClipPlane(bool bEnabled, const FVector4& Plane) const;
	void EnsureShadowMapSize();
	[[nodiscard]] FRHIFramebufferId ColorRestoreFbo() const;
	void DrawFullscreenTriangle() const;
	void RenderPostStack(const FSceneView& View);
	void RenderShadowPass(const FMatrix& LightSpace);
	void RenderPlanarReflectionPass(const FSceneView& View, float PlaneZ);
	void DrawDebug(const FSceneView& View, const FMatrix& LightSpace, bool bHasLightSpace);
	/** Draws and empties the world's debug line batch (UWorld::LineBatcher). */
	void FlushWorldLines(const FSceneView& View, FDebugDraw* WorldLines);
	/**
	 * After the scene: the world origin axes with no depth test, then the view orientation gizmo in a fixed-size
	 * square at the bottom-left of the draw framebuffer.
	 */
	void DrawAxesGizmo(const FSceneView& View);
	/** Binds the texture's GPU copy, or Fallback when there is no valid texture. */
	void BindTexture(const UTexture2D* Texture, const FTexture2DResource& Fallback, uint32 Unit);
	void DrawSubMesh(const FShader& Shader, const FStaticMeshSceneProxy& Object, int32 InSubMeshIndex,
		const FMaterial& InMaterial, const FMatrix& InView, const FMatrix& InProjection, const FMatrix& LightSpace,
		const FDrawOptions& Options);
	/** Fills FrameMeshes, SkeletalDraws and the light lists from the scene. */
	void GatherScene(FSceneInterface* InScene);
	void DrawQueuedSkeletal(const FMatrix& InView, const FMatrix& InProjection, const FMatrix& LightSpace,
		bool bInReceiveShadows, float ShadowSourceAngle, const FFrustum* CameraFrustum,
		bool bUseWorldClipPlane = false);

	struct FSkeletalDrawItem
	{
		const USkeletalMesh* Mesh = nullptr;
		/** The material of the mesh's proxy. */
		FMaterial Material;
		FMatrix Model = FMatrix::Identity;
		/** Skin matrices in the GL memory layout (uploaded as they are). */
		TArray<FMatrix> BoneMatrices;
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
	/** The renderer's own lines: the bounds view and the axes gizmo. */
	FDebugDraw DebugDraw;
	FLineBatchRenderer LineBatch;
	FUniformBuffer CameraUbo;
	FUniformBuffer LightsUbo;
	TUniquePtr<FTexture2DResource> WhiteTexture;
	TUniquePtr<FTexture2DResource> FlatNormalTexture;
	/** The GPU copies of the engine's meshes and textures. */
	FRenderResourceCache Resources;
	/** The scene's proxies, gathered in its order at the start of Render. */
	TArray<FSkeletalDrawItem> SkeletalDraws;
	TArray<const FStaticMeshSceneProxy*> FrameMeshes;
	TArray<const FLightSceneProxy*> FrameDirectionalLights;
	TArray<const FLightSceneProxy*> FramePointLights;
	FFrameStats FrameStats{};
	FPostProcessSettings Post{};

	FRHIVertexArrayId FullscreenVao = InvalidVertexArray;
	FRHITextureId AoNoiseTexture = InvalidTexture;
	FVector AoKernel[MaxAoSamples];

	int32 FbWidth = 0;
	int32 FbHeight = 0;
	FRHIFramebufferId DrawTargetFbo = InvalidFramebuffer;
	/** The show flags of the family being rendered. */
	FEngineShowFlags ShowFlags;
	bool bSceneGeometryEnabled = true;
};
