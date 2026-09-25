#pragma once

#include "CoreMinimal.h"
#include "Engine/Level.h"
#include "Material.h"
#include "ResourceCache.h"

/**
 * Engine basic shapes (UE-like /Engine/BasicShapes: Cube, Sphere, Plane).
 * Unit meshes; the size comes from the transform scale. The plane lies on XZ (Y = 0).
 * UV tiling lives on FMaterial::UvScale, not on the shape.
 */
enum class EBasicShape
{
	Cube,
	Sphere,
	Plane,
};

/** Placeable basic shape: transform + material + optional mesh options. */
struct ENGINE_API FBasicShape
{
	EBasicShape Type = EBasicShape::Cube;
	FTransform Transform;
	FMaterial Material{};
	/** When false, MakeStaticMesh uses FResourceCache::DefaultMaterial() (checker). */
	bool bHasCustomMaterial = false;

	/** Sphere tessellation (ignored for Cube / Plane). */
	int32 SphereSegments = 24;
	int32 SphereRings = 16;

	[[nodiscard]] static FBasicShape Cube(
		const FTransform& InTransform = FTransform::Identity, FMaterial InMaterial = {}, bool bHasMaterial = false);
	[[nodiscard]] static FBasicShape Sphere(const FTransform& InTransform = FTransform::Identity,
		FMaterial InMaterial = {}, bool bHasMaterial = false, int32 Segments = 24, int32 Rings = 16);
	/** Size sets the uniform XZ scale (UE-like ground plane extent). */
	[[nodiscard]] static FBasicShape Plane(float Size = 1.0f, const FTransform& InTransform = FTransform::Identity,
		FMaterial InMaterial = {}, bool bHasMaterial = false);

	/** Builds a level UStaticMeshComponent (mesh + transform + material override). */
	[[nodiscard]] UStaticMeshComponent MakeStaticMesh(FResourceCache& Resources) const;
};

[[nodiscard]] bool TryParseBasicShapeName(const FString& Name, EBasicShape& Out);
/** UE-like BlockingVolume: invisible collision box (Cube + bCollisionEnabled + bHidden). */
[[nodiscard]] bool IsBlockingVolumeName(const FString& Name);
/** UE-like FPlayerStart: spawn point only (no mesh). */
[[nodiscard]] bool IsPlayerStartName(const FString& Name);
[[nodiscard]] TSharedPtr<UStaticMesh> MeshForBasicShape(
	FResourceCache& Resources, EBasicShape Shape, int32 InSphereSegments = 24, int32 InSphereRings = 16);
