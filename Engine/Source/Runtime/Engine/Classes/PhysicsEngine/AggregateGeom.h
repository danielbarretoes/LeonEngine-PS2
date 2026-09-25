#pragma once

#include "CoreMinimal.h"
#include "PhysicsEngine/BoxElem.h"
#include "AggregateGeom.generated.h"

/**
 * A body's simple collision shapes (UE: FKAggregateGeom). Leon has boxes only (UE also has spheres, capsules and
 * convex hulls).
 */
USTRUCT()
struct ENGINE_API FKAggregateGeom
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FKBoxElem> BoxElems;

	/** Number of shapes (UE: GetElementCount). */
	[[nodiscard]] int32 GetElementCount() const
	{
		return BoxElems.Num();
	}

	/** The axis-aligned bounds of every shape once Transform is applied; an invalid box without shapes (UE). */
	[[nodiscard]] FBox CalcAABB(const FTransform& Transform) const;

	void EmptyElements()
	{
		BoxElems.Empty();
	}
};
