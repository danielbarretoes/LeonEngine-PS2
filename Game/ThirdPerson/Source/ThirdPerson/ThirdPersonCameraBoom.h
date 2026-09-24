#pragma once

#include "CoreTypes.h"

class FThirdPersonLevel;

/**
 * Orbit follow camera: spherical boom around a smoothed look-at point, pulled in when props block
 * it (TP_ThirdPerson: CameraBoom spring arm + FollowCamera). Angles in 1/256 turn.
 */
class FThirdPersonCameraBoom
{
public:
	float TargetArmLength = 28.0f;
	float MinArmLength = 12.0f;
	float SocketOffsetY = 3.0f;
	float ProbeRadius = 4.0f;
	float Yaw256 = 0.0f;
	float Pitch256 = 28.0f; // elevation above the look-at point

	/** Snaps the look-at point and the arm to the target (no smoothing). */
	void ResetTo(float TargetX, float TargetY, float TargetZ);

	/** Right stick: orbit (negated so stick right / up match the expected yaw / pitch). */
	void AddOrbitInput(float RightX, float RightY);

	/** Current yaw wrapped to [0, 255]. */
	uint32 GetWrappedYaw256() const;

	/** Follows the target: XZ snappy, Y slightly soft (jump / land). */
	void Follow(float TargetX, float TargetY, float TargetZ);

	/** Probes the props and publishes the camera as the PS2 RHI view target. */
	void UpdateViewTarget(const FThirdPersonLevel& Level);

private:
	float CurrentArmLength = 28.0f;
	float LookX = 0.0f;
	float LookY = 0.0f;
	float LookZ = 0.0f;
};
