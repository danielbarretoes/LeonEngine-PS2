#pragma once

#include "MeshData.h"
#include <string>


/// Wavefront OBJ → MeshData (CPU only). Edit-time / cook — not linked by shipping `leon_engine`.
[[nodiscard]] MeshData LoadObj(const std::string& path);

