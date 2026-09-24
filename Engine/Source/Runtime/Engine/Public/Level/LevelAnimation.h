#pragma once

#include <cstddef>
#include <vector>


/// Runtime animation hooks produced while loading a Level JSON (spin / bob / light orbit).
struct FLevelAnimation {
    struct FStaticMeshSpin {
        std::size_t meshIndex = 0;
        float yawDegreesPerSec = 0.0f; // added to transform.rotation.y each frame
    };
    struct FStaticMeshBob {
        std::size_t meshIndex = 0;
        float baseY = 0.0f;
        float amplitude = 0.0f;
        float speed = 1.0f;
    };
    struct FPointOrbit {
        std::size_t lightIndex = 0;
        float radius = 1.0f;
        float height = 1.0f;
        float heightAmp = 0.0f;
        float speed = 1.0f;
    };

    std::vector<FStaticMeshSpin> spins;
    std::vector<FStaticMeshBob> bobs;
    std::vector<FPointOrbit> orbits;

    void clear() {
        spins.clear();
        bobs.clear();
        orbits.clear();
    }
};

