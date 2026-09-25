#pragma once

#include "CoreMinimal.h"

class AStaticMeshActor;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;
class UWorld;

/**
 * Engine basic shapes (UE: the /Engine/BasicShapes meshes Cube, Sphere and Plane).
 * Unit meshes; the size comes from the transform scale. The plane lies on XY (Z = 0), facing +Z.
 * UV tiling lives on the material's UVScale, not on the shape.
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
	/** The material every section draws with; null for the engine's default material (GetDefaultMaterial). */
	UMaterialInterface* Material = nullptr;

	/** Sphere tessellation (ignored for Cube / Plane). */
	int32 SphereSegments = 24;
	int32 SphereRings = 16;

	[[nodiscard]] static FBasicShape Cube(
		const FTransform& InTransform = FTransform::Identity, UMaterialInterface* InMaterial = nullptr);
	[[nodiscard]] static FBasicShape Sphere(const FTransform& InTransform = FTransform::Identity,
		UMaterialInterface* InMaterial = nullptr, int32 Segments = 24, int32 Rings = 16);
	/** Size sets the uniform XY scale (UE-like ground plane extent). */
	[[nodiscard]] static FBasicShape Plane(float Size = 1.0f, const FTransform& InTransform = FTransform::Identity,
		UMaterialInterface* InMaterial = nullptr);

	/**
	 * Gives a component the shape's mesh and its material (every section's slot 0; the default material without
	 * one).
	 */
	void ApplyTo(UStaticMeshComponent& Component) const;

	/** Spawns an AStaticMeshActor at Transform showing the shape (UE: placing a /Engine/BasicShapes mesh). */
	AStaticMeshActor* SpawnIn(UWorld& World) const;
};

[[nodiscard]] bool TryParseBasicShapeName(const FString& Name, EBasicShape& Out);
/** The `.llev` BlockingVolume class name (an ABlockingVolume: an invisible box, plan decision D16). */
[[nodiscard]] bool IsBlockingVolumeName(const FString& Name);
/** The `.llev` PlayerStart class name (an APlayerStart: a spawn point, no mesh). */
[[nodiscard]] bool IsPlayerStartName(const FString& Name);

/**
 * The mesh of a basic shape: `/Engine/BasicShapes/Cube`, `Plane` or `Sphere` (100 cm), or a sphere of another
 * tessellation (FLegacyAssetLoader makes them until the engine content is packaged).
 */
[[nodiscard]] UStaticMesh* MeshForBasicShape(EBasicShape Shape, int32 InSphereSegments = 24, int32 InSphereRings = 16);
