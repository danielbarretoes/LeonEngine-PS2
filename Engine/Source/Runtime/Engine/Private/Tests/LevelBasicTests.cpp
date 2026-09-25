#include "CoreMinimal.h"
#include "Engine/Level.h"
#include "Level/BasicLight.h"
#include "Level/BasicShape.h"
#include "Level/Light.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLevelLightDirectionFromRotationRoundTripTest,
	"System.Engine.Level.LightDirectionFromRotationRoundTrip",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLevelLightDirectionFromRotationRoundTripTest::RunTest(const FString& Parameters)
{
	// A light rotation turns into a unit direction and back into the same pitch and yaw.
	const FVector Rot = FVector(45.0f, 90.0f, 0.0f);
	const FVector Dir = LightDirectionFromRotation(Rot);
	TestEqual("Direction is unit", Dir.Size(), 1.0f, 1.0e-4f);
	const FVector Back = RotationFromLightDirection(Dir);
	TestEqual("Pitch round-trips", Back.X, Rot.X, 1.0e-2f);
	TestEqual("Yaw round-trips", Back.Y, Rot.Y, 1.0e-2f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLevelDirectionalLightGetDirectionMatchesTransformTest,
	"System.Engine.Level.DirectionalLightGetDirectionMatchesTransform",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLevelDirectionalLightGetDirectionMatchesTransformTest::RunTest(const FString& Parameters)
{
	// A directional light pitched down by its transform points downward.
	FDirectionalLight Light;
	Light.Transform.RotationDegrees = FVector(30.0f, 0.0f, 0.0f);
	const FVector Dir = Light.GetDirection();
	TestTrue("Points down", Dir.Y < 0.0f);
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
	// The factories set the shape type, and the plane size becomes its XZ scale.
	const FBasicShape Cube = FBasicShape::Cube();
	TestTrue("Cube type", Cube.Type == EBasicShape::Cube);
	const FBasicShape Plane = FBasicShape::Plane(4.0f);
	TestTrue("Plane type", Plane.Type == EBasicShape::Plane);
	TestEqual("Plane scale X", Plane.Transform.Scale.X, 4.0f, 1.0e-5f);
	TestEqual("Plane scale Z", Plane.Transform.Scale.Z, 4.0f, 1.0e-5f);
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

	ULevel Level;
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
	ULevel Level;
	UStaticMeshComponent Mesh{};
	Mesh.Tag = "player";
	Mesh.Transform.Position = FVector(1.0f, 2.0f, 3.0f);
	Level.AddStaticMesh(MoveTemp(Mesh));

	FPlayerStart Start{};
	Start.Transform.Position = FVector(5.0f, 0.0f, -2.0f);
	Level.AddPlayerStart(Start);

	TestEqual("One static mesh", Level.GetStaticMeshes().Num(), 1);
	TestEqual("Tag found", Level.FindStaticMeshIndexByTag("player"), static_cast<SIZE_T>(0));
	TestEqual("Missing tag", Level.FindStaticMeshIndexByTag("missing"), ULevel::Npos);
	const FPlayerStart* Found = Level.FindPlayerStart();
	if (!TestNotNull("PlayerStart found", Found))
	{
		return false;
	}
	TestEqual("PlayerStart X", Found->Transform.Position.X, 5.0f, 1.0e-5f);

	Level.Clear();
	TestEqual("No static meshes", Level.GetStaticMeshes().Num(), 0);
	TestEqual("No PlayerStarts", Level.GetPlayerStarts().Num(), 0);
	TestEqual("No directional lights", Level.GetDirectionalLights().Num(), 0);
	TestNull("No PlayerStart after Clear", Level.FindPlayerStart());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
