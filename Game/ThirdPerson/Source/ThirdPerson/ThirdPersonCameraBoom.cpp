#include "ThirdPersonCameraBoom.h"

#include "HAL/PlatformMath.h"
#include "PS2RHI.h"
#include "ThirdPersonLevel.h"

namespace
{
	constexpr float CameraYawRate = 2.8f;
	constexpr float CameraPitchRate = 1.6f;
	constexpr float PitchMin = 14.0f; // avoid grazing the floor (near-plane smear)
	constexpr float PitchMax = 72.0f;
	constexpr float CameraGroundClearance = 3.5f;
	constexpr float TwoPi = 6.28318530718f;
}

void FThirdPersonCameraBoom::ResetTo(float TargetX, float TargetY, float TargetZ)
{
	CurrentArmLength = TargetArmLength;
	LookX = TargetX;
	LookY = TargetY + SocketOffsetY;
	LookZ = TargetZ;
}

void FThirdPersonCameraBoom::AddOrbitInput(float RightX, float RightY)
{
	Yaw256 -= RightX * CameraYawRate;
	while (Yaw256 < 0.0f)
	{
		Yaw256 += 256.0f;
	}
	while (Yaw256 >= 256.0f)
	{
		Yaw256 -= 256.0f;
	}
	Pitch256 -= RightY * CameraPitchRate;
	if (Pitch256 < PitchMin)
	{
		Pitch256 = PitchMin;
	}
	if (Pitch256 > PitchMax)
	{
		Pitch256 = PitchMax;
	}
}

uint32 FThirdPersonCameraBoom::GetWrappedYaw256() const
{
	float Yaw = Yaw256;
	while (Yaw < 0.0f)
	{
		Yaw += 256.0f;
	}
	while (Yaw >= 256.0f)
	{
		Yaw -= 256.0f;
	}
	return static_cast<uint32>(Yaw) & 255u;
}

void FThirdPersonCameraBoom::Follow(float TargetX, float TargetY, float TargetZ)
{
	LookX = TargetX;
	LookZ = TargetZ;
	LookY = LookY * 0.70f + (TargetY + SocketOffsetY) * 0.30f;
}

void FThirdPersonCameraBoom::UpdateViewTarget(const FThirdPersonLevel& Level)
{
	const uint32 Yaw = GetWrappedYaw256();
	const uint32 Pitch = static_cast<uint32>(Pitch256) & 255u;
	const float HorizontalUnit = FPlatformMath::Cos256(Pitch);
	const float DirectionX = FPlatformMath::Sin256(Yaw) * HorizontalUnit;
	const float DirectionY = FPlatformMath::Sin256(Pitch);
	const float DirectionZ = FPlatformMath::Cos256(Yaw) * HorizontalUnit;

	const float Probed = Level.ProbeBoomLength(LookX, LookY, LookZ, LookX + DirectionX * TargetArmLength,
		LookY + DirectionY * TargetArmLength, LookZ + DirectionZ * TargetArmLength, TargetArmLength, MinArmLength,
		ProbeRadius);

	// Snap in fast on collision; ease out when clear.
	if (Probed < CurrentArmLength)
	{
		CurrentArmLength = Probed;
	}
	else
	{
		CurrentArmLength = CurrentArmLength * 0.82f + Probed * 0.18f;
	}

	FPS2ViewTarget View{};
	View.LocationX = LookX + DirectionX * CurrentArmLength;
	View.LocationY = LookY + DirectionY * CurrentArmLength;
	View.LocationZ = LookZ + DirectionZ * CurrentArmLength;
	if (View.LocationY < CameraGroundClearance)
	{
		View.LocationY = CameraGroundClearance;
	}
	View.Pitch = -(static_cast<float>(Pitch) * TwoPi) / 256.0f;
	View.Yaw = (static_cast<float>(Yaw) * TwoPi) / 256.0f;
	FPS2RHI::SetViewTarget(View);
}
