#pragma once

#include <cstddef>
#include <cstdint>
#include <glm/vec3.hpp>
#include <leon/level/Level.h>
#include <string>
#include <string_view>
#include <vector>

namespace game {

enum class EZombiesInteract : std::uint8_t {
    Door = 0,
    WallBuy,
    Ammo,
    Perk,
    PackAPunch,
};

enum class EZombiesPerk : std::uint8_t {
    None = 0,
    Juggernog,
    SpeedCola,
    DoubleTap,
    QuickRevive,
};

struct ZombiesInteractable {
    EZombiesInteract type = EZombiesInteract::Door;
    int cost = 0;
    std::string payload; // weapon id / perk name
    std::size_t meshIndex = leon::Level::npos;
    glm::vec3 position{0.0f};
    glm::vec3 halfExtents{1.0f};
    bool spent = false; // doors stay open; one-shot machines
};

/// Prefer TriggerVolumes / PainCausingVolumes; fall back to mesh tags (Door:/Lava/…).
void CollectZombiesTownActors(const leon::Level& level, std::vector<ZombiesInteractable>& outBuys,
                              std::vector<leon::PainCausingVolume>& outPain);

[[nodiscard]] EZombiesPerk ParsePerkPayload(std::string_view payload);
[[nodiscard]] const char* PerkDisplayName(EZombiesPerk perk);

} // namespace game
