#include "ShooterCharacterMovement.h"

#include "GameFramework/Character.h"

UShooterCharacterMovement::UShooterCharacterMovement(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Counter-Strike's movement at 1 unit = 2.54 cm (the class comment has the table).
	bInstantVelocity = false;
	MaxWalkSpeed = 635.0f;
	MaxWalkSpeedCrouched = 212.0f;
	MaxAcceleration = 3175.0f;
	GroundFriction = 4.0f;
	BrakingFrictionFactor = 1.0f;
	BrakingDecelerationWalking = 762.0f;
	BrakingDecelerationFalling = 0.0f;
	FallingLateralFriction = 0.0f;
	AirControl = 0.3f;
	Gravity = 2032.0f;
	JumpZVelocity = 682.0f;
	MaxStepHeight = 45.0f;
	WalkableFloorZ = 0.7f;
	// The CS duck hull is 36 units (91 cm) tall.
	CrouchedHalfHeight = 46.0f;
	NavAgentProps.bCanCrouch = true;
	// The whole map (Leon's movement clamps the feet to this square; de_leon is 60 x 50 m).
	WalkBounds = 100000.0f;
}

float UShooterCharacterMovement::GetMaxSpeed() const
{
	const float Speed = Super::GetMaxSpeed();
	if (bIsWalking && !IsCrouching() && GetCharacterOwner() != nullptr && GetCharacterOwner()->IsMovingOnGround())
	{
		return Speed * WalkSpeedModifier;
	}
	return Speed;
}
