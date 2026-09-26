#pragma once

#include "CollisionQuery.h"
#include "CoreMinimal.h"
#include "Engine/World.h"
#include "Physics/PhysScene.h"
#include "Templates/SubclassOf.h"

class AController;
class APointLight;
class FDebugDraw;
class UDamageType;
class USoundBase;

/** Static gameplay helpers: world traces over FPhysScene and damage (UE: UGameplayStatics / UKismetSystemLibrary). */
class ENGINE_API UGameplayStatics
{
public:
	[[nodiscard]] static bool LineTraceSingleByChannel(UWorld& World, FHitResult& OutHit, const FVector& Start,
		const FVector& End, ECollisionChannel Channel, const FCollisionQueryParams& Params = {},
		FDebugDraw* Debug = nullptr)
	{
		return World.GetPhysicsScene().LineTraceSingleByChannel(OutHit, Start, End, Channel, Params, Debug);
	}

	/** UE: UWorld::SweepSingleByChannel over the world's physics scene (FPhysScene::SweepSingleByChannel). */
	[[nodiscard]] static bool SweepSingleByChannel(UWorld& World, FHitResult& OutHit, const FVector& Start,
		const FVector& End, ECollisionChannel Channel, const FCollisionShape& CollisionShape,
		const FCollisionQueryParams& Params = {})
	{
		return World.GetPhysicsScene().SweepSingleByChannel(
			OutHit, Start, End, FQuat::Identity, Channel, CollisionShape, Params);
	}

	[[nodiscard]] static bool SphereTraceSingleByChannel(UWorld& World, FHitResult& OutHit, const FVector& Start,
		const FVector& End, float Radius, ECollisionChannel Channel, const FCollisionQueryParams& Params = {},
		FDebugDraw* Debug = nullptr)
	{
		return World.GetPhysicsScene().SphereTraceSingleByChannel(OutHit, Start, End, Radius, Channel, Params, Debug);
	}

	[[nodiscard]] static bool CapsuleTraceSingleByChannel(UWorld& World, FHitResult& OutHit, const FVector& Start,
		const FVector& End, float Radius, float HalfHeight, ECollisionChannel Channel,
		const FCollisionQueryParams& Params = {}, FDebugDraw* Debug = nullptr)
	{
		return World.GetPhysicsScene().CapsuleTraceSingleByChannel(
			OutHit, Start, End, Radius, HalfHeight, Channel, Params, Debug);
	}

	/** Melee / sweep helper: capsule along a segment (forwards to CapsuleTraceSingleByChannel). */
	[[nodiscard]] static bool SweepCapsuleAlongSegment(UWorld& World, FHitResult& OutHit, const FVector& Start,
		const FVector& End, float Radius, float HalfHeight, ECollisionChannel Channel,
		const FCollisionQueryParams& Params = {}, FDebugDraw* Debug = nullptr)
	{
		return CapsuleTraceSingleByChannel(World, OutHit, Start, End, Radius, HalfHeight, Channel, Params, Debug);
	}

	/**
	 * The world of a context object: a world, an actor's or a component's world, else the world the object is in
	 * (UE: GEngine->GetWorldFromContextObject). Null without one.
	 */
	[[nodiscard]] static UWorld* GetWorldFromContextObject(const UObject* WorldContextObject);

	/**
	 * Damage with no direction (UE: ApplyDamage): AActor::TakeDamage with an FDamageEvent of DamageTypeClass
	 * (UDamageType without one). Returns what the actor took; nothing for a null actor, zero damage or an actor that
	 * cannot be damaged.
	 */
	static float ApplyDamage(AActor* DamagedActor, float BaseDamage, AController* EventInstigator, AActor* DamageCauser,
		TSubclassOf<UDamageType> DamageTypeClass);

	/**
	 * Damage at a point: a shot along HitFromDirection that struck HitInfo (UE: ApplyPointDamage). AActor::TakeDamage
	 * with an FPointDamageEvent. Returns what the actor took.
	 */
	static float ApplyPointDamage(AActor* DamagedActor, float BaseDamage, const FVector& HitFromDirection,
		const FHitResult& HitInfo, AController* EventInstigator, AActor* DamageCauser,
		TSubclassOf<UDamageType> DamageTypeClass);

	/**
	 * Damage from Origin to every damageable actor with a component within DamageRadius (UE: ApplyRadialDamage):
	 * ApplyRadialDamageWithFalloff with no inner radius and a linear falloff, or none with bDoFullDamage. True when an
	 * actor was damaged.
	 */
	static bool ApplyRadialDamage(const UObject* WorldContextObject, float BaseDamage, const FVector& Origin,
		float DamageRadius, TSubclassOf<UDamageType> DamageTypeClass, const TArray<AActor*>& IgnoreActors,
		AActor* DamageCauser = nullptr, AController* InstigatedByController = nullptr, bool bDoFullDamage = false,
		ECollisionChannel DamagePreventionChannel = ECC_Visibility);

	/**
	 * Radial damage with a falloff (UE: ApplyRadialDamageWithFalloff). The dynamic bodies (every object type but
	 * WorldStatic) within DamageOuterRadius of Origin are found (FPhysScene::OverlapMultiByObjectType); a component
	 * is reached when a line from Origin to the centre of its body on DamagePreventionChannel hits nothing or the
	 * component itself (UE's ComponentIsDamageableFrom; ECC_MAX skips the test); each actor reached, except the
	 * causer and IgnoreActors, gets one AActor::TakeDamage of BaseDamage with an FRadialDamageEvent holding its
	 * components' hits, which scales it by the falloff at the closest hit (FRadialDamageParams::GetDamageScale). True
	 * when an actor was damaged.
	 */
	static bool ApplyRadialDamageWithFalloff(const UObject* WorldContextObject, float BaseDamage, float MinimumDamage,
		const FVector& Origin, float DamageInnerRadius, float DamageOuterRadius, float DamageFalloff,
		TSubclassOf<UDamageType> DamageTypeClass, const TArray<AActor*>& IgnoreActors, AActor* DamageCauser = nullptr,
		AController* InstigatedByController = nullptr, ECollisionChannel DamagePreventionChannel = ECC_Visibility);

	/** Every live actor of the class (or a subclass) in the world, in spawn order (UE: GetAllActorsOfClass). */
	static void GetAllActorsOfClass(const UWorld& World, TSubclassOf<AActor> ActorClass, TArray<AActor*>& OutActors);

	/** Every live actor with the tag, in spawn order (UE: GetAllActorsWithTag). */
	static void GetAllActorsWithTag(const UWorld& World, FName Tag, TArray<AActor*>& OutActors);

	/**
	 * The value of Key in URL options (`?Key=Value?Other`), compared without case; empty when absent or valueless
	 * (UE: ParseOption).
	 */
	[[nodiscard]] static FString ParseOption(const FString& Options, const FString& Key);
	/** Key is in the options, with or without a value (UE: HasOption). */
	[[nodiscard]] static bool HasOption(const FString& Options, const FString& Key);
	/** Key's value as an integer, DefaultValue when absent (UE: GetIntOption). */
	[[nodiscard]] static int32 GetIntOption(const FString& Options, const FString& Key, int32 DefaultValue);

	/**
	 * Plays a sound wave once, not spatialized, on the engine's audio device (UE: PlaySound2D; silent headless). A
	 * null sound or no engine plays nothing.
	 */
	static void PlaySound2D(const UObject* WorldContextObject, USoundBase* Sound, float VolumeMultiplier = 1.0f);

	/** Plays a sound wave once at Location, spatialized for the listener (UE: PlaySoundAtLocation). */
	static void PlaySoundAtLocation(
		const UObject* WorldContextObject, USoundBase* Sound, const FVector& Location, float VolumeMultiplier = 1.0f);

	/**
	 * Leaves an impact mark on a surface (Leon; UE: SpawnDecalAtLocation with a decal material): a square of Size cm on
	 * the surface at Location, across Normal, tinting it with Color (A: the strength) for LifeSpan seconds (0: until
	 * the world's pool of 64 recycles it, oldest first). Returns the mark's slot, INDEX_NONE without a world.
	 */
	static int32 SpawnImpactMark(const UObject* WorldContextObject, const FVector& Location, const FVector& Normal,
		float Size, const FLinearColor& Color, float LifeSpan = 0.0f);

	/**
	 * Draws a shot's streak from Start to End for LifeSpan seconds (Leon; UE spawns a beam or tracer emitter): Width
	 * cm wide, facing the camera, Color added to the scene (above 1 glows) and fading out.
	 */
	static void SpawnTracer(const UObject* WorldContextObject, const FVector& Start, const FVector& End,
		const FLinearColor& Color, float Width, float LifeSpan);

	/**
	 * Lights the world around Location for LifeSpan seconds: a transient APointLight that destroys itself (Leon's
	 * dynamic light helper for muzzle flashes and explosions; UE games attach a light to their emitter). A lit scene
	 * shades its first four point lights. Returns the light, null without a world.
	 */
	static APointLight* SpawnPointLightAtLocation(const UObject* WorldContextObject, const FVector& Location,
		const FLinearColor& Color, float Intensity, float AttenuationRadius, float LifeSpan);
};
