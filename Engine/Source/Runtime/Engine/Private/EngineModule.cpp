#include "Modules/ModuleManager.h"
#include "RendererInterface.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, Engine)

IRendererModule* GetRendererModulePtr()
{
	return FModuleManager::GetModulePtr<IRendererModule>("Renderer");
}

IRendererModule& GetRendererModule()
{
	return FModuleManager::LoadModuleChecked<IRendererModule>("Renderer");
}

void ReleaseAssetRenderResources(const UObject* Asset)
{
	if (IRendererModule* Renderer = GetRendererModulePtr())
	{
		Renderer->ReleaseAssetResources(Asset);
	}
}
