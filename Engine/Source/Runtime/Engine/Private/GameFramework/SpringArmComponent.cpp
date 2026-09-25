#include "GameFramework/SpringArmComponent.h"

#include "Debug/DebugDraw.h"
#include "Engine/World.h"
#include "Physics/PhysScene.h"

namespace
{

	[[nodiscard]] FPhysScene* ResolvePhysScene(AActor* Owner, FPhysScene* ExplicitScene)
	{
		if (ExplicitScene != nullptr)
		{
			return ExplicitScene;
		}
		if (Owner == nullptr)
		{
			return nullptr;
		}
		UWorld* World = Owner->GetWorld();
		return World != nullptr ? &World->GetPhysicsScene() : nullptr;
	}

} // namespace

FVector USpringArmComponent::GetBoomDirection(float YawDegrees, float PitchDegrees)
{
	return FRotator(PitchDegrees, YawDegrees, 0.0f).Vector();
}

FVector USpringArmComponent::GetTargetLocation(const FVector& ActorLocation) const
{
	// The view's right axis (the same as YawRelativeMove's).
	const FVector Right = FRotationMatrix(FRotator(0.0f, BoomYawDegrees + 180.0f, 0.0f)).GetUnitAxis(EAxis::Y);
	return ActorLocation + FVector(0.0f, 0.0f, SocketOffsetZ) + (Right * SocketOffsetX);
}

void USpringArmComponent::SnapLagState(const FVector& ActorLocation)
{
	LaggedTarget = GetTargetLocation(ActorLocation);
	LaggedYawDegrees = BoomYawDegrees;
	LaggedPitchDegrees = BoomPitchDegrees;
	LaggedArmLength = TargetArmLength;
	bLagInitialized = true;
}

float USpringArmComponent::GetLookFacingYawDegrees() const
{
	// The camera looks back along the boom.
	return FRotator::NormalizeAxis(BoomYawDegrees + 180.0f);
}

float USpringArmComponent::ExpSmoothAlpha(float Speed, float DeltaTime)
{
	if (Speed <= 0.0f || DeltaTime <= 0.0f)
	{
		return 1.0f;
	}
	return 1.0f - FMath::Exp(-Speed * DeltaTime);
}

float USpringArmComponent::LerpAngleDegrees(float FromDegrees, float ToDegrees, float Alpha)
{
	float Delta = FMath::Fmod(ToDegrees - FromDegrees + 540.0f, 360.0f) - 180.0f;
	return FromDegrees + (Delta * Alpha);
}

void USpringArmComponent::UpdateLag(float DeltaTime, const FVector& ActorLocation)
{
	const FVector DesiredTarget = GetTargetLocation(ActorLocation);
	if (!bLagInitialized)
	{
		LaggedTarget = DesiredTarget;
		LaggedYawDegrees = BoomYawDegrees;
		LaggedPitchDegrees = BoomPitchDegrees;
		LaggedArmLength = TargetArmLength;
		bLagInitialized = true;
		return;
	}

	const float PosAlpha = bEnableCameraLag ? ExpSmoothAlpha(CameraLagSpeed, DeltaTime) : 1.0f;
	LaggedTarget = FMath::Lerp(LaggedTarget, DesiredTarget, PosAlpha);

	const float RotAlpha = bEnableCameraRotationLag ? ExpSmoothAlpha(CameraRotationLagSpeed, DeltaTime) : 1.0f;
	LaggedYawDegrees = LerpAngleDegrees(LaggedYawDegrees, BoomYawDegrees, RotAlpha);
	LaggedPitchDegrees = FMath::Lerp(LaggedPitchDegrees, BoomPitchDegrees, RotAlpha);

	const float ArmAlpha = ExpSmoothAlpha(ArmLengthLagSpeed, DeltaTime);
	LaggedArmLength = FMath::Lerp(LaggedArmLength, TargetArmLength, ArmAlpha);
	LaggedArmLength = FMath::Clamp(LaggedArmLength, ArmLengthMin, ArmLengthMax);
}

float USpringArmComponent::ProbeArmLength(FPhysScene& PhysScene, const FVector& Target, float YawDegrees,
	float PitchDegrees, float DesiredLength, FDebugDraw* DebugDraw) const
{
	const float Length = FMath::Clamp(DesiredLength, ArmLengthMin, ArmLengthMax);
	if (ProbeSize <= 0.0f || Length <= ArmLengthMin + 1.0e-2f)
	{
		return Length;
	}

	const FVector BoomDir = GetBoomDirection(YawDegrees, PitchDegrees);
	const FVector End = Target + BoomDir * Length;

	FCollisionQueryParams Params{};
	Params.bTraceFloorPlane = false;
	if (DebugDraw != nullptr)
	{
		Params.DrawDebugType = EDrawDebugTrace::ForOneFrame;
	}

	FHitResult Hit{};
	if (!PhysScene.SphereTraceSingleByChannel(Hit, Target, End, ProbeSize, ProbeChannel, Params, DebugDraw) ||
		!Hit.bBlockingHit)
	{
		return Length;
	}

	// Pull in slightly past the sweep center so the near clip stays clear of the surface.
	const float Cleared = Hit.Distance - CollisionProbeOffset;
	return FMath::Clamp(Cleared, ArmLengthMin, Length);
}

void USpringArmComponent::ApplyToCamera(UCameraComponent& Camera, const FVector& ActorLocation, float DeltaTime,
	FPhysScene* PhysScene, FDebugDraw* DebugDraw)
{
	UpdateLag(DeltaTime, ActorLocation);

	float ArmLength = LaggedArmLength;
	FPhysScene* Phys = ResolvePhysScene(GetOwner(), PhysScene);
	if (bDoCollisionTest && Phys != nullptr)
	{
		// Flow: lag desired length → sphere probe target→eye → snap in on hit (no lerp through walls)
		const float Probed =
			ProbeArmLength(*Phys, LaggedTarget, LaggedYawDegrees, LaggedPitchDegrees, ArmLength, DebugDraw);
		if (Probed < ArmLength)
		{
			ArmLength = Probed;
			LaggedArmLength = Probed;
		}
	}

	Camera.SetMode(ECameraMode::Orbit);
	Camera.SetTarget(LaggedTarget);
	Camera.SetDistance(ArmLength);
	// The camera looks back along the boom.
	Camera.SetViewRotation(FRotator(-LaggedPitchDegrees, LaggedYawDegrees + 180.0f, 0.0f));
}

void USpringArmComponent::ApplyToCamera(UCameraComponent& Camera, float DeltaTime, FDebugDraw* DebugDraw)
{
	const FVector ActorLocation = GetOwner() != nullptr ? GetOwner()->GetActorLocation() : GetComponentLocation();
	ApplyToCamera(Camera, ActorLocation, DeltaTime, nullptr, DebugDraw);
}
