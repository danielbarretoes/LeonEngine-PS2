#pragma once

#include "CoreMinimal.h"

class FDebugDraw;

/** No level mesh (UE: INDEX_NONE for an item); the value of ULevel::Npos. */
inline constexpr SIZE_T NoLevelMeshIndex = static_cast<SIZE_T>(-1);

/** UE-like ECollisionChannel (micro-engine subset). */
enum class ECollisionChannel : uint8
{
	WorldStatic, // Static PhysScene bodies
	WorldDynamic, // Dynamic PhysScene bodies
	Pawn, // Both (character / pawn queries)
	Visibility, // Both (generic line/sphere checks)
};

/** UE-like EDrawDebugTrace: draw the query for one frame when an FDebugDraw* is passed. */
enum class EDrawDebugTrace : uint8
{
	None,
	ForOneFrame,
};

/** UE-like FHitResult for FPhysScene traces (legacy Y-up metres until P7). */
struct PHYSICSCORE_API FHitResult
{
	bool bBlockingHit = false;
	/** Normalized distance along [Start, End] in [0, 1]. */
	float Time = 1.0f;
	float Distance = 0.0f;
	/** World location of the sweep shape center at the blocking time (UE: Location). */
	FVector Location = FVector::ZeroVector;
	/** Surface contact point (UE: ImpactPoint); equals Location for line traces. */
	FVector ImpactPoint = FVector::ZeroVector;
	/** Unit normal pointing toward the trace start (away from the surface). */
	FVector ImpactNormal = FVector(0.0f, 1.0f, 0.0f);
	FVector TraceStart = FVector::ZeroVector;
	FVector TraceEnd = FVector::ZeroVector;
	SIZE_T LevelMeshIndex = NoLevelMeshIndex;
	/** True when the hit is the virtual infinite floor plane (FCollisionQueryParams). */
	bool bFloorPlane = false;
};

/** UE-like FCollisionQueryParams. */
struct PHYSICSCORE_API FCollisionQueryParams
{
	SIZE_T SkipLevelMeshIndex = NoLevelMeshIndex;
	/** Include an infinite horizontal floor at FloorY (UCharacterMovementComponent floor). */
	bool bTraceFloorPlane = false;
	float FloorY = 0.0f;
	/** When not None, FPhysScene traces draw into the provided FDebugDraw* (F2 / gameplay debug). */
	EDrawDebugTrace DrawDebugType = EDrawDebugTrace::None;
};

/** UE-like DrawDebugLineTrace / Kismet System Library helpers (one frame into FDebugDraw; defined in Engine). */
void DrawDebugLineTrace(FDebugDraw& Draw, const FVector& Start, const FVector& End, const TArray<FHitResult>& Hits);
void DrawDebugSphereTrace(
	FDebugDraw& Draw, const FVector& Start, const FVector& End, float Radius, const TArray<FHitResult>& Hits);
void DrawDebugCapsuleTrace(FDebugDraw& Draw, const FVector& Start, const FVector& End, float Radius, float HalfHeight,
	const TArray<FHitResult>& Hits);
