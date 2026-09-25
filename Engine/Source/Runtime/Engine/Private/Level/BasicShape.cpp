#include "Level/BasicShape.h"

#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Materials/Material.h"
#include "Primitives.h"
#include "UObject/Package.h"

// Class-name parsers live in Content/LevelClassNames.cpp (shared with the level format / cook).

namespace
{

	/** The tessellation of the /Engine/BasicShapes/Sphere package. */
	constexpr int32 DefaultSphereSegments = 24;
	constexpr int32 DefaultSphereRings = 16;

} // namespace

UStaticMesh* MeshForBasicShape(EBasicShape Shape, int32 InSphereSegments, int32 InSphereRings)
{
	switch (Shape)
	{
		case EBasicShape::Cube:
			return LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		case EBasicShape::Sphere:
			return GetSphereMesh(InSphereSegments, InSphereRings);
		case EBasicShape::Plane:
			// 100 cm plane with 0-1 UVs; the tiling is the material's UVScale.
			return LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	}
	return nullptr;
}

UStaticMesh* GetSphereMesh(int32 Segments, int32 Rings)
{
	if (Segments == DefaultSphereSegments && Rings == DefaultSphereRings)
	{
		return LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	}
	const FString Name = FString::Printf("Sphere_%dx%d", Segments, Rings);
	const FString PackageName = FString(TEXT("/Temp/BasicShapes/")) + Name;
	if (UStaticMesh* Found = FindObject<UStaticMesh>(nullptr, *(PackageName + TEXT(".") + Name)))
	{
		if (!Found->IsPendingKill())
		{
			return Found;
		}
	}
	UPackage* Package = CreatePackage(*PackageName);
	Package->SetFlags(RF_Transient);
	FName MeshName(*Name);
	if (FindObjectFast<UObject>(Package, MeshName) != nullptr)
	{
		// A pending-kill mesh of that name still waits for the garbage collector.
		MeshName = MakeUniqueObjectName(Package, UStaticMesh::StaticClass(), MeshName);
	}
	UStaticMesh* Mesh = NewObject<UStaticMesh>(Package, MeshName, RF_Public | RF_Standalone | RF_Transient);
	(void)Mesh->BuildFromMeshData(MakeSphere(Segments, Rings));
	return Mesh;
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
