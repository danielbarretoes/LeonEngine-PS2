#include "Level/BasicShape.h"

// Class-name parsers live in Content/LevelClassNames.cpp (shared with the level format / cook).

TSharedPtr<UStaticMesh> MeshForBasicShape(
	FResourceCache& Resources, EBasicShape Shape, int32 InSphereSegments, int32 InSphereRings)
{
	switch (Shape)
	{
		case EBasicShape::Cube:
			return Resources.GetCubeMesh();
		case EBasicShape::Sphere:
			return Resources.GetSphereMesh(InSphereSegments, InSphereRings);
		case EBasicShape::Plane:
			// Unit plane with 0-1 UVs; the tiling is FMaterial::UvScale.
			return Resources.GetPlaneMesh(1.0f, 1.0f);
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
	Shape.Transform.SetScale3D(FVector(Size, 1.0f, Size));
	Shape.Material = MoveTemp(InMaterial);
	Shape.bHasCustomMaterial = bHasMaterial;
	return Shape;
}

UStaticMeshComponent FBasicShape::MakeStaticMesh(FResourceCache& Resources) const
{
	UStaticMeshComponent Component;
	Component.Mesh = MeshForBasicShape(Resources, Type, SphereSegments, SphereRings);
	Component.Transform = Transform;
	Component.bMaterialOverride = true;
	Component.Material = bHasCustomMaterial ? Material : Resources.DefaultMaterial();
	return Component;
}
