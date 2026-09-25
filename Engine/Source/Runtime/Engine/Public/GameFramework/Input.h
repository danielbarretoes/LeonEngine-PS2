#pragma once

#include "Camera/CameraComponent.h"
#include "CoreMinimal.h"

/**
 * Unit ground-plane (XY) move for a view or control rotation (only its yaw is used): MoveInput.X along its forward,
 * MoveInput.Y along its right, the X and Y axes of FRotationMatrix(FRotator(0, Rotation.Yaw, 0)) as in UE's
 * AddMovementInput from the control rotation. Zero input gives a zero move.
 */
[[nodiscard]] FVector YawRelativeMove(const FRotator& Rotation, const FVector2D& MoveInput);

/** YawRelativeMove for the camera's view rotation. */
[[nodiscard]] FVector CameraRelativeMove(const UCameraComponent& Camera, const FVector2D& MoveInput);
