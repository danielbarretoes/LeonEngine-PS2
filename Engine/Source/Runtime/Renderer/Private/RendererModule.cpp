#include "RendererModule.h"

#include "Modules/ModuleManager.h"
#include "RendererLog.h"
#include "ScenePrivate.h"

DEFINE_LOG_CATEGORY(LogRenderer);

IMPLEMENT_MODULE(FRendererModule, Renderer)

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

void FRendererModule::ShutdownModule()
{
	for (FSceneInterface* Scene : Scenes)
	{
		delete Scene;
	}
	Scenes.Empty();
}
