#include "Components/ActorComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/DamageEvents.h"
#include "EngineLogs.h"
#include "GameFramework/Actor.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"

// The damage events and UGameplayStatics' damage helpers (UE: DamageEvents / GameplayStatics.cpp), and the actor
// queries that walk the world.

namespace
{

	/** The damage type class to send: the given one, else UDamageType (UE). */
	TSubclassOf<UDamageType> ValidDamageTypeClass(TSubclassOf<UDamageType> DamageTypeClass)
	{
		return DamageTypeClass != nullptr ? DamageTypeClass : TSubclassOf<UDamageType>(UDamageType::StaticClass());
	}

	/** The body of a component in the scene, or INDEX_NONE (Leon: an overlap reports it). */
	int32 FindBody(const FPhysScene& Scene, const UPrimitiveComponent& Component, int32 HintIndex)
	{
		if (HintIndex != INDEX_NONE && Scene.GetBodyOwner(HintIndex) == &Component)
		{
			return HintIndex;
		}
		return Scene.FindComponentBody(Component);
	}

	/**
	 * Whether radial damage from Origin reaches a component (UE: ComponentIsDamageableFrom): a line on TraceChannel
	 * from Origin to the centre of its body, ignoring the causer and IgnoreActors, must hit nothing or the component
	 * itself. OutHitResult is the hit on the component, or a hit made up at the component's location facing Origin
	 * (UE: the component's centre).
	 */
	bool ComponentIsDamageableFrom(UWorld& World, UPrimitiveComponent& VictimComp, int32 BodyIndex,
		const FVector& Origin, const AActor* IgnoredActor, const TArray<AActor*>& IgnoreActors,
		ECollisionChannel TraceChannel, FHitResult& OutHitResult)
	{
		FCollisionQueryParams LineParams(FName(TEXT("ComponentIsVisibleFrom")), true, IgnoredActor);
		for (const AActor* Ignored : IgnoreActors)
		{
			LineParams.AddIgnoredActor(Ignored);
		}
		const FPhysScene& Scene = World.GetPhysicsScene();
		const FVector TraceEnd =
			BodyIndex != INDEX_NONE ? Scene.GetBodyBounds(BodyIndex).GetCenter() : VictimComp.GetComponentLocation();
		FVector TraceStart = Origin;
		if (Origin == TraceEnd)
		{
			// A tiny nudge so the trace is not empty (UE).
			TraceStart.Z += 0.01f;
		}
		if (TraceChannel != ECC_MAX)
		{
			if (Scene.LineTraceSingleByChannel(OutHitResult, TraceStart, TraceEnd, TraceChannel, LineParams))
			{
				if (OutHitResult.GetComponent() == &VictimComp)
				{
					return true;
				}
				const AActor* Blocker = OutHitResult.GetActor();
				UE_LOG(LogEngine, Log, "Radial damage to %s blocked by %s", *VictimComp.GetName(),
					Blocker != nullptr ? *Blocker->GetName() : "a body without an actor");
				return false;
			}
		}
		// Nothing in the way: a hit at the component, its normal back toward the origin (UE).
		const FVector FakeHitLoc = TraceEnd;
		OutHitResult = FHitResult();
		OutHitResult.bBlockingHit = true;
		OutHitResult.Location = FakeHitLoc;
		OutHitResult.ImpactPoint = FakeHitLoc;
		OutHitResult.ImpactNormal = (Origin - FakeHitLoc).GetSafeNormal();
		OutHitResult.TraceStart = TraceStart;
		OutHitResult.TraceEnd = TraceEnd;
		OutHitResult.Component = &VictimComp;
		OutHitResult.Actor = VictimComp.GetOwner();
		OutHitResult.ComponentID = static_cast<SIZE_T>(VictimComp.GetUniqueID());
		OutHitResult.BodyIndex = BodyIndex;
		return true;
	}

} // namespace

void FDamageEvent::GetBestHitInfo(
	const AActor* HitActor, const AActor* HitInstigator, FHitResult& OutHitInfo, FVector& OutImpulseDir) const
{
	if (HitActor == nullptr)
	{
		return;
	}
	// The actor struck at its location, from the instigator (UE).
	OutHitInfo = FHitResult();
	OutHitInfo.Actor = const_cast<AActor*>(HitActor);
	OutHitInfo.Component = Cast<UPrimitiveComponent>(HitActor->GetRootComponent());
	OutHitInfo.bBlockingHit = true;
	OutHitInfo.ImpactPoint = HitActor->GetActorLocation();
	OutHitInfo.Location = OutHitInfo.ImpactPoint;
	OutImpulseDir = HitInstigator != nullptr
		? (OutHitInfo.ImpactPoint - HitInstigator->GetActorLocation()).GetSafeNormal()
		: FVector::ZeroVector;
	OutHitInfo.ImpactNormal = -OutImpulseDir;
}

void FPointDamageEvent::GetBestHitInfo(
	const AActor* /*HitActor*/, const AActor* /*HitInstigator*/, FHitResult& OutHitInfo, FVector& OutImpulseDir) const
{
	OutHitInfo = HitInfo;
	OutImpulseDir = ShotDirection;
}

void FRadialDamageEvent::GetBestHitInfo(
	const AActor* HitActor, const AActor* HitInstigator, FHitResult& OutHitInfo, FVector& OutImpulseDir) const
{
	if (ComponentHits.Num() == 0)
	{
		FDamageEvent::GetBestHitInfo(HitActor, HitInstigator, OutHitInfo, OutImpulseDir);
		return;
	}
	OutHitInfo = ComponentHits[0];
	OutImpulseDir = (OutHitInfo.ImpactPoint - Origin).GetSafeNormal();
}

float FRadialDamageParams::GetDamageScale(float DistanceFromEpicenter) const
{
	const float ValidatedInnerRadius = FMath::Max(0.0f, InnerRadius);
	const float ValidatedOuterRadius = FMath::Max(OuterRadius, ValidatedInnerRadius);
	const float ValidatedDist = FMath::Max(0.0f, DistanceFromEpicenter);
	if (ValidatedDist >= ValidatedOuterRadius)
	{
		return 0.0f;
	}
	if (DamageFalloff == 0.0f || ValidatedDist <= ValidatedInnerRadius)
	{
		return 1.0f;
	}
	const float DamageScale =
		1.0f - ((ValidatedDist - ValidatedInnerRadius) / (ValidatedOuterRadius - ValidatedInnerRadius));
	return FMath::Pow(DamageScale, DamageFalloff);
}

UWorld* UGameplayStatics::GetWorldFromContextObject(const UObject* WorldContextObject)
{
	if (WorldContextObject == nullptr)
	{
		return nullptr;
	}
	if (const UWorld* World = Cast<UWorld>(WorldContextObject))
	{
		return const_cast<UWorld*>(World);
	}
	if (const AActor* Actor = Cast<AActor>(WorldContextObject))
	{
		return Actor->GetWorld();
	}
	if (const UActorComponent* Component = Cast<UActorComponent>(WorldContextObject))
	{
		return Component->GetWorld();
	}
	return WorldContextObject->GetTypedOuter<UWorld>();
}

float UGameplayStatics::ApplyDamage(AActor* DamagedActor, float BaseDamage, AController* EventInstigator,
	AActor* DamageCauser, TSubclassOf<UDamageType> DamageTypeClass)
{
	if (DamagedActor == nullptr || BaseDamage == 0.0f)
	{
		return 0.0f;
	}
	const FDamageEvent DamageEvent(ValidDamageTypeClass(DamageTypeClass));
	return DamagedActor->TakeDamage(BaseDamage, DamageEvent, EventInstigator, DamageCauser);
}

float UGameplayStatics::ApplyPointDamage(AActor* DamagedActor, float BaseDamage, const FVector& HitFromDirection,
	const FHitResult& HitInfo, AController* EventInstigator, AActor* DamageCauser,
	TSubclassOf<UDamageType> DamageTypeClass)
{
	if (DamagedActor == nullptr || BaseDamage == 0.0f)
	{
		return 0.0f;
	}
	const FPointDamageEvent PointDamageEvent(
		BaseDamage, HitInfo, HitFromDirection, ValidDamageTypeClass(DamageTypeClass));
	return DamagedActor->TakeDamage(BaseDamage, PointDamageEvent, EventInstigator, DamageCauser);
}

bool UGameplayStatics::ApplyRadialDamage(const UObject* WorldContextObject, float BaseDamage, const FVector& Origin,
	float DamageRadius, TSubclassOf<UDamageType> DamageTypeClass, const TArray<AActor*>& IgnoreActors,
	AActor* DamageCauser, AController* InstigatedByController, bool bDoFullDamage,
	ECollisionChannel DamagePreventionChannel)
{
	const float DamageFalloff = bDoFullDamage ? 0.0f : 1.0f;
	return ApplyRadialDamageWithFalloff(WorldContextObject, BaseDamage, 0.0f, Origin, 0.0f, DamageRadius, DamageFalloff,
		DamageTypeClass, IgnoreActors, DamageCauser, InstigatedByController, DamagePreventionChannel);
}

bool UGameplayStatics::ApplyRadialDamageWithFalloff(const UObject* WorldContextObject, float BaseDamage,
	float MinimumDamage, const FVector& Origin, float DamageInnerRadius, float DamageOuterRadius, float DamageFalloff,
	TSubclassOf<UDamageType> DamageTypeClass, const TArray<AActor*>& IgnoreActors, AActor* DamageCauser,
	AController* InstigatedByController, ECollisionChannel DamagePreventionChannel)
{
	UWorld* World = GetWorldFromContextObject(WorldContextObject);
	if (World == nullptr)
	{
		return false;
	}
	FCollisionQueryParams SphereParams(FName(TEXT("ApplyRadialDamage")), false, DamageCauser);
	for (const AActor* Ignored : IgnoreActors)
	{
		SphereParams.AddIgnoredActor(Ignored);
	}
	TArray<FOverlapResult> Overlaps;
	(void)World->GetPhysicsScene().OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity,
		FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllDynamicObjects),
		FCollisionShape::MakeSphere(DamageOuterRadius), SphereParams);

	// The components reached, per actor, in the order the actors were first met (UE collects a TMap).
	TArray<AActor*> Victims;
	TArray<TArray<FHitResult>> VictimHits;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* OverlapActor = Overlap.GetActor();
		UPrimitiveComponent* Component = Overlap.GetComponent();
		if (OverlapActor == nullptr || Component == nullptr || !OverlapActor->CanBeDamaged() ||
			OverlapActor == DamageCauser || OverlapActor->IsPendingKillPending())
		{
			continue;
		}
		FHitResult Hit;
		const int32 BodyIndex = FindBody(World->GetPhysicsScene(), *Component, Overlap.ItemIndex);
		if (!ComponentIsDamageableFrom(
				*World, *Component, BodyIndex, Origin, DamageCauser, IgnoreActors, DamagePreventionChannel, Hit))
		{
			continue;
		}
		int32 VictimIndex = Victims.Find(OverlapActor);
		if (VictimIndex == INDEX_NONE)
		{
			VictimIndex = Victims.Add(OverlapActor);
			VictimHits.AddDefaulted();
		}
		VictimHits[VictimIndex].Add(Hit);
	}

	FRadialDamageEvent DamageEvent;
	DamageEvent.DamageTypeClass = ValidDamageTypeClass(DamageTypeClass);
	DamageEvent.Origin = Origin;
	DamageEvent.Params =
		FRadialDamageParams(BaseDamage, MinimumDamage, DamageInnerRadius, DamageOuterRadius, DamageFalloff);
	bool bAppliedDamage = false;
	for (int32 Index = 0; Index < Victims.Num(); ++Index)
	{
		DamageEvent.ComponentHits = VictimHits[Index];
		(void)Victims[Index]->TakeDamage(BaseDamage, DamageEvent, InstigatedByController, DamageCauser);
		bAppliedDamage = true;
	}
	return bAppliedDamage;
}

void UGameplayStatics::GetAllActorsOfClass(
	const UWorld& World, TSubclassOf<AActor> ActorClass, TArray<AActor*>& OutActors)
{
	OutActors.Reset();
	if (ActorClass == nullptr)
	{
		return;
	}
	World.ForEach<AActor>(
		[&](AActor& Actor)
		{
			if (Actor.IsA(ActorClass))
			{
				OutActors.Add(&Actor);
			}
		});
}

void UGameplayStatics::GetAllActorsWithTag(const UWorld& World, FName Tag, TArray<AActor*>& OutActors)
{
	OutActors.Reset();
	if (Tag.IsNone())
	{
		return;
	}
	World.ForEach<AActor>(
		[&](AActor& Actor)
		{
			if (Actor.ActorHasTag(Tag))
			{
				OutActors.Add(&Actor);
			}
		});
}
