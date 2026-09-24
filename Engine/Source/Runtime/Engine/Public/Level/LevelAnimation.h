#pragma once

#include <cstddef>
#include <vector>


/// Runtime animation hooks produced while loading a Level JSON (spin / bob / light orbit).
struct ENGINE_API FLevelAnimation {
    struct FStaticMeshSpin {
        std::size_t MeshIndex = 0;
        float YawDegreesPerSec = 0.0f; // added to transform.rotation.y each frame
    };
    struct FStaticMeshBob {
        std::size_t MeshIndex = 0;
        float BaseY = 0.0f;
        float Amplitude = 0.0f;
        float Speed = 1.0f;
    };
    struct FPointOrbit {
        std::size_t LightIndex = 0;
        float Radius = 1.0f;
        float Height = 1.0f;
        float HeightAmp = 0.0f;
        float Speed = 1.0f;
    };

    std::vector<FStaticMeshSpin> Spins;
    std::vector<FStaticMeshBob> Bobs;
    std::vector<FPointOrbit> Orbits;

    void Clear() {
        Spins.clear();
        Bobs.clear();
        Orbits.clear();
    }
};

