#pragma once

#include "CoreMinimal.h"

/**
 * Transform of the legacy Y-up metre world (level data, lights, scene components): XYZ Euler degrees, applied as
 * T * Rx * Ry * Rz * S. It goes away in P7, when the level readers convert to UE's Z-up centimetre FTransform.
 * The matrices it builds use the renderer's GL conventions (LegacyGLMath.h).
 */
struct ENGINE_API FLegacyTransform
{
	FVector Position = FVector::ZeroVector;
	FVector RotationDegrees = FVector::ZeroVector; // XYZ Euler, degrees
	FVector Scale = FVector::OneVector;

	FLegacyTransform() = default;

	FLegacyTransform(const FVector& InPosition, const FVector& InRotationDegrees, const FVector& InScale)
		: Position(InPosition)
		, RotationDegrees(InRotationDegrees)
		, Scale(InScale)
	{
	}

	/** T * Rx * Ry * Rz * S with a scale kept away from zero (GL convention, LegacyGLMath.h). */
	[[nodiscard]] FMatrix ModelMatrix() const;
};
