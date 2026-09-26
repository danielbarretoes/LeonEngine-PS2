#include "GameFramework/ProjectileMovementComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

namespace
{

	/** How far a sweep stops short of what it hit, cm (UE: the primitive's pull-back after a blocking hit). */
	constexpr float PullBackDistance = 0.125f;

	/** A step shorter than this is not simulated, seconds (UE: MIN_TICK_TIME). */
	constexpr float MinTickTime = 1.0e-6f;

} // namespace

UProjectileMovementComponent::UProjectileMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bRotationFollowsVelocity = false;
	bShouldBounce = false;
	bInitialVelocityInLocalSpace = true;
	bSimulationEnabled = true;
	// UE: a unit velocity along the component's forward, which InitialSpeed scales.
	Velocity = FVector(1.0f, 0.0f, 0.0f);
	bWantsInitializeComponent = true;
	SetComponentTickEnabled(true);
}

void UProjectileMovementComponent::InitializeComponent()
{
	Super::InitializeComponent();
	if (Velocity.SizeSquared() <= 0.0f)
	{
		return;
	}
	if (InitialSpeed > 0.0f)
	{
		Velocity = Velocity.GetSafeNormal() * InitialSpeed;
	}
	if (bInitialVelocityInLocalSpace)
	{
		SetVelocityInLocalSpace(Velocity);
	}
	if (bRotationFollowsVelocity && UpdatedComponent != nullptr)
	{
		UpdatedComponent->SetWorldRotation(Velocity.Rotation());
	}
}

void UProjectileMovementComponent::SetVelocityInLocalSpace(const FVector& NewVelocity)
{
	if (UpdatedComponent != nullptr)
	{
		Velocity = UpdatedComponent->GetComponentQuat().RotateVector(NewVelocity);
	}
}

float UProjectileMovementComponent::GetGravityZ() const
{
	const UWorld* World = GetWorld();
	const float WorldGravityZ = World != nullptr ? World->GetGravityZ() : UWorld::DefaultGravityZ;
	return WorldGravityZ * ProjectileGravityScale;
}

FVector UProjectileMovementComponent::LimitVelocity(FVector NewVelocity) const
{
	if (MaxSpeed > 0.0f)
	{
		NewVelocity = NewVelocity.GetClampedToMaxSize(MaxSpeed);
	}
	return NewVelocity;
}

FVector UProjectileMovementComponent::ComputeVelocity(const FVector& InitialVelocity, float DeltaTime) const
{
	const FVector Acceleration(0.0f, 0.0f, GetGravityZ());
	return LimitVelocity(InitialVelocity + (Acceleration * DeltaTime));
}

FVector UProjectileMovementComponent::ComputeMoveDelta(const FVector& InVelocity, float DeltaTime) const
{
	// Velocity Verlet over the step: v * t + a * t^2 / 2 (UE).
	const FVector Acceleration(0.0f, 0.0f, GetGravityZ());
	return (InVelocity * DeltaTime) + (Acceleration * (0.5f * DeltaTime * DeltaTime));
}

FVector UProjectileMovementComponent::ComputeBounceResult(const FHitResult& Hit) const
{
	FVector TempVelocity = Velocity;
	const FVector Normal = Hit.ImpactNormal;
	const float VDotNormal = FVector::DotProduct(TempVelocity, Normal);
	if (VDotNormal < 0.0f)
	{
		// The normal part goes: only the tangential velocity is left, which the friction slows; then the restitution
		// gives back Bounciness of the normal part, the other way (UE).
		const FVector ProjectedNormal = Normal * -VDotNormal;
		TempVelocity += ProjectedNormal;
		TempVelocity *= FMath::Clamp(1.0f - Friction, 0.0f, 1.0f);
		TempVelocity += ProjectedNormal * FMath::Max(Bounciness, 0.0f);
		TempVelocity = LimitVelocity(TempVelocity);
	}
	return TempVelocity;
}

void UProjectileMovementComponent::StopSimulating(const FHitResult& HitResult)
{
	Velocity = FVector::ZeroVector;
	SetUpdatedComponent(nullptr);
	OnProjectileStop.Broadcast(HitResult);
}

void UProjectileMovementComponent::MoveUpdatedComponent(const FVector& Delta, FHitResult& OutHit)
{
	OutHit = FHitResult();
	const FVector Start = UpdatedComponent->GetComponentLocation();
	UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(UpdatedComponent);
	UWorld* World = GetWorld();
	const float DeltaSize = Delta.Size();
	FVector NewLocation = Start + Delta;
	if (Primitive != nullptr && World != nullptr && DeltaSize > 0.0f)
	{
		// UE: the primitive's sweep on its object type with its responses, ignoring its owner and MoveIgnoreActors.
		FCollisionQueryParams Params(FName(TEXT("ProjectileMovement")), false, GetOwner());
		Params.AddIgnoredComponent(Primitive);
		for (const AActor* Ignored : Primitive->MoveIgnoreActors)
		{
			Params.AddIgnoredActor(Ignored);
		}
		const FCollisionResponseParams Responses(Primitive->GetCollisionResponseToChannels());
		FHitResult Hit;
		if (World->GetPhysicsScene().SweepSingleByChannel(Hit, Start, Start + Delta, FQuat::Identity,
				Primitive->GetCollisionObjectType(), Primitive->GetCollisionShape(), Params, Responses) &&
			Hit.bBlockingHit)
		{
			const float Time = FMath::Max(0.0f, Hit.Time - (PullBackDistance / DeltaSize));
			NewLocation = Start + (Delta * Time);
			OutHit = Hit;
		}
	}
	UpdatedComponent->SetWorldLocation(NewLocation);
	if (bRotationFollowsVelocity && !Velocity.IsNearlyZero(0.01f))
	{
		UpdatedComponent->SetWorldRotation(Velocity.Rotation());
	}
	if (Primitive != nullptr)
	{
		Primitive->SendPhysicsTransform();
	}
}

bool UProjectileMovementComponent::HandleImpact(const FHitResult& Hit)
{
	if (!bShouldBounce)
	{
		StopSimulating(Hit);
		return false;
	}
	const FVector OldVelocity = Velocity;
	Velocity = ComputeBounceResult(Hit);
	OnProjectileBounce.Broadcast(Hit, OldVelocity);
	// The event may change the velocity (UE checks the threshold after it).
	Velocity = LimitVelocity(Velocity);
	if (Velocity.SizeSquared() < FMath::Square(BounceVelocityStopSimulatingThreshold))
	{
		StopSimulating(Hit);
		return false;
	}
	return true;
}

void UProjectileMovementComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);
	if (HasStoppedSimulation() || DeltaTime <= 0.0f)
	{
		return;
	}
	const AActor* Owner = GetOwner();
	float RemainingTime = DeltaTime;
	int32 Iterations = 0;
	while (RemainingTime >= MinTickTime && Iterations < MaxSimulationIterations && !HasStoppedSimulation() &&
		(Owner == nullptr || !Owner->IsPendingKillPending()))
	{
		++Iterations;
		const float TimeTick =
			MaxSimulationTimeStep > 0.0f ? FMath::Min(RemainingTime, MaxSimulationTimeStep) : RemainingTime;
		RemainingTime -= TimeTick;

		const FVector OldVelocity = Velocity;
		const FVector MoveDelta = ComputeMoveDelta(OldVelocity, TimeTick);
		FHitResult Hit;
		MoveUpdatedComponent(MoveDelta, Hit);
		if (!Hit.bBlockingHit)
		{
			Velocity = ComputeVelocity(OldVelocity, TimeTick);
			continue;
		}
		// The velocity at the hit, then the bounce; the rest of the step goes on after it (UE).
		Velocity = Hit.Time > 1.0e-4f ? ComputeVelocity(OldVelocity, TimeTick * Hit.Time) : OldVelocity;
		if (!HandleImpact(Hit))
		{
			break;
		}
		RemainingTime += TimeTick * (1.0f - Hit.Time);
	}
}
