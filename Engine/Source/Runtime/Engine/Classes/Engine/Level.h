#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Level.generated.h"

class AActor;
class AWorldSettings;
class UWorld;

/**
 * A level of a world (UE: ULevel): its outer is the owning UWorld and it holds the world's actors in Actors (spawned
 * with the level as their outer, so the level keeps them alive through the garbage collector).
 *
 * A map (`.lmap` package) saves the world's persistent level with its actors and their components; loading the map
 * gives them back (PostLoad reconnects the level to its world). Their components give the physics scene its bodies
 * (CreatePhysicsState) and the renderer's scene its proxies (CreateRenderState_Concurrent) once the world registers
 * them (UWorld::UpdateWorldComponents).
 */
UCLASS()
class ENGINE_API ULevel : public UObject
{
	GENERATED_BODY()

public:
	/** The world this level belongs to (UE: OwningWorld); null for a standalone level. */
	UPROPERTY(Transient)
	UWorld* OwningWorld = nullptr;

	/**
	 * The actors of the level, in spawn order (UE: Actors). An entry becomes null when its actor is destroyed during a
	 * world tick; the world compacts the array once the tick ends. A saved map keeps the order; transient actors (the
	 * game mode, the players' actors) are not saved.
	 */
	UPROPERTY()
	TArray<AActor*> Actors;

	/** The level's settings actor (UE: GetWorldSettings); null for a level without one. */
	[[nodiscard]] AWorldSettings* GetWorldSettings() const
	{
		return WorldSettings;
	}
	void SetWorldSettings(AWorldSettings* NewWorldSettings)
	{
		WorldSettings = NewWorldSettings;
	}

	// UObject
	/** The owning world is the outer, and the actors that were not saved (transient ones) leave no null slot (UE). */
	void PostLoad() override;

private:
	/** UE: WorldSettings. */
	UPROPERTY()
	AWorldSettings* WorldSettings = nullptr;
};
