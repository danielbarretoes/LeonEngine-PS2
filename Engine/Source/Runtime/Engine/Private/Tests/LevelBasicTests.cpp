#include "CoreMinimal.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "Level/Light.h"
#include "Misc/AutomationTest.h"
#include "Tests/LegacyCoordinateConversion.h"
#include "Tests/ScopedTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLevelLightRotationRoundTripTest, "System.Engine.Level.LightRotationRoundTrip",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLevelLightRotationRoundTripTest::RunTest(const FString& Parameters)
{
	// A light rotation turns into a unit direction and back into the same pitch and yaw.
	FDirectionalLight Light;
	Light.Transform.SetRotation(FLegacyCoordinateConversion::ConvertLightRotation(45.0f, 90.0f));
	TestEqual("Direction is unit", Light.GetDirection().Size(), 1.0f, 1.0e-4f);
	float Pitch = 0.0f;
	float Yaw = 0.0f;
	FLegacyCoordinateConversion::ToLegacyLightRotation(Light.Transform.GetRotation(), Pitch, Yaw);
	TestEqual("Pitch round-trips", Pitch, 45.0f, 1.0e-2f);
	TestEqual("Yaw round-trips", Yaw, 90.0f, 1.0e-2f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLevelDirectionalLightGetDirectionMatchesTransformTest,
	"System.Engine.Level.DirectionalLightGetDirectionMatchesTransform",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLevelDirectionalLightGetDirectionMatchesTransformTest::RunTest(const FString& Parameters)
{
	// A directional light pitched down by its transform points downward, along its forward axis.
	FDirectionalLight Light;
	Light.Transform.SetRotation(FRotator(-30.0f, 45.0f, 0.0f).Quaternion());
	const FVector Dir = Light.GetDirection();
	TestTrue("Points down", Dir.Z < 0.0f);
	TestTrue("Forward axis", Dir.Equals(FRotator(-30.0f, 45.0f, 0.0f).Vector(), 1.0e-5f));
	// The default sun is the legacy pitch 60.3, yaw 142.1.
	TestTrue("Default sun",
		FDirectionalLight().GetDirection().Equals(
			FLegacyCoordinateConversion::ConvertLightRotation(60.3f, 142.1f).GetForwardVector(), 1.0e-5f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLevelStoresMeshesPlayerStartsAndTagsTest,
	"System.Engine.Level.StoresMeshesPlayerStartsAndTags",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLevelStoresMeshesPlayerStartsAndTagsTest::RunTest(const FString& Parameters)
{
	// The level keeps static mesh actors and player starts, finds actors by tag and empties on Clear.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AStaticMeshActor* Mesh = World.SpawnActor<AStaticMeshActor>(FVector(100.0f, 200.0f, 300.0f), FRotator::ZeroRotator);
	Mesh->Tags.Add(FName("player"));
	World.SpawnActor<APlayerStart>(FVector(500.0f, 0.0f, -200.0f), FRotator::ZeroRotator);

	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(World, AStaticMeshActor::StaticClass(), Found);
	TestEqual("One static mesh", Found.Num(), 1);
	UGameplayStatics::GetAllActorsWithTag(World, FName("player"), Found);
	TestTrue("Tag found", Found.Num() == 1 && Found[0] == Mesh);
	UGameplayStatics::GetAllActorsWithTag(World, FName("missing"), Found);
	TestEqual("Missing tag", Found.Num(), 0);
	UGameplayStatics::GetAllActorsOfClass(World, APlayerStart::StaticClass(), Found);
	if (!TestEqual("PlayerStart found", Found.Num(), 1))
	{
		return false;
	}
	TestEqual("PlayerStart X", Found[0]->GetActorLocation().X, 500.0f, 1.0e-3f);

	World.Clear();
	UGameplayStatics::GetAllActorsOfClass(World, AStaticMeshActor::StaticClass(), Found);
	TestEqual("No static meshes", Found.Num(), 0);
	UGameplayStatics::GetAllActorsOfClass(World, APlayerStart::StaticClass(), Found);
	TestEqual("No PlayerStarts", Found.Num(), 0);
	UGameplayStatics::GetAllActorsOfClass(World, ADirectionalLight::StaticClass(), Found);
	TestEqual("No directional lights", Found.Num(), 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
