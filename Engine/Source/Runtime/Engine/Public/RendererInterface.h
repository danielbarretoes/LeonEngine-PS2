#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "ShaderCore.h"

class FCanvas;
class FSceneInterface;
class FSceneViewFamily;
class UObject;
class UWorld;

/** Per-frame counters of the scene pass (color pass after frustum culling) and the GPU time of each pass. */
struct ENGINE_API FFrameStats
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
};

/**
 * The Renderer module's interface to the engine (UE: IRendererModule, which UE declares in RendererInterface.h too):
 * Engine only knows this, FSceneInterface, FSceneView and FCanvas, and finds the module by name (GetRendererModule),
 * so Engine never includes a Renderer header. The Renderer module implements it (FRendererModule) and depends on
 * Engine, as in UE.
 *
 * A frame (UGameEngine::Render): the world sends its changes (UWorld::SendAllEndOfFrameUpdates), the engine renders the
 * view family (BeginRenderingViewFamily), then its HUD and debug text draw into the frame's canvas, which it flushes
 * (FCanvas::Flush_GameThread → DrawCanvas).
 */
class IRendererModule : public IModuleInterface
{
public:
	/**
	 * Creates the renderer's GPU objects (shaders, targets, the canvas and line batch renderers) once the window has
	 * its OpenGL context (Leon; UE's render resources are created on the render thread). False when a shader or a
	 * target cannot be made.
	 */
	virtual bool InitRenderer(const FString& ShaderDirectory) = 0;
	/** Frees every GPU object, the cached copies of the engine's assets included, before the context goes. */
	virtual void ShutdownRenderer() = 0;

	/**
	 * Frees the GPU copy the renderer keeps of an asset (a texture's, a static or skeletal mesh's), if it has one; the
	 * next draw of the asset makes a new copy. Leon's renderer keeps the copies keyed by asset (UE: the assets own
	 * their render resources and release them with BeginReleaseResource); the assets call it when their data changes
	 * and in BeginDestroy (ReleaseAssetRenderResources), so a copy never outlives its asset. Leon has no render thread:
	 * it runs at once, on the game thread, while the context is current.
	 */
	virtual void ReleaseAssetResources(const UObject* Asset) = 0;
	[[nodiscard]] virtual bool IsRendererInitialized() const = 0;

	/** A new scene for a world (UE: AllocateScene); the world frees it with RemoveScene. */
	[[nodiscard]] virtual FSceneInterface* AllocateScene(UWorld* World) = 0;
	/** Frees a scene AllocateScene returned, with the proxies it still holds (UE: RemoveScene). */
	virtual void RemoveScene(FSceneInterface* Scene) = 0;

	/**
	 * Clears the family's render target and draws its view of the scene (UE: BeginRenderingViewFamily): shadows, the
	 * planar mirror, the opaque and translucent meshes, the world's debug lines, post processing and the show flags'
	 * overlays. Canvas is the frame's canvas (unused by the scene pass; UE takes it for the view's debug text).
	 */
	virtual void BeginRenderingViewFamily(FCanvas* Canvas, FSceneViewFamily* ViewFamily) = 0;

	/** Draws a canvas's 2D items over the frame (Leon: FCanvas::Flush_GameThread calls it). */
	virtual void DrawCanvas(const FCanvas& Canvas) = 0;

	/** Hot reload: the shaders whose files changed, or all of them when forced (Leon; UE: RecompileShaders). */
	virtual EShaderReloadResult ReloadShaders(bool bForce) = 0;

	/** The last frame's counters and pass times. */
	[[nodiscard]] virtual const FFrameStats& GetFrameStats() const = 0;

	/** Reads the draw framebuffer as bottom-up BGR rows with no padding (UE: FViewport::ReadPixels for screenshots). */
	virtual void ReadFramebufferBgr(int32 Width, int32 Height, TArray<uint8>& OutBgr) const = 0;
};

/** The Renderer module, or null in a target that does not link it (UE: FModuleManager::GetModulePtr). */
[[nodiscard]] ENGINE_API IRendererModule* GetRendererModulePtr();

/** The Renderer module; a target without it is a fatal error (UE: GetRendererModule, EngineModule.h). */
[[nodiscard]] ENGINE_API IRendererModule& GetRendererModule();

/**
 * Frees the renderer's GPU copy of an asset (IRendererModule::ReleaseAssetResources) when the target links the
 * Renderer module; nothing otherwise (LeonCook, the tools). The asset classes call it (UTexture::ReleaseResource,
 * UStaticMesh::ReleaseResources, USkeletalMesh::ReleaseResources).
 */
ENGINE_API void ReleaseAssetRenderResources(const UObject* Asset);
