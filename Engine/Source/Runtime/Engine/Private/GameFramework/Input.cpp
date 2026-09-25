#include "GameFramework/Input.h"

FVector YawRelativeMove(const FRotator& Rotation, const FVector2D& MoveInput)
{
	if (MoveInput.IsZero())
	{
		return FVector(0.0f);
	}

	const FMatrix Yaw = FRotationMatrix(FRotator(0.0f, Rotation.Yaw, 0.0f));
	const FVector Forward = Yaw.GetUnitAxis(EAxis::X);
	const FVector Right = Yaw.GetUnitAxis(EAxis::Y);

	FVector Move = (Forward * MoveInput.X) + (Right * MoveInput.Y);
	const float Len = Move.Size();
	if (Len > 1.0e-4f)
	{
		Move /= Len;
	}
	return Move;
}

FVector CameraRelativeMove(const UCameraComponent& Camera, const FVector2D& MoveInput)
{
	return YawRelativeMove(Camera.GetViewRotation(), MoveInput);
}
