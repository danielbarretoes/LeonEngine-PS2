#include <algorithm>
#include <filesystem>
#include <iostream>
#include "Misc/Paths.h"
#include "LeonMeshFormat.h"
#include "MaterialAsset.h"
#include "MeshData.h"
#include "Primitives.h"
#include "ResourceCache.h"


std::string FResourceCache::normalizeKey(const std::string& path) {
    std::error_code ec;
    const std::string key = std::filesystem::weakly_canonical(std::filesystem::path(path), ec)
                                .lexically_normal()
                                .string();
    return ec ? path : key;
}

std::shared_ptr<UStaticMesh> FResourceCache::cacheMesh(const std::string& key, FMeshData data) {
    if (const auto it = meshes_.find(key); it != meshes_.end()) {
        return it->second;
    }
    if (data.empty()) {
        return nullptr;
    }
    auto mesh = std::make_shared<UStaticMesh>(gpuUploadEnabled_ ? UStaticMesh::Upload(data)
                                                               : UStaticMesh::CreateCpu(data));
    if (!mesh->Valid()) {
        return nullptr;
    }
    meshes_.emplace(key, mesh);
    return mesh;
}

std::shared_ptr<UStaticMesh> FResourceCache::LoadStaticMesh(const std::string& path) {
    const std::string cacheKey = normalizeKey(path);
    if (const auto it = meshes_.find(cacheKey); it != meshes_.end()) {
        return it->second;
    }

    FMeshData data;
    if (IsLeonMeshPath(path)) {
        if (!LoadLeonMeshFile(path, data)) {
            std::cerr << "ResourceCache: failed to load .lmesh '" << path << "'\n";
            return nullptr;
        }
    } else {
        // Shipping / runtime: cooked `.lmesh` only (OBJ/FBX/glTF via Editor Import / leon-cook).
        std::cerr << "ResourceCache: expected .lmesh path, got '" << path << "'\n";
        return nullptr;
    }

    // Bind diffuse textures referenced by the MTL before GPU upload.
    for (std::size_t i = 0; i < data.materials.size() && i < data.albedoMapPaths.size(); ++i) {
        if (data.albedoMapPaths[i].empty()) {
            continue;
        }
        data.materials[i].albedoMap = LoadTexture(data.albedoMapPaths[i]);
        if (data.materials[i].albedoMap == nullptr) {
            std::cerr << "ResourceCache: missing albedo map '" << data.albedoMapPaths[i] << "'\n";
        }
    }

    return cacheMesh(cacheKey, std::move(data));
}

std::shared_ptr<UTexture2D> FResourceCache::LoadTexture(const std::string& path) {
    if (!gpuUploadEnabled_) {
        return nullptr;
    }
    const std::string cacheKey = normalizeKey(path);
    if (const auto it = textures_.find(cacheKey); it != textures_.end()) {
        return it->second;
    }

    auto texture = std::make_shared<UTexture2D>(UTexture2D::LoadFromFile(path));
    if (!texture->Valid()) {
        return nullptr;
    }
    textures_.emplace(cacheKey, texture);
    return texture;
}

std::shared_ptr<FEnvironmentMap> FResourceCache::LoadEnvMap(const std::string& path, int faceSize) {
    if (!gpuUploadEnabled_) {
        return nullptr;
    }
    const std::string cacheKey =
        normalizeKey(path) + ":cube:" + std::to_string(std::max(16, faceSize));
    if (const auto it = envMaps_.find(cacheKey); it != envMaps_.end()) {
        return it->second;
    }

    auto env = std::make_shared<FEnvironmentMap>(FEnvironmentMap::LoadFromHdr(path, faceSize));
    if (!env->Valid()) {
        std::cerr << "ResourceCache: failed to load env map '" << path << "'\n";
        return nullptr;
    }
    envMaps_.emplace(cacheKey, env);
    return env;
}

std::shared_ptr<UTexture2D> FResourceCache::CheckerTexture(int size) {
    if (!gpuUploadEnabled_) {
        return nullptr;
    }
    size = std::max(size, 2);
    const std::string cacheKey = "proc:checker:" + std::to_string(size);
    if (const auto it = textures_.find(cacheKey); it != textures_.end()) {
        return it->second;
    }

    auto texture = std::make_shared<UTexture2D>(UTexture2D::CreateChecker(size));
    if (!texture->Valid()) {
        return nullptr;
    }
    textures_.emplace(cacheKey, texture);
    return texture;
}

std::shared_ptr<UTexture2D> FResourceCache::BumpNormalTexture(int size) {
    if (!gpuUploadEnabled_) {
        return nullptr;
    }
    size = std::max(size, 8);
    const std::string cacheKey = "proc:normal:bump:" + std::to_string(size);
    if (const auto it = textures_.find(cacheKey); it != textures_.end()) {
        return it->second;
    }
    auto texture = std::make_shared<UTexture2D>(UTexture2D::CreateBumpNormal(size));
    if (!texture->Valid()) {
        return nullptr;
    }
    textures_.emplace(cacheKey, texture);
    return texture;
}

FMaterial FResourceCache::LoadMaterial(const std::string& path) {
    const std::string cacheKey = normalizeKey(path);
    if (const auto it = materials_.find(cacheKey); it != materials_.end()) {
        return it->second;
    }

    FMaterial material;
    if (!LoadMaterialFile(*this, path, material)) {
        std::cerr << "ResourceCache: using default material (failed '" << path << "')\n";
        return DefaultMaterial();
    }
    materials_.emplace(cacheKey, material);
    return material;
}

FMaterial FResourceCache::DefaultMaterial() {
    constexpr const char* kKey = "engine:default";
    if (const auto it = materials_.find(kKey); it != materials_.end()) {
        return it->second;
    }

    FMaterial material;
    const std::string lmatPath = FPaths::ResolveAssetPath("assets/Materials/M_Default.lmat");
    if (std::filesystem::exists(lmatPath) && LoadMaterialFile(*this, lmatPath, material)) {
        materials_.emplace(kKey, material);
        return material;
    }

    if (gpuUploadEnabled_) {
        material = MakeDefaultCheckerMaterial(*this);
    } else {
        material.shading = EMaterialShadingModel::BlinnPhong;
        material.albedo = {0.55f, 0.55f, 0.58f};
        material.shininess = 16.0f;
        material.syncRoughnessFromShininess();
    }
    materials_.emplace(kKey, material);
    return material;
}

std::shared_ptr<UStaticMesh> FResourceCache::GetCubeMesh() {
    return cacheMesh("proc:cube", MakeCube());
}

std::shared_ptr<UStaticMesh> FResourceCache::GetPlaneMesh(float size, float uvScale) {
    const std::string key = "proc:plane:" + std::to_string(size) + ":" + std::to_string(uvScale);
    return cacheMesh(key, MakePlane(size, uvScale));
}

std::shared_ptr<UStaticMesh> FResourceCache::GetSphereMesh(int segments, int rings) {
    const std::string key = "proc:sphere:" + std::to_string(segments) + ":" + std::to_string(rings);
    return cacheMesh(key, MakeSphere(segments, rings));
}

void FResourceCache::clear() {
    meshes_.clear();
    textures_.clear();
    envMaps_.clear();
    materials_.clear();
}

void FResourceCache::InvalidateMaterial(const std::string& path) {
    materials_.erase(normalizeKey(path));
}

