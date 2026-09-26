#include "AIController.h"

#include "AI/Navigation/NavigationSystem.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"

namespace
{

	constexpr float PathRebuildIntervalSeconds = 0.35f;
	/**
	 * Tight waypoint arrive — must stay well below typical obstacle half-width so path
	 * corners are not skipped via straight-line distance through a blocker (cm).
	 */
	constexpr float WaypointArriveRadius = 45.0f;

	/** A path point this much above the feet, this near, is jumped onto (above a step), cm. */
	constexpr float JumpRise = 50.0f;
	constexpr float JumpTriggerDistance = 150.0f;

	/** Moving less than this in StuckSeconds while following a path repaths, cm. */
	constexpr float StuckDistance = 30.0f;
	constexpr float StuckSeconds = 1.5f;

	/** What a path point's waypoint asks of the follower (ANavigationWaypoint::Flags). */
	namespace EPathPointFlags
	{
		enum Type : uint8
		{
			None = 0,
			Jump = 1 << 0,
			Crouch = 1 << 1,
		};
	} // namespace EPathPointFlags

	const FName JumpFlag(TEXT("Jump"));
	const FName CrouchFlag(TEXT("Crouch"));

} // namespace

const UNavigationSystem* AAIController::GetEffectiveNavigation() const
{
	if (Navigation != nullptr)
	{
		return Navigation;
	}
	const UWorld* World = GetWorld();
	return World != nullptr ? &World->GetNavigationSystem() : nullptr;
}

AAIController::AAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void AAIController::ClearPath()
{
	Path.Reset();
	PathPointFlags.Reset();
	PathIndex = 0;
	bUsePath = false;
	PathRebuildCooldown = 0.0f;
}

void AAIController::RebuildPath()
{
	ClearPath();
	ACharacter* Character = GetCharacter();
	const UNavigationSystem* Nav = GetEffectiveNavigation();
	if (Character == nullptr || Nav == nullptr || !Nav->HasNavigationData() || !bHasTarget)
	{
		return;
	}
	TArray<FVector> Found;
	TArray<int32> FoundNodes;
	if (!Nav->FindPath(Character->GetActorLocation(), Target, Found, &FoundNodes) || Found.Num() == 0)
	{
		return;
	}
	Path = MoveTemp(Found);
	PathPointFlags.SetNumZeroed(Path.Num());
	const TArray<UNavigationSystem::FNode>& Nodes = Nav->GetNodes();
	for (int32 Index = 0; Index < FoundNodes.Num(); ++Index)
	{
		if (FoundNodes[Index] == INDEX_NONE)
		{
			continue;
		}
		const TArray<FName>& Flags = Nodes[FoundNodes[Index]].Flags;
		PathPointFlags[Index] = uint8((Flags.Contains(JumpFlag) ? EPathPointFlags::Jump : EPathPointFlags::None) |
			(Flags.Contains(CrouchFlag) ? EPathPointFlags::Crouch : EPathPointFlags::None));
	}
	PathIndex = 0;
	bUsePath = true;
}

void AAIController::UpdatePathCrouch(ACharacter& Character)
{
	// The links on both sides of a Crouch point are low: crouched from the point before it to the point after it.
	const auto IsCrouchPoint = [this](int32 Index)
	{ return PathPointFlags.IsValidIndex(Index) && (PathPointFlags[Index] & EPathPointFlags::Crouch) != 0; };
	const bool bWantsCrouch = bHasTarget && bUsePath && (IsCrouchPoint(PathIndex) || IsCrouchPoint(PathIndex - 1));
	if (bWantsCrouch && !bCrouchedForPath && Character.CanCrouch())
	{
		Character.Crouch();
		bCrouchedForPath = true;
	}
	else if (!bWantsCrouch && bCrouchedForPath)
	{
		Character.UnCrouch();
		bCrouchedForPath = false;
	}
}

void AAIController::ResetStuckCheck()
{
	const ACharacter* Character = GetCharacter();
	StuckCheckLocation = Character != nullptr ? Character->GetActorLocation() : FVector::ZeroVector;
	StuckTime = 0.0f;
}

FVector AAIController::SteerToward(const FVector& From, const FVector& To, float InArriveRadius) const
{
	const FVector Delta = To - From;
	const FVector Flat = FVector(Delta.X, Delta.Y, 0.0f);
	const float DistSq = FVector::DotProduct(Flat, Flat);
	const float Arrive = InArriveRadius * InArriveRadius;
	if (DistSq <= Arrive)
	{
		return {};
	}
	const float Len = FMath::Sqrt(DistSq);
	return Flat / Len;
}

FVector AAIController::SteerWithNavFallback(const FVector& From) const
{
	// No path: toward the nearest waypoint first (the graph has no surface to project on), then the goal's nearest.
	const UNavigationSystem* Nav = GetEffectiveNavigation();
	if (Nav == nullptr || !Nav->HasNavigationData())
	{
		return SteerToward(From, Target, ArriveRadius);
	}
	FVector OnMesh = FVector::ZeroVector;
	if (!Nav->ProjectPointToNavigation(From, OnMesh))
	{
		return {};
	}
	const FVector ToMesh = SteerToward(From, OnMesh, WaypointArriveRadius);
	if (FVector::DotProduct(ToMesh, ToMesh) > 1.0e-8f)
	{
		return ToMesh;
	}
	FVector GoalNav = FVector::ZeroVector;
	if (!Nav->ProjectPointToNavigation(Target, GoalNav))
	{
		return {};
	}
	return SteerToward(From, GoalNav, ArriveRadius);
}

void AAIController::MoveToLocation(const FVector& WorldPosition)
{
	MoveActor = nullptr;
	Target = WorldPosition;
	bHasTarget = true;
	ResetStuckCheck();
	RebuildPath();
}

void AAIController::MoveToActor(AActor* Actor)
{
	if (Actor == nullptr)
	{
		StopMovement();
		return;
	}
	const bool bSameActor = (MoveActor == Actor);
	MoveActor = Actor;
	bHasTarget = true;
	Target = Actor->GetActorLocation();
	// Repath on acquire; TickAI refreshes on an interval while chasing (a failed path is retried at that pace too).
	if (!bSameActor)
	{
		ResetStuckCheck();
		RebuildPath();
	}
}

void AAIController::StopMovement()
{
	bHasTarget = false;
	MoveActor = nullptr;
	ClearPath();
}

FVector AAIController::TickAI(float DeltaTime)
{
	ACharacter* Character = GetCharacter();
	if (Character == nullptr)
	{
		return {};
	}

	if (MoveActor != nullptr)
	{
		if (MoveActor->IsPendingKillPending())
		{
			MoveActor = nullptr;
			bHasTarget = false;
			ClearPath();
		}
		else
		{
			Target = MoveActor->GetActorLocation();
			bHasTarget = true;
			PathRebuildCooldown += DeltaTime;
			if (PathRebuildCooldown >= PathRebuildIntervalSeconds)
			{
				PathRebuildCooldown = 0.0f;
				RebuildPath();
			}
		}
	}

	FVector Wish = FVector::ZeroVector;
	if (bHasTarget)
	{
		const FVector From = Character->GetActorLocation();
		if (bUsePath && Path.Num() > 0)
		{
			// Advance at most along truly-reached waypoints (tight radius — no Euclidean
			// shortcut through a plate/ramp whose width is smaller than ArriveRadius).
			while (PathIndex + 1 < Path.Num())
			{
				const FVector& Wp = Path[PathIndex];
				const FVector D = Wp - From;
				const float DistSq = D.X * D.X + D.Y * D.Y;
				if (DistSq <= WaypointArriveRadius * WaypointArriveRadius)
				{
					++PathIndex;
				}
				else
				{
					break;
				}
			}
			bool bOnFinalSegment = PathIndex + 1 >= Path.Num();
			// A copy: a repath below replaces Path.
			FVector Wp = Path[FMath::Min(PathIndex, Path.Num() - 1)];
			// A point above a step or flagged Jump, close: jump onto it (a crate, a ledge, a gap).
			const bool bJumpPoint =
				PathPointFlags.IsValidIndex(PathIndex) && (PathPointFlags[PathIndex] & EPathPointFlags::Jump) != 0;
			if ((bJumpPoint || Wp.Z - From.Z > JumpRise) &&
				FVector::DistSquared2D(Wp, From) <= FMath::Square(JumpTriggerDistance) && Character->IsMovingOnGround())
			{
				Character->Jump();
			}
			// No progress for a while (a pawn in the way, a corner): find the path again.
			StuckTime += DeltaTime;
			if (FVector::DistSquared2D(From, StuckCheckLocation) > FMath::Square(StuckDistance))
			{
				StuckCheckLocation = From;
				StuckTime = 0.0f;
			}
			else if (StuckTime >= StuckSeconds)
			{
				StuckTime = 0.0f;
				++NumRepathsWhenStuck;
				RebuildPath();
				if (!bUsePath || Path.Num() == 0)
				{
					UpdatePathCrouch(*Character);
					Character->AddMovementInput(Wish);
					return Wish;
				}
				bOnFinalSegment = PathIndex + 1 >= Path.Num();
				Wp = Path[FMath::Min(PathIndex, Path.Num() - 1)];
			}
			if (bOnFinalSegment)
			{
				Wish = SteerToward(From, Target, ArriveRadius);
			}
			else
			{
				Wish = SteerToward(From, Wp, WaypointArriveRadius);
			}
		}
		else
		{
			Wish = SteerWithNavFallback(From);
		}
	}

	UpdatePathCrouch(*Character);
	Character->AddMovementInput(Wish);
	return Wish;
}
