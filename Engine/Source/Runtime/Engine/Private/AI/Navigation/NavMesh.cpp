#include "AI/Navigation/NavMesh.h"

#include <cmath>


bool FNavMesh::WorldToCell(float X, float Z, int& OutIx, int& OutIz) const {
    if (!IsValid() || CellSize <= 0.0f) {
        return false;
    }
    OutIx = static_cast<int>(std::floor((X - OriginX) / CellSize));
    OutIz = static_cast<int>(std::floor((Z - OriginZ) / CellSize));
    if (OutIx < 0) {
        OutIx = 0;
    }
    if (OutIz < 0) {
        OutIz = 0;
    }
    if (OutIx >= Width) {
        OutIx = Width - 1;
    }
    if (OutIz >= Depth) {
        OutIz = Depth - 1;
    }
    return true;
}

