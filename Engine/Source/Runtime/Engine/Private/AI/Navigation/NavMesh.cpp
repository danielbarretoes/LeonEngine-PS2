#include "AI/Navigation/NavMesh.h"

#include <cmath>

namespace leon {

bool NavMesh::WorldToCell(float x, float z, int& outIx, int& outIz) const {
    if (!IsValid() || cellSize <= 0.0f) {
        return false;
    }
    outIx = static_cast<int>(std::floor((x - originX) / cellSize));
    outIz = static_cast<int>(std::floor((z - originZ) / cellSize));
    if (outIx < 0) {
        outIx = 0;
    }
    if (outIz < 0) {
        outIz = 0;
    }
    if (outIx >= width) {
        outIx = width - 1;
    }
    if (outIz >= depth) {
        outIz = depth - 1;
    }
    return true;
}

} // namespace leon
