#include "AIController.h"

#include "AI/Navigation/NavigationSystem.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>

namespace
{

	constexpr float PathRebuildIntervalSeconds = 0.35f;
	/// Tight waypoint arrive — must stay well below typical obstacle half-width so path
	/// corners are not skipped via straight-line distance through a blocker.
	constexpr float WaypointArriveRadius = 0.45f;

} // namespace

void AAIController::ClearPath()
{
	Path.clear();
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
	std::vector<glm::vec3> Found;
	if (!Navigation->FindPath(Character->GetActorLocation(), Target, Found) || Found.empty())
	{
		return;
	}
	Path = std::move(Found);
	PathIndex = 0;
	bUsePath = true;
}

glm::vec3 AAIController::SteerToward(const glm::vec3& From, const glm::vec3& To, float InArriveRadius) const
{
	const glm::vec3 Delta = To - From;
	const glm::vec3 Flat{Delta.x, 0.0f, Delta.z};
	const float DistSq = glm::dot(Flat, Flat);
	const float Arrive = InArriveRadius * InArriveRadius;
	if (DistSq <= Arrive)
	{
		return {};
	}
	const float Len = std::sqrt(DistSq);
	return Flat / Len;
}

glm::vec3 AAIController::SteerWithNavFallback(const glm::vec3& From) const
{
	// Nav is authoritative: never charge the goal in a straight line through blockers.
	if (Navigation == nullptr || !Navigation->HasNavMesh())
	{
		return SteerToward(From, Target, ArriveRadius);
	}
	glm::vec3 OnMesh{};
	if (!Navigation->ProjectPointToNavigation(From, OnMesh))
	{
		return {};
	}
	const glm::vec3 ToMesh = SteerToward(From, OnMesh, WaypointArriveRadius);
	if (glm::dot(ToMesh, ToMesh) > 1.0e-8f)
	{
		return ToMesh;
	}
	glm::vec3 GoalNav{};
	if (!Navigation->ProjectPointToNavigation(Target, GoalNav))
	{
		return {};
	}
	return SteerToward(From, GoalNav, ArriveRadius);
}

void AAIController::MoveToLocation(const glm::vec3& WorldPosition)
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

glm::vec3 AAIController::TickAI(float DeltaTime)
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

	glm::vec3 Wish = WishDir;
	if (bHasTarget)
	{
		const glm::vec3 From = Character->GetActorLocation();
		if (bUsePath && !Path.empty())
		{
			// Advance at most along truly-reached waypoints (tight radius — no Euclidean
			// shortcut through a plate/ramp whose width is smaller than ArriveRadius).
			while (PathIndex + 1 < Path.size())
			{
				const glm::vec3& Wp = Path[PathIndex];
				const glm::vec3 D = Wp - From;
				const float DistSq = D.x * D.x + D.z * D.z;
				if (DistSq <= WaypointArriveRadius * WaypointArriveRadius)
				{
					++PathIndex;
				}
				else
				{
					break;
				}
			}
			const bool bOnFinalSegment = PathIndex + 1 >= Path.size();
			const glm::vec3& Wp = Path[std::min(PathIndex, Path.size() - 1)];
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
