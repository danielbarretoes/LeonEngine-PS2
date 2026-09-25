#include "Level/BasicShape.h"

#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Primitives.h"

// Class-name parsers live in Content/LevelClassNames.cpp (shared with the level format / cook).

UStaticMesh* MeshForBasicShape(
	FResourceCache& Resources, EBasicShape Shape, int32 InSphereSegments, int32 InSphereRings)
{
	switch (Shape)
	{
		case EBasicShape::Cube:
			return Resources.GetCubeMesh();
		case EBasicShape::Sphere:
			return Resources.GetSphereMesh(InSphereSegments, InSphereRings);
		case EBasicShape::Plane:
			// 100 cm plane with 0-1 UVs; the tiling is FMaterial::UvScale.
			return Resources.GetPlaneMesh(PrimitiveEdgeLength, 1.0f);
	}
	return nullptr;
}

FBasicShape FBasicShape::Cube(const FTransform& InTransform, FMaterial InMaterial, bool bHasMaterial)
{
	FBasicShape Shape;
	Shape.Type = EBasicShape::Cube;
	Shape.Transform = InTransform;
	Shape.Material = MoveTemp(InMaterial);
	Shape.bHasCustomMaterial = bHasMaterial;
	return Shape;
}

FBasicShape FBasicShape::Sphere(
	const FTransform& InTransform, FMaterial InMaterial, bool bHasMaterial, int32 Segments, int32 Rings)
{
	FBasicShape Shape;
	Shape.Type = EBasicShape::Sphere;
	Shape.Transform = InTransform;
	Shape.Material = MoveTemp(InMaterial);
	Shape.bHasCustomMaterial = bHasMaterial;
	Shape.SphereSegments = Segments;
	Shape.SphereRings = Rings;
	return Shape;
}

FBasicShape FBasicShape::Plane(float Size, const FTransform& InTransform, FMaterial InMaterial, bool bHasMaterial)
{
	FBasicShape Shape;
	Shape.Type = EBasicShape::Plane;
	Shape.Transform = InTransform;
	Shape.Transform.SetScale3D(FVector(Size, Size, 1.0f));
	Shape.Material = MoveTemp(InMaterial);
	Shape.bHasCustomMaterial = bHasMaterial;
	return Shape;
}

void FBasicShape::ApplyTo(UStaticMeshComponent& Component, FResourceCache& Resources) const
{
	(void)Component.SetStaticMesh(MeshForBasicShape(Resources, Type, SphereSegments, SphereRings));
	// The procedural shapes have one section with no material of their own: slot 0 draws every section.
	Component.SetMaterial(0, bHasCustomMaterial ? Material : Resources.DefaultMaterial());
}

AStaticMeshActor* FBasicShape::SpawnIn(UWorld& World, FResourceCache& Resources) const
{
	AStaticMeshActor* Actor = World.SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Transform);
	if (Actor != nullptr)
	{
		ApplyTo(*Actor->GetStaticMeshComponent(), Resources);
	}
	return Actor;
}
