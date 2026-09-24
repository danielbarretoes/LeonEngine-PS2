#pragma once

#include <glm/vec3.hpp>

#include <vector>


class Character;

/// Apply point damage to a Character (Unreal `UGameplayStatics::ApplyPointDamage` lite).
/// Returns applied damage (via `Character::TakeDamage`). `HitFromDirection` / `DamageCauser`
/// reserved for future knockback / attribution.
float ApplyPointDamage(Character* DamagedActor, float BaseDamage, const glm::vec3& HitFromDirection,
                       Character* DamageCauser = nullptr);

/// Apply radial damage with linear falloff by distance (Unreal `ApplyRadialDamage` lite).
/// Returns total applied damage across all actors.
float ApplyRadialDamage(const std::vector<Character*>& Actors, float BaseDamage,
                        const glm::vec3& Origin, float DamageRadius,
                        Character* DamageCauser = nullptr);

