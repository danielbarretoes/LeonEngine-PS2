#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Controller.h"
#include "AIController.generated.h"

class AActor;
class UNavigationSystem;

/** High-level AAIController mode for games that do not run a UBehaviorTree. */
UENUM()
enum class EAILogicState : uint8
{
	Idle = 0,
	MoveTo = 1,
	Chase = 2,
};

/**
 * Drives a possessed Pawn with simple steering (Unreal-style AAIController), an actor the world spawns.
 * MoveTo* follows a path of the navigation (SetNavigationSystem's, else the world's waypoint graph) when it has
 * navigation data, else a straight line on XY (PathFollowing-lite): the path's points one by one, a jump onto a point
 * above a step, and a new path when the pawn has not moved for a while (stuck).
 */
UCLASS()
class AIMODULE_API AAIController : public AController
{
	GENERATED_BODY()

public:
	AAIController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	void SetWishDirection(const FVector& WishDirXY)
	{
		WishDir = WishDirXY;
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

	/** Optional: a navigation to path on instead of the world's (tests, a second graph). */
	void SetNavigationSystem(UNavigationSystem* InNavigation)
	{
		Navigation = InNavigation;
	}
	[[nodiscard]] UNavigationSystem* GetNavigationSystem() const
	{
		return Navigation;
	}
	/** The navigation MoveTo* paths on: SetNavigationSystem's, else the world's. */
	[[nodiscard]] const UNavigationSystem* GetEffectiveNavigation() const;
	/** How many times a stuck pawn found its path again. */
	[[nodiscard]] int32 GetNumRepathsWhenStuck() const
	{
		return NumRepathsWhenStuck;
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
	/** Starts the stuck clock over from where the pawn stands (a new goal). */
	void ResetStuckCheck();
	void ClearPath();
	[[nodiscard]] FVector SteerToward(const FVector& From, const FVector& To, float InArriveRadius) const;
	[[nodiscard]] FVector SteerWithNavFallback(const FVector& From) const;

	FVector WishDir = FVector::ZeroVector;
	FVector Target = FVector::ZeroVector;

	/** The actor MoveToActor chases; cleared by the collector when it is destroyed. */
	UPROPERTY()
	AActor* MoveActor = nullptr;

	/** Not a UObject (UNavigationSystem lite): the caller keeps it alive. */
	UNavigationSystem* Navigation = nullptr;
	TArray<FVector> Path;
	int32 PathIndex = 0;
	float PathRebuildCooldown = 0.0f;
	/** Where the pawn was when the stuck clock started, and for how long it has not moved away. */
	FVector StuckCheckLocation = FVector::ZeroVector;
	float StuckTime = 0.0f;
	int32 NumRepathsWhenStuck = 0;
	bool bHasTarget = false;
	bool bUsePath = false;
	/** cm */
	UPROPERTY()
	float ArriveRadius = 35.0f;

	UPROPERTY()
	EAILogicState LogicState = EAILogicState::Idle;
};
