#include "ZombiesInteract.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <string_view>

namespace game {
namespace {

[[nodiscard]] std::string Trim(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
        s.remove_prefix(1);
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
        s.remove_suffix(1);
    }
    return std::string(s);
}

[[nodiscard]] bool StartsWithIgnoreCase(std::string_view s, std::string_view prefix) {
    if (s.size() < prefix.size()) {
        return false;
    }
    for (std::size_t i = 0; i < prefix.size(); ++i) {
        const char a = static_cast<char>(std::tolower(static_cast<unsigned char>(s[i])));
        const char b = static_cast<char>(std::tolower(static_cast<unsigned char>(prefix[i])));
        if (a != b) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool EqualsIgnoreCase(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        const char ca = static_cast<char>(std::tolower(static_cast<unsigned char>(a[i])));
        const char cb = static_cast<char>(std::tolower(static_cast<unsigned char>(b[i])));
        if (ca != cb) {
            return false;
        }
    }
    return true;
}

/// Nearest static mesh to `pos` (for door collision hide). Returns Level::npos if none close.
[[nodiscard]] std::size_t FindNearestMeshIndex(const leon::Level& level, const glm::vec3& pos,
                                               float maxDist = 0.75f) {
    std::size_t best = leon::Level::npos;
    float bestD2 = maxDist * maxDist;
    const auto& meshes = level.StaticMeshes();
    for (std::size_t i = 0; i < meshes.size(); ++i) {
        const glm::vec3 d = meshes[i].transform.position - pos;
        const float d2 = glm::dot(d, d);
        if (d2 < bestD2) {
            bestD2 = d2;
            best = i;
        }
    }
    return best;
}

[[nodiscard]] bool TryParseTriggerPayload(const leon::TriggerVolume& vol,
                                          ZombiesInteractable& out) {
    const std::string payload = Trim(vol.payload);
    out.cost = (std::max)(0, vol.interactCost);
    out.position = vol.transform.position;
    const float r = vol.interactRadius > 0.0f ? vol.interactRadius : 2.0f;
    out.halfExtents = {r, r, r};
    out.spent = false;
    out.meshIndex = leon::Level::npos;

    if (payload.empty() || EqualsIgnoreCase(payload, "Door")) {
        out.type = EZombiesInteract::Door;
        out.payload.clear();
        return true;
    }
    if (StartsWithIgnoreCase(payload, "WallBuy:")) {
        out.type = EZombiesInteract::WallBuy;
        out.payload = payload.substr(8);
        return !out.payload.empty();
    }
    if (EqualsIgnoreCase(payload, "Ammo")) {
        out.type = EZombiesInteract::Ammo;
        out.payload.clear();
        return true;
    }
    if (StartsWithIgnoreCase(payload, "Perk:")) {
        out.type = EZombiesInteract::Perk;
        out.payload = payload.substr(5);
        return !out.payload.empty();
    }
    if (EqualsIgnoreCase(payload, "PaP") || EqualsIgnoreCase(payload, "PackAPunch")) {
        out.type = EZombiesInteract::PackAPunch;
        out.payload.clear();
        return true;
    }
    return false;
}

void CollectFromTriggerVolumes(const leon::Level& level, std::vector<ZombiesInteractable>& outBuys) {
    for (const leon::TriggerVolume& vol : level.TriggerVolumes()) {
        ZombiesInteractable buy{};
        if (!TryParseTriggerPayload(vol, buy)) {
            continue;
        }
        if (buy.type == EZombiesInteract::Door) {
            buy.meshIndex = FindNearestMeshIndex(level, buy.position);
        }
        outBuys.push_back(std::move(buy));
    }
}

void CollectFromMeshTags(const leon::Level& level, std::vector<ZombiesInteractable>& outBuys,
                         std::vector<leon::PainCausingVolume>& outPain) {
    const auto& meshes = level.StaticMeshes();
    for (std::size_t i = 0; i < meshes.size(); ++i) {
        const leon::StaticMeshComponent& mesh = meshes[i];
        const std::string tag = Trim(mesh.tag);
        if (tag.empty()) {
            continue;
        }

        const glm::vec3 half = glm::abs(mesh.transform.scale) * 0.5f;
        const glm::vec3 pos = mesh.transform.position;

        if (StartsWithIgnoreCase(tag, "Lava")) {
            leon::PainCausingVolume lava{};
            lava.transform.position = pos;
            // Expand Y so feet near the thin visual surface register (legacy tag path).
            lava.transform.scale = {half.x * 2.0f, half.y * 2.0f + 0.75f, half.z * 2.0f};
            lava.transform.position.y += 0.225f;
            lava.damagePerSecond = 12.0f / 0.35f;
            lava.damageInterval = 0.35f;
            lava.tag = "Lava";
            outPain.push_back(lava);
            continue;
        }

        ZombiesInteractable buy{};
        buy.meshIndex = i;
        buy.position = pos;
        buy.halfExtents = half;

        if (StartsWithIgnoreCase(tag, "Door:")) {
            buy.type = EZombiesInteract::Door;
            buy.cost = std::max(0, std::atoi(tag.c_str() + 5));
            outBuys.push_back(buy);
        } else if (StartsWithIgnoreCase(tag, "WallBuy:")) {
            buy.type = EZombiesInteract::WallBuy;
            std::string rest = tag.substr(8);
            const auto colon = rest.find(':');
            if (colon == std::string::npos) {
                continue;
            }
            buy.payload = rest.substr(0, colon);
            buy.cost = std::max(0, std::atoi(rest.c_str() + colon + 1));
            outBuys.push_back(buy);
        } else if (StartsWithIgnoreCase(tag, "Ammo:")) {
            buy.type = EZombiesInteract::Ammo;
            buy.cost = std::max(0, std::atoi(tag.c_str() + 5));
            outBuys.push_back(buy);
        } else if (StartsWithIgnoreCase(tag, "Perk:")) {
            buy.type = EZombiesInteract::Perk;
            std::string rest = tag.substr(5);
            const auto colon = rest.find(':');
            if (colon == std::string::npos) {
                continue;
            }
            buy.payload = rest.substr(0, colon);
            buy.cost = std::max(0, std::atoi(rest.c_str() + colon + 1));
            outBuys.push_back(buy);
        } else if (StartsWithIgnoreCase(tag, "PaP:")) {
            buy.type = EZombiesInteract::PackAPunch;
            buy.cost = std::max(0, std::atoi(tag.c_str() + 4));
            outBuys.push_back(buy);
        }
    }
}

} // namespace

EZombiesPerk ParsePerkPayload(std::string_view payload) {
    std::string p = Trim(payload);
    for (char& c : p) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    if (p == "jugg" || p == "juggernog") {
        return EZombiesPerk::Juggernog;
    }
    if (p == "speed" || p == "speedcola") {
        return EZombiesPerk::SpeedCola;
    }
    if (p == "doubletap" || p == "dt" || p == "double") {
        return EZombiesPerk::DoubleTap;
    }
    if (p == "quickrevive" || p == "qr" || p == "revive") {
        return EZombiesPerk::QuickRevive;
    }
    return EZombiesPerk::None;
}

const char* PerkDisplayName(EZombiesPerk perk) {
    switch (perk) {
    case EZombiesPerk::Juggernog:
        return "Juggernog";
    case EZombiesPerk::SpeedCola:
        return "Speed Cola";
    case EZombiesPerk::DoubleTap:
        return "Double Tap";
    case EZombiesPerk::QuickRevive:
        return "Quick Revive";
    default:
        return "Perk";
    }
}

void CollectZombiesTownActors(const leon::Level& level, std::vector<ZombiesInteractable>& outBuys,
                              std::vector<leon::PainCausingVolume>& outPain) {
    outBuys.clear();
    outPain.clear();

    // Prefer typed level actors; mesh-tag parser remains for older stamped maps.
    if (!level.TriggerVolumes().empty()) {
        CollectFromTriggerVolumes(level, outBuys);
    }
    if (!level.PainCausingVolumes().empty()) {
        outPain = level.PainCausingVolumes();
    }

    if (outBuys.empty() || outPain.empty()) {
        std::vector<ZombiesInteractable> tagBuys;
        std::vector<leon::PainCausingVolume> tagPain;
        CollectFromMeshTags(level, tagBuys, tagPain);
        if (outBuys.empty()) {
            outBuys = std::move(tagBuys);
        }
        if (outPain.empty()) {
            outPain = std::move(tagPain);
        }
    }
}

} // namespace game
