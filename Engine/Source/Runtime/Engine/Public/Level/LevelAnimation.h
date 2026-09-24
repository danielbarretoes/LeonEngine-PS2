#pragma once

#include <cstddef>
#include <vector>

namespace leon {

/// Runtime animation hooks produced while loading a Level JSON (spin / bob / light orbit).
struct LevelAnimation {
    struct StaticMeshSpin {
        std::size_t meshIndex = 0;
        float yawDegreesPerSec = 0.0f; // added to transform.rotation.y each frame
    };
    struct StaticMeshBob {
        std::size_t meshIndex = 0;
        float baseY = 0.0f;
        float amplitude = 0.0f;
        float speed = 1.0f;
    };
    struct PointOrbit {
        std::size_t lightIndex = 0;
        float radius = 1.0f;
        float height = 1.0f;
        float heightAmp = 0.0f;
        float speed = 1.0f;
    };

    std::vector<StaticMeshSpin> spins;
    std::vector<StaticMeshBob> bobs;
    std::vector<PointOrbit> orbits;

    void clear() {
        spins.clear();
        bobs.clear();
        orbits.clear();
    }
};

} // namespace leon
