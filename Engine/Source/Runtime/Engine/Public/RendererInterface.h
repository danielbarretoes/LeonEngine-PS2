#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class FSceneInterface;
class UWorld;

/**
 * The Renderer module's interface to the engine (UE: IRendererModule): Engine only knows this and FSceneInterface,
 * and finds the module by name (GetRendererModule), so Engine never includes a Renderer header. The Renderer module
 * implements it (FRendererModule) and depends on Engine, as in UE.
 */
class IRendererModule : public IModuleInterface
{
public:
	/** A new scene for a world (UE: AllocateScene); the world frees it with RemoveScene. */
	[[nodiscard]] virtual FSceneInterface* AllocateScene(UWorld* World) = 0;
	/** Frees a scene AllocateScene returned, with the proxies it still holds (UE: RemoveScene). */
	virtual void RemoveScene(FSceneInterface* Scene) = 0;
};

/** The Renderer module, or null in a target that does not link it (UE: FModuleManager::GetModulePtr). */
[[nodiscard]] ENGINE_API IRendererModule* GetRendererModulePtr();

/** The Renderer module; a target without it is a fatal error (UE: GetRendererModule, EngineModule.h). */
[[nodiscard]] ENGINE_API IRendererModule& GetRendererModule();
