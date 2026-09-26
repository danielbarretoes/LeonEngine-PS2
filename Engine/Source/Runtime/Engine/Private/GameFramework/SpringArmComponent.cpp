#include "GameFramework/SpringArmComponent.h"

#include "Debug/DebugDraw.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
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

USpringArmComponent::USpringArmComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FRotator USpringArmComponent::GetTargetRotation() const
{
	if (bUsePawnControlRotation)
	{
		if (const APawn* OwningPawn = Cast<APawn>(GetOwner()))
		{
			return OwningPawn->GetViewRotation();
		}
	}
	return GetComponentRotation();
}

FVector USpringArmComponent::GetArmOrigin(const FVector& ActorLocation) const
{
	// With no roll, the socket offset's Y goes along the view's right axis (the same as YawRelativeMove's).
	const FVector Socket = FRotationMatrix(GetTargetRotation()).TransformVector(SocketOffset);
	return ActorLocation + TargetOffset + Socket;
}

void USpringArmComponent::SnapLagState(const FVector& ActorLocation)
{
	LaggedOrigin = GetArmOrigin(ActorLocation);
	LaggedRotation = GetTargetRotation();
	LaggedArmLength = TargetArmLength;
	bLagInitialized = true;
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
	const FVector DesiredOrigin = GetArmOrigin(ActorLocation);
	const FRotator DesiredRotation = GetTargetRotation();
	if (!bLagInitialized)
	{
		LaggedOrigin = DesiredOrigin;
		LaggedRotation = DesiredRotation;
		LaggedArmLength = TargetArmLength;
		bLagInitialized = true;
		return;
	}

	const float PosAlpha = bEnableCameraLag ? ExpSmoothAlpha(CameraLagSpeed, DeltaTime) : 1.0f;
	LaggedOrigin = FMath::Lerp(LaggedOrigin, DesiredOrigin, PosAlpha);

	const float RotAlpha = bEnableCameraRotationLag ? ExpSmoothAlpha(CameraRotationLagSpeed, DeltaTime) : 1.0f;
	LaggedRotation.Yaw = LerpAngleDegrees(LaggedRotation.Yaw, DesiredRotation.Yaw, RotAlpha);
	LaggedRotation.Pitch = FMath::Lerp(LaggedRotation.Pitch, DesiredRotation.Pitch, RotAlpha);
	LaggedRotation.Roll = 0.0f;

	const float ArmAlpha = ExpSmoothAlpha(ArmLengthLagSpeed, DeltaTime);
	LaggedArmLength = FMath::Lerp(LaggedArmLength, TargetArmLength, ArmAlpha);
	LaggedArmLength = FMath::Clamp(LaggedArmLength, ArmLengthMin, ArmLengthMax);
}

float USpringArmComponent::ProbeArmLength(FPhysScene& PhysScene, const FVector& Origin, const FRotator& Rotation,
	float DesiredLength, FDebugDraw* DebugDraw) const
{
	const float Length = FMath::Clamp(DesiredLength, ArmLengthMin, ArmLengthMax);
	if (ProbeSize <= 0.0f || Length <= ArmLengthMin + 1.0e-2f)
	{
		return Length;
	}

	// The camera sits behind the origin, looking along the rotation.
	const FVector ArmDirection = -Rotation.Vector();
	const FVector End = Origin + ArmDirection * Length;

	// UE: FCollisionQueryParams(SCENE_QUERY_STAT(SpringArm), false, GetOwner()): the arm never hits its own actor.
	FCollisionQueryParams Params{};
	Params.AddIgnoredActor(GetOwner());
	Params.bTraceFloorPlane = false;
	if (DebugDraw != nullptr)
	{
		Params.DrawDebugType = EDrawDebugTrace::ForOneFrame;
	}

	FHitResult Hit{};
	if (!PhysScene.SphereTraceSingleByChannel(Hit, Origin, End, ProbeSize, ProbeChannel, Params, DebugDraw) ||
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
		// Flow: lag desired length → sphere probe origin→eye → snap in on hit (no lerp through walls)
		const float Probed = ProbeArmLength(*Phys, LaggedOrigin, LaggedRotation, ArmLength, DebugDraw);
		if (Probed < ArmLength)
		{
			ArmLength = Probed;
			LaggedArmLength = Probed;
		}
	}

	Camera.SetMode(ECameraMode::Orbit);
	Camera.SetTarget(LaggedOrigin);
	Camera.SetDistance(ArmLength);
	Camera.SetViewRotation(LaggedRotation);
}

void USpringArmComponent::ApplyToCamera(UCameraComponent& Camera, float DeltaTime, FDebugDraw* DebugDraw)
{
	const FVector ActorLocation = GetOwner() != nullptr ? GetOwner()->GetActorLocation() : GetComponentLocation();
	ApplyToCamera(Camera, ActorLocation, DeltaTime, nullptr, DebugDraw);
}
