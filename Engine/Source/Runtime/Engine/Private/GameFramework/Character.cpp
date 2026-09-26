#include "GameFramework/Character.h"

#include "Animation/CharacterAnimInstance.h"
#include "Engine/World.h"

namespace
{

	/** UE yaw in degrees of a horizontal direction: 0 = +X, 90 = +Y. */
	float YawFromMove(const FVector& Move)
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

const FName ACharacter::CapsuleComponentName(TEXT("CollisionCylinder"));
const FName ACharacter::CharacterMovementComponentName(TEXT("CharMoveComp"));
const FName ACharacter::MeshComponentName(TEXT("CharacterMesh0"));

ACharacter::ACharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.DoNotCreateDefaultSubobject(AActor::DefaultSceneRootName))
{
	// The capsule is the root: its relative transform is the actor's (the feet, see the class comment).
	CapsuleComponent = CreateDefaultSubobject<UCapsuleComponent>(CapsuleComponentName);
	CapsuleComponent->InitCapsuleSize(35.0f, 92.5f);
	CapsuleComponent->bBaseAtComponentLocation = true;
	// UE's Pawn profile, query only (the capsule is swept, never simulated): traces hit characters, except on the
	// Visibility channel (UE) and the Pawn channel (Leon: the world separates overlapping pawns itself).
	CapsuleComponent->SetCollisionObjectType(ECC_Pawn);
	CapsuleComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	CapsuleComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	CapsuleComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CapsuleComponent->SetCanEverAffectNavigation(false);
	RootComponent = CapsuleComponent;

	CharacterMovement = CreateDefaultSubobject<UCharacterMovementComponent>(CharacterMovementComponentName);
	CharacterMovement->UpdatedComponent = CapsuleComponent;

	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(MeshComponentName);
	Mesh->SetupAttachment(GetRootComponent());
	// Legacy content faces +Y (UE: the mannequin mesh's relative yaw of -90).
	Mesh->RelativeRotation = FRotator(0.0f, LegacyContentYaw, 0.0f);

	bIsCrouched = false;
}

bool ACharacter::CanCrouch() const
{
	return !bIsCrouched && CharacterMovement->CanEverCrouch() && !CapsuleComponent->IsSimulatingPhysics();
}

void ACharacter::Crouch(bool /*bClientSimulation*/)
{
	if (CanCrouch())
	{
		CharacterMovement->bWantsToCrouch = true;
	}
}

void ACharacter::UnCrouch(bool /*bClientSimulation*/)
{
	CharacterMovement->bWantsToCrouch = false;
}

void ACharacter::OnStartCrouch(float /*HalfHeightAdjust*/, float /*ScaledHalfHeightAdjust*/)
{
	RecalculateBaseEyeHeight();
}

void ACharacter::OnEndCrouch(float /*HalfHeightAdjust*/, float /*ScaledHalfHeightAdjust*/)
{
	RecalculateBaseEyeHeight();
}

void ACharacter::RecalculateBaseEyeHeight()
{
	if (!bIsCrouched)
	{
		Super::RecalculateBaseEyeHeight();
	}
	else
	{
		BaseEyeHeight = CrouchedEyeHeight;
	}
}

void ACharacter::Reset(const FVector& InLocation, const FRotator& InRotation)
{
	SetActorLocationAndRotation(InLocation, InRotation);
	WishDir = {};
	bWishFromInputVector = false;
	VelocityZ = 0.0f;
	CharacterMovement->Velocity = FVector::ZeroVector;
	SetMovementMode(EMovementMode::Walking);
	bJumpRequested = false;
	bJustLanded = false;
	bYawInitialized = false;
	// A reset character stands (a respawn needs no room check).
	CharacterMovement->bWantsToCrouch = false;
	if (bIsCrouched)
	{
		const ACharacter* DefaultCharacter = GetClass()->GetDefaultObject<ACharacter>();
		CapsuleComponent->SetCapsuleSize(CapsuleComponent->GetUnscaledCapsuleRadius(),
			DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight());
		bIsCrouched = false;
		RecalculateBaseEyeHeight();
	}
	JumpsRemaining = FMath::Max(0, CharacterMovement->MaxJumpCount - 1);
	CurrentFloor = {};
	CapsuleComponent->SendPhysicsTransform();
}

void ACharacter::ApplyReplicatedState(
	const FVector& InLocation, const FRotator& InRotation, float InVelocityZ, bool bGrounded)
{
	SetActorLocationAndRotation(InLocation, InRotation);
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
	Mesh->GetAnimInstance().SetBlendSpaceInput(AnimBlendInput);
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

void ACharacter::FaceRotation(const FRotator& NewRotation, float DeltaTime)
{
	ApplyYaw(NewRotation.Yaw, DeltaTime);
}

bool ACharacter::IsWalkable(const FHitResult& Hit) const
{
	if (!Hit.bBlockingHit)
	{
		return false;
	}
	return Hit.ImpactNormal.Z >= CharacterMovement->WalkableFloorZ;
}

void ACharacter::InitCollisionParams(FCollisionQueryParams& OutParams, FCollisionResponseParams& OutResponseParam) const
{
	OutParams.IgnoreComponentID = static_cast<SIZE_T>(CapsuleComponent->GetUniqueID());
	OutResponseParam.CollisionResponse = CapsuleComponent->GetCollisionResponseToChannels();
}

void ACharacter::FindFloor(
	FPhysScene& PhysScene, FFindFloorResult& OutFloor, float TraceDistance, FDebugDraw* DebugDraw) const
{
	OutFloor = {};
	const float Distance = FMath::Max(TraceDistance, CharacterMovement->Skin);
	const FVector Feet = GetActorLocation();
	// Sphere rests on the feet (center = feet + radius up).
	const FVector SphereCenter = Feet + FVector(0.0f, 0.0f, GetCapsule().GetCapsuleRadius());
	const FVector TraceStart = SphereCenter + FVector(0.0f, 0.0f, CharacterMovement->Skin);
	const FVector TraceEnd = SphereCenter - FVector(0.0f, 0.0f, Distance);

	FCollisionQueryParams Query{};
	FCollisionResponseParams Response;
	InitCollisionParams(Query, Response);
	Query.bTraceFloorPlane = true;
	Query.FloorZ = CharacterMovement->FloorZ;
	Query.DrawDebugType = DebugDraw != nullptr ? EDrawDebugTrace::ForOneFrame : EDrawDebugTrace::None;

	FHitResult Hit{};
	const bool bHitFloor = PhysScene.SphereTraceSingleByChannel(Hit, TraceStart, TraceEnd,
		GetCapsule().GetCapsuleRadius(), GetMovementTraceChannel(), Query, DebugDraw, Response);
	if (!bHitFloor)
	{
		return;
	}

	OutFloor.bBlockingHit = true;
	OutFloor.Hit = Hit;
	OutFloor.bWalkableFloor = IsWalkable(Hit);
	OutFloor.FloorDist = FMath::Max(0.0f, Feet.Z - Hit.ImpactPoint.Z);
}

void ACharacter::ApplyYaw(float TargetYaw, float DeltaTime)
{
	if (!bYawInitialized)
	{
		MutableRotation().Yaw = TargetYaw;
		bYawInitialized = true;
		return;
	}
	const float Delta = ShortestYawDelta(GetActorRotation().Yaw, TargetYaw);
	const float T = 1.0f - FMath::Exp(-CharacterMovement->TurnSharpness * DeltaTime);
	MutableRotation().Yaw += Delta * T;
}

float ACharacter::CapsuleHalfHeight() const
{
	return FMath::Max(0.0f, GetCapsule().GetCapsuleHalfHeight() - GetCapsule().GetCapsuleRadius());
}

FVector ACharacter::CapsuleCenterFromFeet(const FVector& Feet) const
{
	return Feet + FVector(0.0f, 0.0f, GetCapsule().GetCapsuleHalfHeight());
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
	FCollisionResponseParams Response;
	InitCollisionParams(Query, Response);
	Query.bTraceFloorPlane = false;
	Query.DrawDebugType = DebugDraw != nullptr ? EDrawDebugTrace::ForOneFrame : EDrawDebugTrace::None;

	TArray<FHitResult> Hits;
	(void)PhysScene.CapsuleTraceMultiByChannel(Hits, StartCenter, EndCenter, GetCapsule().GetCapsuleRadius(), HalfH,
		GetMovementTraceChannel(), Query, DebugDraw, Response);

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
		ClampPositionXY(Feet, CharacterMovement->WalkBounds);
		if (OutHit != nullptr)
		{
			*OutHit = {};
		}
		return true;
	}

	float T = Block->Time;
	T = FMath::Max(0.0f, T - (CharacterMovement->Skin / DeltaLen));
	Feet += Delta * T;
	// Nudge out of the wall so the next iteration does not re-hit at t=0.
	FVector N = FVector(Block->ImpactNormal.X, Block->ImpactNormal.Y, 0.0f);
	const float NLen = N.Size();
	if (NLen > 1.0e-4f)
	{
		N /= NLen;
		Feet += N * CharacterMovement->Skin;
	}
	// Sweep stops before overlap; ResolveCapsuleSides push never fires — shove from the hit.
	(void)PhysScene.ApplyCapsuleSweepPush(Block->ComponentID, FVector2D(WishDir.X, WishDir.Y), Block->ImpactNormal,
		CharacterMovement->PushStrength, CharacterMovement->WalkBounds);
	ClampPositionXY(Feet, CharacterMovement->WalkBounds);
	if (OutHit != nullptr)
	{
		*OutHit = *Block;
	}
	return false;
}

void ACharacter::ResolveSides(FPhysScene& PhysScene, bool bApplyPush)
{
	FCapsuleContactParams Params{};
	Params.PushStrength = CharacterMovement->PushStrength;
	Params.StepUp = CharacterMovement->MaxStepHeight;
	Params.Skin = CharacterMovement->Skin;
	Params.WalkBounds = CharacterMovement->WalkBounds;
	FCollisionQueryParams Query{};
	FCollisionResponseParams Response;
	InitCollisionParams(Query, Response);
	FVector Feet = MutableLocation();
	PhysScene.ResolveCapsuleSides(GetCapsule(), Feet, FVector2D(WishDir.X, WishDir.Y), Params, Query.IgnoreComponentID,
		bApplyPush, GetMovementTraceChannel(), Response);
	MutableLocation() = Feet;
}

bool ACharacter::TryStepUp(FPhysScene& PhysScene, const FVector& ForwardDelta, FDebugDraw* DebugDraw)
{
	if (!IsMovingOnGround() || CharacterMovement->MaxStepHeight <= 1.0e-2f)
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
	FCollisionResponseParams Response;
	InitCollisionParams(Query, Response);
	Query.bTraceFloorPlane = false;
	Query.DrawDebugType = DebugDraw != nullptr ? EDrawDebugTrace::ForOneFrame : EDrawDebugTrace::None;

	// 1) Raise by MaxStepHeight; only a true ceiling (downward normal) aborts.
	const FVector UpStart = CapsuleCenterFromFeet(Feet);
	const FVector UpEnd = UpStart + FVector(0.0f, 0.0f, CharacterMovement->MaxStepHeight);
	TArray<FHitResult> UpHits;
	(void)PhysScene.CapsuleTraceMultiByChannel(UpHits, UpStart, UpEnd, GetCapsule().GetCapsuleRadius(), HalfH,
		GetMovementTraceChannel(), Query, DebugDraw, Response);
	for (const FHitResult& UpHit : UpHits)
	{
		if (UpHit.bBlockingHit && UpHit.ImpactNormal.Z < -0.5f)
		{
			return false;
		}
	}
	Feet.Z = StartFeet.Z + CharacterMovement->MaxStepHeight;

	// 2) Forward onto the ledge while elevated. One frame of leftover is often << radius;
	// probe at least ~half-radius so QuerySupportZ can see the top.
	const float FwdLen = Fwd.Size();
	const float MinFwd = FMath::Max(GetCapsule().GetCapsuleRadius() * 0.5f, CharacterMovement->Skin * 4.0f);
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
	FindFloor(PhysScene, Floor, CharacterMovement->MaxStepHeight + (CharacterMovement->Skin * 4.0f), DebugDraw);
	if (!Floor.bWalkableFloor)
	{
		Feet = StartFeet;
		return false;
	}
	const float HeightGain = Floor.Hit.ImpactPoint.Z - StartFeet.Z;
	if (HeightGain < CharacterMovement->Skin || HeightGain > CharacterMovement->MaxStepHeight + CharacterMovement->Skin)
	{
		Feet = StartFeet;
		return false;
	}
	// Sphere FindFloor can report a phantom shelf in front of an AABB; require real support.
	const float Support =
		PhysScene.QuerySupportZ(GetCapsule(), Feet, CharacterMovement->FloorZ, CharacterMovement->MaxStepHeight,
			CharacterMovement->Skin, Query.IgnoreComponentID, GetMovementTraceChannel(), Response);
	if (Support < StartFeet.Z + CharacterMovement->Skin)
	{
		Feet = StartFeet;
		return false;
	}

	Feet.Z = FMath::Max(Floor.Hit.ImpactPoint.Z, Support);
	if (Feet.Z - StartFeet.Z > CharacterMovement->MaxStepHeight + CharacterMovement->Skin)
	{
		Feet = StartFeet;
		return false;
	}
	VelocityZ = 0.0f;
	SetMovementMode(EMovementMode::Walking);
	CurrentFloor = Floor;
	ClampPositionXY(Feet, CharacterMovement->WalkBounds);
	return true;
}

void ACharacter::MoveHorizontal(FPhysScene& PhysScene, float DeltaTime, FDebugDraw* DebugDraw)
{
	if (!CharacterMovement->bInstantVelocity)
	{
		MoveHorizontalWithVelocity(PhysScene, DeltaTime, DebugDraw);
		return;
	}
	const float Len = WishDir.Size();
	if (Len <= 1.0e-4f)
	{
		ResolveSides(PhysScene, true);
		return;
	}

	const FVector Dir = WishDir / Len;
	if (bOrientRotationToMovement)
	{
		ApplyYaw(YawFromMove(Dir) + CharacterMovement->ModelYawOffset, DeltaTime);
	}

	// Flow: SafeMove → step-up (if Walking + blocked) → slide → ResolveCapsuleSides.
	// Falling uses AirControl fraction of the speed (Unreal AirControl lite).
	float SpeedScale = 1.0f;
	if (IsFalling())
	{
		SpeedScale = FMath::Clamp(CharacterMovement->AirControl, 0.0f, 1.0f);
	}
	FVector Remaining = Dir * (CharacterMovement->GetMaxSpeed() * SpeedScale * DeltaTime);
	Remaining.Z = 0.0f;
	MoveAlongFloor(PhysScene, Remaining, DeltaTime, DebugDraw);
	ResolveSides(PhysScene, true);
}

void ACharacter::MoveHorizontalWithVelocity(FPhysScene& PhysScene, float DeltaTime, FDebugDraw* DebugDraw)
{
	UCharacterMovementComponent& Move = *CharacterMovement;

	// UE: the input vector, at most 1 long, is an acceleration of MaxAcceleration and scales the speed it reaches
	// (ScaleInputAcceleration, ComputeAnalogInputModifier).
	const FVector Input = FVector(WishDir.X, WishDir.Y, 0.0f).GetClampedToMaxSize(1.0f);
	Move.Acceleration = Input * Move.MaxAcceleration;
	Move.AnalogInputModifier = FMath::Clamp(Input.Size(), 0.0f, 1.0f);
	if (bOrientRotationToMovement && !Input.IsNearlyZero())
	{
		ApplyYaw(YawFromMove(Input) + Move.ModelYawOffset, DeltaTime);
	}

	// UE: PhysWalking's CalcVelocity with the ground friction and braking; PhysFalling's with the air control on the
	// acceleration, the lateral friction and the falling braking (the vertical velocity is Leon's VelocityZ).
	Move.Velocity.Z = 0.0f;
	if (IsFalling())
	{
		const FVector WalkAcceleration = Move.Acceleration;
		if (!Move.Acceleration.IsZero())
		{
			Move.Acceleration = Move.GetAirControl(DeltaTime, Move.AirControl, Move.Acceleration);
		}
		Move.CalcVelocity(DeltaTime, Move.FallingLateralFriction, false, Move.BrakingDecelerationFalling);
		Move.Acceleration = WalkAcceleration;
	}
	else
	{
		Move.CalcVelocity(DeltaTime, Move.GroundFriction, false, Move.BrakingDecelerationWalking);
	}

	const FVector Before = GetActorLocation();
	FVector Remaining = Move.Velocity * DeltaTime;
	Remaining.Z = 0.0f;
	if (!Remaining.IsNearlyZero(1.0e-4f))
	{
		MoveAlongFloor(PhysScene, Remaining, DeltaTime, DebugDraw);
	}
	ResolveSides(PhysScene, true);

	// UE: the velocity becomes what the move did (a wall stops it), never faster than it was.
	FVector Moved = GetActorLocation() - Before;
	Moved.Z = 0.0f;
	FVector NewVelocity = Moved / DeltaTime;
	const float OldSpeed = Move.Velocity.Size();
	if (NewVelocity.SizeSquared() > FMath::Square(OldSpeed))
	{
		NewVelocity = NewVelocity.GetSafeNormal() * OldSpeed;
	}
	Move.Velocity = NewVelocity;
}

void ACharacter::MoveAlongFloor(FPhysScene& PhysScene, FVector Remaining, float /*DeltaTime*/, FDebugDraw* DebugDraw)
{
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
			VelocityZ = CharacterMovement->JumpZVelocity;
			SetMovementMode(EMovementMode::Falling);
			if (bCanGroundJump)
			{
				JumpsRemaining = FMath::Max(0, CharacterMovement->MaxJumpCount - 1);
			}
			else
			{
				--JumpsRemaining;
			}
			if (auto* CharacterAnim = Cast<UCharacterAnimInstance>(&Mesh->GetAnimInstance()))
			{
				CharacterAnim->NotifyJumped();
			}
		}
	}
	bJumpRequested = false;

	VelocityZ -= CharacterMovement->Gravity * DeltaTime;
	MutableLocation().Z += VelocityZ * DeltaTime;

	// Flow: FindFloor → IsWalkable → snap Walking or reject steep (Falling, no tunnel)
	const float LandWindow = FMath::Max(CharacterMovement->MaxStepHeight + CharacterMovement->Skin,
		(FMath::Abs(VelocityZ) * DeltaTime) + (CharacterMovement->Skin * 4.0f));
	FindFloor(PhysScene, CurrentFloor, LandWindow, DebugDraw);

	if (VelocityZ <= 0.0f && CurrentFloor.bBlockingHit)
	{
		const float SurfaceZ = CurrentFloor.Hit.ImpactPoint.Z;
		if (CurrentFloor.bWalkableFloor)
		{
			MutableLocation().Z = SurfaceZ;
			VelocityZ = 0.0f;
			SetMovementMode(EMovementMode::Walking);
			JumpsRemaining = FMath::Max(0, CharacterMovement->MaxJumpCount - 1);
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
			if (CurrentFloor.FloorDist <= (CharacterMovement->Skin * 4.0f) && VelocityZ < 0.0f)
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
	// UE: crouch or stand up as asked before moving.
	CharacterMovement->UpdateCharacterStateBeforeMovement(PhysScene);
	MoveHorizontal(PhysScene, DeltaTime, DebugDraw);
	IntegrateVertical(PhysScene, DeltaTime, DebugDraw);
	ResolveSides(PhysScene, false);
	if (!CharacterMovement->bInstantVelocity)
	{
		CharacterMovement->Velocity.Z = VelocityZ;
	}
}

void ACharacter::TickCharacterMovement(float DeltaTime, FDebugDraw* DebugDraw)
{
	UWorld* LocalWorld = GetWorld();
	if (LocalWorld == nullptr)
	{
		return;
	}
	// UE: the movement consumes the pawn's input vector each tick (the player's axes); a wish set with the legacy
	// AddMovementInput(Wish) stays until it is changed.
	const FVector PendingInput = ConsumeMovementInputVector();
	if (!PendingInput.IsNearlyZero())
	{
		WishDir = FVector(PendingInput.X, PendingInput.Y, 0.0f);
		bWishFromInputVector = true;
	}
	else if (bWishFromInputVector)
	{
		WishDir = FVector::ZeroVector;
		bWishFromInputVector = false;
	}
	PerformMovement(LocalWorld->GetPhysicsScene(), DeltaTime, DebugDraw);
	CapsuleComponent->SendPhysicsTransform();
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
	CapsuleComponent->SendPhysicsTransform();
}

void ACharacter::ResolvePawnOverlap(ACharacter& Other)
{
	if (this == &Other)
	{
		return;
	}

	FVector& A = MutableLocation();
	FVector& B = Other.MutableLocation();
	const float ATop = A.Z + GetCapsule().GetCapsuleHalfHeight() * 2.0f;
	const float BTop = B.Z + Other.GetCapsule().GetCapsuleHalfHeight() * 2.0f;
	if (ATop < B.Z || BTop < A.Z)
	{
		return;
	}

	FVector2D Delta = FVector2D(A.X - B.X, A.Y - B.Y);
	float Dist = Delta.Size();
	const float MinDist = GetCapsule().GetCapsuleRadius() + Other.GetCapsule().GetCapsuleRadius();
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
	ClampPositionXY(A, CharacterMovement->WalkBounds);
	ClampPositionXY(B, Other.CharacterMovement->WalkBounds);
}

void ACharacter::Tick(float DeltaTime)
{
	if (auto* CharacterAnim = Cast<UCharacterAnimInstance>(&Mesh->GetAnimInstance()))
	{
		CharacterAnim->SetMovementState(IsFalling(), VelocityZ, ConsumeJustLanded());
	}
	else
	{
		(void)ConsumeJustLanded();
	}
	Mesh->TickComponent(DeltaTime);
}
