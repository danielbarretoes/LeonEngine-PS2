#include "Level/BasicShape.h"


// Class-name parsers live in Content/LevelClassNames.cpp (shared with ContentValidator / cook).

std::shared_ptr<UStaticMesh> MeshForBasicShape(FResourceCache& Resources, EBasicShape Shape,
                                              int InSphereSegments, int InSphereRings) {
    switch (Shape) {
    case EBasicShape::Cube:
        return Resources.GetCubeMesh();
    case EBasicShape::Sphere:
        return Resources.GetSphereMesh(InSphereSegments, InSphereRings);
    case EBasicShape::Plane:
        // Unit plane with 0–1 UVs; tiling is FMaterial::uvScale.
        return Resources.GetPlaneMesh(1.0f, 1.0f);
    }
    return nullptr;
}

FBasicShape FBasicShape::Cube(FTransform InTransform, FMaterial InMaterial, bool bHasMaterial) {
    FBasicShape Shape;
    Shape.Type = EBasicShape::Cube;
    Shape.Transform = InTransform;
    Shape.Material = std::move(InMaterial);
    Shape.bHasCustomMaterial = bHasMaterial;
    return Shape;
}

FBasicShape FBasicShape::Sphere(FTransform InTransform, FMaterial InMaterial, bool bHasMaterial,
                              int Segments, int Rings) {
    FBasicShape Shape;
    Shape.Type = EBasicShape::Sphere;
    Shape.Transform = InTransform;
    Shape.Material = std::move(InMaterial);
    Shape.bHasCustomMaterial = bHasMaterial;
    Shape.SphereSegments = Segments;
    Shape.SphereRings = Rings;
    return Shape;
}

FBasicShape FBasicShape::Plane(float Size, FTransform InTransform, FMaterial InMaterial, bool bHasMaterial) {
    FBasicShape Shape;
    Shape.Type = EBasicShape::Plane;
    Shape.Transform = InTransform;
    Shape.Transform.Scale.x = Size;
    Shape.Transform.Scale.y = 1.0f;
    Shape.Transform.Scale.z = Size;
    Shape.Material = std::move(InMaterial);
    Shape.bHasCustomMaterial = bHasMaterial;
    return Shape;
}

UStaticMeshComponent FBasicShape::MakeStaticMesh(FResourceCache& Resources) const {
    UStaticMeshComponent Component;
    Component.Mesh = MeshForBasicShape(Resources, Type, SphereSegments, SphereRings);
    Component.Transform = Transform;
    Component.bMaterialOverride = true;
    Component.Material = bHasCustomMaterial ? Material : Resources.DefaultMaterial();
    return Component;
}

