#pragma once

#include "MeshData.h"

/**
 * Edge of the basic shapes in world units (100 cm, like UE's /Engine/BasicShapes): a scale of 1 is one legacy metre.
 * PhysicsCore's BasicShapeSize (the collision box of a scaled cube) matches it.
 */
inline constexpr float PrimitiveEdgeLength = 100.0f;

/** Cube of edge PrimitiveEdgeLength centered at the origin: [-50, 50]^3 cm. */
[[nodiscard]] FMeshData MakeCube();

/** Axis-aligned ground plane on XZ (Y = 0) of edge Size (world units), centered at the origin. */
[[nodiscard]] FMeshData MakePlane(float Size, float UvScale = 1.0f);

/** UV sphere centered at the origin with diameter PrimitiveEdgeLength (radius 50 cm). */
[[nodiscard]] FMeshData MakeSphere(int32 Segments = 24, int32 Rings = 16);
