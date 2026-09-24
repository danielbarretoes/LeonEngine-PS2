#pragma once

#include <leon/render/EnvMap.h>
#include <leon/render/Material.h>
#include <leon/render/MeshData.h>
#include <leon/render/StaticMesh.h>
#include <leon/render/Texture.h>
#include <memory>
#include <string>
#include <unordered_map>

namespace leon {

/// Path- and key-keyed cache for GPU meshes/textures: OBJ/file loads plus
/// procedural checker/bump normals, material assets, and cube/plane/sphere meshes.
class ResourceCache {
public:
    [[nodiscard]] std::shared_ptr<StaticMesh> LoadStaticMesh(const std::string& path);
    [[nodiscard]] std::shared_ptr<Texture> LoadTexture(const std::string& path);
    [[nodiscard]] std::shared_ptr<EnvMap> LoadEnvMap(const std::string& path,
                                                     int faceSize = EnvMap::kDefaultFaceSize);
    [[nodiscard]] std::shared_ptr<Texture> CheckerTexture(int size = 64);
    [[nodiscard]] std::shared_ptr<Texture> BumpNormalTexture(int size = 256);

    /// Load `.lmat` material asset (cached by resolved path). On failure → DefaultMaterial().
    [[nodiscard]] Material LoadMaterial(const std::string& path);
    /// Grayscale checker template (Unreal-like default / WorldGrid placeholder).
    [[nodiscard]] Material DefaultMaterial();

    [[nodiscard]] std::shared_ptr<StaticMesh> GetCubeMesh();
    [[nodiscard]] std::shared_ptr<StaticMesh> GetPlaneMesh(float size = 8.0f, float uvScale = 4.0f);
    [[nodiscard]] std::shared_ptr<StaticMesh> GetSphereMesh(int segments = 24, int rings = 16);

    /// When false, meshes stay CPU-only and textures/env maps are skipped (headless server).
    void SetGpuUploadEnabled(bool enabled) { gpuUploadEnabled_ = enabled; }
    [[nodiscard]] bool IsGpuUploadEnabled() const { return gpuUploadEnabled_; }

    void clear();
    /// Drop a cached material so the next `loadMaterial` reloads from disk.
    void InvalidateMaterial(const std::string& path);

private:
    [[nodiscard]] static std::string normalizeKey(const std::string& path);
    [[nodiscard]] std::shared_ptr<StaticMesh> cacheMesh(const std::string& key, MeshData data);

    bool gpuUploadEnabled_ = true;
    std::unordered_map<std::string, std::shared_ptr<StaticMesh>> meshes_;
    std::unordered_map<std::string, std::shared_ptr<Texture>> textures_;
    std::unordered_map<std::string, std::shared_ptr<EnvMap>> envMaps_;
    std::unordered_map<std::string, Material> materials_;
};

} // namespace leon
