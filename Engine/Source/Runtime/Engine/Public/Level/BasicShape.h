#pragma once

#include "Engine/Level.h"
#include "Material.h"
#include "Math/Transform.h"
#include "ResourceCache.h"

#include <memory>
#include <string_view>

/// Engine basic shapes (Unreal-like `/Engine/BasicShapes`: Cube, Sphere, FPlane).
/// Unit meshes; size comes from `transform.scale`. FPlane lies on XZ (y = 0).
/// UV tiling lives on `FMaterial::uvScale`, not on the shape.
enum class EBasicShape
{
	Cube,
	Sphere,
	Plane,
};

/// Placeable basic shape: transform + material + optional mesh options.
struct ENGINE_API FBasicShape
{
	EBasicShape Type = EBasicShape::Cube;
	FTransform Transform{};
	FMaterial Material{};
	/// When false, `MakeStaticMesh` uses `FResourceCache::DefaultMaterial()` (checker).
	bool bHasCustomMaterial = false;

	/// Sphere tessellation (ignored for Cube / FPlane).
	int SphereSegments = 24;
	int SphereRings = 16;

	[[nodiscard]] static FBasicShape Cube(
		FTransform InTransform = {}, FMaterial InMaterial = {}, bool bHasMaterial = false);
	[[nodiscard]] static FBasicShape Sphere(FTransform InTransform = {}, FMaterial InMaterial = {},
		bool bHasMaterial = false, int Segments = 24, int Rings = 16);
	/// `size` sets uniform XZ scale (Unreal-like ground plane extent).
	[[nodiscard]] static FBasicShape Plane(
		float Size = 1.0f, FTransform InTransform = {}, FMaterial InMaterial = {}, bool bHasMaterial = false);

	/// Build a Level `UStaticMeshComponent` (mesh + transform + material override).
	[[nodiscard]] UStaticMeshComponent MakeStaticMesh(FResourceCache& Resources) const;
};

[[nodiscard]] bool TryParseBasicShapeName(std::string_view Name, EBasicShape& Out);
/// Unreal-like BlockingVolume — invisible collision box (Cube + collisionEnabled + hidden).
[[nodiscard]] bool IsBlockingVolumeName(std::string_view Name);
/// Unreal-like FPlayerStart — spawn point only (no mesh).
[[nodiscard]] bool IsPlayerStartName(std::string_view Name);
[[nodiscard]] std::shared_ptr<UStaticMesh> MeshForBasicShape(
	FResourceCache& Resources, EBasicShape Shape, int InSphereSegments = 24, int InSphereRings = 16);
