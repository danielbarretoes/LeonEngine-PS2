#pragma once

#include <glm/vec3.hpp>

#include <cstddef>
#include "Engine/Level.h"
#include <span>
#include <string>
#include <vector>


class ACharacter;

/// True when character feet lie inside the volume AABB
/// (center = transform.position, half-extents = abs(scale) * 0.5).
[[nodiscard]] bool CharacterOverlapsPainVolume(const ACharacter& Ch, const FPainCausingVolume& Vol);

/// Apply one pain tick: `damagePerSecond * damageInterval` via UGameplayStatics::ApplyPointDamage.
void ApplyPainVolumeDamage(ACharacter& Ch, const FPainCausingVolume& Vol);

/// Authority tick: global accumulator (Zombies lava style). When `tickAccum` reaches the
/// smallest positive `damageInterval` among volumes, damages each alive character that
/// overlaps any volume (one tick from the first overlapping volume).
void TickPainCausingVolumes(const std::vector<FPainCausingVolume>& Volumes,
                            std::span<ACharacter*> Characters, float DeltaTime, float& TickAccum);

/// Nearest FTriggerVolume whose XZ distance from `feet` is within min(maxDist, interactRadius).
/// Returns Level::npos if none.
[[nodiscard]] std::size_t FindBestTriggerVolume(const std::vector<FTriggerVolume>& Volumes,
                                                const glm::vec3& Feet, float MaxDist);

/// Default `[F] … [cost]` prompt from payload / interactCost (Door, WallBuy:…, Perk:…).
[[nodiscard]] std::string FormatDefaultInteractPrompt(const FTriggerVolume& Volume);

