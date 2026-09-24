#pragma once

#include "MeshData.h"

/// Unit cube centered at the origin [-0.5, 0.5]^3.
[[nodiscard]] FMeshData MakeCube();

/// Axis-aligned ground plane on XZ (y = 0), centered at origin.
[[nodiscard]] FMeshData MakePlane(float Size, float UvScale = 1.0f);

/// UV sphere centered at the origin with radius 0.5.
[[nodiscard]] FMeshData MakeSphere(int Segments = 24, int Rings = 16);
