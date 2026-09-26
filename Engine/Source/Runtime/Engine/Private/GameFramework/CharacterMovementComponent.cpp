#include "GameFramework/CharacterMovementComponent.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Physics/PhysScene.h"

namespace
{

	/** UE: MIN_TICK_TIME, the smallest braking sub-step. */
	constexpr float MinTickTime = 1.0e-6f;
	/** UE: BRAKE_TO_STOP_VELOCITY, cm/s: below it braking stops the character. */
	constexpr float BrakeToStopVelocity = 10.0f;
	/** UE: BrakingSubStepTime (1 / 33 s), clamped to [1 / 75, 1 / 20]. */
	constexpr float BrakingSubStepTime = 1.0f / 33.0f;

} // namespace

UCharacterMovementComponent::UCharacterMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bWantsToCrouch = false;
	// UE: the character movement ticks (PrimaryComponentTick), after the character's controller.
	SetComponentTickEnabled(true);
}

void UCharacterMovementComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);
	if (CharacterOwner != nullptr)
	{
		CharacterOwner->TickCharacterMovement(DeltaTime);
	}
}

void UCharacterMovementComponent::PostInitProperties()
{
	Super::PostInitProperties();
	CharacterOwner = Cast<ACharacter>(GetOwner());
}

float UCharacterMovementComponent::GetMaxSpeed() const
{
	if (CharacterOwner != nullptr && CharacterOwner->IsMovingOnGround() && IsCrouching())
	{
		return MaxWalkSpeedCrouched;
	}
	return MaxWalkSpeed;
}

void UCharacterMovementComponent::CalcVelocity(
	float DeltaTime, float Friction, bool /*bFluid*/, float BrakingDeceleration)
{
	if (DeltaTime < MinTickTime)
	{
		return;
	}
	Friction = FMath::Max(0.0f, Friction);
	const float MaxSpeed = GetMaxSpeed();

	// UE: the analog input scales the speed the input reaches.
	const float MaxInputSpeed = FMath::Max(MaxSpeed * AnalogInputModifier, 0.0f);
	const bool bZeroAcceleration = Acceleration.IsZero();
	const bool bVelocityOverMax = Velocity.SizeSquared() > FMath::Square(MaxSpeed);

	// Brake without an input, or when going faster than the speed.
	if (bZeroAcceleration || bVelocityOverMax)
	{
		const FVector OldVelocity = Velocity;
		const float ActualBrakingFriction = bUseSeparateBrakingFriction ? BrakingFriction : Friction;
		ApplyVelocityBraking(DeltaTime, ActualBrakingFriction, BrakingDeceleration);

		// Do not brake below the speed while the input still pushes along the velocity (UE).
		if (bVelocityOverMax && Velocity.SizeSquared() < FMath::Square(MaxSpeed) &&
			FVector::DotProduct(Acceleration, OldVelocity) > 0.0f)
		{
			Velocity = OldVelocity.GetSafeNormal() * MaxSpeed;
		}
	}
	else if (!bZeroAcceleration)
	{
		// Friction turns the velocity toward the input without changing its size (UE).
		const FVector AccelDir = Acceleration.GetSafeNormal();
		const float VelSize = Velocity.Size();
		Velocity = Velocity - (Velocity - AccelDir * VelSize) * FMath::Min(DeltaTime * Friction, 1.0f);
	}

	// Accelerate, up to the input's speed (or the current one when already faster).
	if (!bZeroAcceleration)
	{
		const float NewMaxInputSpeed =
			Velocity.SizeSquared() > FMath::Square(MaxInputSpeed) ? Velocity.Size() : MaxInputSpeed;
		Velocity += Acceleration * DeltaTime;
		Velocity = Velocity.GetClampedToMaxSize(NewMaxInputSpeed);
	}
}

void UCharacterMovementComponent::ApplyVelocityBraking(float DeltaTime, float Friction, float BrakingDeceleration)
{
	if (Velocity.IsZero() || DeltaTime < MinTickTime)
	{
		return;
	}

	const float FrictionFactor = FMath::Max(0.0f, BrakingFrictionFactor);
	Friction = FMath::Max(0.0f, Friction * FrictionFactor);
	BrakingDeceleration = FMath::Max(0.0f, BrakingDeceleration);
	const bool bZeroFriction = Friction == 0.0f;
	const bool bZeroBraking = BrakingDeceleration == 0.0f;
	if (bZeroFriction && bZeroBraking)
	{
		return;
	}

	const FVector OldVel = Velocity;

	// Sub-steps keep the result close at low frame rates (UE).
	float RemainingTime = DeltaTime;
	const float MaxTimeStep = FMath::Clamp(BrakingSubStepTime, 1.0f / 75.0f, 1.0f / 20.0f);

	// Decelerate against the velocity's direction.
	const FVector RevAccel = bZeroBraking ? FVector::ZeroVector : (-BrakingDeceleration * Velocity.GetSafeNormal());
	while (RemainingTime >= MinTickTime)
	{
		// Zero friction uses a constant deceleration, so no need for iteration.
		const float Dt = (RemainingTime > MaxTimeStep && !bZeroFriction) ? FMath::Min(MaxTimeStep, RemainingTime * 0.5f)
																		 : RemainingTime;
		RemainingTime -= Dt;

		// The friction scales with the velocity, the braking does not.
		Velocity = Velocity + ((-Friction) * Velocity + RevAccel) * Dt;

		// Do not reverse the direction.
		if ((Velocity | OldVel) <= 0.0f)
		{
			Velocity = FVector::ZeroVector;
			return;
		}
	}

	// Clamp to zero when nearly zero, or below the stopping speed with a braking deceleration.
	if (Velocity.SizeSquared() <= KINDA_SMALL_NUMBER ||
		(!bZeroBraking && Velocity.SizeSquared() <= FMath::Square(BrakeToStopVelocity)))
	{
		Velocity = FVector::ZeroVector;
	}
}

FVector UCharacterMovementComponent::GetAirControl(
	float /*DeltaTime*/, float TickAirControl, const FVector& FallAcceleration)
{
	// UE's BoostAirControl: more control at low speed.
	if (TickAirControl != 0.0f && AirControlBoostMultiplier > 0.0f &&
		Velocity.SizeSquared2D() < FMath::Square(AirControlBoostVelocityThreshold))
	{
		TickAirControl = FMath::Min(1.0f, AirControlBoostMultiplier * TickAirControl);
	}
	return FallAcceleration * TickAirControl;
}

bool UCharacterMovementComponent::IsCrouching() const
{
	return CharacterOwner != nullptr && CharacterOwner->bIsCrouched;
}

bool UCharacterMovementComponent::CanCrouchInCurrentState() const
{
	if (!CanEverCrouch() || CharacterOwner == nullptr)
	{
		return false;
	}
	return CharacterOwner->IsMovingOnGround() || CharacterOwner->IsFalling();
}

void UCharacterMovementComponent::Crouch(bool /*bClientSimulation*/)
{
	if (CharacterOwner == nullptr || !CanCrouchInCurrentState())
	{
		return;
	}
	UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
	const float OldUnscaledHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
	if (CharacterOwner->bIsCrouched && OldUnscaledHalfHeight == CrouchedHalfHeight)
	{
		return;
	}

	// UE: the capsule takes the crouched half height (at least its radius).
	Capsule->SetCapsuleSize(Capsule->GetUnscaledCapsuleRadius(), CrouchedHalfHeight);
	const float HalfHeightAdjust = OldUnscaledHalfHeight - Capsule->GetUnscaledCapsuleHalfHeight();
	const float ComponentScale = OldUnscaledHalfHeight > 0.0f
		? CharacterOwner->GetCapsule().GetCapsuleHalfHeight() / Capsule->GetUnscaledCapsuleHalfHeight()
		: 1.0f;
	const float ScaledHalfHeightAdjust = HalfHeightAdjust * ComponentScale;

	// The capsule stands on the feet: on the ground they stay (UE's bCrouchMaintainsBaseLocation); in the air the
	// capsule keeps its centre, so the feet come up (legs tucked, UE).
	if (!CharacterOwner->IsMovingOnGround())
	{
		CharacterOwner->MutableLocation().Z += ScaledHalfHeightAdjust;
	}

	CharacterOwner->bIsCrouched = true;
	CharacterOwner->OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	Capsule->SendPhysicsTransform();
}

bool UCharacterMovementComponent::UnCrouch(bool /*bClientSimulation*/)
{
	if (CharacterOwner == nullptr || !CharacterOwner->bIsCrouched)
	{
		return true;
	}
	UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
	const ACharacter* DefaultCharacter = CharacterOwner->GetClass()->GetDefaultObject<ACharacter>();
	const float StandHalfHeight = DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	const float OldUnscaledHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
	const float HalfHeightAdjust = StandHalfHeight - OldUnscaledHalfHeight;
	const float ComponentScale = OldUnscaledHalfHeight > 0.0f
		? CharacterOwner->GetCapsule().GetCapsuleHalfHeight() / OldUnscaledHalfHeight
		: 1.0f;
	const float ScaledHalfHeightAdjust = HalfHeightAdjust * ComponentScale;

	FPhysScene* PhysScene = CrouchPhysScene;
	if (PhysScene == nullptr && CharacterOwner->GetWorld() != nullptr)
	{
		PhysScene = &CharacterOwner->GetWorld()->GetPhysicsScene();
	}

	FVector Feet = CharacterOwner->GetActorLocation();
	const float Radius = CharacterOwner->GetCapsule().GetCapsuleRadius();
	const float CrouchedHalf = CharacterOwner->GetCapsule().GetCapsuleHalfHeight();
	FVector NewFeet = Feet;
	if (!CharacterOwner->IsMovingOnGround() && PhysScene != nullptr)
	{
		// In the air the capsule grows around its centre (UE): the feet go down, not below the floor under them.
		NewFeet.Z -= ScaledHalfHeightAdjust;
		FFindFloorResult Floor;
		CharacterOwner->FindFloor(*PhysScene, Floor, ScaledHalfHeightAdjust + Skin);
		if (Floor.bBlockingHit && Floor.bWalkableFloor)
		{
			NewFeet.Z = FMath::Max(NewFeet.Z, Floor.Hit.ImpactPoint.Z);
		}
	}

	// UE: the standing capsule must not encroach blocking geometry. Leon sweeps the crouched capsule from where it is
	// up by the height the top gains; a surface facing down on the way is a ceiling that keeps the character crouched.
	const float TopNow = Feet.Z + 2.0f * CrouchedHalf;
	const float TopStanding = NewFeet.Z + 2.0f * (CrouchedHalf + ScaledHalfHeightAdjust);
	if (PhysScene != nullptr && TopStanding > TopNow)
	{
		FCollisionQueryParams Query;
		FCollisionResponseParams Response;
		CharacterOwner->InitCollisionParams(Query, Response);
		const float Cylinder = FMath::Max(0.0f, CrouchedHalf - Radius);
		const FVector Start = Feet + FVector(0.0f, 0.0f, CrouchedHalf);
		const FVector End = Start + FVector(0.0f, 0.0f, TopStanding - TopNow);
		TArray<FHitResult> Hits;
		(void)PhysScene->CapsuleTraceMultiByChannel(
			Hits, Start, End, Radius, Cylinder, CharacterOwner->GetMovementTraceChannel(), Query, nullptr, Response);
		for (const FHitResult& Hit : Hits)
		{
			if (Hit.bBlockingHit && !Hit.bFloorPlane && Hit.ImpactNormal.Z < -0.1f)
			{
				return false;
			}
		}
	}

	Capsule->SetCapsuleSize(Capsule->GetUnscaledCapsuleRadius(), StandHalfHeight);
	CharacterOwner->MutableLocation().Z = NewFeet.Z;
	CharacterOwner->bIsCrouched = false;
	CharacterOwner->OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	Capsule->SendPhysicsTransform();
	return true;
}

void UCharacterMovementComponent::UpdateCharacterStateBeforeMovement(FPhysScene& PhysScene)
{
	if (CharacterOwner == nullptr)
	{
		return;
	}
	CrouchPhysScene = &PhysScene;
	const bool bIsCrouching = IsCrouching();
	if (bIsCrouching && (!bWantsToCrouch || !CanCrouchInCurrentState()))
	{
		(void)UnCrouch(false);
	}
	else if (!bIsCrouching && bWantsToCrouch && CanCrouchInCurrentState())
	{
		Crouch(false);
	}
	CrouchPhysScene = nullptr;
}
