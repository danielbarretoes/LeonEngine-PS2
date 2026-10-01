#include "Components/StaticMeshComponent.h"
#include "CoreMinimal.h"
#include "Engine/Level.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "MapOverview.h"
#include "Materials/Material.h"
#include "Misc/AutomationTest.h"
#include "PixelFormat.h"
#include "Primitives.h"
#include "Tests/ScopedTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

// LeonEd's map overview (Docs/PLANS/ps2-polish.md P7): a floor with a low red block, a tall tower and a roof on
// pillars, seen from above. The overview's square holds the map, the world-to-overview projection puts the block where
// the render drew it (north up, east right), what stands through the solid height is an obstacle, what is above the
// clip height is cut away (the floor under the roof shows), and the same world gives the same bytes.

namespace
{

	/**
	 * A floor 2000 cm square (a flat box, its top at Z = 0); a red block 200 cm square and 60 cm tall at (500, 300); a
	 * blue tower 200 cm square and 600 cm tall at (-500, -500); a green roof 400 cm square from 400 to 420 cm over
	 * (500, -500), on nothing (what matters is what is under it).
	 */
	struct FOverviewScene
	{
		explicit FOverviewScene(UWorld& World)
		{
			World.PersistentLevel->SetWorldSettings(World.SpawnActor<AWorldSettings>());
			UStaticMesh* Cube = NewObject<UStaticMesh>();
			(void)Cube->BuildFromMeshData(MakeCube());
			const auto Spawn = [&World, Cube](const FVector& Location, const FVector& Scale, const FLinearColor& Color)
			{
				AStaticMeshActor* Actor = World.SpawnActor<AStaticMeshActor>(Location, FRotator::ZeroRotator);
				UStaticMeshComponent* Component = Actor->GetStaticMeshComponent();
				(void)Component->SetStaticMesh(Cube);
				UMaterial* Material = NewObject<UMaterial>();
				Material->BaseColor = Color;
				Component->SetMaterial(0, Material);
				Actor->SetActorScale3D(Scale);
			};
			// MakeCube is 100 cm on a side, centred.
			Spawn(FVector(0.0f, 0.0f, -10.0f), FVector(20.0f, 20.0f, 0.2f), FLinearColor(0.5f, 0.5f, 0.5f));
			Spawn(FVector(500.0f, 300.0f, 30.0f), FVector(2.0f, 2.0f, 0.6f), FLinearColor(0.9f, 0.1f, 0.1f));
			Spawn(FVector(-500.0f, -500.0f, 300.0f), FVector(2.0f, 2.0f, 6.0f), FLinearColor(0.1f, 0.1f, 0.9f));
			Spawn(FVector(500.0f, -500.0f, 410.0f), FVector(4.0f, 4.0f, 0.2f), FLinearColor(0.1f, 0.9f, 0.1f));
			World.SendAllEndOfFrameUpdates();
		}
	};

	/** The rendered colour at UV (V from the top) of Resolution x Resolution pixels, the top row first. */
	FColor PixelAt(const TArray<FColor>& Pixels, int32 Resolution, const FVector2D& UV)
	{
		const int32 X = FMath::Clamp(int32(UV.X * float(Resolution)), 0, Resolution - 1);
		const int32 Y = FMath::Clamp(int32(UV.Y * float(Resolution)), 0, Resolution - 1);
		return Pixels[(Y * Resolution) + X];
	}

	FString Describe(const TCHAR* What, const FColor& Color)
	{
		return FString::Printf("%s (%d, %d, %d)", What, Color.R, Color.G, Color.B);
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLeonEdMapOverviewTest, "System.LeonEd.MapOverview.RenderAndProject",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FLeonEdMapOverviewTest::RunTest(const FString& Parameters)
{
	TArray<uint8> FirstBytes;
	for (int32 Run = 0; Run < 2; ++Run)
	{
		FScopedTestWorld TestWorld;
		UWorld& World = *TestWorld;
		const FOverviewScene Scene(World);
		FMapOverviewSettings Settings;
		Settings.Margin = 100.0f;
		UTexture2D* Overview = NewObject<UTexture2D>();
		const AWorldSettings* WorldSettings = World.GetWorldSettings();
		if (!TestNotNull("The world's settings", WorldSettings) ||
			!TestTrue("Built", FMapOverview::Build(World, Settings, *Overview)))
		{
			return false;
		}
		const FWorldOverviewSettings& Kept = WorldSettings->OverviewSettings;
		TestTrue("128 x 128, PF_P8, one level",
			Overview->GetSizeX() == 128 && Overview->GetSizeY() == 128 && Overview->GetPixelFormat() == PF_P8 &&
				Overview->GetNumMips() == 1);
		TestTrue("Kept on the world's settings", Kept.Texture == Overview && Kept.IsValid());
		TestTrue("Its square: the floor's 2000 cm and the margins, centred",
			FMath::IsNearlyEqual(Kept.Size, 2200.0f, 1.0f) && Kept.Center.Equals(FVector2D::ZeroVector, 1.0f));
		// The projection: the square's centre and corners (north-west is the top left), a point outside it.
		TestTrue("The centre", Kept.GetUV(FVector(0.0f, 0.0f, 0.0f)).Equals(FVector2D(0.5f, 0.5f)));
		TestTrue("North-west: the top left",
			Kept.GetUV(FVector(1100.0f, -1100.0f, 0.0f)).Equals(FVector2D(0.0f, 0.0f), 1.0e-4f));
		TestTrue("South-east: the bottom right",
			Kept.GetUV(FVector(-1100.0f, 1100.0f, 0.0f)).Equals(FVector2D(1.0f, 1.0f), 1.0e-4f));
		TestTrue("North-east: the top right",
			Kept.GetUV(FVector(1100.0f, 1100.0f, 50.0f)).Equals(FVector2D(1.0f, 0.0f), 1.0e-4f));
		TestTrue("Outside: past 1", Kept.GetUV(FVector(-2000.0f, 0.0f, 0.0f)).Y > 1.0f);

		// The render agrees with the projection: the low block red where it projects, the floor grey, the margin
		// dark, the tower an obstacle's grey (not its blue top: cut away), the floor under the roof (not its green).
		TArray<FColor> Pixels;
		if (!TestTrue(
				"Rendered", FMapOverview::Render(World, Settings, Kept.Center, Kept.Size, -20.0f, 600.0f, Pixels)))
		{
			return false;
		}
		const int32 Resolution = Settings.Resolution;
		const FColor Block = PixelAt(Pixels, Resolution, Kept.GetUV(FVector(500.0f, 300.0f, 0.0f)));
		const FColor Floor = PixelAt(Pixels, Resolution, Kept.GetUV(FVector(-300.0f, 400.0f, 0.0f)));
		const FColor Margin = PixelAt(Pixels, Resolution, FVector2D(0.01f, 0.01f));
		const FColor Tower = PixelAt(Pixels, Resolution, Kept.GetUV(FVector(-500.0f, -500.0f, 0.0f)));
		const FColor UnderRoof = PixelAt(Pixels, Resolution, Kept.GetUV(FVector(500.0f, -500.0f, 0.0f)));
		TestTrue(*Describe(TEXT("The block red"), Block), Block.R > Block.G + 40 && Block.R > Block.B + 40);
		TestTrue(*Describe(TEXT("The floor grey and light"), Floor),
			Floor.R > 100 && FMath::Abs(int32(Floor.R) - int32(Floor.B)) < 30);
		TestTrue(*Describe(TEXT("The margin dark"), Margin), Margin.R < 40 && Margin.G < 40 && Margin.B < 40);
		TestTrue(*Describe(TEXT("The tower an obstacle: dark grey, not its blue top"), Tower),
			Tower.R > 40 && Tower.R < 90 && int32(Tower.B) < int32(Tower.R) + 20);
		TestTrue(*Describe(TEXT("Under the roof: the floor, not the roof's green"), UnderRoof),
			UnderRoof.R > 100 && FMath::Abs(int32(UnderRoof.G) - int32(UnderRoof.R)) < 30);

		const FByteBulkData& Data = Overview->GetPlatformData().Mips[0].BulkData;
		TArray<uint8> Bytes;
		Bytes.Append(static_cast<const uint8*>(Data.LockReadOnly()), int32(Data.GetBulkDataSize()));
		Data.Unlock();
		if (Run == 0)
		{
			FirstBytes = MoveTemp(Bytes);
		}
		else
		{
			TestTrue("The same bytes again (deterministic)", Bytes == FirstBytes);
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
