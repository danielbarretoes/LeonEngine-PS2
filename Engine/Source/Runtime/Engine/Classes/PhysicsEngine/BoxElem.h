#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "BoxElem.generated.h"

/**
 * A box of a body's simple collision, in the body's local space (UE: FKBoxElem; Leon has no FKShapeElem base, name or
 * per-shape collision settings). X, Y and Z are the full edge lengths in centimetres, as in UE.
 */
USTRUCT()
struct ENGINE_API FKBoxElem
{
	GENERATED_BODY()

	FKBoxElem() = default;
	FKBoxElem(float InX, float InY, float InZ)
		: X(InX)
		, Y(InY)
		, Z(InZ)
	{
	}

	/** The box's centre in local space (cm). */
	UPROPERTY()
	FVector Center = FVector::ZeroVector;

	/** The box's rotation in local space. */
	UPROPERTY()
	FRotator Rotation = FRotator::ZeroRotator;

	/** Extent along X (cm). */
	UPROPERTY()
	float X = 1.0f;

	/** Extent along Y (cm). */
	UPROPERTY()
	float Y = 1.0f;

	/** Extent along Z (cm). */
	UPROPERTY()
	float Z = 1.0f;

	/** The box's axis-aligned bounds once Transform is applied (UE: CalcAABB). */
	[[nodiscard]] FBox CalcAABB(const FTransform& Transform) const;
};
