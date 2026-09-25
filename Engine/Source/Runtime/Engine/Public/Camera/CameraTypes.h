#pragma once

#include "CoreMinimal.h"

/**
 * A point of view (UE: FMinimalViewInfo, Camera/CameraTypes.h): where the view is, where it looks and its field of
 * view. Leon's field of view is vertical (see UCameraComponent).
 */
struct ENGINE_API FMinimalViewInfo
{
	/** UE: Location. */
	FVector Location = FVector::ZeroVector;
	/** UE: Rotation. */
	FRotator Rotation = FRotator::ZeroRotator;
	/** Vertical field of view, degrees (UE: FOV, horizontal there). */
	float FOV = 60.0f;
};
