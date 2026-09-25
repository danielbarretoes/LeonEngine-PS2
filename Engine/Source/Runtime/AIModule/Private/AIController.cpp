#include "AIController.h"

#include "AI/Navigation/NavigationSystem.h"
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

} // namespace

AAIController::AAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void AAIController::ClearPath()
{
	Path.Reset();
	PathIndex = 0;
	bUsePath = false;
	PathRebuildCooldown = 0.0f;
}

void AAIController::RebuildPath()
{
	ClearPath();
	ACharacter* Character = GetCharacter();
	if (Character == nullptr || Navigation == nullptr || !Navigation->HasNavMesh() || !bHasTarget)
	{
		return;
	}
	TArray<FVector> Found;
	if (!Navigation->FindPath(Character->GetActorLocation(), Target, Found) || Found.Num() == 0)
	{
		return;
	}
	Path = MoveTemp(Found);
	PathIndex = 0;
	bUsePath = true;
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
	// Nav is authoritative: never charge the goal in a straight line through blockers.
	if (Navigation == nullptr || !Navigation->HasNavMesh())
	{
		return SteerToward(From, Target, ArriveRadius);
	}
	FVector OnMesh = FVector::ZeroVector;
	if (!Navigation->ProjectPointToNavigation(From, OnMesh))
	{
		return {};
	}
	const FVector ToMesh = SteerToward(From, OnMesh, WaypointArriveRadius);
	if (FVector::DotProduct(ToMesh, ToMesh) > 1.0e-8f)
	{
		return ToMesh;
	}
	FVector GoalNav = FVector::ZeroVector;
	if (!Navigation->ProjectPointToNavigation(Target, GoalNav))
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
	LogicState = EAILogicState::MoveTo;
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
	LogicState = EAILogicState::Chase;
	Target = Actor->GetActorLocation();
	// Repath on acquire / when not following; TickAI refreshes on an interval while chasing.
	if (!bSameActor || !bUsePath)
	{
		RebuildPath();
	}
}

void AAIController::StopMovement()
{
	bHasTarget = false;
	MoveActor = nullptr;
	LogicState = EAILogicState::Idle;
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
			if (!bUsePath || PathRebuildCooldown >= PathRebuildIntervalSeconds)
			{
				PathRebuildCooldown = 0.0f;
				RebuildPath();
			}
		}
	}

	FVector Wish = WishDir;
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
			const bool bOnFinalSegment = PathIndex + 1 >= Path.Num();
			const FVector& Wp = Path[FMath::Min(PathIndex, Path.Num() - 1)];
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

	Character->AddMovementInput(Wish);
	return Wish;
}
