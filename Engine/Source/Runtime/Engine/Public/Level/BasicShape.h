#pragma once

#include "CoreMinimal.h"
#include "MaterialShared.h"
#include "ResourceCache.h"

class AStaticMeshActor;
class UStaticMeshComponent;
class UWorld;

/**
 * Engine basic shapes (UE-like /Engine/BasicShapes: Cube, Sphere, Plane).
 * Unit meshes; the size comes from the transform scale. The plane lies on XY (Z = 0), facing +Z.
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
	/** Size sets the uniform XY scale (UE-like ground plane extent). */
	[[nodiscard]] static FBasicShape Plane(float Size = 1.0f, const FTransform& InTransform = FTransform::Identity,
		FMaterial InMaterial = {}, bool bHasMaterial = false);

	/**
	 * Gives a component the shape's mesh and its material (every section's slot 0; the default material without
	 * one).
	 */
	void ApplyTo(UStaticMeshComponent& Component, FResourceCache& Resources) const;

	/** Spawns an AStaticMeshActor at Transform showing the shape (UE: placing a /Engine/BasicShapes mesh). */
	AStaticMeshActor* SpawnIn(UWorld& World, FResourceCache& Resources) const;
};

[[nodiscard]] bool TryParseBasicShapeName(const FString& Name, EBasicShape& Out);
/** The `.llev` BlockingVolume class name (an ABlockingVolume: an invisible box, plan decision D16). */
[[nodiscard]] bool IsBlockingVolumeName(const FString& Name);
/** The `.llev` PlayerStart class name (an APlayerStart: a spawn point, no mesh). */
[[nodiscard]] bool IsPlayerStartName(const FString& Name);
[[nodiscard]] UStaticMesh* MeshForBasicShape(
	FResourceCache& Resources, EBasicShape Shape, int32 InSphereSegments = 24, int32 InSphereRings = 16);
