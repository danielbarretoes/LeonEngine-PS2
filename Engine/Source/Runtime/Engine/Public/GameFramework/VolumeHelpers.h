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
[[nodiscard]] bool CharacterOverlapsPainVolume(const ACharacter& ch, const FPainCausingVolume& vol);

/// Apply one pain tick: `damagePerSecond * damageInterval` via UGameplayStatics::ApplyPointDamage.
void ApplyPainVolumeDamage(ACharacter& ch, const FPainCausingVolume& vol);

/// Authority tick: global accumulator (Zombies lava style). When `tickAccum` reaches the
/// smallest positive `damageInterval` among volumes, damages each alive character that
/// overlaps any volume (one tick from the first overlapping volume).
void TickPainCausingVolumes(const std::vector<FPainCausingVolume>& volumes,
                            std::span<ACharacter*> characters, float deltaTime, float& tickAccum);

/// Nearest FTriggerVolume whose XZ distance from `feet` is within min(maxDist, interactRadius).
/// Returns Level::npos if none.
[[nodiscard]] std::size_t FindBestTriggerVolume(const std::vector<FTriggerVolume>& volumes,
                                                const glm::vec3& feet, float maxDist);

/// Default `[F] … [cost]` prompt from payload / interactCost (Door, WallBuy:…, Perk:…).
[[nodiscard]] std::string FormatDefaultInteractPrompt(const FTriggerVolume& volume);

