#pragma once

/// Unreal-like UGameplayStatics / UWorld trace helpers over FPhysScene.
#include <glm/vec3.hpp>
#include "Engine/World.h"
#include "CollisionQuery.h"


class FDebugDraw;

[[nodiscard]] inline bool LineTraceSingleByChannel(World& world, FHitResult& outHit,
                                                   const glm::vec3& start, const glm::vec3& end,
                                                   ECollisionChannel channel,
                                                   const FCollisionQueryParams& params = {},
                                                   FDebugDraw* debug = nullptr) {
    return world.GetPhysicsScene().LineTraceSingleByChannel(outHit, start, end, channel, params,
                                                            debug);
}

[[nodiscard]] inline bool SphereTraceSingleByChannel(World& world, FHitResult& outHit,
                                                     const glm::vec3& start, const glm::vec3& end,
                                                     float radius, ECollisionChannel channel,
                                                     const FCollisionQueryParams& params = {},
                                                     FDebugDraw* debug = nullptr) {
    return world.GetPhysicsScene().SphereTraceSingleByChannel(outHit, start, end, radius, channel,
                                                              params, debug);
}

[[nodiscard]] inline bool CapsuleTraceSingleByChannel(World& world, FHitResult& outHit,
                                                      const glm::vec3& start, const glm::vec3& end,
                                                      float radius, float halfHeight,
                                                      ECollisionChannel channel,
                                                      const FCollisionQueryParams& params = {},
                                                      FDebugDraw* debug = nullptr) {
    return world.GetPhysicsScene().CapsuleTraceSingleByChannel(outHit, start, end, radius,
                                                               halfHeight, channel, params, debug);
}

/// Melee / sweep helper: capsule along a segment (forwards to CapsuleTraceSingleByChannel).
[[nodiscard]] inline bool SweepCapsuleAlongSegment(World& world, FHitResult& outHit,
                                                   const glm::vec3& start, const glm::vec3& end,
                                                   float radius, float halfHeight,
                                                   ECollisionChannel channel,
                                                   const FCollisionQueryParams& params = {},
                                                   FDebugDraw* debug = nullptr) {
    return CapsuleTraceSingleByChannel(world, outHit, start, end, radius, halfHeight, channel,
                                       params, debug);
}

