#pragma once

#include "Math/Transform.h"
#include "Engine/Level.h"
#include "Material.h"
#include "ResourceCache.h"
#include <memory>
#include <string_view>


/// Engine basic shapes (Unreal-like `/Engine/BasicShapes`: Cube, Sphere, FPlane).
/// Unit meshes; size comes from `transform.scale`. FPlane lies on XZ (y = 0).
/// UV tiling lives on `FMaterial::uvScale`, not on the shape.
enum class EBasicShape {
    Cube,
    Sphere,
    Plane,
};

/// Placeable basic shape: transform + material + optional mesh options.
struct BasicShape {
    EBasicShape type = EBasicShape::Cube;
    FTransform transform{};
    FMaterial material{};
    /// When false, `MakeStaticMesh` uses `FResourceCache::DefaultMaterial()` (checker).
    bool hasCustomMaterial = false;

    /// Sphere tessellation (ignored for Cube / FPlane).
    int sphereSegments = 24;
    int sphereRings = 16;

    [[nodiscard]] static BasicShape cube(FTransform transform = {}, FMaterial material = {},
                                         bool hasMaterial = false);
    [[nodiscard]] static BasicShape sphere(FTransform transform = {}, FMaterial material = {},
                                           bool hasMaterial = false, int segments = 24,
                                           int rings = 16);
    /// `size` sets uniform XZ scale (Unreal-like ground plane extent).
    [[nodiscard]] static BasicShape plane(float size = 1.0f, FTransform transform = {},
                                          FMaterial material = {}, bool hasMaterial = false);

    /// Build a Level `StaticMeshComponent` (mesh + transform + material override).
    [[nodiscard]] StaticMeshComponent MakeStaticMesh(FResourceCache& resources) const;
};

[[nodiscard]] bool tryParseBasicShapeName(std::string_view name, EBasicShape& out);
/// Unreal-like BlockingVolume — invisible collision box (Cube + collisionEnabled + hidden).
[[nodiscard]] bool isBlockingVolumeName(std::string_view name);
/// Unreal-like PlayerStart — spawn point only (no mesh).
[[nodiscard]] bool isPlayerStartName(std::string_view name);
[[nodiscard]] std::shared_ptr<UStaticMesh> MeshForBasicShape(FResourceCache& resources,
                                                            EBasicShape shape,
                                                            int sphereSegments = 24,
                                                            int sphereRings = 16);

