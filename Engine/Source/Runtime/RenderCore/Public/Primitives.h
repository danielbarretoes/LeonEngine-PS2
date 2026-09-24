#pragma once

#include "MeshData.h"

namespace leon {

/// Unit cube centered at the origin [-0.5, 0.5]^3.
[[nodiscard]] MeshData MakeCube();

/// Axis-aligned ground plane on XZ (y = 0), centered at origin.
[[nodiscard]] MeshData MakePlane(float size, float uvScale = 1.0f);

/// UV sphere centered at the origin with radius 0.5.
[[nodiscard]] MeshData MakeSphere(int segments = 24, int rings = 16);

} // namespace leon
