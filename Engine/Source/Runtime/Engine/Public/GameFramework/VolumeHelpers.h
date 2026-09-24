#pragma once

#include <glm/vec3.hpp>

#include <cstddef>
#include <leon/level/Level.h>
#include <span>
#include <string>
#include <vector>

namespace leon {

class Character;

/// True when character feet lie inside the volume AABB
/// (center = transform.position, half-extents = abs(scale) * 0.5).
[[nodiscard]] bool CharacterOverlapsPainVolume(const Character& ch, const PainCausingVolume& vol);

/// Apply one pain tick: `damagePerSecond * damageInterval` via ApplyPointDamage.
void ApplyPainVolumeDamage(Character& ch, const PainCausingVolume& vol);

/// Authority tick: global accumulator (Zombies lava style). When `tickAccum` reaches the
/// smallest positive `damageInterval` among volumes, damages each alive character that
/// overlaps any volume (one tick from the first overlapping volume).
void TickPainCausingVolumes(const std::vector<PainCausingVolume>& volumes,
                            std::span<Character*> characters, float deltaTime, float& tickAccum);

/// Nearest TriggerVolume whose XZ distance from `feet` is within min(maxDist, interactRadius).
/// Returns Level::npos if none.
[[nodiscard]] std::size_t FindBestTriggerVolume(const std::vector<TriggerVolume>& volumes,
                                                const glm::vec3& feet, float maxDist);

/// Default `[F] … [cost]` prompt from payload / interactCost (Door, WallBuy:…, Perk:…).
[[nodiscard]] std::string FormatDefaultInteractPrompt(const TriggerVolume& volume);

} // namespace leon
