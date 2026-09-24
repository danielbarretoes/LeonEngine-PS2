#include "GameFramework/Character.h"

#include "Engine/World.h"
#include "SceneRenderer.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

namespace
{

	float YawDegreesFromMoveXz(const glm::vec3& MoveXz)
	{
		constexpr float RadToDeg = 180.0f / std::numbers::pi_v<float>;
		return std::atan2(MoveXz.x, MoveXz.z) * RadToDeg;
	}

	float ShortestYawDelta(float FromDeg, float ToDeg)
	{
		float Delta = std::fmod(ToDeg - FromDeg + 540.0f, 360.0f) - 180.0f;
		if (Delta <= -180.0f)
		{
			Delta += 360.0f;
		}
		return Delta;
	}

} // namespace

ACharacter::ACharacter()
{
	RegisterComponent(&Mesh);
	(void)Mesh.AttachToComponent(&GetRootComponent());
}

void ACharacter::SetHealth(float InHealth)
{
	Health = std::clamp(InHealth, 0.0f, MaxHealth);
	bAlive = Health > 0.0f;
}

void ACharacter::SetMaxHealth(float InMaxHealth)
{
	MaxHealth = std::max(0.0f, InMaxHealth);
	if (Health > MaxHealth)
	{
		Health = MaxHealth;
	}
}

float ACharacter::TakeDamage(float DamageAmount)
{
	if (!bAlive || DamageAmount <= 0.0f)
	{
		return 0.0f;
	}
	const float Applied = std::min(Health, DamageAmount);
	Health = std::max(0.0f, Health - DamageAmount);
	if (Health <= 0.0f)
	{
		Die();
	}
	return Applied;
}

void ACharacter::Die()
{
	Health = 0.0f;
	bAlive = false;
}

void ACharacter::Revive(float NewHealth)
{
	Health = std::clamp(NewHealth, 0.0f, MaxHealth);
	bAlive = true;
}

void ACharacter::Reset(const glm::vec3& InLocation, float InYawDegrees)
{
	SetActorLocationAndRotation(InLocation, InYawDegrees);
	WishDir = {};
	VelocityY = 0.0f;
	SetMovementMode(EMovementMode::Walking);
	bJumpRequested = false;
	bJustLanded = false;
	bYawInitialized = false;
	JumpsRemaining = std::max(0, Movement.MaxJumpCount - 1);
	CurrentFloor = {};
	Health = MaxHealth;
	bAlive = true;
}

void ACharacter::ApplyReplicatedState(
	const glm::vec3& InLocation, float InYawDegrees, float InVelocityY, bool bGrounded)
{
	SetActorLocationAndRotation(InLocation, InYawDegrees);
	WishDir = {};
	VelocityY = InVelocityY;
	SetMovementMode(bGrounded ? EMovementMode::Walking : EMovementMode::Falling);
	bJumpRequested = false;
	bYawInitialized = true;
}

void ACharacter::SetMovementMode(EMovementMode NewMode)
{
	if (NewMode == EMovementMode::None)
	{
		NewMode = EMovementMode::Walking;
	}
	MovementMode = NewMode;
}

void ACharacter::AddMovementInput(const glm::vec3& WishDirXz)
{
	WishDir = WishDirXz;
}

void ACharacter::SetAnimBlendInput(float SpeedAlpha)
{
	AnimBlendInput = std::clamp(SpeedAlpha, 0.0f, 1.0f);
	Mesh.GetAnimInstance().SetBlendSpaceInput(AnimBlendInput);
}

bool ACharacter::ConsumeJustLanded()
{
	const bool bLanded = bJustLanded;
	bJustLanded = false;
	return bLanded;
}

void ACharacter::Jump()
{
	bJumpRequested = true;
}

void ACharacter::FaceRotation(float InYawDegrees, float DeltaTime)
{
	ApplyYaw(InYawDegrees, DeltaTime);
}

bool ACharacter::IsWalkable(const FHitResult& Hit) const
{
	if (!Hit.bBlockingHit)
	{
		return false;
	}
	return Hit.ImpactNormal.y >= Movement.WalkableFloorZ;
}

void ACharacter::FindFloor(
	FPhysScene& PhysScene, FFindFloorResult& OutFloor, float TraceDistance, FDebugDraw* DebugDraw) const
{
	OutFloor = {};
	const float Distance = std::max(TraceDistance, Movement.Skin);
	const glm::vec3 Feet = GetActorLocation();
	// Sphere rests on the feet (center = feet + radius up).
	const glm::vec3 SphereCenter = Feet + glm::vec3{0.0f, Capsule.Radius, 0.0f};
	const glm::vec3 TraceStart = SphereCenter + glm::vec3{0.0f, Movement.Skin, 0.0f};
	const glm::vec3 TraceEnd = SphereCenter - glm::vec3{0.0f, Distance, 0.0f};

	FCollisionQueryParams Query{};
	Query.SkipLevelMeshIndex = GetLevelMeshIndex();
	Query.bTraceFloorPlane = true;
	Query.FloorY = Movement.FloorY;
	Query.DrawDebugType = DebugDraw != nullptr ? EDrawDebugTrace::ForOneFrame : EDrawDebugTrace::None;

	FHitResult Hit{};
	const bool bHitFloor = PhysScene.SphereTraceSingleByChannel(
		Hit, TraceStart, TraceEnd, Capsule.Radius, ECollisionChannel::Visibility, Query, DebugDraw);
	if (!bHitFloor)
	{
		return;
	}

	OutFloor.bBlockingHit = true;
	OutFloor.Hit = Hit;
	OutFloor.bWalkableFloor = IsWalkable(Hit);
	OutFloor.FloorDist = std::max(0.0f, Feet.y - Hit.ImpactPoint.y);
}

void ACharacter::ApplyYaw(float TargetYawDegrees, float DeltaTime)
{
	if (!bYawInitialized)
	{
		MutableYawDegrees() = TargetYawDegrees;
		bYawInitialized = true;
		return;
	}
	const float Delta = ShortestYawDelta(GetActorYaw(), TargetYawDegrees);
	const float T = 1.0f - std::exp(-Movement.TurnSharpness * DeltaTime);
	MutableYawDegrees() += Delta * T;
}

float ACharacter::CapsuleHalfHeight() const
{
	return std::max(0.0f, Capsule.Height * 0.5f - Capsule.Radius);
}

glm::vec3 ACharacter::CapsuleCenterFromFeet(const glm::vec3& Feet) const
{
	return Feet + glm::vec3{0.0f, Capsule.Height * 0.5f, 0.0f};
}

bool ACharacter::BlocksHorizontalMove(const FHitResult& Hit) const
{
	if (!Hit.bBlockingHit || Hit.bFloorPlane)
	{
		return false;
	}
	// Walkable tops must not stop XZ travel (standing on / stepping onto AABB).
	if (IsWalkable(Hit) || Hit.ImpactNormal.y > 0.5f)
	{
		return false;
	}
	return true;
}

glm::vec3 ACharacter::ComputeSlideVector(const glm::vec3& Delta, const glm::vec3& ImpactNormal)
{
	glm::vec3 N{ImpactNormal.x, 0.0f, ImpactNormal.z};
	const float NLen = glm::length(N);
	if (NLen < 1.0e-4f)
	{
		return glm::vec3{0.0f};
	}
	N /= NLen;
	glm::vec3 Slide = Delta - N * glm::dot(Delta, N);
	Slide.y = 0.0f;
	return Slide;
}

bool ACharacter::SafeMoveUpdatedComponent(
	FPhysScene& PhysScene, const glm::vec3& Delta, FHitResult* OutHit, FDebugDraw* DebugDraw)
{
	glm::vec3& Feet = MutableLocation();
	const float DeltaLen = glm::length(Delta);
	if (DeltaLen < 1.0e-6f)
	{
		return true;
	}

	const glm::vec3 StartCenter = CapsuleCenterFromFeet(Feet);
	const glm::vec3 EndCenter = StartCenter + Delta;
	const float HalfH = CapsuleHalfHeight();

	FCollisionQueryParams Query{};
	Query.SkipLevelMeshIndex = GetLevelMeshIndex();
	Query.bTraceFloorPlane = false;
	Query.DrawDebugType = DebugDraw != nullptr ? EDrawDebugTrace::ForOneFrame : EDrawDebugTrace::None;

	std::vector<FHitResult> Hits;
	(void)PhysScene.CapsuleTraceMultiByChannel(
		Hits, StartCenter, EndCenter, Capsule.Radius, HalfH, ECollisionChannel::Visibility, Query, DebugDraw);

	const FHitResult* Block = nullptr;
	for (const FHitResult& Hit : Hits)
	{
		if (BlocksHorizontalMove(Hit))
		{
			Block = &Hit;
			break;
		}
	}

	if (Block == nullptr)
	{
		Feet += Delta;
		ClampPositionXZ(Feet, Movement.WalkBounds);
		if (OutHit != nullptr)
		{
			*OutHit = {};
		}
		return true;
	}

	float T = Block->Time;
	T = std::max(0.0f, T - (Movement.Skin / DeltaLen));
	Feet += Delta * T;
	// Nudge out of the wall so the next iteration does not re-hit at t=0.
	glm::vec3 N{Block->ImpactNormal.x, 0.0f, Block->ImpactNormal.z};
	const float NLen = glm::length(N);
	if (NLen > 1.0e-4f)
	{
		N /= NLen;
		Feet += N * Movement.Skin;
	}
	// Sweep stops before overlap; ResolveCapsuleSides push never fires — shove from the hit.
	(void)PhysScene.ApplyCapsuleSweepPush(
		Block->LevelMeshIndex, {WishDir.x, WishDir.z}, Block->ImpactNormal, Movement.PushStrength, Movement.WalkBounds);
	ClampPositionXZ(Feet, Movement.WalkBounds);
	if (OutHit != nullptr)
	{
		*OutHit = *Block;
	}
	return false;
}

void ACharacter::ResolveSides(FPhysScene& PhysScene, bool bApplyPush)
{
	FCapsuleContactParams Params{};
	Params.PushStrength = Movement.PushStrength;
	Params.StepUp = Movement.MaxStepHeight;
	Params.Skin = Movement.Skin;
	Params.WalkBounds = Movement.WalkBounds;
	PhysScene.ResolveCapsuleSides(
		Capsule, MutableLocation(), {WishDir.x, WishDir.z}, Params, GetLevelMeshIndex(), bApplyPush);
}

bool ACharacter::TryStepUp(FPhysScene& PhysScene, const glm::vec3& ForwardDelta, FDebugDraw* DebugDraw)
{
	if (!IsMovingOnGround() || Movement.MaxStepHeight <= 1.0e-4f)
	{
		return false;
	}
	glm::vec3 Fwd = ForwardDelta;
	Fwd.y = 0.0f;
	if (glm::length(Fwd) < 1.0e-5f)
	{
		return false;
	}

	glm::vec3& Feet = MutableLocation();
	const glm::vec3 StartFeet = Feet;
	const float HalfH = CapsuleHalfHeight();

	FCollisionQueryParams Query{};
	Query.SkipLevelMeshIndex = GetLevelMeshIndex();
	Query.bTraceFloorPlane = false;
	Query.DrawDebugType = DebugDraw != nullptr ? EDrawDebugTrace::ForOneFrame : EDrawDebugTrace::None;

	// 1) Raise by MaxStepHeight; only a true ceiling (downward normal) aborts.
	const glm::vec3 UpStart = CapsuleCenterFromFeet(Feet);
	const glm::vec3 UpEnd = UpStart + glm::vec3{0.0f, Movement.MaxStepHeight, 0.0f};
	std::vector<FHitResult> UpHits;
	(void)PhysScene.CapsuleTraceMultiByChannel(
		UpHits, UpStart, UpEnd, Capsule.Radius, HalfH, ECollisionChannel::Visibility, Query, DebugDraw);
	for (const FHitResult& UpHit : UpHits)
	{
		if (UpHit.ImpactNormal.y < -0.5f)
		{
			return false;
		}
	}
	Feet.y = StartFeet.y + Movement.MaxStepHeight;

	// 2) Forward onto the ledge while elevated. One frame of leftover is often << radius;
	// probe at least ~half-radius so QuerySupportY can see the top.
	const float FwdLen = glm::length(Fwd);
	const float MinFwd = std::max(Capsule.Radius * 0.5f, Movement.Skin * 4.0f);
	if (FwdLen > 1.0e-5f && FwdLen < MinFwd)
	{
		Fwd *= (MinFwd / FwdLen);
	}
	FHitResult FwdHit{};
	const bool bCleared = SafeMoveUpdatedComponent(PhysScene, Fwd, &FwdHit, DebugDraw);
	if (!bCleared && FwdHit.Time < 0.15f)
	{
		Feet = StartFeet;
		return false;
	}

	// 3) Land on a raised walkable support within MaxStepHeight (not the floor below).
	FFindFloorResult Floor{};
	FindFloor(PhysScene, Floor, Movement.MaxStepHeight + (Movement.Skin * 4.0f), DebugDraw);
	if (!Floor.bWalkableFloor)
	{
		Feet = StartFeet;
		return false;
	}
	const float HeightGain = Floor.Hit.ImpactPoint.y - StartFeet.y;
	if (HeightGain < Movement.Skin || HeightGain > Movement.MaxStepHeight + Movement.Skin)
	{
		Feet = StartFeet;
		return false;
	}
	// Sphere FindFloor can report a phantom shelf in front of an AABB; require real support.
	const float Support = PhysScene.QuerySupportY(
		Capsule, Feet, Movement.FloorY, Movement.MaxStepHeight, Movement.Skin, GetLevelMeshIndex());
	if (Support < StartFeet.y + Movement.Skin)
	{
		Feet = StartFeet;
		return false;
	}

	Feet.y = std::max(Floor.Hit.ImpactPoint.y, Support);
	if (Feet.y - StartFeet.y > Movement.MaxStepHeight + Movement.Skin)
	{
		Feet = StartFeet;
		return false;
	}
	VelocityY = 0.0f;
	SetMovementMode(EMovementMode::Walking);
	CurrentFloor = Floor;
	ClampPositionXZ(Feet, Movement.WalkBounds);
	return true;
}

void ACharacter::MoveHorizontal(FPhysScene& PhysScene, float DeltaTime, FDebugDraw* DebugDraw)
{
	const float Len = glm::length(WishDir);
	if (Len <= 1.0e-4f)
	{
		ResolveSides(PhysScene, true);
		return;
	}

	const glm::vec3 Dir = WishDir / Len;
	if (bOrientRotationToMovement)
	{
		ApplyYaw(YawDegreesFromMoveXz(Dir) + Movement.ModelYawOffsetDegrees, DeltaTime);
	}

	// Flow: SafeMove → step-up (if Walking + blocked) → slide → ResolveCapsuleSides.
	// Falling uses AirControl fraction of MaxWalkSpeed (Unreal AirControl lite).
	float SpeedScale = 1.0f;
	if (IsFalling())
	{
		SpeedScale = std::clamp(Movement.AirControl, 0.0f, 1.0f);
	}
	glm::vec3 Remaining = Dir * (Movement.MaxWalkSpeed * SpeedScale * DeltaTime);
	Remaining.y = 0.0f;
	constexpr int MaxSlideIterations = 2;
	for (int I = 0; I < MaxSlideIterations; ++I)
	{
		if (glm::length(Remaining) < 1.0e-5f)
		{
			break;
		}
		FHitResult Hit{};
		if (SafeMoveUpdatedComponent(PhysScene, Remaining, &Hit, DebugDraw))
		{
			break;
		}
		const float Used = std::clamp(Hit.Time, 0.0f, 1.0f);
		glm::vec3 Leftover = Remaining * (1.0f - Used);
		Leftover.y = 0.0f;
		if (TryStepUp(PhysScene, Leftover, DebugDraw))
		{
			break;
		}
		Leftover = ComputeSlideVector(Leftover, Hit.ImpactNormal);
		if (glm::dot(Leftover, Leftover) < 1.0e-8f)
		{
			break;
		}
		Remaining = Leftover;
	}

	ResolveSides(PhysScene, true);
}

void ACharacter::IntegrateVertical(FPhysScene& PhysScene, float DeltaTime, FDebugDraw* DebugDraw)
{
	const bool bWasGrounded = IsMovingOnGround();
	if (bJumpRequested)
	{
		const bool bCanGroundJump = bWasGrounded;
		const bool bCanAirJump = !bWasGrounded && JumpsRemaining > 0;
		if (bCanGroundJump || bCanAirJump)
		{
			VelocityY = Movement.JumpZVelocity;
			SetMovementMode(EMovementMode::Falling);
			if (bCanGroundJump)
			{
				JumpsRemaining = std::max(0, Movement.MaxJumpCount - 1);
			}
			else
			{
				--JumpsRemaining;
			}
			if (auto* CharacterAnim = dynamic_cast<UCharacterAnimInstance*>(&Mesh.GetAnimInstance()))
			{
				CharacterAnim->NotifyJumped();
			}
		}
	}
	bJumpRequested = false;

	VelocityY -= Movement.Gravity * DeltaTime;
	MutableLocation().y += VelocityY * DeltaTime;

	// Flow: FindFloor → IsWalkable → snap (Walking) or reject steep (Falling, no tunnel)
	const float LandWindow =
		std::max(Movement.MaxStepHeight + Movement.Skin, (std::abs(VelocityY) * DeltaTime) + (Movement.Skin * 4.0f));
	FindFloor(PhysScene, CurrentFloor, LandWindow, DebugDraw);

	if (VelocityY <= 0.0f && CurrentFloor.bBlockingHit)
	{
		const float SurfaceY = CurrentFloor.Hit.ImpactPoint.y;
		if (CurrentFloor.bWalkableFloor)
		{
			MutableLocation().y = SurfaceY;
			VelocityY = 0.0f;
			SetMovementMode(EMovementMode::Walking);
			JumpsRemaining = std::max(0, Movement.MaxJumpCount - 1);
			if (!bWasGrounded)
			{
				bJustLanded = true;
			}
		}
		else
		{
			// Unreal: unwalkable floor (steep) — stay Falling, do not sink through.
			if (MutableLocation().y < SurfaceY)
			{
				MutableLocation().y = SurfaceY;
			}
			if (CurrentFloor.FloorDist <= (Movement.Skin * 4.0f) && VelocityY < 0.0f)
			{
				VelocityY = 0.0f;
			}
			SetMovementMode(EMovementMode::Falling);
		}
	}
	else
	{
		SetMovementMode(EMovementMode::Falling);
	}
}

void ACharacter::PerformMovement(FPhysScene& PhysScene, float DeltaTime, FDebugDraw* DebugDraw)
{
	MoveHorizontal(PhysScene, DeltaTime, DebugDraw);
	IntegrateVertical(PhysScene, DeltaTime, DebugDraw);
	ResolveSides(PhysScene, false);
}

void ACharacter::TickCharacterMovement(float DeltaTime, FDebugDraw* DebugDraw)
{
	UWorld* LocalWorld = GetWorld();
	if (LocalWorld == nullptr)
	{
		return;
	}
	PerformMovement(LocalWorld->GetPhysicsScene(), DeltaTime, DebugDraw);
}

void ACharacter::ResolveOverlaps(FPhysScene& PhysScene)
{
	ResolveSides(PhysScene, false);
}

void ACharacter::ResolveOverlaps()
{
	UWorld* LocalWorld = GetWorld();
	if (LocalWorld == nullptr)
	{
		return;
	}
	ResolveOverlaps(LocalWorld->GetPhysicsScene());
}

void ACharacter::ResolvePawnOverlap(ACharacter& Other)
{
	if (this == &Other)
	{
		return;
	}

	glm::vec3& A = MutableLocation();
	glm::vec3& B = Other.MutableLocation();
	const float ATop = A.y + Capsule.Height;
	const float bTop = B.y + Other.Capsule.Height;
	if (ATop < B.y || bTop < A.y)
	{
		return;
	}

	glm::vec2 Delta{A.x - B.x, A.z - B.z};
	float Dist = glm::length(Delta);
	const float MinDist = Capsule.Radius + Other.Capsule.Radius;
	if (Dist >= MinDist - 1.0e-5f)
	{
		return;
	}

	glm::vec2 Normal{};
	if (Dist < 1.0e-4f)
	{
		// Deterministic axis when centers coincide (avoid NaN / jitter).
		Normal = (GetUniqueID() <= Other.GetUniqueID()) ? glm::vec2{1.0f, 0.0f} : glm::vec2{-1.0f, 0.0f};
		Dist = 0.0f;
	}
	else
	{
		Normal = Delta / Dist;
	}

	const float Penetration = MinDist - Dist;
	const float Half = Penetration * 0.5f;
	A.x += Normal.x * Half;
	A.z += Normal.y * Half;
	B.x -= Normal.x * Half;
	B.z -= Normal.y * Half;
	ClampPositionXZ(A, Movement.WalkBounds);
	ClampPositionXZ(B, Other.Movement.WalkBounds);
}

void ACharacter::Tick(float DeltaTime)
{
	if (auto* CharacterAnim = dynamic_cast<UCharacterAnimInstance*>(&Mesh.GetAnimInstance()))
	{
		CharacterAnim->SetMovementState(IsFalling(), VelocityY, ConsumeJustLanded());
	}
	else
	{
		(void)ConsumeJustLanded();
	}
	Mesh.TickComponent(DeltaTime);
}

void ACharacter::SubmitMeshDraw(FSceneRenderer& Renderer) const
{
	Mesh.SubmitDraw(Renderer);
}
