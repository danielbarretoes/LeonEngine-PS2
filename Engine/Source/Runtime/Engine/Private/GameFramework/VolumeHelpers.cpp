#include "GameFramework/VolumeHelpers.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <limits>
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"

namespace {

[[nodiscard]] bool PointInPainAabb(const glm::vec3& point, const FPainCausingVolume& vol) {
    const glm::vec3 half = glm::abs(vol.transform.Scale) * 0.5f;
    const glm::vec3 min = vol.transform.Position - half;
    const glm::vec3 max = vol.transform.Position + half;
    return point.x >= min.x && point.x <= max.x && point.y >= min.y && point.y <= max.y &&
           point.z >= min.z && point.z <= max.z;
}

[[nodiscard]] float SmallestPositiveInterval(const std::vector<FPainCausingVolume>& volumes) {
    float best = (std::numeric_limits<float>::max)();
    for (const FPainCausingVolume& vol : volumes) {
        if (vol.damageInterval > 0.0f && vol.damageInterval < best) {
            best = vol.damageInterval;
        }
    }
    return best;
}

} // namespace

bool CharacterOverlapsPainVolume(const ACharacter& ch, const FPainCausingVolume& vol) {
    return PointInPainAabb(ch.GetActorLocation(), vol);
}

void ApplyPainVolumeDamage(ACharacter& ch, const FPainCausingVolume& vol) {
    const float amount = vol.damagePerSecond * vol.damageInterval;
    if (amount <= 0.0f) {
        return;
    }
    (void)UGameplayStatics::ApplyPointDamage(&ch, amount, glm::vec3{0.0f, -1.0f, 0.0f});
}

void TickPainCausingVolumes(const std::vector<FPainCausingVolume>& volumes,
                            std::span<ACharacter*> characters, float deltaTime, float& tickAccum) {
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

    for (ACharacter* ch : characters) {
        if (ch == nullptr || !ch->IsAlive()) {
            continue;
        }
        for (const FPainCausingVolume& vol : volumes) {
            if (!CharacterOverlapsPainVolume(*ch, vol)) {
                continue;
            }
            ApplyPainVolumeDamage(*ch, vol);
            break;
        }
    }
}

std::size_t FindBestTriggerVolume(const std::vector<FTriggerVolume>& volumes, const glm::vec3& feet,
                                  float maxDist) {
    if (volumes.empty() || maxDist <= 0.0f) {
        return ULevel::npos;
    }

    std::size_t best = ULevel::npos;
    float bestDist = maxDist;
    for (std::size_t i = 0; i < volumes.size(); ++i) {
        const FTriggerVolume& vol = volumes[i];
        const float radius = vol.interactRadius > 0.0f ? vol.interactRadius : maxDist;
        const float limit = radius < maxDist ? radius : maxDist;
        const glm::vec3 delta{feet.x - vol.transform.Position.x, 0.0f,
                              feet.z - vol.transform.Position.z};
        const float dist = glm::length(delta);
        if (dist < bestDist && dist <= limit) {
            bestDist = dist;
            best = i;
        }
    }
    return best;
}

std::string FormatDefaultInteractPrompt(const FTriggerVolume& volume) {
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

