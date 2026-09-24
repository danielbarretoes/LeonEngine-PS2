#pragma once

#include "MeshData.h"
#include <string>


/// Wavefront OBJ → FMeshData (CPU only). Edit-time / cook — not linked by shipping `leon_engine`.
[[nodiscard]] FMeshData LoadObj(const std::string& Path);

