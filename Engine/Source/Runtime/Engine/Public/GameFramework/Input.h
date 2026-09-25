#pragma once

#include "Camera/CameraComponent.h"
#include "CoreMinimal.h"

/** Keyboard move axes: X = strafe (right), Z = forward (from mapped Move* actions). */
struct ENGINE_API FMoveAxes2D
{
	float X = 0.0f;
	float Z = 0.0f;

	[[nodiscard]] bool Any() const
	{
		return X != 0.0f || Z != 0.0f;
	}
};

/** Unit ground-plane (XY) move for the camera's view yaw: Axes.Z along its forward, Axes.X along its right. */
[[nodiscard]] FVector CameraRelativeMove(const UCameraComponent& Camera, const FMoveAxes2D& Axes);

/**
 * Same as CameraRelativeMove from an explicit view yaw (e.g. a SpringArm's look yaw): forward and right are the X and Y
 * axes of FRotationMatrix(FRotator(0, Yaw, 0)).
 */
[[nodiscard]] FVector YawRelativeMove(float YawDegrees, const FMoveAxes2D& Axes);
