#include "Level/BasicShape.h"

namespace leon {

// Class-name parsers live in Content/LevelClassNames.cpp (shared with ContentValidator / cook).

std::shared_ptr<StaticMesh> MeshForBasicShape(ResourceCache& resources, EBasicShape shape,
                                              int sphereSegments, int sphereRings) {
    switch (shape) {
    case EBasicShape::Cube:
        return resources.GetCubeMesh();
    case EBasicShape::Sphere:
        return resources.GetSphereMesh(sphereSegments, sphereRings);
    case EBasicShape::Plane:
        // Unit plane with 0–1 UVs; tiling is Material::uvScale.
        return resources.GetPlaneMesh(1.0f, 1.0f);
    }
    return nullptr;
}

BasicShape BasicShape::cube(Transform transform, Material material, bool hasMaterial) {
    BasicShape shape;
    shape.type = EBasicShape::Cube;
    shape.transform = transform;
    shape.material = std::move(material);
    shape.hasCustomMaterial = hasMaterial;
    return shape;
}

BasicShape BasicShape::sphere(Transform transform, Material material, bool hasMaterial,
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

BasicShape BasicShape::plane(float size, Transform transform, Material material, bool hasMaterial) {
    BasicShape shape;
    shape.type = EBasicShape::Plane;
    shape.transform = transform;
    shape.transform.scale.x = size;
    shape.transform.scale.y = 1.0f;
    shape.transform.scale.z = size;
    shape.material = std::move(material);
    shape.hasCustomMaterial = hasMaterial;
    return shape;
}

StaticMeshComponent BasicShape::MakeStaticMesh(ResourceCache& resources) const {
    StaticMeshComponent component;
    component.mesh = MeshForBasicShape(resources, type, sphereSegments, sphereRings);
    component.transform = transform;
    component.materialOverride = true;
    component.material = hasCustomMaterial ? material : resources.DefaultMaterial();
    return component;
}

} // namespace leon
