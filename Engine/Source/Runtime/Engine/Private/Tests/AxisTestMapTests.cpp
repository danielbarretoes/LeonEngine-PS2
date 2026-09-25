#include "Camera/CameraComponent.h"
#include "CoreMinimal.h"
#include "Engine/DirectionalLight.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAxisTestMapNoMirroringTest, "System.Engine.AxisTestMap.NoMirroring",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FAxisTestMapNoMirroringTest::RunTest(const FString& Parameters)
{
	// /Engine/Maps/AxisTest, imported from Engine/SourceArt/Maps/AxisTest.glb (MakeAxisTest.py): glTF's right-handed
	// Y-up metres land in the engine's X forward, Y right, Z up, left-handed centimetres. A glTF node at (1, 0, 0) m is
	// at (100, 0, 0) cm, glTF +Y (up) is +Z and glTF +Z is +Y; from the start, looking along +X with +Z up, the +X cube
	// is ahead, the +Y cube on the right of the view and the +Z cube above it.
	const TCHAR* const MapName = TEXT("/Engine/Maps/AxisTest");
	if (!FPackageName::DoesPackageExist(MapName))
	{
		AddError(TEXT("/Engine/Maps/AxisTest.lmap is missing (import Engine/SourceArt/ImportList.ini)"));
		return false;
	}
	UPackage* Package = LoadPackage(nullptr, MapName, LOAD_None);
	UWorld* World = UWorld::FindWorldInPackage(Package);
	if (!TestNotNull("The map's world", World))
	{
		return false;
	}
	const auto Find = [World](const TCHAR* Name)
	{ return Cast<AActor>(StaticFindObjectFast(nullptr, World->PersistentLevel, FName(Name))); };
	const AActor* Red = Find(TEXT("AxisX_Red"));
	const AActor* Green = Find(TEXT("AxisY_Green"));
	const AActor* Blue = Find(TEXT("AxisZ_Blue"));
	const AActor* Marker = Find(TEXT("Marker_1m"));
	const APlayerStart* Start = Cast<APlayerStart>(Find(TEXT("PlayerStart")));
	const ADirectionalLight* Sun = Cast<ADirectionalLight>(Find(TEXT("Sun")));
	if (!TestNotNull("Red", Red) || !TestNotNull("Green", Green) || !TestNotNull("Blue", Blue) ||
		!TestNotNull("Marker", Marker) || !TestNotNull("Start", Start) || !TestNotNull("Sun", Sun))
	{
		return false;
	}

	// Units x 100, axes (x, z, y).
	TestTrue("1 m along glTF +X is 100 cm along +X",
		Marker->GetActorLocation().Equals(FVector(100.0f, 0.0f, 0.0f), 1.0e-3f));
	TestTrue("glTF (3, 0.5, 0) m", Red->GetActorLocation().Equals(FVector(300.0f, 0.0f, 50.0f), 1.0e-3f));
	TestTrue("glTF +Z is +Y", Green->GetActorLocation().Equals(FVector(0.0f, 200.0f, 50.0f), 1.0e-3f));
	TestTrue("glTF +Y (up) is +Z", Blue->GetActorLocation().Equals(FVector(0.0f, 0.0f, 250.0f), 1.0e-3f));
	TestTrue("The start", Start->GetActorLocation().Equals(FVector(-600.0f, 0.0f, 170.0f), 1.0e-3f));
	TestEqual("The start faces +X", Start->GetActorRotation().Yaw, 0.0f, 1.0e-3f);
	const FVector SunDirection = Sun->GetLightComponent()->GetDirection();
	TestTrue("The sun shines down, along +X and +Y",
		SunDirection.X > 0.0f && SunDirection.Y > 0.0f && SunDirection.Z < 0.0f);

	// The view from the start: UE view space is x right, y up, z forward (ViewMatrices.h).
	TStrongObjectPtr<UCameraComponent> Camera(NewObject<UCameraComponent>());
	Camera->SetMode(ECameraMode::FreeLook);
	Camera->SetEyeLocation(Start->GetActorLocation());
	Camera->SetViewRotation(Start->GetActorRotation());
	const FMatrix View = Camera->ViewMatrix();
	const FVector4 RedView = View.TransformPosition(Red->GetActorLocation());
	const FVector4 GreenView = View.TransformPosition(Green->GetActorLocation());
	const FVector4 BlueView = View.TransformPosition(Blue->GetActorLocation());
	TestTrue("+X is ahead, centred", RedView.Z > 0.0f && FMath::Abs(RedView.X) < 1.0e-2f);
	TestTrue("+Y is on the right (not mirrored)", GreenView.Z > 0.0f && GreenView.X > 0.0f);
	TestTrue("+Z is up", BlueView.Z > 0.0f && BlueView.Y > 0.0f && FMath::Abs(BlueView.X) < 1.0e-2f);

	Package->MarkPendingKill();
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
