#pragma once

#include "CollisionQuery.h"
#include "CoreMinimal.h"
#include "Engine/World.h"
#include "Physics/PhysScene.h"

class ACharacter;
class FDebugDraw;
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
	 * Applies point damage to a character through ACharacter::TakeDamage and returns the applied amount.
	 * HitFromDirection / DamageCauser are reserved for knockback / attribution.
	 */
	static float ApplyPointDamage(ACharacter* DamagedActor, float BaseDamage, const FVector& HitFromDirection,
		ACharacter* DamageCauser = nullptr);

	/** Every live actor of the class (or a subclass) in the world, in spawn order (UE: GetAllActorsOfClass). */
	static void GetAllActorsOfClass(const UWorld& World, TSubclassOf<AActor> ActorClass, TArray<AActor*>& OutActors);

	/** Every live actor with the tag, in spawn order (UE: GetAllActorsWithTag). */
	static void GetAllActorsWithTag(const UWorld& World, FName Tag, TArray<AActor*>& OutActors);

	/** Radial damage with linear falloff by distance; returns the total applied across all actors. */
	static float ApplyRadialDamage(const TArray<ACharacter*>& Actors, float BaseDamage, const FVector& Origin,
		float DamageRadius, ACharacter* DamageCauser = nullptr);

	/**
	 * Plays a sound wave once, not spatialized, on the engine's audio device (UE: PlaySound2D; silent headless). A
	 * null sound or no engine plays nothing.
	 */
	static void PlaySound2D(const UObject* WorldContextObject, USoundBase* Sound, float VolumeMultiplier = 1.0f);

	/** Plays a sound wave once at Location, spatialized for the listener (UE: PlaySoundAtLocation). */
	static void PlaySoundAtLocation(
		const UObject* WorldContextObject, USoundBase* Sound, const FVector& Location, float VolumeMultiplier = 1.0f);
};
