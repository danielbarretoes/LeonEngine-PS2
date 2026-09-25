#pragma once

#include "CollisionQuery.h"
#include "Engine/World.h"
#include "Migration/GlmInterop.h"
#include "Physics/PhysScene.h"

#include <glm/vec3.hpp>

#include <vector>

class ACharacter;
class FDebugDraw;

/** Static gameplay helpers: world traces over FPhysScene and damage (UE: UGameplayStatics / UKismetSystemLibrary). */
class ENGINE_API UGameplayStatics
{
public:
	[[nodiscard]] static bool LineTraceSingleByChannel(UWorld& World, FHitResult& OutHit, const glm::vec3& Start,
		const glm::vec3& End, ECollisionChannel Channel, const FCollisionQueryParams& Params = {},
		FDebugDraw* Debug = nullptr)
	{
		return World.GetPhysicsScene().LineTraceSingleByChannel(
			OutHit, FromGlm(Start), FromGlm(End), Channel, Params, Debug);
	}

	[[nodiscard]] static bool SphereTraceSingleByChannel(UWorld& World, FHitResult& OutHit, const glm::vec3& Start,
		const glm::vec3& End, float Radius, ECollisionChannel Channel, const FCollisionQueryParams& Params = {},
		FDebugDraw* Debug = nullptr)
	{
		return World.GetPhysicsScene().SphereTraceSingleByChannel(
			OutHit, FromGlm(Start), FromGlm(End), Radius, Channel, Params, Debug);
	}

	[[nodiscard]] static bool CapsuleTraceSingleByChannel(UWorld& World, FHitResult& OutHit, const glm::vec3& Start,
		const glm::vec3& End, float Radius, float HalfHeight, ECollisionChannel Channel,
		const FCollisionQueryParams& Params = {}, FDebugDraw* Debug = nullptr)
	{
		return World.GetPhysicsScene().CapsuleTraceSingleByChannel(
			OutHit, FromGlm(Start), FromGlm(End), Radius, HalfHeight, Channel, Params, Debug);
	}

	/** Melee / sweep helper: capsule along a segment (forwards to CapsuleTraceSingleByChannel). */
	[[nodiscard]] static bool SweepCapsuleAlongSegment(UWorld& World, FHitResult& OutHit, const glm::vec3& Start,
		const glm::vec3& End, float Radius, float HalfHeight, ECollisionChannel Channel,
		const FCollisionQueryParams& Params = {}, FDebugDraw* Debug = nullptr)
	{
		return CapsuleTraceSingleByChannel(World, OutHit, Start, End, Radius, HalfHeight, Channel, Params, Debug);
	}

	/**
	 * Applies point damage to a character through ACharacter::TakeDamage and returns the applied amount.
	 * HitFromDirection / DamageCauser are reserved for knockback / attribution.
	 */
	static float ApplyPointDamage(ACharacter* DamagedActor, float BaseDamage, const glm::vec3& HitFromDirection,
		ACharacter* DamageCauser = nullptr);

	/** Radial damage with linear falloff by distance; returns the total applied across all actors. */
	static float ApplyRadialDamage(const std::vector<ACharacter*>& Actors, float BaseDamage, const glm::vec3& Origin,
		float DamageRadius, ACharacter* DamageCauser = nullptr);
};
