#include "ThirdPersonCharacter.h"

#include "HAL/PlatformMath.h"
#include "PS2RHI.h"
#include "ThirdPersonLevel.h"

namespace
{
	constexpr float MoveSpeed = 0.55f;
	constexpr float Gravity = 0.045f;
	constexpr float JumpSpeed = 0.95f;
	constexpr float GroundSkin = 0.08f;

	float MaxF(float A, float B)
	{
		return A > B ? A : B;
	}

	float MinF(float A, float B)
	{
		return A < B ? A : B;
	}
}

void FThirdPersonCharacter::SpawnAt(const FThirdPersonLevel& Level)
{
	const float SpawnSupport = Level.FindSupportY(0.0f, 0.0f, HalfWidth, 0.0f);
	LocationY =
		(SpawnSupport > FThirdPersonLevel::NoSupport ? SpawnSupport : FThirdPersonLevel::GroundTopY) + HalfHeight;
}

void FThirdPersonCharacter::Move(
	float ForwardInput, float RightInput, uint32 CameraYaw256, bool bJump, const FThirdPersonLevel& Level)
{
	const float ForwardX = -FPlatformMath::Sin256(CameraYaw256);
	const float ForwardZ = -FPlatformMath::Cos256(CameraYaw256);
	const float RightX = FPlatformMath::Cos256(CameraYaw256);
	const float RightZ = -FPlatformMath::Sin256(CameraYaw256);
	// The camera basis is orthonormal, so the wish length matches the stick magnitude.
	float WishX = ForwardX * ForwardInput + RightX * RightInput;
	float WishZ = ForwardZ * ForwardInput + RightZ * RightInput;
	// Each stick axis reaches +-1 independently: cap diagonals to unit length.
	const float WishLengthSquared = WishX * WishX + WishZ * WishZ;
	if (WishLengthSquared > 1.0f)
	{
		// One Newton step from 1 is enough for a length in (1, sqrt(2)].
		const float InvLength = 1.0f / (0.5f * (WishLengthSquared + 1.0f));
		WishX *= InvLength;
		WishZ *= InvLength;
	}

	if (WishX != 0.0f || WishZ != 0.0f)
	{
		VelocityX = WishX * MoveSpeed;
		VelocityZ = WishZ * MoveSpeed;
		Yaw256 = CameraYaw256;
	}
	else
	{
		VelocityX *= 0.7f;
		VelocityZ *= 0.7f;
		if (VelocityX > -0.02f && VelocityX < 0.02f)
		{
			VelocityX = 0.0f;
		}
		if (VelocityZ > -0.02f && VelocityZ < 0.02f)
		{
			VelocityZ = 0.0f;
		}
	}

	if (bJump && bOnGround)
	{
		VelocityY = JumpSpeed;
		bOnGround = false;
	}

	VelocityY -= Gravity;
	LocationX += VelocityX;
	LocationY += VelocityY;
	LocationZ += VelocityZ;

	const float Bound = FThirdPersonLevel::ArenaHalfExtent - 2.0f;
	LocationX = MinF(MaxF(LocationX, -Bound), Bound);
	LocationZ = MinF(MaxF(LocationZ, -Bound), Bound);
	Level.ResolveWallCollisions(LocationX, LocationZ, HalfWidth, LocationY - HalfHeight, LocationY + HalfHeight);

	const float FeetY = LocationY - HalfHeight;
	const float Support = Level.FindSupportY(LocationX, LocationZ, HalfWidth, FeetY);
	if (Support > FThirdPersonLevel::NoSupport && VelocityY <= 0.0f && FeetY <= Support + GroundSkin)
	{
		LocationY = Support + HalfHeight;
		VelocityY = 0.0f;
		bOnGround = true;
	}
	else
	{
		bOnGround = false;
	}
}

void FThirdPersonCharacter::Draw(const FPS2Material& Material) const
{
	FPS2RHI::BindMaterial(Material);
	(void)FPS2RHI::DrawBox(LocationX, LocationY, LocationZ, Yaw256, 0, HalfWidth, HalfHeight, HalfWidth);
	(void)FPS2RHI::DrawBox(LocationX, LocationY + HalfHeight * 0.85f, LocationZ, Yaw256, 0, HalfWidth * 0.55f,
		HalfWidth * 0.55f, HalfWidth * 0.55f);
}
