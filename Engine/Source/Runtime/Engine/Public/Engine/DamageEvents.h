#pragma once

#include "CollisionQuery.h"
#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"

class AActor;
class UDamageType;

/**
 * The description of a hit that AActor::TakeDamage receives (UE: FDamageEvent, EngineTypes.h in 4.27; DamageEvents.h
 * from UE 5.1): the damage type, and in the subclasses where the damage came from. A receiver asks what the event is
 * with IsOfType(FPointDamageEvent::ClassID) and then casts it (UE's pattern; the events are not UObjects):
 *
 *     if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
 *     {
 *         const FPointDamageEvent& PointEvent = static_cast<const FPointDamageEvent&>(DamageEvent);
 *     }
 *
 * Leon: plain structs (UE's are USTRUCTs for Blueprint); the ids are UE's (0, 1, 2).
 */
struct ENGINE_API FDamageEvent
{
	FDamageEvent() = default;
	explicit FDamageEvent(TSubclassOf<UDamageType> InDamageTypeClass)
		: DamageTypeClass(InDamageTypeClass)
	{
	}
	FDamageEvent(const FDamageEvent&) = default;
	FDamageEvent& operator=(const FDamageEvent&) = default;
	virtual ~FDamageEvent() = default;

	/** The kind of damage; null means UDamageType's defaults (UE: DamageTypeClass). */
	TSubclassOf<UDamageType> DamageTypeClass;

	/** The id of the plain event (UE: FDamageEvent::ClassID). */
	static constexpr int32 ClassID = 0;

	/** The id of the event's class (UE: GetTypeID). */
	[[nodiscard]] virtual int32 GetTypeID() const
	{
		return FDamageEvent::ClassID;
	}

	/** True for a plain event's id and for this event's own (UE: IsOfType). */
	[[nodiscard]] bool IsOfType(int32 InID) const
	{
		return (FDamageEvent::ClassID == InID) || (GetTypeID() == InID);
	}

	/**
	 * The hit to show and the direction of the impulse (UE: GetBestHitInfo): a plain event makes a hit on HitActor's
	 * location, the impulse from HitInstigator towards it (none without an instigator).
	 */
	virtual void GetBestHitInfo(
		const AActor* HitActor, const AActor* HitInstigator, FHitResult& OutHitInfo, FVector& OutImpulseDir) const;
};

/** A hit at one point: a bullet (UE: FPointDamageEvent). */
struct ENGINE_API FPointDamageEvent : public FDamageEvent
{
	FPointDamageEvent() = default;
	FPointDamageEvent(float InDamage, const FHitResult& InHitInfo, const FVector& InShotDirection,
		TSubclassOf<UDamageType> InDamageTypeClass)
		: FDamageEvent(InDamageTypeClass)
		, Damage(InDamage)
		, ShotDirection(InShotDirection)
		, HitInfo(InHitInfo)
	{
	}

	/** The damage the hit carries (UE: Damage; TakeDamage's amount is the one applied). */
	float Damage = 0.0f;
	/** The direction of the shot, a unit vector (UE: ShotDirection). */
	FVector ShotDirection = FVector::ZeroVector;
	/** Where the shot struck (UE: HitInfo). */
	FHitResult HitInfo;

	/** UE: FPointDamageEvent::ClassID. */
	static constexpr int32 ClassID = 1;

	[[nodiscard]] int32 GetTypeID() const override
	{
		return FPointDamageEvent::ClassID;
	}

	/** The hit itself and the shot's direction (UE). */
	void GetBestHitInfo(const AActor* HitActor, const AActor* HitInstigator, FHitResult& OutHitInfo,
		FVector& OutImpulseDir) const override;
};

/**
 * How radial damage falls off with the distance from its origin (UE: FRadialDamageParams): full damage within
 * InnerRadius, then down to MinimumDamage at OuterRadius along (1 - t)^DamageFalloff, nothing beyond.
 */
struct ENGINE_API FRadialDamageParams
{
	FRadialDamageParams() = default;
	FRadialDamageParams(
		float InBaseDamage, float InMinimumDamage, float InInnerRadius, float InOuterRadius, float InDamageFalloff)
		: BaseDamage(InBaseDamage)
		, MinimumDamage(InMinimumDamage)
		, InnerRadius(InInnerRadius)
		, OuterRadius(InOuterRadius)
		, DamageFalloff(InDamageFalloff)
	{
	}
	/** Full damage within the radius (UE's two-argument constructor: the inner and outer radius are the same). */
	FRadialDamageParams(float InBaseDamage, float InRadius)
		: BaseDamage(InBaseDamage)
		, InnerRadius(InRadius)
		, OuterRadius(InRadius)
	{
	}

	/** The damage at the origin (UE: BaseDamage). */
	float BaseDamage = 0.0f;
	/** The damage at OuterRadius (UE: MinimumDamage). */
	float MinimumDamage = 0.0f;
	/** Full damage up to here, cm (UE: InnerRadius). */
	float InnerRadius = 0.0f;
	/** No damage from here on, cm (UE: OuterRadius). */
	float OuterRadius = 0.0f;
	/** The falloff exponent; 0 is full damage over the whole radius (UE: DamageFalloff). */
	float DamageFalloff = 1.0f;

	/**
	 * The fraction of the damage at a distance (UE: GetDamageScale): 1 within InnerRadius or without a falloff, 0 from
	 * OuterRadius, (1 - (Distance - Inner) / (Outer - Inner))^DamageFalloff between.
	 */
	[[nodiscard]] float GetDamageScale(float DistanceFromEpicenter) const;

	/** UE: GetMaxRadius. */
	[[nodiscard]] float GetMaxRadius() const
	{
		return FMath::Max(FMath::Max(InnerRadius, OuterRadius), 0.0f);
	}
};

/** Damage from a point that reaches the components around it: an explosion (UE: FRadialDamageEvent). */
struct ENGINE_API FRadialDamageEvent : public FDamageEvent
{
	FRadialDamageParams Params;
	/** The explosion's centre (UE: Origin). */
	FVector Origin = FVector::ZeroVector;
	/**
	 * The receiver's components the damage reached, each with the point it struck (UE: ComponentHits);
	 * AActor::InternalTakeRadialDamage uses the closest one.
	 */
	TArray<FHitResult> ComponentHits;

	/** UE: FRadialDamageEvent::ClassID. */
	static constexpr int32 ClassID = 2;

	[[nodiscard]] int32 GetTypeID() const override
	{
		return FRadialDamageEvent::ClassID;
	}

	/** The first component hit, pushed away from the origin (UE). */
	void GetBestHitInfo(const AActor* HitActor, const AActor* HitInstigator, FHitResult& OutHitInfo,
		FVector& OutImpulseDir) const override;
};
