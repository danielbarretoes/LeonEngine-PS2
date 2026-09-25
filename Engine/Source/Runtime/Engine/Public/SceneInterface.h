#pragma once

#include "CoreMinimal.h"

class FScene;
class ULightComponent;
class UPrimitiveComponent;
class UWorld;

/**
 * The renderer's copy of a world (UE: FSceneInterface): the scene proxies of its primitives and its lights. The world
 * allocates it through IRendererModule::AllocateScene when it is created and frees it through RemoveScene when it is
 * destroyed; UWorld::Scene stays null when nothing can render (`-nullrhi`, a target without the Renderer module), and
 * the components then keep no render state beyond their flag.
 *
 * Components add themselves when they register (UPrimitiveComponent / ULightComponent::CreateRenderState_Concurrent)
 * and remove themselves when they unregister; UWorld::SendAllEndOfFrameUpdates sends the moved transforms before a
 * frame is drawn. Leon runs one thread: every call takes effect at once (UE queues them for the render thread).
 *
 * The primitives and lights keep the spawn order of their actors, then their order in the actor (a proxy recreated
 * for a changed component keeps its place): the renderer's draw order, shadow casters and first planar mirror depend
 * on it, as they depended on the order of the level's meshes.
 */
class ENGINE_API FSceneInterface
{
public:
	virtual ~FSceneInterface() = default;

	/** Adds the component's proxy (UPrimitiveComponent::CreateSceneProxy; nothing when it makes none). */
	virtual void AddPrimitive(UPrimitiveComponent* Primitive) = 0;
	/** Removes and deletes the component's proxy. */
	virtual void RemovePrimitive(UPrimitiveComponent* Primitive) = 0;
	/** Gives the component's proxy its current world transform. */
	virtual void UpdatePrimitiveTransform(UPrimitiveComponent* Primitive) = 0;

	/** Adds the light's proxy (ULightComponent::CreateSceneProxy). */
	virtual void AddLight(ULightComponent* Light) = 0;
	/** Removes and deletes the light's proxy. */
	virtual void RemoveLight(ULightComponent* Light) = 0;
	/** Gives the light's proxy its current world transform. */
	virtual void UpdateLightTransform(ULightComponent* Light) = 0;

	/** The world the scene belongs to. */
	[[nodiscard]] virtual UWorld* GetWorld() const = 0;

	/** The renderer's scene behind the interface (UE: GetRenderScene); only the Renderer module uses it. */
	[[nodiscard]] virtual FScene* GetRenderScene()
	{
		return nullptr;
	}

	/** How many primitive proxies and lights the scene holds. */
	[[nodiscard]] virtual int32 GetNumPrimitives() const = 0;
	[[nodiscard]] virtual int32 GetNumLights() const = 0;
};
