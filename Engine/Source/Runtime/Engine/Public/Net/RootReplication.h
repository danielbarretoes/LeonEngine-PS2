#pragma once

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

#include <cmath>
#include <cstdint>
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "Net/NetProtocol.h"

namespace Leon::Net
{

/// Capture Actor root location + yaw for replication (no full USceneComponent graph).
[[nodiscard]] inline FPawnSnap CaptureActorRoot(std::uint8_t slot, const AActor& actor,
                                               float velocityY = 0.0f, float animBlend = 0.0f,
                                               float boomYaw = 0.0f, float boomPitch = 0.0f,
                                               bool grounded = true) {
    FPawnSnap snap{};
    snap.Slot = slot;
    const glm::vec3& loc = actor.GetActorLocation();
    snap.X = loc.x;
    snap.Y = loc.y;
    snap.Z = loc.z;
    snap.Yaw = actor.GetActorYaw();
    snap.VelY = velocityY;
    snap.AnimBlend = animBlend;
    snap.BoomYaw = boomYaw;
    snap.BoomPitch = boomPitch;
    snap.Grounded = grounded ? 1 : 0;
    snap.Health = 100.0f;
    snap.Flags = PawnSnapAlive;
    return snap;
}

/// Capture Character movement-relevant fields for snapshots.
[[nodiscard]] inline FPawnSnap CaptureCharacterRoot(std::uint8_t slot, const ACharacter& character,
                                                   float boomYaw = 0.0f, float boomPitch = 0.0f) {
    FPawnSnap snap =
        CaptureActorRoot(slot, character, character.GetVelocityZ(), character.GetAnimBlendInput(),
                         boomYaw, boomPitch, character.IsMovingOnGround());
    return snap;
}

/// Apply a replicated root snapshot onto an Actor (location + yaw only).
inline void ApplyActorRoot(AActor& actor, const FPawnSnap& snap) {
    actor.SetActorLocationAndRotation({snap.X, snap.Y, snap.Z}, snap.Yaw);
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

} // namespace Leon::Net
