#include "CoreMinimal.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Frustum.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Level/LegacyLevelDataComponent.h"
#include "Level/LevelLoader.h"
#include "Misc/AutomationTest.h"
#include "Tests/LegacyGolden.h"
#include "Tests/ScopedTestWorld.h"

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

	FScopedTestWorld TestWorld;
	const FString LevelPath = FString(LEON_ROOT_DIR) + "/Engine/Content/LevelTemplates/Starter.llev";
	if (!TestTrue("Starter level loaded", LoadLevelFile(*TestWorld, LevelPath)))
	{
		return false;
	}

	const UWorld& World = *TestWorld;
	TArray<AActor*> MeshActors;
	UGameplayStatics::GetAllActorsOfClass(World, AStaticMeshActor::StaticClass(), MeshActors);
	TArray<FVector> MeshBoxes;
	for (const AActor* Actor : MeshActors)
	{
		const UStaticMeshComponent* Component = CastChecked<AStaticMeshActor>(Actor)->GetStaticMeshComponent();
		if (const UStaticMesh* Mesh = Component->GetStaticMesh())
		{
			const FBox Box = TransformLocalBox(Mesh->GetBoundingBox().Min, Mesh->GetBoundingBox().Max,
				Component->GetComponentTransform().ToMatrixWithScale());
			MeshBoxes.Add(Box.Min);
			MeshBoxes.Add(Box.Max);
		}
	}
	TArray<AActor*> Lights;
	UGameplayStatics::GetAllActorsOfClass(World, ADirectionalLight::StaticClass(), Lights);
	TArray<FVector> LightDirections;
	for (const AActor* Light : Lights)
	{
		LightDirections.Add(CastChecked<ADirectionalLight>(Light)->GetLightComponent()->GetDirection());
	}
	UGameplayStatics::GetAllActorsOfClass(World, APointLight::StaticClass(), Lights);
	TArray<FVector> PointLightPositions;
	for (const AActor* Light : Lights)
	{
		PointLightPositions.Add(Light->GetActorLocation());
	}
	const TArray<int32> Counts = {
		MeshActors.Num(), MeshBoxes.Num() / 2, LightDirections.Num(), PointLightPositions.Num()};

	// The camera the level opens with: its framing, kept on the world settings.
	UCameraComponent& Camera = *NewObject<UCameraComponent>();
	World.GetWorldSettings()->FindComponentByClass<ULegacyLevelDataComponent>()->ApplyCameraFraming(Camera);
	const TArray<FVector> CameraPoints = {Camera.GetTarget(), Camera.GetCameraLocation()};
	// The table holds the legacy angles of the camera's mode.
	float LegacyYaw = 0.0f;
	float LegacyPitch = 0.0f;
	if (Camera.GetMode() == ECameraMode::FreeLook)
	{
		FLegacyCoordinateConversion::ToLegacyFreeLookRotation(Camera.GetViewRotation(), LegacyYaw, LegacyPitch);
	}
	else
	{
		FLegacyCoordinateConversion::ToLegacyOrbitRotation(Camera.GetViewRotation(), LegacyYaw, LegacyPitch);
	}
	const TArray<float> CameraAngles = {LegacyYaw, LegacyPitch};
	const TArray<float> CameraDistance = {Camera.GetDistance()};

	static const int32 ExpectedCounts[4] = {1, 1, 1, 0};
	/** Min then max corner of each static mesh's world box. */
	static const FVector ExpectedMeshBoxes[] = {FVector(-10.0f, 0.0f, -10.0f), FVector(10.0f, 0.0f, 10.0f)};
	static const FVector ExpectedLightDirections[] = {FVector(-0.321393818f, -0.766044438f, 0.556670427f)};
	/** Camera target, then eye. */
	static const FVector ExpectedCameraPoints[2] = {FVector(0.0f, 0.0f, 0.0f), FVector(0.0f, 5.0f, 12.0f)};
	/** Camera yaw, then pitch, legacy degrees. */
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
