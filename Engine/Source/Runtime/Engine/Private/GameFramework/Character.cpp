#include "GameFramework/Character.h"

#include "Engine/World.h"
#include "SceneRenderer.h"

namespace
{

	/** UE yaw of a horizontal direction: 0 = +X, 90 = +Y. */
	float YawDegreesFromMove(const FVector& Move)
	{
		constexpr float RadToDeg = 180.0f / PI;
		return FMath::Atan2(Move.Y, Move.X) * RadToDeg;
	}

	float ShortestYawDelta(float FromDeg, float ToDeg)
	{
		float Delta = FMath::Fmod(ToDeg - FromDeg + 540.0f, 360.0f) - 180.0f;
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
	// Legacy content faces +Y (UE: the mannequin mesh's relative yaw of -90).
	Mesh.RelativeRotation = FRotator(0.0f, LegacyContentYawDegrees, 0.0f);
}

void ACharacter::SetHealth(float InHealth)
{
	Health = FMath::Clamp(InHealth, 0.0f, MaxHealth);
	bAlive = Health > 0.0f;
}

void ACharacter::SetMaxHealth(float InMaxHealth)
{
	MaxHealth = FMath::Max(0.0f, InMaxHealth);
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
	const float Applied = FMath::Min(Health, DamageAmount);
	Health = FMath::Max(0.0f, Health - DamageAmount);
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
	Health = FMath::Clamp(NewHealth, 0.0f, MaxHealth);
	bAlive = true;
}

void ACharacter::Reset(const FVector& InLocation, float InYawDegrees)
{
	SetActorLocationAndRotation(InLocation, InYawDegrees);
	WishDir = {};
	VelocityZ = 0.0f;
	SetMovementMode(EMovementMode::Walking);
	bJumpRequested = false;
	bJustLanded = false;
	bYawInitialized = false;
	JumpsRemaining = FMath::Max(0, Movement.MaxJumpCount - 1);
	CurrentFloor = {};
	Health = MaxHealth;
	bAlive = true;
}

void ACharacter::ApplyReplicatedState(const FVector& InLocation, float InYawDegrees, float InVelocityZ, bool bGrounded)
{
	SetActorLocationAndRotation(InLocation, InYawDegrees);
	WishDir = {};
	VelocityZ = InVelocityZ;
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

void ACharacter::AddMovementInput(const FVector& WishDirXY)
{
	WishDir = WishDirXY;
}

void ACharacter::SetAnimBlendInput(float SpeedAlpha)
{
	AnimBlendInput = FMath::Clamp(SpeedAlpha, 0.0f, 1.0f);
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
	return Hit.ImpactNormal.Z >= Movement.WalkableFloorZ;
}

void ACharacter::FindFloor(
	FPhysScene& PhysScene, FFindFloorResult& OutFloor, float TraceDistance, FDebugDraw* DebugDraw) const
{
	OutFloor = {};
	const float Distance = FMath::Max(TraceDistance, Movement.Skin);
	const FVector Feet = GetActorLocation();
	// Sphere rests on the feet (center = feet + radius up).
	const FVector SphereCenter = Feet + FVector(0.0f, 0.0f, Capsule.GetCapsuleRadius());
	const FVector TraceStart = SphereCenter + FVector(0.0f, 0.0f, Movement.Skin);
	const FVector TraceEnd = SphereCenter - FVector(0.0f, 0.0f, Distance);

	FCollisionQueryParams Query{};
	Query.SkipLevelMeshIndex = GetLevelMeshIndex();
	Query.bTraceFloorPlane = true;
	Query.FloorZ = Movement.FloorZ;
	Query.DrawDebugType = DebugDraw != nullptr ? EDrawDebugTrace::ForOneFrame : EDrawDebugTrace::None;

	FHitResult Hit{};
	const bool bHitFloor = PhysScene.SphereTraceSingleByChannel(
		Hit, TraceStart, TraceEnd, Capsule.GetCapsuleRadius(), ECollisionChannel::Visibility, Query, DebugDraw);
	if (!bHitFloor)
	{
		return;
	}

	OutFloor.bBlockingHit = true;
	OutFloor.Hit = Hit;
	OutFloor.bWalkableFloor = IsWalkable(Hit);
	OutFloor.FloorDist = FMath::Max(0.0f, Feet.Z - Hit.ImpactPoint.Z);
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
	const float T = 1.0f - FMath::Exp(-Movement.TurnSharpness * DeltaTime);
	MutableYawDegrees() += Delta * T;
}

float ACharacter::CapsuleHalfHeight() const
{
	return FMath::Max(0.0f, Capsule.GetCapsuleHalfHeight() - Capsule.GetCapsuleRadius());
}

FVector ACharacter::CapsuleCenterFromFeet(const FVector& Feet) const
{
	return Feet + FVector(0.0f, 0.0f, Capsule.GetCapsuleHalfHeight());
}

bool ACharacter::BlocksHorizontalMove(const FHitResult& Hit) const
{
	if (!Hit.bBlockingHit || Hit.bFloorPlane)
	{
		return false;
	}
	// Walkable tops must not stop XY travel (standing on / stepping onto AABB).
	if (IsWalkable(Hit) || Hit.ImpactNormal.Z > 0.5f)
	{
		return false;
	}
	return true;
}

FVector ACharacter::ComputeSlideVector(const FVector& Delta, const FVector& ImpactNormal)
{
	FVector N = FVector(ImpactNormal.X, ImpactNormal.Y, 0.0f);
	const float NLen = N.Size();
	if (NLen < 1.0e-4f)
	{
		return FVector(0.0f);
	}
	N /= NLen;
	FVector Slide = Delta - N * FVector::DotProduct(Delta, N);
	Slide.Z = 0.0f;
	return Slide;
}

bool ACharacter::SafeMoveUpdatedComponent(
	FPhysScene& PhysScene, const FVector& Delta, FHitResult* OutHit, FDebugDraw* DebugDraw)
{
	FVector& Feet = MutableLocation();
	const float DeltaLen = Delta.Size();
	if (DeltaLen < 1.0e-4f)
	{
		return true;
	}

	const FVector StartCenter = CapsuleCenterFromFeet(Feet);
	const FVector EndCenter = StartCenter + Delta;
	const float HalfH = CapsuleHalfHeight();

	FCollisionQueryParams Query{};
	Query.SkipLevelMeshIndex = GetLevelMeshIndex();
	Query.bTraceFloorPlane = false;
	Query.DrawDebugType = DebugDraw != nullptr ? EDrawDebugTrace::ForOneFrame : EDrawDebugTrace::None;

	TArray<FHitResult> Hits;
	(void)PhysScene.CapsuleTraceMultiByChannel(Hits, StartCenter, EndCenter, Capsule.GetCapsuleRadius(), HalfH,
		ECollisionChannel::Visibility, Query, DebugDraw);

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
		ClampPositionXY(Feet, Movement.WalkBounds);
		if (OutHit != nullptr)
		{
			*OutHit = {};
		}
		return true;
	}

	float T = Block->Time;
	T = FMath::Max(0.0f, T - (Movement.Skin / DeltaLen));
	Feet += Delta * T;
	// Nudge out of the wall so the next iteration does not re-hit at t=0.
	FVector N = FVector(Block->ImpactNormal.X, Block->ImpactNormal.Y, 0.0f);
	const float NLen = N.Size();
	if (NLen > 1.0e-4f)
	{
		N /= NLen;
		Feet += N * Movement.Skin;
	}
	// Sweep stops before overlap; ResolveCapsuleSides push never fires — shove from the hit.
	(void)PhysScene.ApplyCapsuleSweepPush(Block->LevelMeshIndex, FVector2D(WishDir.X, WishDir.Y), Block->ImpactNormal,
		Movement.PushStrength, Movement.WalkBounds);
	ClampPositionXY(Feet, Movement.WalkBounds);
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
	FVector Feet = MutableLocation();
	PhysScene.ResolveCapsuleSides(
		Capsule, Feet, FVector2D(WishDir.X, WishDir.Y), Params, GetLevelMeshIndex(), bApplyPush);
	MutableLocation() = Feet;
}

bool ACharacter::TryStepUp(FPhysScene& PhysScene, const FVector& ForwardDelta, FDebugDraw* DebugDraw)
{
	if (!IsMovingOnGround() || Movement.MaxStepHeight <= 1.0e-2f)
	{
		return false;
	}
	FVector Fwd = ForwardDelta;
	Fwd.Z = 0.0f;
	if (Fwd.Size() < 1.0e-3f)
	{
		return false;
	}

	FVector& Feet = MutableLocation();
	const FVector StartFeet = Feet;
	const float HalfH = CapsuleHalfHeight();

	FCollisionQueryParams Query{};
	Query.SkipLevelMeshIndex = GetLevelMeshIndex();
	Query.bTraceFloorPlane = false;
	Query.DrawDebugType = DebugDraw != nullptr ? EDrawDebugTrace::ForOneFrame : EDrawDebugTrace::None;

	// 1) Raise by MaxStepHeight; only a true ceiling (downward normal) aborts.
	const FVector UpStart = CapsuleCenterFromFeet(Feet);
	const FVector UpEnd = UpStart + FVector(0.0f, 0.0f, Movement.MaxStepHeight);
	TArray<FHitResult> UpHits;
	(void)PhysScene.CapsuleTraceMultiByChannel(
		UpHits, UpStart, UpEnd, Capsule.GetCapsuleRadius(), HalfH, ECollisionChannel::Visibility, Query, DebugDraw);
	for (const FHitResult& UpHit : UpHits)
	{
		if (UpHit.ImpactNormal.Z < -0.5f)
		{
			return false;
		}
	}
	Feet.Z = StartFeet.Z + Movement.MaxStepHeight;

	// 2) Forward onto the ledge while elevated. One frame of leftover is often << radius;
	// probe at least ~half-radius so QuerySupportZ can see the top.
	const float FwdLen = Fwd.Size();
	const float MinFwd = FMath::Max(Capsule.GetCapsuleRadius() * 0.5f, Movement.Skin * 4.0f);
	if (FwdLen > 1.0e-3f && FwdLen < MinFwd)
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
	const float HeightGain = Floor.Hit.ImpactPoint.Z - StartFeet.Z;
	if (HeightGain < Movement.Skin || HeightGain > Movement.MaxStepHeight + Movement.Skin)
	{
		Feet = StartFeet;
		return false;
	}
	// Sphere FindFloor can report a phantom shelf in front of an AABB; require real support.
	const float Support = PhysScene.QuerySupportZ(
		Capsule, Feet, Movement.FloorZ, Movement.MaxStepHeight, Movement.Skin, GetLevelMeshIndex());
	if (Support < StartFeet.Z + Movement.Skin)
	{
		Feet = StartFeet;
		return false;
	}

	Feet.Z = FMath::Max(Floor.Hit.ImpactPoint.Z, Support);
	if (Feet.Z - StartFeet.Z > Movement.MaxStepHeight + Movement.Skin)
	{
		Feet = StartFeet;
		return false;
	}
	VelocityZ = 0.0f;
	SetMovementMode(EMovementMode::Walking);
	CurrentFloor = Floor;
	ClampPositionXY(Feet, Movement.WalkBounds);
	return true;
}

void ACharacter::MoveHorizontal(FPhysScene& PhysScene, float DeltaTime, FDebugDraw* DebugDraw)
{
	const float Len = WishDir.Size();
	if (Len <= 1.0e-4f)
	{
		ResolveSides(PhysScene, true);
		return;
	}

	const FVector Dir = WishDir / Len;
	if (bOrientRotationToMovement)
	{
		ApplyYaw(YawDegreesFromMove(Dir) + Movement.ModelYawOffsetDegrees, DeltaTime);
	}

	// Flow: SafeMove → step-up (if Walking + blocked) → slide → ResolveCapsuleSides.
	// Falling uses AirControl fraction of MaxWalkSpeed (Unreal AirControl lite).
	float SpeedScale = 1.0f;
	if (IsFalling())
	{
		SpeedScale = FMath::Clamp(Movement.AirControl, 0.0f, 1.0f);
	}
	FVector Remaining = Dir * (Movement.MaxWalkSpeed * SpeedScale * DeltaTime);
	Remaining.Z = 0.0f;
	constexpr int MaxSlideIterations = 2;
	for (int I = 0; I < MaxSlideIterations; ++I)
	{
		if (Remaining.Size() < 1.0e-3f)
		{
			break;
		}
		FHitResult Hit{};
		if (SafeMoveUpdatedComponent(PhysScene, Remaining, &Hit, DebugDraw))
		{
			break;
		}
		const float Used = FMath::Clamp(Hit.Time, 0.0f, 1.0f);
		FVector Leftover = Remaining * (1.0f - Used);
		Leftover.Z = 0.0f;
		if (TryStepUp(PhysScene, Leftover, DebugDraw))
		{
			break;
		}
		Leftover = ComputeSlideVector(Leftover, Hit.ImpactNormal);
		if (FVector::DotProduct(Leftover, Leftover) < 1.0e-4f)
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
			VelocityZ = Movement.JumpZVelocity;
			SetMovementMode(EMovementMode::Falling);
			if (bCanGroundJump)
			{
				JumpsRemaining = FMath::Max(0, Movement.MaxJumpCount - 1);
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

	VelocityZ -= Movement.Gravity * DeltaTime;
	MutableLocation().Z += VelocityZ * DeltaTime;

	// Flow: FindFloor → IsWalkable → snap Walking or reject steep (Falling, no tunnel)
	const float LandWindow = FMath::Max(
		Movement.MaxStepHeight + Movement.Skin, (FMath::Abs(VelocityZ) * DeltaTime) + (Movement.Skin * 4.0f));
	FindFloor(PhysScene, CurrentFloor, LandWindow, DebugDraw);

	if (VelocityZ <= 0.0f && CurrentFloor.bBlockingHit)
	{
		const float SurfaceZ = CurrentFloor.Hit.ImpactPoint.Z;
		if (CurrentFloor.bWalkableFloor)
		{
			MutableLocation().Z = SurfaceZ;
			VelocityZ = 0.0f;
			SetMovementMode(EMovementMode::Walking);
			JumpsRemaining = FMath::Max(0, Movement.MaxJumpCount - 1);
			if (!bWasGrounded)
			{
				bJustLanded = true;
			}
		}
		else
		{
			// Unreal: unwalkable floor steep — stay Falling, do not sink through.
			if (MutableLocation().Z < SurfaceZ)
			{
				MutableLocation().Z = SurfaceZ;
			}
			if (CurrentFloor.FloorDist <= (Movement.Skin * 4.0f) && VelocityZ < 0.0f)
			{
				VelocityZ = 0.0f;
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

	FVector& A = MutableLocation();
	FVector& B = Other.MutableLocation();
	const float ATop = A.Z + Capsule.GetCapsuleHalfHeight() * 2.0f;
	const float BTop = B.Z + Other.Capsule.GetCapsuleHalfHeight() * 2.0f;
	if (ATop < B.Z || BTop < A.Z)
	{
		return;
	}

	FVector2D Delta = FVector2D(A.X - B.X, A.Y - B.Y);
	float Dist = Delta.Size();
	const float MinDist = Capsule.GetCapsuleRadius() + Other.Capsule.GetCapsuleRadius();
	if (Dist >= MinDist - 1.0e-3f)
	{
		return;
	}

	FVector2D Normal{};
	if (Dist < 1.0e-2f)
	{
		// Deterministic axis when centers coincide (avoid NaN / jitter).
		Normal = (GetUniqueID() <= Other.GetUniqueID()) ? FVector2D(1.0f, 0.0f) : FVector2D(-1.0f, 0.0f);
		Dist = 0.0f;
	}
	else
	{
		Normal = Delta / Dist;
	}

	const float Penetration = MinDist - Dist;
	const float Half = Penetration * 0.5f;
	A.X += Normal.X * Half;
	A.Y += Normal.Y * Half;
	B.X -= Normal.X * Half;
	B.Y -= Normal.Y * Half;
	ClampPositionXY(A, Movement.WalkBounds);
	ClampPositionXY(B, Other.Movement.WalkBounds);
}

void ACharacter::Tick(float DeltaTime)
{
	if (auto* CharacterAnim = dynamic_cast<UCharacterAnimInstance*>(&Mesh.GetAnimInstance()))
	{
		CharacterAnim->SetMovementState(IsFalling(), VelocityZ, ConsumeJustLanded());
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
