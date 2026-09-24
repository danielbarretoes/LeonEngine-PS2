#include "Level/BasicShape.h"


// Class-name parsers live in Content/LevelClassNames.cpp (shared with ContentValidator / cook).

std::shared_ptr<UStaticMesh> MeshForBasicShape(FResourceCache& resources, EBasicShape shape,
                                              int sphereSegments, int sphereRings) {
    switch (shape) {
    case EBasicShape::Cube:
        return resources.GetCubeMesh();
    case EBasicShape::Sphere:
        return resources.GetSphereMesh(sphereSegments, sphereRings);
    case EBasicShape::Plane:
        // Unit plane with 0–1 UVs; tiling is FMaterial::uvScale.
        return resources.GetPlaneMesh(1.0f, 1.0f);
    }
    return nullptr;
}

BasicShape BasicShape::cube(FTransform transform, FMaterial material, bool hasMaterial) {
    BasicShape shape;
    shape.type = EBasicShape::Cube;
    shape.transform = transform;
    shape.material = std::move(material);
    shape.hasCustomMaterial = hasMaterial;
    return shape;
}

BasicShape BasicShape::sphere(FTransform transform, FMaterial material, bool hasMaterial,
                              int segments, int rings) {
    BasicShape shape;
    shape.type = EBasicShape::Sphere;
    shape.transform = transform;
    shape.material = std::move(material);
    shape.hasCustomMaterial = hasMaterial;
    shape.sphereSegments = segments;
    shape.sphereRings = rings;
    return shape;
}

BasicShape BasicShape::plane(float size, FTransform transform, FMaterial material, bool hasMaterial) {
    BasicShape shape;
    shape.type = EBasicShape::Plane;
    shape.transform = transform;
    shape.transform.Scale.x = size;
    shape.transform.Scale.y = 1.0f;
    shape.transform.Scale.z = size;
    shape.material = std::move(material);
    shape.hasCustomMaterial = hasMaterial;
    return shape;
}

StaticMeshComponent BasicShape::MakeStaticMesh(FResourceCache& resources) const {
    StaticMeshComponent component;
    component.mesh = MeshForBasicShape(resources, type, sphereSegments, sphereRings);
    component.transform = transform;
    component.materialOverride = true;
    component.material = hasCustomMaterial ? material : resources.DefaultMaterial();
    return component;
}

