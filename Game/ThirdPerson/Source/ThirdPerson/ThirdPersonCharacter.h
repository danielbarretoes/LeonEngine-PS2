#pragma once

#include "CoreTypes.h"

class FThirdPersonLevel;
struct FPS2Material;

/**
 * Player character: camera-relative movement, jump, gravity, step-up and wall push-out against the
 * level props (TP_ThirdPerson: ThirdPersonCharacter + CharacterMovementComponent, reduced).
 */
class FThirdPersonCharacter
{
public:
	static constexpr float HalfWidth = 1.6f;
	static constexpr float HalfHeight = 2.4f;

	float LocationX = 0.0f;
	float LocationY = 2.0f;
	float LocationZ = 0.0f;
	uint32 Yaw256 = 0;
	float VelocityX = 0.0f;
	float VelocityY = 0.0f;
	float VelocityZ = 0.0f;
	bool bOnGround = true;

	/** Stands the character on whatever supports the origin. */
	void SpawnAt(const FThirdPersonLevel& Level);

	/** Left stick (forward / back, strafe) relative to the camera yaw; bJump on press. */
	void Move(float ForwardInput, float RightInput, uint32 CameraYaw256, bool bJump, const FThirdPersonLevel& Level);

	void Draw(const FPS2Material& Material) const;
};
