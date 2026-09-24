#pragma once

#include "EnvironmentMap.h"
#include "Material.h"
#include "MeshData.h"
#include "StaticMesh.h"
#include "Texture2D.h"
#include <memory>
#include <string>
#include <unordered_map>


/// Path- and key-keyed cache for GPU meshes/textures: OBJ/file loads plus
/// procedural checker/bump normals, material assets, and cube/plane/sphere meshes.
class FResourceCache {
public:
    [[nodiscard]] std::shared_ptr<UStaticMesh> LoadStaticMesh(const std::string& path);
    [[nodiscard]] std::shared_ptr<UTexture2D> LoadTexture(const std::string& path);
    [[nodiscard]] std::shared_ptr<FEnvironmentMap> LoadEnvMap(const std::string& path,
                                                     int faceSize = FEnvironmentMap::kDefaultFaceSize);
    [[nodiscard]] std::shared_ptr<UTexture2D> CheckerTexture(int size = 64);
    [[nodiscard]] std::shared_ptr<UTexture2D> BumpNormalTexture(int size = 256);

    /// Load `.lmat` material asset (cached by resolved path). On failure → DefaultMaterial().
    [[nodiscard]] FMaterial LoadMaterial(const std::string& path);
    /// Grayscale checker template (Unreal-like default / WorldGrid placeholder).
    [[nodiscard]] FMaterial DefaultMaterial();

    [[nodiscard]] std::shared_ptr<UStaticMesh> GetCubeMesh();
    [[nodiscard]] std::shared_ptr<UStaticMesh> GetPlaneMesh(float size = 8.0f, float uvScale = 4.0f);
    [[nodiscard]] std::shared_ptr<UStaticMesh> GetSphereMesh(int segments = 24, int rings = 16);

    /// When false, meshes stay CPU-only and textures/env maps are skipped (headless server).
    void SetGpuUploadEnabled(bool enabled) { gpuUploadEnabled_ = enabled; }
    [[nodiscard]] bool IsGpuUploadEnabled() const { return gpuUploadEnabled_; }

    void clear();
    /// Drop a cached material so the next `loadMaterial` reloads from disk.
    void InvalidateMaterial(const std::string& path);

private:
    [[nodiscard]] static std::string normalizeKey(const std::string& path);
    [[nodiscard]] std::shared_ptr<UStaticMesh> cacheMesh(const std::string& key, FMeshData data);

    bool gpuUploadEnabled_ = true;
    std::unordered_map<std::string, std::shared_ptr<UStaticMesh>> meshes_;
    std::unordered_map<std::string, std::shared_ptr<UTexture2D>> textures_;
    std::unordered_map<std::string, std::shared_ptr<FEnvironmentMap>> envMaps_;
    std::unordered_map<std::string, FMaterial> materials_;
};

