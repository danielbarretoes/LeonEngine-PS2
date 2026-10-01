#include "Components/StaticMeshComponent.h"
#include "CoreMinimal.h"
#include "Engine/StaticMesh.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "StaticMeshResources.h"
#include "UObject/Package.h"
#include "UObject/UObjectIterator.h"

#if WITH_DEV_AUTOMATION_TESTS

// The project's content as the PS2 draws it.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterContentMeshQuantizationTest, "ShooterGame.Content.MeshQuantization",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterContentMeshQuantizationTest::RunTest(const FString& Parameters)
{
	// Every static mesh of the project (the maps', the weapons', the characters'): LPS2 v2 spreads each axis of a mesh
	// over int16, so a drawn position is at most half a step, and at most 0.5 cm, from the source's (the collision
	// triangles keep the source's positions: every drawn vertex must be that close to one of them). The maps' actors
	// scale their meshes, and the error with them: in the world too, at most 0.5 cm.
	constexpr float MaxError = 0.5f;
	TArray<FString> Files;
	IFileManager::Get().FindFilesRecursive(Files, *FPaths::ProjectContentDir(), TEXT("SM_*.lasset"), true, false);
	Files.Sort();
	TestTrue("The project's meshes", Files.Num() >= 10);
	for (const FString& File : Files)
	{
		const FString PackageName = FPackageName::FilenameToLongPackageName(File);
		const UStaticMesh* Mesh =
			LoadObject<UStaticMesh>(nullptr, *(PackageName + TEXT(".") + FPaths::GetBaseFilename(File)));
		if (!TestNotNull(*PackageName, Mesh))
		{
			continue;
		}
		const FLPS2Mesh& RenderData = Mesh->GetLODResources().RenderData;
		const FLPS2MeshHeader& Header = RenderData.GetHeader();
		const float HalfStep =
			FMath::Max3(Header.PositionScale[0], Header.PositionScale[1], Header.PositionScale[2]) * 0.5f;
		const TArray<FVector>& Source = Mesh->GetPhysicsTriMeshData().Vertices;
		float Worst = 0.0f;
		for (int32 BatchIndex = 0; BatchIndex < RenderData.GetNumBatches(); ++BatchIndex)
		{
			const FLPS2Batch& Batch = RenderData.GetBatch(BatchIndex);
			const int16* Positions = RenderData.GetPositions(Batch);
			for (uint32 Vertex = 0; Vertex < Batch.NumVertices; ++Vertex)
			{
				const FVector Drawn = RenderData.DequantizePosition(Positions + (Vertex * 3));
				float Nearest = TNumericLimits<float>::Max();
				for (const FVector& Point : Source)
				{
					const FVector Delta = (Drawn - Point).GetAbs();
					Nearest = FMath::Min(Nearest, FMath::Max3(Delta.X, Delta.Y, Delta.Z));
				}
				Worst = FMath::Max(Worst, Nearest);
			}
		}
		UE_LOG(LogTemp, Display, "%s",
			*FString::Printf("%s: half a step %.4f cm, worst %.4f cm (%d triangles, %d strip vertices, %d batches)",
				*PackageName, double(HalfStep), double(Worst), RenderData.GetNumTriangles(),
				RenderData.GetNumVertices(), RenderData.GetNumBatches()));
		TestTrue(
			*FString::Printf("%s: half a step within %.1f cm", *PackageName, double(MaxError)), HalfStep <= MaxError);
		TestTrue(*FString::Printf("%s: every vertex within %.1f cm", *PackageName, double(MaxError)),
			Worst <= FMath::Min(MaxError, HalfStep + 0.001f));
	}

	// The maps' placed meshes (de_leon's, de_harbor's): in the world too, at most 0.5 cm.
	for (const TCHAR* MapName : {TEXT("/Game/Maps/de_leon"), TEXT("/Game/Maps/de_harbor")})
	{
		UPackage* Map = LoadPackage(nullptr, MapName, LOAD_None);
		if (!TestNotNull(MapName, Map))
		{
			continue;
		}
		int32 NumPlaced = 0;
		float WorstPlaced = 0.0f;
		for (TObjectIterator<UStaticMeshComponent> It; It; ++It)
		{
			if (It->GetOutermost() != Map || It->GetStaticMesh() == nullptr)
			{
				continue;
			}
			const FLPS2MeshHeader& Header = It->GetStaticMesh()->GetLODResources().RenderData.GetHeader();
			const FVector Scale = It->GetComponentTransform().GetScale3D().GetAbs();
			// Half a step on each axis, through the component's scale (a rotation does not lengthen the error vector).
			const float Error = 0.5f *
				FMath::Sqrt(FMath::Square(Scale.X * Header.PositionScale[0]) +
					FMath::Square(Scale.Y * Header.PositionScale[1]) +
					FMath::Square(Scale.Z * Header.PositionScale[2]));
			WorstPlaced = FMath::Max(WorstPlaced, Error);
			++NumPlaced;
		}
		UE_LOG(LogTemp, Display, "%s",
			*FString::Printf("%s: %d placed meshes, worst quantization in the world %.4f cm", MapName, NumPlaced,
				double(WorstPlaced)));
		TestTrue(*FString::Printf("%s places meshes", MapName), NumPlaced > 0);
		TestTrue(
			*FString::Printf("%s's placed meshes within %.1f cm", MapName, double(MaxError)), WorstPlaced <= MaxError);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
