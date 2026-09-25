#pragma once

#include "MeshData.h"

/** Unit cube centered at the origin [-0.5, 0.5]^3. */
[[nodiscard]] FMeshData MakeCube();

/** Axis-aligned ground plane on XZ (Y = 0), centered at the origin. */
[[nodiscard]] FMeshData MakePlane(float Size, float UvScale = 1.0f);

/** UV sphere centered at the origin with radius 0.5. */
[[nodiscard]] FMeshData MakeSphere(int32 Segments = 24, int32 Rings = 16);
