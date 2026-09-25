#include "GameFramework/Input.h"

namespace
{

	constexpr float DegToRad = PI / 180.0f;

} // namespace

FVector YawRelativeMoveXz(float YawDegrees, const FMoveAxes2D& Axes)
{
	if (!Axes.Any())
	{
		return FVector(0.0f);
	}

	const float YawRad = YawDegrees * DegToRad;
	// Match Camera orbit: world offset ≈ (cos(p)*cos(y), …, cos(p)*sin(y)); look ≈ -offset.xz
	const FVector Forward = FVector(-FMath::Cos(YawRad), 0.0f, -FMath::Sin(YawRad));
	const FVector Right = FVector(FMath::Sin(YawRad), 0.0f, -FMath::Cos(YawRad));

	FVector Move = (Forward * Axes.Z) + (Right * Axes.X);
	const float Len = Move.Size();
	if (Len > 1.0e-4f)
	{
		Move /= Len;
	}
	return Move;
}

FVector CameraRelativeMoveXz(const UCameraComponent& Camera, const FMoveAxes2D& Axes)
{
	return YawRelativeMoveXz(Camera.GetYawDegrees(), Axes);
}
