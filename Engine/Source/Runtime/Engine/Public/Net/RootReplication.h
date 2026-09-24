#pragma once

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

#include <cmath>
#include <cstdint>
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "Net/NetProtocol.h"

namespace leon {
namespace net {

/// Capture Actor root location + yaw for replication (no full SceneComponent graph).
[[nodiscard]] inline PawnSnap CaptureActorRoot(std::uint8_t slot, const Actor& actor,
                                               float velocityY = 0.0f, float animBlend = 0.0f,
                                               float boomYaw = 0.0f, float boomPitch = 0.0f,
                                               bool grounded = true) {
    PawnSnap snap{};
    snap.slot = slot;
    const glm::vec3& loc = actor.GetActorLocation();
    snap.x = loc.x;
    snap.y = loc.y;
    snap.z = loc.z;
    snap.yaw = actor.GetActorYaw();
    snap.velY = velocityY;
    snap.animBlend = animBlend;
    snap.boomYaw = boomYaw;
    snap.boomPitch = boomPitch;
    snap.grounded = grounded ? 1 : 0;
    snap.health = 100.0f;
    snap.flags = kPawnSnapAlive;
    return snap;
}

/// Capture Character movement-relevant fields for snapshots.
[[nodiscard]] inline PawnSnap CaptureCharacterRoot(std::uint8_t slot, const Character& character,
                                                   float boomYaw = 0.0f, float boomPitch = 0.0f) {
    PawnSnap snap =
        CaptureActorRoot(slot, character, character.GetVelocityZ(), character.GetAnimBlendInput(),
                         boomYaw, boomPitch, character.IsMovingOnGround());
    return snap;
}

/// Apply a replicated root snapshot onto an Actor (location + yaw only).
inline void ApplyActorRoot(Actor& actor, const PawnSnap& snap) {
    actor.SetActorLocationAndRotation({snap.x, snap.y, snap.z}, snap.yaw);
}

/// Distance relevancy check (Unreal Net relevancy lite). `maxDist <= 0` always relevant.
[[nodiscard]] inline bool IsPawnRelevant(const glm::vec3& viewer, const glm::vec3& pawn,
                                         float maxDist) {
    if (maxDist <= 0.0f) {
        return true;
    }
    const glm::vec3 d = pawn - viewer;
    const float distSq = d.x * d.x + d.y * d.y + d.z * d.z;
    return distSq <= (maxDist * maxDist);
}

[[nodiscard]] inline bool IsPawnRelevantXZ(const glm::vec3& viewer, const glm::vec3& pawn,
                                           float maxDist) {
    if (maxDist <= 0.0f) {
        return true;
    }
    const float dx = pawn.x - viewer.x;
    const float dz = pawn.z - viewer.z;
    return (dx * dx + dz * dz) <= (maxDist * maxDist);
}

/// Default XZ cull radius for AI / remote pawns in snapshots (meters).
constexpr float kDefaultAiRelevancyXZ = 64.0f;

/// True if `pawn` is within `maxDist` XZ of any viewer in [begin, end).
template <typename Iterator>
[[nodiscard]] bool IsPawnRelevantToAnyXZ(Iterator begin, Iterator end, const glm::vec3& pawn,
                                         float maxDist = kDefaultAiRelevancyXZ) {
    if (maxDist <= 0.0f) {
        return true;
    }
    for (Iterator it = begin; it != end; ++it) {
        if (IsPawnRelevantXZ(*it, pawn, maxDist)) {
            return true;
        }
    }
    return false;
}

} // namespace net
} // namespace leon
