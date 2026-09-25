#pragma once

#include "CoreMinimal.h"
#include "RendererInterface.h"

/**
 * The Renderer module (UE: FRendererModule): it implements IRendererModule, which Engine finds by name, and owns the
 * scenes it allocates for the worlds.
 */
class FRendererModule final : public IRendererModule
{
public:
	// IRendererModule
	[[nodiscard]] FSceneInterface* AllocateScene(UWorld* World) override;
	void RemoveScene(FSceneInterface* Scene) override;

	// IModuleInterface
	void ShutdownModule() override;

private:
	/** The live scenes, freed by RemoveScene (or at shutdown). */
	TArray<FSceneInterface*> Scenes;
};
