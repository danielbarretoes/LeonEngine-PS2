#include "GameFramework/SpringArmComponent.h"

#include "Debug/DebugDraw.h"
#include "Engine/World.h"
#include "Migration/GlmInterop.h"
#include "Physics/PhysScene.h"

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>

#include <cmath>

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

glm::vec3 USpringArmComponent::GetBoomDirection(float YawDegrees, float PitchDegrees)
{
	const float YawRad = YawDegrees * (glm::pi<float>() / 180.0f);
	const float PitchRad = PitchDegrees * (glm::pi<float>() / 180.0f);
	const glm::vec3 Dir{
		std::cos(PitchRad) * std::cos(YawRad),
		std::sin(PitchRad),
		std::cos(PitchRad) * std::sin(YawRad),
	};
	const float Len = glm::length(Dir);
	if (Len < 1.0e-6f)
	{
		return glm::vec3{0.0f, 0.0f, 1.0f};
	}
	return Dir / Len;
}

glm::vec3 USpringArmComponent::GetTargetLocation(const glm::vec3& ActorLocation) const
{
	constexpr float DegToRad = glm::pi<float>() / 180.0f;
	const float YawRad = BoomYawDegrees * DegToRad;
	// Same right basis as yawRelativeMoveXZ.
	const glm::vec3 Right{std::sin(YawRad), 0.0f, -std::cos(YawRad)};
	return ActorLocation + glm::vec3{0.0f, SocketOffsetZ, 0.0f} + (Right * SocketOffsetX);
}

void USpringArmComponent::SnapLagState(const glm::vec3& ActorLocation)
{
	LaggedTarget = GetTargetLocation(ActorLocation);
	LaggedYawDegrees = BoomYawDegrees;
	LaggedPitchDegrees = BoomPitchDegrees;
	LaggedArmLength = TargetArmLength;
	bLagInitialized = true;
}

float USpringArmComponent::GetLookFacingYawDegrees() const
{
	constexpr float DegToRad = glm::pi<float>() / 180.0f;
	constexpr float RadToDeg = 180.0f / glm::pi<float>();
	const float YawRad = BoomYawDegrees * DegToRad;
	// Same forward as yawRelativeMoveXZ / Camera orbit look on XZ.
	return std::atan2(-std::cos(YawRad), -std::sin(YawRad)) * RadToDeg;
}

float USpringArmComponent::ExpSmoothAlpha(float Speed, float DeltaTime)
{
	if (Speed <= 0.0f || DeltaTime <= 0.0f)
	{
		return 1.0f;
	}
	return 1.0f - std::exp(-Speed * DeltaTime);
}

float USpringArmComponent::LerpAngleDegrees(float FromDegrees, float ToDegrees, float Alpha)
{
	float Delta = std::fmod(ToDegrees - FromDegrees + 540.0f, 360.0f) - 180.0f;
	return FromDegrees + (Delta * Alpha);
}

void USpringArmComponent::UpdateLag(float DeltaTime, const glm::vec3& ActorLocation)
{
	const glm::vec3 DesiredTarget = GetTargetLocation(ActorLocation);
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
	LaggedTarget = glm::mix(LaggedTarget, DesiredTarget, PosAlpha);

	const float RotAlpha = bEnableCameraRotationLag ? ExpSmoothAlpha(CameraRotationLagSpeed, DeltaTime) : 1.0f;
	LaggedYawDegrees = LerpAngleDegrees(LaggedYawDegrees, BoomYawDegrees, RotAlpha);
	LaggedPitchDegrees = glm::mix(LaggedPitchDegrees, BoomPitchDegrees, RotAlpha);

	const float ArmAlpha = ExpSmoothAlpha(ArmLengthLagSpeed, DeltaTime);
	LaggedArmLength = glm::mix(LaggedArmLength, TargetArmLength, ArmAlpha);
	LaggedArmLength = std::clamp(LaggedArmLength, ArmLengthMin, ArmLengthMax);
}

float USpringArmComponent::ProbeArmLength(FPhysScene& PhysScene, const glm::vec3& Target, float YawDegrees,
	float PitchDegrees, float DesiredLength, FDebugDraw* DebugDraw) const
{
	const float Length = std::clamp(DesiredLength, ArmLengthMin, ArmLengthMax);
	if (ProbeSize <= 0.0f || Length <= ArmLengthMin + 1.0e-4f)
	{
		return Length;
	}

	const glm::vec3 BoomDir = GetBoomDirection(YawDegrees, PitchDegrees);
	const glm::vec3 End = Target + BoomDir * Length;

	FCollisionQueryParams Params{};
	Params.bTraceFloorPlane = false;
	if (DebugDraw != nullptr)
	{
		Params.DrawDebugType = EDrawDebugTrace::ForOneFrame;
	}

	FHitResult Hit{};
	if (!PhysScene.SphereTraceSingleByChannel(
			Hit, FromGlm(Target), FromGlm(End), ProbeSize, ProbeChannel, Params, DebugDraw) ||
		!Hit.bBlockingHit)
	{
		return Length;
	}

	// Pull in slightly past the sweep center so the near clip stays clear of the surface.
	const float Cleared = Hit.Distance - CollisionProbeOffset;
	return std::clamp(Cleared, ArmLengthMin, Length);
}

void USpringArmComponent::ApplyToCamera(UCameraComponent& Camera, const glm::vec3& ActorLocation, float DeltaTime,
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
	Camera.SetYawPitch(LaggedYawDegrees, LaggedPitchDegrees);
}

void USpringArmComponent::ApplyToCamera(UCameraComponent& Camera, float DeltaTime, FDebugDraw* DebugDraw)
{
	const glm::vec3 ActorLocation = GetOwner() != nullptr ? GetOwner()->GetActorLocation() : GetComponentLocation();
	ApplyToCamera(Camera, ActorLocation, DeltaTime, nullptr, DebugDraw);
}
