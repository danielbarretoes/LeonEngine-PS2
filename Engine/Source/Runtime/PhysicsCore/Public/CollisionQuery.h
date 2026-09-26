#pragma once

#include "CollisionResponseContainer.h"
#include "CoreMinimal.h"
#include "UObject/WeakObjectPtrTemplates.h"

class AActor;
class FDebugDraw;
class UPrimitiveComponent;

/** No body owner (UE: an empty FCollisionQueryParams::IgnoreComponents, a null FHitResult::Component). */
inline constexpr SIZE_T NoComponentID = static_cast<SIZE_T>(-1);

/** UE-like EDrawDebugTrace: draw the query for one frame when an FDebugDraw* is passed. */
enum class EDrawDebugTrace : uint8
{
	None,
	ForOneFrame,
};

/** UE-like FHitResult for FPhysScene traces (centimetres, Z up). */
struct PHYSICSCORE_API FHitResult
{
	/** The query stopped here; false for an overlap (a touch) a Multi query reports on its way (UE). */
	bool bBlockingHit = false;
	/** Normalized distance along [Start, End] in [0, 1]. */
	float Time = 1.0f;
	float Distance = 0.0f;
	/** World location of the sweep shape center at the blocking time (UE: Location). */
	FVector Location = FVector::ZeroVector;
	/** Surface contact point (UE: ImpactPoint); equals Location for line traces. */
	FVector ImpactPoint = FVector::ZeroVector;
	/** Unit normal pointing toward the trace start (away from the surface). */
	FVector ImpactNormal = FVector(0.0f, 0.0f, 1.0f);
	FVector TraceStart = FVector::ZeroVector;
	FVector TraceEnd = FVector::ZeroVector;
	/** The hit body's ComponentID (UE: FHitResult::Component's unique id). */
	SIZE_T ComponentID = NoComponentID;
	/** True when the hit is the virtual infinite floor plane (FCollisionQueryParams). */
	bool bFloorPlane = false;
	/** The physics scene body that was hit (Leon: its index in FPhysScene::GetBodies), INDEX_NONE for the planes. */
	int32 BodyIndex = INDEX_NONE;
	/** The actor that owns the hit component (UE: Actor); unset for a body without a component. */
	TWeakObjectPtr<AActor> Actor;
	/** The hit component (UE: Component); unset for a body without a component. */
	TWeakObjectPtr<UPrimitiveComponent> Component;

	/** UE: GetActor. */
	[[nodiscard]] AActor* GetActor() const
	{
		return Actor.Get();
	}
	/** UE: GetComponent. */
	[[nodiscard]] UPrimitiveComponent* GetComponent() const
	{
		return Component.Get();
	}
};

/**
 * What a query leaves out and how it runs (UE: FCollisionQueryParams): the components and actors it ignores, and
 * Leon's infinite floor plane and debug drawing.
 */
struct PHYSICSCORE_API FCollisionQueryParams
{
	/** A body id the query ignores (Leon's single id; UE: AddIgnoredComponent). */
	SIZE_T IgnoreComponentID = NoComponentID;
	/** Include an infinite horizontal floor at height FloorZ (UCharacterMovementComponent floor). */
	bool bTraceFloorPlane = false;
	float FloorZ = 0.0f;
	/** When not None, FPhysScene traces draw into the provided FDebugDraw* (F2 / gameplay debug). */
	EDrawDebugTrace DrawDebugType = EDrawDebugTrace::None;
	/** Kept for the UE signature: Leon traces the triangles of static meshes whenever they have them. */
	bool bTraceComplex = false;
	/** Names the query in logs (UE: TraceTag). */
	FName TraceTag;

	FCollisionQueryParams() = default;
	/** UE's constructor: a tag, complex tracing and an actor to ignore (defined in Engine, which knows AActor). */
	FCollisionQueryParams(FName InTraceTag, bool bInTraceComplex = false, const AActor* InIgnoreActor = nullptr);

	/** Ignores a component's body (UE: AddIgnoredComponent; defined in Engine). */
	void AddIgnoredComponent(const UPrimitiveComponent* InIgnoreComponent);
	/** Ignores every body of an actor (UE: AddIgnoredActor; defined in Engine). */
	void AddIgnoredActor(const AActor* InIgnoreActor);
	/** Ignores the bodies of a component or an actor by unique id (UE: the uint32 overloads). */
	void AddIgnoredComponentID(SIZE_T InComponentID)
	{
		IgnoreComponents.AddUnique(InComponentID);
	}
	void AddIgnoredActorID(SIZE_T InActorID)
	{
		IgnoreActors.AddUnique(InActorID);
	}
	/** Forgets the ignored components and actors (UE: ClearIgnoredComponents / ClearIgnoredActors). */
	void ClearIgnoredComponents()
	{
		IgnoreComponents.Reset();
		IgnoreComponentID = NoComponentID;
	}
	void ClearIgnoredActors()
	{
		IgnoreActors.Reset();
	}
	[[nodiscard]] const TArray<SIZE_T>& GetIgnoredComponents() const
	{
		return IgnoreComponents;
	}
	[[nodiscard]] const TArray<SIZE_T>& GetIgnoredActors() const
	{
		return IgnoreActors;
	}

	/** True when a body of this component and actor is left out (IgnoreComponentID, the ignored lists). */
	[[nodiscard]] bool IsIgnored(SIZE_T ComponentID, SIZE_T ActorID) const;

private:
	/** UE: IgnoreComponents (unique ids). */
	TArray<SIZE_T> IgnoreComponents;
	/** UE: IgnoreActors (unique ids). */
	TArray<SIZE_T> IgnoreActors;
};

/**
 * A query's responses to the object types of the bodies it meets (UE: FCollisionResponseParams): a body is hit when
 * both its response to the trace channel and this response to its object type are not Ignore; the weaker one says
 * whether it blocks or overlaps.
 */
struct PHYSICSCORE_API FCollisionResponseParams
{
	FCollisionResponseContainer CollisionResponse;

	/** Starts from the default container (UE). */
	FCollisionResponseParams();
	explicit FCollisionResponseParams(ECollisionResponse DefaultResponse)
		: CollisionResponse(DefaultResponse)
	{
	}
	explicit FCollisionResponseParams(const FCollisionResponseContainer& ResponseContainer)
		: CollisionResponse(ResponseContainer)
	{
	}

	/** Blocks every object type (UE: DefaultResponseParam). */
	static const FCollisionResponseParams DefaultResponseParam;
};

/** The object types an object query looks for (UE: FCollisionObjectQueryParams). */
struct PHYSICSCORE_API FCollisionObjectQueryParams
{
	/** One bit per channel (UE: ObjectTypesToQuery, ECC_TO_BITFIELD). */
	int32 ObjectTypesToQuery = 0;

	FCollisionObjectQueryParams() = default;
	explicit FCollisionObjectQueryParams(ECollisionChannel QueryChannel)
	{
		AddObjectTypesToQuery(QueryChannel);
	}

	void AddObjectTypesToQuery(ECollisionChannel QueryChannel)
	{
		ObjectTypesToQuery |= (1 << static_cast<int32>(QueryChannel));
	}
	void RemoveObjectTypesToQuery(ECollisionChannel QueryChannel)
	{
		ObjectTypesToQuery &= ~(1 << static_cast<int32>(QueryChannel));
	}
	[[nodiscard]] bool IsValid() const
	{
		return ObjectTypesToQuery != 0;
	}
	[[nodiscard]] bool Contains(ECollisionChannel ObjectType) const
	{
		return (ObjectTypesToQuery & (1 << static_cast<int32>(ObjectType))) != 0;
	}
};

/** UE-like DrawDebugLineTrace / Kismet System Library helpers (one frame into FDebugDraw; defined in Engine). */
void DrawDebugLineTrace(FDebugDraw& Draw, const FVector& Start, const FVector& End, const TArray<FHitResult>& Hits);
void DrawDebugSphereTrace(
	FDebugDraw& Draw, const FVector& Start, const FVector& End, float Radius, const TArray<FHitResult>& Hits);
void DrawDebugCapsuleTrace(FDebugDraw& Draw, const FVector& Start, const FVector& End, float Radius, float HalfHeight,
	const TArray<FHitResult>& Hits);
