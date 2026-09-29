#include "CoreMinimal.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Materials/Material.h"
#include "MeshData.h"
#include "Misc/AutomationTest.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Physics/PhysScene.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "Tests/ScopedTestWorld.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

// ps2-shipping N30f: the physical materials (UE's UPhysicalMaterial, EPhysicalSurface, UMaterial::PhysMaterial) and
// what the collision queries report of them (FHitResult::PhysMaterial, FaceIndex).

namespace
{

	/** A material whose physical material is of Surface. */
	UMaterial* MakeSurfaceMaterial(EPhysicalSurface Surface)
	{
		UMaterial* Material = NewObject<UMaterial>(GetTransientPackage());
		UPhysicalMaterial* PhysMaterial = NewObject<UPhysicalMaterial>(GetTransientPackage());
		PhysMaterial->SurfaceType = Surface;
		Material->PhysMaterial = PhysMaterial;
		return Material;
	}

	/**
	 * Two floor quads side by side at Z = 50 cm: x in [-200, 0] is the first section (slot 1), x in [0, 200] the
	 * second (slot 0), so the slots are not the sections' order.
	 */
	UStaticMesh* MakeTwoSlotFloor()
	{
		FMeshData Data;
		const FVector Up(0.0f, 0.0f, 1.0f);
		for (const float X0 : {-200.0f, 0.0f})
		{
			const float X1 = X0 + 200.0f;
			Data.Vertices.Add(FVertex(FVector(X0, -200.0f, 50.0f), Up, FVector2D(0.0f, 0.0f)));
			Data.Vertices.Add(FVertex(FVector(X1, -200.0f, 50.0f), Up, FVector2D(1.0f, 0.0f)));
			Data.Vertices.Add(FVertex(FVector(X1, 200.0f, 50.0f), Up, FVector2D(1.0f, 1.0f)));
			Data.Vertices.Add(FVertex(FVector(X0, 200.0f, 50.0f), Up, FVector2D(0.0f, 1.0f)));
		}
		Data.Indices = {0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7};
		Data.Submeshes.Add(FMeshSection{0, 6, 1});
		Data.Submeshes.Add(FMeshSection{6, 6, 0});
		UStaticMesh* Mesh = NewObject<UStaticMesh>();
		(void)Mesh->BuildFromMeshData(Data);
		return Mesh;
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhysicalMaterialTraceSurfaceTest, "System.Engine.PhysicalMaterial.TraceSurface",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FPhysicalMaterialTraceSurfaceTest::RunTest(const FString& Parameters)
{
	// A trace that asks for the physical material gets the one of the hit triangle's material (its slot, from the
	// collision triangles' MaterialIndices), and of the first material on a simple shape; one that does not ask, none.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	UStaticMesh* Mesh = MakeTwoSlotFloor();
	const TArray<uint16>& Slots = Mesh->GetPhysicsTriMeshData().MaterialIndices;
	TestTrue("A slot a triangle", Slots.Num() == 4 && Slots[0] == 1 && Slots[1] == 1 && Slots[2] == 0 && Slots[3] == 0);

	UStaticMeshComponent& Component = *World.SpawnActor<AStaticMeshActor>()->GetStaticMeshComponent();
	(void)Component.SetStaticMesh(Mesh);
	UMaterial* Wood = MakeSurfaceMaterial(SurfaceType4);
	UMaterial* Metal = MakeSurfaceMaterial(SurfaceType3);
	Component.SetMaterial(0, Wood);
	Component.SetMaterial(1, Metal);
	Component.SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	const FPhysScene& Scene = World.GetPhysicsScene();
	TestTrue("Its triangles", Scene.GetBodies()[0].CollisionShape == EBodyCollisionShape::TriangleMesh);

	FCollisionQueryParams Params;
	Params.bReturnPhysicalMaterial = true;
	FHitResult Hit;
	TestTrue("Over the first section",
		Scene.LineTraceSingleByChannel(
			Hit, FVector(-100.0f, 0.0f, 300.0f), FVector(-100.0f, 0.0f, -100.0f), ECC_WorldStatic, Params));
	TestTrue("A triangle of slot 1", Hit.FaceIndex == 0 || Hit.FaceIndex == 1);
	TestTrue("Slot 1's material's", Hit.PhysMaterial.Get() == Metal->PhysMaterial);
	TestEqual("Metal", UPhysicalMaterial::DetermineSurfaceType(Hit.PhysMaterial.Get()), SurfaceType3);
	int32 Section = INDEX_NONE;
	TestTrue("The component's face material",
		Component.GetMaterialFromCollisionFaceIndex(Hit.FaceIndex, Section) == Metal && Section == 1);

	TArray<FHitResult> Hits;
	TestTrue("Over the second section (a multi trace)",
		Scene.LineTraceMultiByChannel(
			Hits, FVector(100.0f, 0.0f, 300.0f), FVector(100.0f, 0.0f, -100.0f), ECC_WorldStatic, Params));
	TestTrue("Slot 0: wood",
		Hits.Num() == 1 && UPhysicalMaterial::DetermineSurfaceType(Hits[0].PhysMaterial.Get()) == SurfaceType4);

	TestTrue("Not asked for",
		Scene.LineTraceSingleByChannel(
			Hit, FVector(100.0f, 0.0f, 300.0f), FVector(100.0f, 0.0f, -100.0f), ECC_WorldStatic));
	TestTrue("None without asking", !Hit.PhysMaterial.IsValid() && Hit.FaceIndex != INDEX_NONE);
	TestEqual("None is the default surface", UPhysicalMaterial::DetermineSurfaceType(nullptr), SurfaceType_Default);

	// The simple shape (UCX_ boxes answer the traces): the first material's.
	Mesh->GetBodySetup()->CollisionTraceFlag = CTF_UseSimpleAsComplex;
	Component.RecreatePhysicsState();
	TestTrue("A box now", Scene.GetBodies()[0].CollisionShape == EBodyCollisionShape::Box);
	TestTrue("The box",
		Scene.LineTraceSingleByChannel(
			Hit, FVector(-100.0f, 0.0f, 300.0f), FVector(-100.0f, 0.0f, -100.0f), ECC_WorldStatic, Params));
	TestTrue("No triangle, the first material's",
		Hit.FaceIndex == INDEX_NONE && Hit.PhysMaterial.Get() == Wood->PhysMaterial);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhysicalMaterialSurfaceNamesTest, "System.Engine.PhysicalMaterial.SurfaceNames",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FPhysicalMaterialSurfaceNamesTest::RunTest(const FString& Parameters)
{
	// The Engine config names the surface types (UPhysicsSettings, `+PhysicalSurfaces=(Type=SurfaceType1,Name=...)`):
	// a name finds its type, and so do an enumerator and Default.
	UPhysicsSettings& Settings = *NewObject<UPhysicsSettings>(GetTransientPackage());
	FPhysicalSurfaceName Concrete;
	Concrete.Type = SurfaceType1;
	Concrete.Name = FName(TEXT("Concrete"));
	Settings.PhysicalSurfaces.Add(Concrete);
	EPhysicalSurface Surface = SurfaceType_Max;
	TestTrue("A name", Settings.FindSurfaceType(TEXT("Concrete"), Surface) && Surface == SurfaceType1);
	TestTrue("An enumerator", Settings.FindSurfaceType(TEXT("SurfaceType12"), Surface) && Surface == SurfaceType12);
	TestTrue("Default", Settings.FindSurfaceType(TEXT("Default"), Surface) && Surface == SurfaceType_Default);
	TestFalse("An unknown name", Settings.FindSurfaceType(TEXT("Lava"), Surface));
	TestFalse("Past the last type", Settings.FindSurfaceType(TEXT("SurfaceType63"), Surface));
	TestEqual("Its name", Settings.GetSurfaceName(SurfaceType1), FName(TEXT("Concrete")));
	TestEqual("A type the config does not name", Settings.GetSurfaceName(SurfaceType2), FName(NAME_None));
	TestTrue("The engine's settings exist", UPhysicsSettings::Get() != nullptr);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
