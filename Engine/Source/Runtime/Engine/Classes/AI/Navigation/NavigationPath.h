#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "NavigationPath.generated.h"

/**
 * A path found by the navigation (UE: UNavigationPath, which UNavigationSystemV1::FindPathToLocationSynchronously
 * returns): its points from the start to the end.
 */
UCLASS(Transient)
class ENGINE_API UNavigationPath : public UObject
{
	GENERATED_BODY()

public:
	UNavigationPath(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The points, the start first and the end last (UE: PathPoints). */
	UPROPERTY()
	TArray<FVector> PathPoints;

	/** A path was found (UE: IsValid). */
	[[nodiscard]] bool IsValid() const
	{
		return PathPoints.Num() > 1;
	}
	/** The length along the points, cm (UE: GetPathLength). */
	[[nodiscard]] float GetPathLength() const;
};
