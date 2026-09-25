#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NavigationWaypoint.generated.h"

/**
 * A point of a map's waypoint graph (Leon, plan phase P15; UE 4.27 navigates on a navmesh, and UE3's APathNode /
 * ANavigationPoint are the closest counterparts). The map importer places one per `NavWaypoint*` node, with the links
 * and flags of the node's extras; the waypoint navigation data and its path finding come later (P20), so for now the
 * engine only keeps them.
 *
 * Links are one way: a waypoint links the waypoints its node lists. Flags are names the navigation gives meaning to
 * (`Jump`, `Crouch`, ...).
 */
UCLASS()
class ENGINE_API ANavigationWaypoint : public AActor
{
	GENERATED_BODY()

public:
	ANavigationWaypoint(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The waypoints a path may go to from this one. */
	UPROPERTY()
	TArray<ANavigationWaypoint*> Links;

	/** What the navigation must know about this point. */
	UPROPERTY()
	TArray<FName> Flags;

	/** True when the waypoint has the flag (compared without case, as FName). */
	[[nodiscard]] bool HasFlag(FName Flag) const
	{
		return Flags.Contains(Flag);
	}
};
