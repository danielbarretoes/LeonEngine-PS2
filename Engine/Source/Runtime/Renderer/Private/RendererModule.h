#pragma once

#include "CanvasRenderer.h"
#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "SceneRenderer.h"

/**
 * The Renderer module (UE: FRendererModule): it implements IRendererModule, which Engine finds by name. It owns the
 * scenes it allocates for the worlds, the scene renderer (with its GPU copies of the engine's assets) and the canvas
 * renderer.
 */
class FRendererModule final : public IRendererModule
{
public:
	// IRendererModule
	bool InitRenderer(const FString& ShaderDirectory) override;
	void ShutdownRenderer() override;
	void ReleaseAssetResources(const UObject* Asset) override;
	[[nodiscard]] bool IsRendererInitialized() const override
	{
		return bRendererInitialized;
	}
	[[nodiscard]] FSceneInterface* AllocateScene(UWorld* World) override;
	void RemoveScene(FSceneInterface* Scene) override;
	void BeginRenderingViewFamily(FCanvas* Canvas, FSceneViewFamily* ViewFamily) override;
	void DrawCanvas(const FCanvas& Canvas) override;
	EShaderReloadResult ReloadShaders(bool bForce) override;
	[[nodiscard]] const FFrameStats& GetFrameStats() const override
	{
		return SceneRenderer.GetFrameStats();
	}
	void ReadFramebufferBgr(int32 Width, int32 Height, TArray<uint8>& OutBgr) const override;

	// IModuleInterface
	void ShutdownModule() override;

private:
	/** The live scenes, freed by RemoveScene (or at shutdown). */
	TArray<FSceneInterface*> Scenes;
	FSceneRenderer SceneRenderer;
	FCanvasRenderer CanvasRenderer;
	bool bRendererInitialized = false;
};
