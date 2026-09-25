#include "GameFramework/Input.h"

FVector YawRelativeMove(float YawDegrees, const FMoveAxes2D& Axes)
{
	if (!Axes.Any())
	{
		return FVector(0.0f);
	}

	const FMatrix Yaw = FRotationMatrix(FRotator(0.0f, YawDegrees, 0.0f));
	const FVector Forward = Yaw.GetUnitAxis(EAxis::X);
	const FVector Right = Yaw.GetUnitAxis(EAxis::Y);

	FVector Move = (Forward * Axes.Z) + (Right * Axes.X);
	const float Len = Move.Size();
	if (Len > 1.0e-4f)
	{
		Move /= Len;
	}
	return Move;
}

FVector CameraRelativeMove(const UCameraComponent& Camera, const FMoveAxes2D& Axes)
{
	return YawRelativeMove(Camera.GetYawDegrees(), Axes);
}
