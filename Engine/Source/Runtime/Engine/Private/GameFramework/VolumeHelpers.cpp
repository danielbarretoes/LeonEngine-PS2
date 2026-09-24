#include "GameFramework/VolumeHelpers.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <limits>
#include "GameFramework/Character.h"
#include "GameFramework/Damage.h"

namespace {

[[nodiscard]] bool PointInPainAabb(const glm::vec3& point, const PainCausingVolume& vol) {
    const glm::vec3 half = glm::abs(vol.transform.scale) * 0.5f;
    const glm::vec3 min = vol.transform.position - half;
    const glm::vec3 max = vol.transform.position + half;
    return point.x >= min.x && point.x <= max.x && point.y >= min.y && point.y <= max.y &&
           point.z >= min.z && point.z <= max.z;
}

[[nodiscard]] float SmallestPositiveInterval(const std::vector<PainCausingVolume>& volumes) {
    float best = (std::numeric_limits<float>::max)();
    for (const PainCausingVolume& vol : volumes) {
        if (vol.damageInterval > 0.0f && vol.damageInterval < best) {
            best = vol.damageInterval;
        }
    }
    return best;
}

} // namespace

bool CharacterOverlapsPainVolume(const Character& ch, const PainCausingVolume& vol) {
    return PointInPainAabb(ch.GetActorLocation(), vol);
}

void ApplyPainVolumeDamage(Character& ch, const PainCausingVolume& vol) {
    const float amount = vol.damagePerSecond * vol.damageInterval;
    if (amount <= 0.0f) {
        return;
    }
    (void)ApplyPointDamage(&ch, amount, glm::vec3{0.0f, -1.0f, 0.0f});
}

void TickPainCausingVolumes(const std::vector<PainCausingVolume>& volumes,
                            std::span<Character*> characters, float deltaTime, float& tickAccum) {
    if (volumes.empty() || characters.empty() || deltaTime <= 0.0f) {
        return;
    }
    const float interval = SmallestPositiveInterval(volumes);
    if (!(interval < (std::numeric_limits<float>::max)())) {
        return;
    }

    tickAccum += deltaTime;
    if (tickAccum < interval) {
        return;
    }
    tickAccum = 0.0f;

    for (Character* ch : characters) {
        if (ch == nullptr || !ch->IsAlive()) {
            continue;
        }
        for (const PainCausingVolume& vol : volumes) {
            if (!CharacterOverlapsPainVolume(*ch, vol)) {
                continue;
            }
            ApplyPainVolumeDamage(*ch, vol);
            break;
        }
    }
}

std::size_t FindBestTriggerVolume(const std::vector<TriggerVolume>& volumes, const glm::vec3& feet,
                                  float maxDist) {
    if (volumes.empty() || maxDist <= 0.0f) {
        return Level::npos;
    }

    std::size_t best = Level::npos;
    float bestDist = maxDist;
    for (std::size_t i = 0; i < volumes.size(); ++i) {
        const TriggerVolume& vol = volumes[i];
        const float radius = vol.interactRadius > 0.0f ? vol.interactRadius : maxDist;
        const float limit = radius < maxDist ? radius : maxDist;
        const glm::vec3 delta{feet.x - vol.transform.position.x, 0.0f,
                              feet.z - vol.transform.position.z};
        const float dist = glm::length(delta);
        if (dist < bestDist && dist <= limit) {
            bestDist = dist;
            best = i;
        }
    }
    return best;
}

std::string FormatDefaultInteractPrompt(const TriggerVolume& volume) {
    const std::string& payload = volume.payload;
    const std::string costSuffix =
        volume.interactCost > 0 ? (" [" + std::to_string(volume.interactCost) + "]") : std::string{};

    if (payload.empty()) {
        return "[F] Interact" + costSuffix;
    }
    if (payload == "Door") {
        return "[F] Open Door" + costSuffix;
    }
    if (payload.rfind("WallBuy:", 0) == 0) {
        return "[F] Buy " + payload.substr(8) + costSuffix;
    }
    if (payload.rfind("Perk:", 0) == 0) {
        return "[F] " + payload.substr(5) + costSuffix;
    }
    if (payload == "Ammo") {
        return "[F] Buy Ammo" + costSuffix;
    }
    if (payload == "PackAPunch") {
        return "[F] Pack-a-Punch" + costSuffix;
    }
    return "[F] " + payload + costSuffix;
}

