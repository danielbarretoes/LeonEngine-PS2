#pragma once

#include <leon/core/Transform.h>
#include <leon/level/Level.h>
#include <leon/render/Material.h>
#include <leon/render/ResourceCache.h>
#include <memory>
#include <string_view>

namespace leon {

/// Engine basic shapes (Unreal-like `/Engine/BasicShapes`: Cube, Sphere, Plane).
/// Unit meshes; size comes from `transform.scale`. Plane lies on XZ (y = 0).
/// UV tiling lives on `Material::uvScale`, not on the shape.
enum class EBasicShape {
    Cube,
    Sphere,
    Plane,
};

/// Placeable basic shape: transform + material + optional mesh options.
struct BasicShape {
    EBasicShape type = EBasicShape::Cube;
    Transform transform{};
    Material material{};
    /// When false, `MakeStaticMesh` uses `ResourceCache::DefaultMaterial()` (checker).
    bool hasCustomMaterial = false;

    /// Sphere tessellation (ignored for Cube / Plane).
    int sphereSegments = 24;
    int sphereRings = 16;

    [[nodiscard]] static BasicShape cube(Transform transform = {}, Material material = {},
                                         bool hasMaterial = false);
    [[nodiscard]] static BasicShape sphere(Transform transform = {}, Material material = {},
                                           bool hasMaterial = false, int segments = 24,
                                           int rings = 16);
    /// `size` sets uniform XZ scale (Unreal-like ground plane extent).
    [[nodiscard]] static BasicShape plane(float size = 1.0f, Transform transform = {},
                                          Material material = {}, bool hasMaterial = false);

    /// Build a Level `StaticMeshComponent` (mesh + transform + material override).
    [[nodiscard]] StaticMeshComponent MakeStaticMesh(ResourceCache& resources) const;
};

[[nodiscard]] bool tryParseBasicShapeName(std::string_view name, EBasicShape& out);
/// Unreal-like BlockingVolume — invisible collision box (Cube + collisionEnabled + hidden).
[[nodiscard]] bool isBlockingVolumeName(std::string_view name);
/// Unreal-like PlayerStart — spawn point only (no mesh).
[[nodiscard]] bool isPlayerStartName(std::string_view name);
[[nodiscard]] std::shared_ptr<StaticMesh> MeshForBasicShape(ResourceCache& resources,
                                                            EBasicShape shape,
                                                            int sphereSegments = 24,
                                                            int sphereRings = 16);

} // namespace leon
