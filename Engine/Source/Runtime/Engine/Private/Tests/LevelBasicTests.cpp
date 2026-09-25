#include "CoreMinimal.h"
#include "Engine/Level.h"
#include "LegacyCoordinateConversion.h"
#include "Level/BasicLight.h"
#include "Level/BasicShape.h"
#include "Level/Light.h"
#include "Misc/AutomationTest.h"

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLevelTryParseBasicShapeNameIsCaseInsensitiveTest,
	"System.Engine.Level.TryParseBasicShapeNameIsCaseInsensitive",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLevelTryParseBasicShapeNameIsCaseInsensitiveTest::RunTest(const FString& Parameters)
{
	EBasicShape Shape{};
	TestTrue("Cube parsed", TryParseBasicShapeName("Cube", Shape));
	TestTrue("Cube type", Shape == EBasicShape::Cube);
	TestTrue("sphere parsed", TryParseBasicShapeName("sphere", Shape));
	TestTrue("Sphere type", Shape == EBasicShape::Sphere);
	TestTrue("PLANE parsed", TryParseBasicShapeName("PLANE", Shape));
	TestTrue("Plane type", Shape == EBasicShape::Plane);
	TestFalse("Unknown shape rejected", TryParseBasicShapeName("Octahedron", Shape));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLevelBlockingVolumeAndPlayerStartNameHelpersTest,
	"System.Engine.Level.BlockingVolumeAndPlayerStartNameHelpers",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLevelBlockingVolumeAndPlayerStartNameHelpersTest::RunTest(const FString& Parameters)
{
	// The BlockingVolume name matches in any case; PlayerStart only matches its own name.
	TestTrue("BlockingVolume", IsBlockingVolumeName("BlockingVolume"));
	TestTrue("blockingvolume", IsBlockingVolumeName("blockingvolume"));
	TestTrue("PlayerStart", IsPlayerStartName("PlayerStart"));
	TestFalse("Cube is not a PlayerStart", IsPlayerStartName("Cube"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLevelBasicShapeFactoriesSetTypeAndPlaneScaleTest,
	"System.Engine.Level.BasicShapeFactoriesSetTypeAndPlaneScale",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLevelBasicShapeFactoriesSetTypeAndPlaneScaleTest::RunTest(const FString& Parameters)
{
	// The factories set the shape type, and the plane size becomes its XY scale.
	const FBasicShape Cube = FBasicShape::Cube();
	TestTrue("Cube type", Cube.Type == EBasicShape::Cube);
	const FBasicShape Plane = FBasicShape::Plane(4.0f);
	TestTrue("Plane type", Plane.Type == EBasicShape::Plane);
	TestEqual("Plane scale X", Plane.Transform.GetScale3D().X, 4.0f, 1.0e-5f);
	TestEqual("Plane scale Y", Plane.Transform.GetScale3D().Y, 4.0f, 1.0e-5f);
	TestEqual("Plane scale Z", Plane.Transform.GetScale3D().Z, 1.0f, 1.0e-5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLevelBasicLightParseAndAddToLevelTest,
	"System.Engine.Level.BasicLightParseAndAddToLevel",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLevelBasicLightParseAndAddToLevelTest::RunTest(const FString& Parameters)
{
	// Light names parse to their type, and AddTo appends to the matching level light list.
	EBasicLight Type{};
	TestTrue("DirectionalLight parsed", TryParseBasicLightName("DirectionalLight", Type));
	TestTrue("Directional type", Type == EBasicLight::Directional);
	TestTrue("PointLight parsed", TryParseBasicLightName("PointLight", Type));
	TestTrue("Point type", Type == EBasicLight::Point);

	ULevel& Level = *NewObject<ULevel>();
	Level.ClearLights();
	TestEqual("No directional lights", Level.GetDirectionalLights().Num(), 0);

	FBasicLight::Directional().AddTo(Level);
	FBasicLight::Point().AddTo(Level);
	TestEqual("One directional light", Level.GetDirectionalLights().Num(), 1);
	TestEqual("One point light", Level.GetPointLights().Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLevelStoresMeshesPlayerStartsAndTagsTest,
	"System.Engine.Level.StoresMeshesPlayerStartsAndTags",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLevelStoresMeshesPlayerStartsAndTagsTest::RunTest(const FString& Parameters)
{
	// The level keeps static meshes and PlayerStarts, finds meshes by tag and empties on Clear.
	ULevel& Level = *NewObject<ULevel>();
	FLevelStaticMesh Mesh{};
	Mesh.Tag = "player";
	Mesh.Transform.SetLocation(FVector(100.0f, 200.0f, 300.0f));
	Level.AddStaticMesh(MoveTemp(Mesh));

	FPlayerStart Start{};
	Start.Transform.SetLocation(FVector(500.0f, 0.0f, -200.0f));
	Level.AddPlayerStart(Start);

	TestEqual("One static mesh", Level.GetStaticMeshes().Num(), 1);
	TestEqual("Tag found", Level.FindStaticMeshIndexByTag("player"), static_cast<SIZE_T>(0));
	TestEqual("Missing tag", Level.FindStaticMeshIndexByTag("missing"), ULevel::Npos);
	const FPlayerStart* Found = Level.FindPlayerStart();
	if (!TestNotNull("PlayerStart found", Found))
	{
		return false;
	}
	TestEqual("PlayerStart X", Found->Transform.GetLocation().X, 500.0f, 1.0e-3f);

	Level.Clear();
	TestEqual("No static meshes", Level.GetStaticMeshes().Num(), 0);
	TestEqual("No PlayerStarts", Level.GetPlayerStarts().Num(), 0);
	TestEqual("No directional lights", Level.GetDirectionalLights().Num(), 0);
	TestNull("No PlayerStart after Clear", Level.FindPlayerStart());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
