#include "CoreMinimal.h"
#include "Engine/GameEngine.h"
#include "Engine/Level.h"
#include "Frustum.h"
#include "Level/LevelLoader.h"
#include "Misc/AutomationTest.h"
#include "Tests/LegacyGolden.h"

#if WITH_DEV_AUTOMATION_TESTS

// Level goldens, recorded in the legacy world before P7: what the Starter template looks like once loaded.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGoldenStarterLevelTest, "System.Engine.Golden.StarterLevel",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGoldenStarterLevelTest::RunTest(const FString& Parameters)
{
	// The Starter template loaded headless: the world box of every static mesh, the light directions and
	// positions, and the camera it opens with.
	#ifdef LEON_ROOT_DIR
	constexpr float PositionTolerance = 1.0e-3f;
	constexpr float DirectionTolerance = 1.0e-4f;
	constexpr float AngleTolerance = 1.0e-2f;

	UGameEngine Engine;
	if (!TestTrue("Headless initialize", Engine.InitializeHeadless()))
	{
		return false;
	}
	const FString LevelPath = FString(LEON_ROOT_DIR) + "/Engine/Content/LevelTemplates/Starter.llev";
	if (!TestTrue("Starter level loaded", LoadLevelFile(Engine, LevelPath)))
	{
		Engine.Shutdown();
		return false;
	}

	const ULevel& Level = Engine.GetLevel();
	TArray<FVector> MeshBoxes;
	for (const UStaticMeshComponent& Component : Level.GetStaticMeshes())
	{
		if (Component.Mesh)
		{
			const FBox Box = TransformLocalBox(
				Component.Mesh->GetLocalMin(), Component.Mesh->GetLocalMax(), Component.EffectiveModelMatrix());
			MeshBoxes.Add(Box.Min);
			MeshBoxes.Add(Box.Max);
		}
	}
	TArray<FVector> LightDirections;
	for (const FDirectionalLight& Light : Level.GetDirectionalLights())
	{
		LightDirections.Add(Light.GetDirection());
	}
	TArray<FVector> PointLightPositions;
	for (const FPointLight& Light : Level.GetPointLights())
	{
		PointLightPositions.Add(Light.Transform.GetLocation());
	}
	const TArray<int32> Counts = {
		Level.GetStaticMeshes().Num(), MeshBoxes.Num() / 2, LightDirections.Num(), PointLightPositions.Num()};

	const UCameraComponent& Camera = Engine.GetCamera();
	const TArray<FVector> CameraPoints = {Camera.GetTarget(), Camera.GetCameraLocation()};
	const TArray<float> CameraAngles = {Camera.GetYawDegrees(), Camera.GetPitchDegrees()};
	const TArray<float> CameraDistance = {Camera.GetDistance()};
	Engine.Shutdown();

	static const int32 ExpectedCounts[4] = {1, 1, 1, 0};
	/** Min then max corner of each static mesh's world box. */
	static const FVector ExpectedMeshBoxes[] = {FVector(-10.0f, 0.0f, -10.0f), FVector(10.0f, 0.0f, 10.0f)};
	static const FVector ExpectedLightDirections[] = {FVector(-0.321393818f, -0.766044438f, 0.556670427f)};
	/** Camera target, then eye. */
	static const FVector ExpectedCameraPoints[2] = {FVector(0.0f, 0.0f, 0.0f), FVector(0.0f, 5.0f, 12.0f)};
	/** Camera yaw, then pitch, degrees. */
	static const float ExpectedCameraAngles[2] = {-90.0f, -20.0f};
	static const float ExpectedCameraDistance[1] = {12.0f};
	LegacyGolden::CheckInts(*this, "Counts", Counts, ExpectedCounts, 4);
	LegacyGolden::CheckPositions(
		*this, "MeshBoxes", MeshBoxes, ExpectedMeshBoxes, UE_ARRAY_COUNT(ExpectedMeshBoxes), PositionTolerance);
	LegacyGolden::CheckDirections(*this, "LightDirections", LightDirections, ExpectedLightDirections,
		UE_ARRAY_COUNT(ExpectedLightDirections), DirectionTolerance);
	// Starter has no point lights: the table is empty.
	LegacyGolden::CheckPositions(*this, "PointLightPositions", PointLightPositions, nullptr, 0, PositionTolerance);
	LegacyGolden::CheckPositions(*this, "CameraPoints", CameraPoints, ExpectedCameraPoints, 2, PositionTolerance);
	LegacyGolden::CheckScalars(
		*this, "CameraAngles", CameraAngles, ExpectedCameraAngles, 2, AngleTolerance, LegacyGolden::EUnit::Unitless);
	LegacyGolden::CheckScalars(*this, "CameraDistance", CameraDistance, ExpectedCameraDistance, 1, PositionTolerance,
		LegacyGolden::EUnit::Length);
	#else
	AddInfo("LEON_ROOT_DIR unset");
	#endif
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
