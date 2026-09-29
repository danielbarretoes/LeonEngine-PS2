#include "ShooterCharacterMovement.h"

#include "Engine/TriggerVolume.h"
#include "Engine/World.h"
#include "ShooterCharacter.h"
#include "ShooterGameMode.h"
#include "Weapons/ShooterWeapon.h"

namespace
{

	/** CS's movement runs once a user command, 10 ms at CS's usual 100 frames a second (s). */
	constexpr float CSCommandTime = 0.01f;

	/**
	 * The face of a ladder (a thin box against its wall, CS's func_ladder): the box's thinner horizontal axis, pointing
	 * from its middle to the side the climber at Feet is on.
	 */
	FVector GetLadderNormal(const ATriggerVolume& Ladder, const FVector& Feet)
	{
		const FBox Box = Ladder.GetBrushBounds();
		const FVector Extent = Box.GetExtent();
		const FVector Center = Box.GetCenter();
		if (Extent.X <= Extent.Y)
		{
			return FVector(Feet.X >= Center.X ? 1.0f : -1.0f, 0.0f, 0.0f);
		}
		return FVector(0.0f, Feet.Y >= Center.Y ? 1.0f : -1.0f, 0.0f);
	}

} // namespace

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
	float Speed = Super::GetMaxSpeed();
	// The drawn weapon's weight (CS: 221 units a second with an AK-47, 150 scoped with an AWP).
	const AShooterCharacter* Shooter = Cast<AShooterCharacter>(GetCharacterOwner());
	if (const AShooterWeapon* Weapon = Shooter != nullptr ? Shooter->GetWeapon() : nullptr)
	{
		Speed *= Weapon->GetSpeedModifier();
	}
	if (bIsWalking && !IsCrouching() && GetCharacterOwner() != nullptr && GetCharacterOwner()->IsMovingOnGround())
	{
		Speed *= WalkSpeedModifier;
	}
	return Speed * VelocityModifier;
}

float UShooterCharacterMovement::GetFallDamage(float LandingSpeed) const
{
	if (LandingSpeed <= SafeFallSpeed || FatalFallSpeed <= SafeFallSpeed)
	{
		return 0.0f;
	}
	// CS 1.6's multiplayer FlPlayerFallDamage: (speed - PLAYER_MAX_SAFE_FALL_SPEED) x DAMAGE_FOR_FALL_SPEED x 1.25,
	// with DAMAGE_FOR_FALL_SPEED = 100 / (PLAYER_FATAL_FALL_SPEED - PLAYER_MAX_SAFE_FALL_SPEED).
	return (LandingSpeed - SafeFallSpeed) * 100.0f / (FatalFallSpeed - SafeFallSpeed) * FallDamageScale;
}

void UShooterCharacterMovement::StartJumpStamina()
{
	JumpStamina = JumpStaminaTime;
}

float UShooterCharacterMovement::GetJumpStaminaRatio(float Stamina) const
{
	return FMath::Clamp(1.0f - (Stamina * JumpStaminaSlowdown), 0.0f, 1.0f);
}

void UShooterCharacterMovement::ApplyTagging()
{
	VelocityModifier = FMath::Clamp(TaggingVelocityModifier, 0.0f, 1.0f);
	Velocity.X *= VelocityModifier;
	Velocity.Y *= VelocityModifier;
}

void UShooterCharacterMovement::UpdateMovementTimers(float DeltaTime)
{
	JumpStamina = FMath::Max(0.0f, JumpStamina - DeltaTime);
	if (VelocityModifier < 1.0f)
	{
		const float Recovery = TaggingRecoveryTime > 0.0f
			? (1.0f - FMath::Clamp(TaggingVelocityModifier, 0.0f, 1.0f)) * DeltaTime / TaggingRecoveryTime
			: 1.0f;
		VelocityModifier = FMath::Min(1.0f, VelocityModifier + Recovery);
	}
}

void UShooterCharacterMovement::ResetMovementModifiers()
{
	JumpStamina = 0.0f;
	VelocityModifier = 1.0f;
	bWantsToJumpOffLadder = false;
	bJumpedOffLadder = false;
	CurrentLadder = nullptr;
	if (IsOnLadder())
	{
		GetCharacterOwner()->SetMovementMode(EMovementMode::Falling);
	}
}

bool UShooterCharacterMovement::IsOnLadder() const
{
	const ACharacter* Owner = GetCharacterOwner();
	return Owner != nullptr && Owner->GetMovementMode() == EMovementMode::Custom &&
		Owner->GetCustomMovementMode() == static_cast<uint8>(EShooterCustomMovementMode::Ladder);
}

void UShooterCharacterMovement::JumpOffLadder()
{
	if (IsOnLadder())
	{
		bWantsToJumpOffLadder = true;
	}
}

void UShooterCharacterMovement::CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
{
	const ACharacter* Owner = GetCharacterOwner();
	if (JumpStamina > 0.0f && Owner != nullptr && Owner->IsMovingOnGround() && DeltaTime > 0.0f)
	{
		// CS's ratio once per 10 ms command, whatever the step.
		const float Scale = FMath::Pow(GetJumpStaminaRatio(JumpStamina), DeltaTime / CSCommandTime);
		Velocity.X *= Scale;
		Velocity.Y *= Scale;
	}
	Super::CalcVelocity(DeltaTime, Friction, bFluid, BrakingDeceleration);
}

ATriggerVolume* UShooterCharacterMovement::FindTouchedLadder() const
{
	const ACharacter* Owner = GetCharacterOwner();
	const UWorld* World = Owner != nullptr ? Owner->GetWorld() : nullptr;
	const AShooterGameMode* GameMode = World != nullptr ? World->GetAuthGameMode<AShooterGameMode>() : nullptr;
	if (GameMode == nullptr)
	{
		return nullptr;
	}
	const FCollisionShape Capsule = Owner->GetCapsule();
	return GameMode->FindLadder(
		Owner->GetActorLocation(), Capsule.GetCapsuleRadius(), 2.0f * Capsule.GetCapsuleHalfHeight());
}

void UShooterCharacterMovement::UpdateCharacterStateBeforeMovement(FPhysScene& PhysScene)
{
	Super::UpdateCharacterStateBeforeMovement(PhysScene);
	ACharacter* Owner = GetCharacterOwner();
	if (Owner == nullptr)
	{
		return;
	}
	// A bot looks for no ladder at all (it walks through them).
	ATriggerVolume* Ladder = bCanClimbLadders ? FindTouchedLadder() : nullptr;
	if (Ladder == nullptr)
	{
		bJumpedOffLadder = false;
	}
	if (IsOnLadder())
	{
		if (Ladder == nullptr)
		{
			// Off the top, the bottom or a side: the climb's velocity carries on (up onto the ledge).
			CurrentLadder = nullptr;
			bWantsToJumpOffLadder = false;
			Owner->SetMovementMode(EMovementMode::Falling);
			return;
		}
		CurrentLadder = Ladder;
	}
	else if (Ladder != nullptr && !bJumpedOffLadder)
	{
		// CS: the ladder's move replaces the velocity from the first touch (a fall onto it stops).
		CurrentLadder = Ladder;
		bWantsToJumpOffLadder = false;
		Velocity = FVector::ZeroVector;
		Owner->SetMovementMode(EMovementMode::Custom, static_cast<uint8>(EShooterCustomMovementMode::Ladder));
	}
}

void UShooterCharacterMovement::PhysCustom(FPhysScene& PhysScene, float DeltaTime)
{
	if (IsOnLadder())
	{
		PhysLadder(PhysScene, DeltaTime);
	}
}

void UShooterCharacterMovement::PhysLadder(FPhysScene& PhysScene, float DeltaTime)
{
	ACharacter* Owner = GetCharacterOwner();
	if (CurrentLadder == nullptr)
	{
		Owner->SetMovementMode(EMovementMode::Falling);
		return;
	}
	const FVector Normal = GetLadderNormal(*CurrentLadder, Owner->GetActorLocation());
	if (bWantsToJumpOffLadder)
	{
		// CS: the jump pushes the climber straight off the ladder's face; gravity takes it from the next move on.
		bWantsToJumpOffLadder = false;
		bJumpedOffLadder = true;
		CurrentLadder = nullptr;
		Velocity = Normal * LadderJumpOffSpeed;
		Owner->SetMovementMode(EMovementMode::Falling);
		return;
	}

	// CS's PM_LadderMove: the forward input along the view (its pitch included), the side input along its right.
	const FRotator View = Owner->GetViewRotation();
	const FVector YawForward = FRotator(0.0f, View.Yaw, 0.0f).Vector();
	const FVector YawRight = FRotator(0.0f, View.Yaw + 90.0f, 0.0f).Vector();
	const FVector Input = MaxAcceleration > 0.0f ? Acceleration / MaxAcceleration : FVector::ZeroVector;
	const float Forward = FVector::DotProduct(Input, YawForward) * LadderClimbSpeed;
	const float Right = FVector::DotProduct(Input, YawRight) * LadderClimbSpeed;
	if (FMath::IsNearlyZero(Forward) && FMath::IsNearlyZero(Right))
	{
		// No input: the climber hangs where it is (no gravity on a ladder).
		Velocity = FVector::ZeroVector;
		return;
	}
	const FVector Wish = (View.Vector() * Forward) + (YawRight * Right);
	// The part into the ladder's face turns into climbing (up the face: the ladder is vertical); the rest stays.
	const float IntoFace = FVector::DotProduct(Wish, Normal);
	Velocity = Wish - (Normal * IntoFace) - FVector(0.0f, 0.0f, IntoFace);
	FFindFloorResult Floor;
	Owner->FindFloor(PhysScene, Floor, Skin * 2.0f);
	if (IntoFace > KINDA_SMALL_NUMBER && Floor.bWalkableFloor && Floor.FloorDist <= Skin * 2.0f)
	{
		// CS: backing off the ladder on the floor steps away from it.
		Velocity += Normal * LadderClimbSpeed;
	}

	// Up or down first (a ceiling or the floor stops it), then along the ladder.
	FHitResult Hit;
	if (!SafeMoveUpdatedComponent(PhysScene, FVector(0.0f, 0.0f, Velocity.Z * DeltaTime), Hit))
	{
		Velocity.Z = 0.0f;
	}
	const FVector Along(Velocity.X * DeltaTime, Velocity.Y * DeltaTime, 0.0f);
	if (!SafeMoveUpdatedComponent(PhysScene, Along, Hit))
	{
		Velocity.X = 0.0f;
		Velocity.Y = 0.0f;
	}
}
