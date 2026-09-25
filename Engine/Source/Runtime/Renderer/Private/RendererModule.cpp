#include "RendererModule.h"

#include "Modules/ModuleManager.h"
#include "RendererLog.h"
#include "ScenePrivate.h"
#include "SceneView.h"

DEFINE_LOG_CATEGORY(LogRenderer);

IMPLEMENT_MODULE(FRendererModule, Renderer)

bool FRendererModule::InitRenderer(const FString& ShaderDirectory)
{
	if (bRendererInitialized)
	{
		return true;
	}
	if (!SceneRenderer.Initialize(ShaderDirectory))
	{
		return false;
	}
	if (!CanvasRenderer.Initialize())
	{
		SceneRenderer.Shutdown();
		return false;
	}
	bRendererInitialized = true;
	return true;
}

void FRendererModule::ShutdownRenderer()
{
	if (!bRendererInitialized)
	{
		return;
	}
	CanvasRenderer.Shutdown();
	SceneRenderer.Shutdown();
	bRendererInitialized = false;
}

void FRendererModule::ReleaseAssetResources(const UObject* Asset)
{
	SceneRenderer.ReleaseAssetResources(Asset);
}

FSceneInterface* FRendererModule::AllocateScene(UWorld* World)
{
	FScene* Scene = new FScene(World);
	Scenes.Add(Scene);
	return Scene;
}

void FRendererModule::RemoveScene(FSceneInterface* Scene)
{
	if (Scene != nullptr && Scenes.Remove(Scene) > 0)
	{
		delete Scene;
	}
}

void FRendererModule::BeginRenderingViewFamily(FCanvas* /*Canvas*/, FSceneViewFamily* ViewFamily)
{
	if (!bRendererInitialized || ViewFamily == nullptr)
	{
		return;
	}
	SceneRenderer.BeginFrame(ViewFamily->RenderTargetSizeX, ViewFamily->RenderTargetSizeY);
	SceneRenderer.Render(*ViewFamily);
}

void FRendererModule::DrawCanvas(const FCanvas& Canvas)
{
	if (bRendererInitialized)
	{
		CanvasRenderer.Draw(Canvas);
	}
}

EShaderReloadResult FRendererModule::ReloadShaders(bool bForce)
{
	if (!bRendererInitialized)
	{
		return EShaderReloadResult::Unchanged;
	}
	const EShaderReloadResult Result = SceneRenderer.ReloadShaders(bForce);
	return MergeShaderReload(Result, CanvasRenderer.ReloadShader(bForce));
}

void FRendererModule::ReadFramebufferBgr(int32 Width, int32 Height, TArray<uint8>& OutBgr) const
{
	SceneRenderer.ReadFramebufferBgr(Width, Height, OutBgr);
}

void FRendererModule::ShutdownModule()
{
	for (FSceneInterface* Scene : Scenes)
	{
		delete Scene;
	}
	Scenes.Empty();
}
