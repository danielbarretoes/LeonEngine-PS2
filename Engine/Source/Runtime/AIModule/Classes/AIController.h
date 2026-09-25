#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Controller.h"

class AActor;
class UNavigationSystem;

/** High-level AAIController mode for games that do not run a UBehaviorTree. */
enum class EAILogicState : uint8
{
	Idle = 0,
	MoveTo = 1,
	Chase = 2,
};

/**
 * Drives a possessed Pawn with simple steering (Unreal-style AAIController).
 * When a UNavigationSystem is set, MoveTo* follows a NavMesh path; otherwise line-of-sight XZ.
 */
class AIMODULE_API AAIController : public AController
{
public:
	void SetWishDirection(const FVector& WishDirXz)
	{
		WishDir = WishDirXz;
	}
	void ClearWishDirection()
	{
		WishDir = {};
	}

	void SetLogicState(EAILogicState State)
	{
		LogicState = State;
	}
	[[nodiscard]] EAILogicState GetLogicState() const
	{
		return LogicState;
	}

	/** Optional; enables FindPath for MoveToLocation / MoveToActor. */
	void SetNavigationSystem(UNavigationSystem* InNavigation)
	{
		Navigation = InNavigation;
	}
	[[nodiscard]] UNavigationSystem* GetNavigationSystem() const
	{
		return Navigation;
	}

	void MoveToLocation(const FVector& WorldPosition);
	/** Chase an Actor each TickAI (repaths periodically when nav is available). */
	void MoveToActor(AActor* Actor);
	void StopMovement();

	[[nodiscard]] bool HasMoveTarget() const
	{
		return bHasTarget;
	}
	[[nodiscard]] AActor* GetMoveActor() const
	{
		return MoveActor;
	}
	[[nodiscard]] const FVector& MoveTarget() const
	{
		return Target;
	}
	[[nodiscard]] bool HasPath() const
	{
		return Path.Num() > 0;
	}
	[[nodiscard]] bool IsFollowingPath() const
	{
		return bUsePath && Path.Num() > 0;
	}
	[[nodiscard]] const TArray<FVector>& PathPoints() const
	{
		return Path;
	}

	/**
	 * Goal arrive radius (final target). Waypoint arrive stays tight so large values
	 * cannot skip detour corners through a blocker (Euclidean shortcut).
	 */
	void SetArriveRadius(float Radius)
	{
		ArriveRadius = Radius > 0.0f ? Radius : 0.0f;
	}
	[[nodiscard]] float GetArriveRadius() const
	{
		return ArriveRadius;
	}

	/** Steer possessed Character (path / target wins over manual wish). Returns wish used. */
	FVector TickAI(float DeltaTime);

private:
	void RebuildPath();
	void ClearPath();
	[[nodiscard]] FVector SteerToward(const FVector& From, const FVector& To, float InArriveRadius) const;
	[[nodiscard]] FVector SteerWithNavFallback(const FVector& From) const;

	FVector WishDir = FVector::ZeroVector;
	FVector Target = FVector::ZeroVector;
	AActor* MoveActor = nullptr;
	UNavigationSystem* Navigation = nullptr;
	TArray<FVector> Path;
	int32 PathIndex = 0;
	float PathRebuildCooldown = 0.0f;
	bool bHasTarget = false;
	bool bUsePath = false;
	/** cm */
	float ArriveRadius = 35.0f;
	EAILogicState LogicState = EAILogicState::Idle;
};
