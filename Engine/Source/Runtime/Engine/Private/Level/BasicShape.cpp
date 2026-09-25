#include "Level/BasicShape.h"

#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "LegacyAssetLoader.h"
#include "Materials/Material.h"

// Class-name parsers live in Content/LevelClassNames.cpp (shared with the level format / cook).

UStaticMesh* MeshForBasicShape(EBasicShape Shape, int32 InSphereSegments, int32 InSphereRings)
{
	switch (Shape)
	{
		case EBasicShape::Cube:
			return FLegacyAssetLoader::LoadEngineObject<UStaticMesh>(
				FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube")));
		case EBasicShape::Sphere:
			return FLegacyAssetLoader::GetSphereMesh(InSphereSegments, InSphereRings);
		case EBasicShape::Plane:
			// 100 cm plane with 0-1 UVs; the tiling is the material's UVScale.
			return FLegacyAssetLoader::LoadEngineObject<UStaticMesh>(
				FSoftObjectPath(TEXT("/Engine/BasicShapes/Plane.Plane")));
	}
	return nullptr;
}

FBasicShape FBasicShape::Cube(const FTransform& InTransform, UMaterialInterface* InMaterial)
{
	FBasicShape Shape;
	Shape.Type = EBasicShape::Cube;
	Shape.Transform = InTransform;
	Shape.Material = InMaterial;
	return Shape;
}

FBasicShape FBasicShape::Sphere(
	const FTransform& InTransform, UMaterialInterface* InMaterial, int32 Segments, int32 Rings)
{
	FBasicShape Shape;
	Shape.Type = EBasicShape::Sphere;
	Shape.Transform = InTransform;
	Shape.Material = InMaterial;
	Shape.SphereSegments = Segments;
	Shape.SphereRings = Rings;
	return Shape;
}

FBasicShape FBasicShape::Plane(float Size, const FTransform& InTransform, UMaterialInterface* InMaterial)
{
	FBasicShape Shape;
	Shape.Type = EBasicShape::Plane;
	Shape.Transform = InTransform;
	Shape.Transform.SetScale3D(FVector(Size, Size, 1.0f));
	Shape.Material = InMaterial;
	return Shape;
}

void FBasicShape::ApplyTo(UStaticMeshComponent& Component) const
{
	(void)Component.SetStaticMesh(MeshForBasicShape(Type, SphereSegments, SphereRings));
	// The procedural shapes have one section with no material of their own: slot 0 draws every section.
	Component.SetMaterial(0, Material != nullptr ? Material : UMaterial::GetDefaultMaterial(MD_Surface));
}

AStaticMeshActor* FBasicShape::SpawnIn(UWorld& World) const
{
	AStaticMeshActor* Actor = World.SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Transform);
	if (Actor != nullptr)
	{
		ApplyTo(*Actor->GetStaticMeshComponent());
	}
	return Actor;
}
