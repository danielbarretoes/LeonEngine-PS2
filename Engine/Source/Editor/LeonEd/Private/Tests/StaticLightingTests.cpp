#include "Components/DirectionalLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "CoreMinimal.h"
#include "Engine/DirectionalLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "LPS2Mesh.h"
#include "MeshData.h"
#include "Misc/AutomationTest.h"
#include "Primitives.h"
#include "StaticLightingSystem.h"
#include "StaticMeshResources.h"
#include "Tests/ScopedTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

// LeonEd's static lighting (Docs/PLANS/ps2-shipping.md N22): a plane, a box on it and a sun. The box's shadow darkens
// the plane where the sun cannot reach it, the box occludes the sky near its foot, and the bake is the same bytes on
// every run.

namespace
{

	/** Plane cells a side, and their size (cm): a 1000 cm plane on the XY plane, facing +Z, centred on the origin. */
	constexpr int32 GridCells = 20;
	constexpr float CellSize = 50.0f;

	/**
	 * A plane of GridCells x GridCells quads (the vertex lighting needs vertices across the ground), closed by a quad
	 * 10 cm under it facing down: a slab, as the bake wants its ground.
	 */
	FMeshData MakeGrid()
	{
		FMeshData Data;
		const float Half = float(GridCells) * CellSize * 0.5f;
		for (int32 Y = 0; Y <= GridCells; ++Y)
		{
			for (int32 X = 0; X <= GridCells; ++X)
			{
				Data.Vertices.Add(FVertex(FVector((float(X) * CellSize) - Half, (float(Y) * CellSize) - Half, 0.0f),
					FVector(0.0f, 0.0f, 1.0f), FVector2D(float(X), float(Y))));
			}
		}
		for (int32 Y = 0; Y < GridCells; ++Y)
		{
			for (int32 X = 0; X < GridCells; ++X)
			{
				const uint32 A = uint32((Y * (GridCells + 1)) + X);
				const uint32 B = A + 1;
				const uint32 D = A + uint32(GridCells + 1);
				const uint32 C = D + 1;
				// Front face seen from above (+Z), as MakePlane winds it.
				Data.Indices.Append({A, C, B, A, D, C});
			}
		}
		const uint32 Bottom = uint32(Data.Vertices.Num());
		for (const FVector2D& Corner :
			{FVector2D(-1.0f, -1.0f), FVector2D(1.0f, -1.0f), FVector2D(1.0f, 1.0f), FVector2D(-1.0f, 1.0f)})
		{
			Data.Vertices.Add(FVertex(
				FVector(Corner.X * Half, Corner.Y * Half, -10.0f), FVector(0.0f, 0.0f, -1.0f), FVector2D(0.0f, 0.0f)));
		}
		Data.Indices.Append({Bottom, Bottom + 1, Bottom + 2, Bottom, Bottom + 2, Bottom + 3});
		return Data;
	}

	/** The test's scene: the grid, a 200 cm box standing on its middle, a sun shining down +X at 45 degrees. */
	struct FLitScene
	{
		UStaticMeshComponent* Plane = nullptr;
		UStaticMeshComponent* Box = nullptr;
		ADirectionalLight* Sun = nullptr;

		explicit FLitScene(UWorld& World)
		{
			UStaticMesh* GridMesh = NewObject<UStaticMesh>();
			(void)GridMesh->BuildFromMeshData(MakeGrid());
			UStaticMesh* BoxMesh = NewObject<UStaticMesh>();
			(void)BoxMesh->BuildFromMeshData(MakeCube());
			AStaticMeshActor* PlaneActor =
				World.SpawnActor<AStaticMeshActor>(FVector::ZeroVector, FRotator::ZeroRotator);
			Plane = PlaneActor->GetStaticMeshComponent();
			(void)Plane->SetStaticMesh(GridMesh);
			AStaticMeshActor* BoxActor =
				World.SpawnActor<AStaticMeshActor>(FVector(0.0f, 0.0f, 100.0f), FRotator::ZeroRotator);
			BoxActor->SetActorScale3D(FVector(2.0f));
			Box = BoxActor->GetStaticMeshComponent();
			(void)Box->SetStaticMesh(BoxMesh);
			Sun = World.SpawnActor<ADirectionalLight>(FVector::ZeroVector, FRotator(-45.0f, 0.0f, 0.0f));
		}
	};

	/**
	 * The baked colour of the component's vertex nearest the world point among those facing Normal (the component is
	 * not rotated: its normals are the world's).
	 */
	FColor BakedColorAt(
		const UStaticMeshComponent& Component, const FVector& WorldPoint, const FVector& Normal = FVector::UpVector)
	{
		const FLPS2Mesh& Mesh = Component.GetStaticMesh()->GetLODResources().RenderData;
		const FTransform Transform = Component.GetComponentTransform();
		float Best = BIG_NUMBER;
		FColor Found(0, 0, 0, 0);
		for (int32 BatchIndex = 0; BatchIndex < Mesh.GetNumBatches(); ++BatchIndex)
		{
			const FLPS2Batch& Batch = Mesh.GetBatch(BatchIndex);
			const uint8* Colors = Component.BakedVertexColors.GetBatchColors(Mesh, BatchIndex);
			for (int32 Index = 0; Index < int32(Batch.NumVertices); ++Index)
			{
				if ((FLPS2Mesh::DequantizeNormal(Mesh.GetNormals(Batch) + (Index * 4)) | Normal) < 0.9f)
				{
					continue;
				}
				const FVector Position =
					Transform.TransformPosition(Mesh.DequantizePosition(Mesh.GetPositions(Batch) + (Index * 3)));
				const float Distance = FVector::DistSquared(Position, WorldPoint);
				if (Distance < Best)
				{
					Best = Distance;
					const uint8* Color = Colors + (Index * 4);
					Found = FColor(Color[0], Color[1], Color[2], Color[3]);
				}
			}
		}
		return Found;
	}

	int32 Brightness(const FColor& Color)
	{
		return int32(Color.R) + int32(Color.G) + int32(Color.B);
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLeonEdStaticLightingShadowsTest, "System.LeonEd.StaticLighting.ShadowsAndOcclusion",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FLeonEdStaticLightingShadowsTest::RunTest(const FString& Parameters)
{
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	const FLitScene Scene(World);
	const FStaticLightingStats Stats = FStaticLightingSystem::Build(World);
	TestEqual("Both meshes baked", Stats.NumMeshes, 2);
	TestEqual("One light", Stats.NumLights, 1);
	TestEqual(
		"The occluders: the grid's and the box's triangles", Stats.NumOccluders, (GridCells * GridCells * 2) + 2 + 12);
	TestTrue("The plane's colours match its mesh", Scene.Plane->HasValidBakedVertexColors());
	TestTrue("The box's colours match its mesh", Scene.Box->HasValidBakedVertexColors());

	// The sun travels +X and down: the box's shadow lies on its +X side, 200 cm long (its height).
	const FColor Shadowed = BakedColorAt(*Scene.Plane, FVector(200.0f, 0.0f, 0.0f));
	const FColor LitNearBox = BakedColorAt(*Scene.Plane, FVector(-200.0f, 0.0f, 0.0f));
	const FColor LitFar = BakedColorAt(*Scene.Plane, FVector(450.0f, 450.0f, 0.0f));
	UE_LOG(LogTemp, Display, "%s",
		*FString::Printf("Static lighting: shadowed %d %d %d, lit by the box %d %d %d, lit far %d %d %d", Shadowed.R,
			Shadowed.G, Shadowed.B, LitNearBox.R, LitNearBox.G, LitNearBox.B, LitFar.R, LitFar.G, LitFar.B));
	TestTrue("The shadow is darker than the sunlit ground beside the box",
		Brightness(Shadowed) + 150 < Brightness(LitNearBox));
	TestTrue("The shadow is darker than the open ground", Brightness(Shadowed) + 150 < Brightness(LitFar));
	// Open ground: the whole sky (0.78, 0.85, 1.0) x 0.35 and the sun at 45 degrees (0.707), within a step.
	const FVector Expected = FVector(0.78f, 0.85f, 1.0f) * 0.35f + FVector(0.7071f, 0.7071f, 0.7071f);
	TestTrue("Open ground, red", FMath::Abs(int32(LitFar.R) - FMath::RoundToInt(Expected.X * 255.0f)) <= 1);
	TestTrue("Open ground, blue",
		FMath::Abs(int32(LitFar.B) - FMath::Min(FMath::RoundToInt(Expected.Z * 255.0f), 255)) <= 1);
	TestEqual("The mesh's alpha", int32(LitFar.A), 255);
	// The box's sunlit top against its face in its own shadow (-X faces the sun, +X is away from it).
	TestTrue("The box's face away from the sun is darker than the one toward it",
		Brightness(BakedColorAt(*Scene.Box, FVector(100.0f, 100.0f, 200.0f), FVector::ForwardVector)) + 150 <
			Brightness(BakedColorAt(*Scene.Box, FVector(-100.0f, 100.0f, 200.0f), -FVector::ForwardVector)));

	// The sky alone: the box occludes the ground at its foot, and the ground occludes the foot of its faces.
	Scene.Sun->GetLightComponent()->SetIntensity(0.0f);
	(void)FStaticLightingSystem::Build(World);
	const FColor AtFoot = BakedColorAt(*Scene.Plane, FVector(-150.0f, 0.0f, 0.0f));
	const FColor Open = BakedColorAt(*Scene.Plane, FVector(-450.0f, -450.0f, 0.0f));
	TestTrue("The ground at the box's foot is occluded", Brightness(AtFoot) + 20 < Brightness(Open));
	TestTrue("The corner where the box meets the ground is darker than the top of its side",
		Brightness(BakedColorAt(*Scene.Box, FVector(-100.0f, -100.0f, 0.0f), -FVector::ForwardVector)) + 20 <
			Brightness(BakedColorAt(*Scene.Box, FVector(-100.0f, -100.0f, 200.0f), -FVector::ForwardVector)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLeonEdStaticLightingDeterministicTest, "System.LeonEd.StaticLighting.Deterministic",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FLeonEdStaticLightingDeterministicTest::RunTest(const FString& Parameters)
{
	// The same scene bakes the same bytes: twice in one world, and in another world built the same way.
	TArray<uint8> First;
	TArray<uint8> Second;
	TArray<uint8> Other;
	{
		FScopedTestWorld TestWorld;
		const FLitScene Scene(*TestWorld);
		(void)FStaticLightingSystem::Build(*TestWorld);
		First = Scene.Plane->BakedVertexColors.Data;
		First.Append(Scene.Box->BakedVertexColors.Data);
		(void)FStaticLightingSystem::Build(*TestWorld);
		Second = Scene.Plane->BakedVertexColors.Data;
		Second.Append(Scene.Box->BakedVertexColors.Data);
	}
	{
		FScopedTestWorld TestWorld;
		const FLitScene Scene(*TestWorld);
		(void)FStaticLightingSystem::Build(*TestWorld);
		Other = Scene.Plane->BakedVertexColors.Data;
		Other.Append(Scene.Box->BakedVertexColors.Data);
	}
	TestTrue("Colours baked", First.Num() > 0);
	TestTrue("A second bake, the same bytes", First == Second);
	TestTrue("Another world, the same bytes", First == Other);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
