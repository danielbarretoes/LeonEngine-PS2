#include "CoreMinimal.h"
#include "Engine/GameEngine.h"
#include "Level/BasicLight.h"
#include "Level/LeonLevelFormat.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLevelFormatBytesRoundTripThroughBinaryFormatTest,
	"System.Engine.LevelFormat.BytesRoundTripThroughBinaryFormat",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLevelFormatBytesRoundTripThroughBinaryFormatTest::RunTest(const FString& Parameters)
{
	// A document with a camera, an actor and a light survives SerializeLeonLevel / DeserializeLeonLevel.
	FLevelDocument Doc;
	Doc.Name = "RoundTrip";
	Doc.GameMode = "Default";
	Doc.Camera.Mode = ECameraMode::FreeLook;
	Doc.Camera.Eye = FVector(1.0f, 2.0f, 3.0f);
	Doc.Camera.Yaw = -90.0f;

	FLevelActorRecord Sphere;
	Sphere.ActorClass = ELevelActorClass::Sphere;
	Sphere.Position = FVector(1.0f, 2.0f, 3.0f);
	Sphere.Scale = FVector(0.5f, 0.5f, 0.5f);
	Sphere.Tag = "ball";
	Sphere.SphereSegments = 32;
	Sphere.SphereRings = 20;
	Sphere.bSimulatePhysics = true;
	Sphere.Mobility = EComponentMobility::Movable;
	Sphere.bHasBob = true;
	Sphere.BobBaseY = 2.0f;
	Doc.Actors.Add(Sphere);

	FLevelLightRecord Point;
	Point.LightClass = ELevelLightClass::PointLight;
	Point.bHasOrbit = true;
	Point.OrbitRadius = 4.0f;
	Point.Range = 12.0f;
	Doc.Lights.Add(Point);

	const TArray<uint8> Bytes = SerializeLeonLevel(Doc);
	TestTrue("Bytes written", Bytes.Num() > 16);

	FLevelDocument Restored;
	if (!TestTrue("Deserialized", DeserializeLeonLevel(Bytes, Restored)))
	{
		return false;
	}
	TestEqual("Name", Restored.Name, "RoundTrip");
	TestEqual("GameMode", Restored.GameMode, "Default");
	TestTrue("Camera mode", Restored.Camera.Mode == ECameraMode::FreeLook);
	if (!TestEqual("Actor count", Restored.Actors.Num(), 1))
	{
		return false;
	}
	TestTrue("Actor class", Restored.Actors[0].ActorClass == ELevelActorClass::Sphere);
	TestEqual("Actor tag", Restored.Actors[0].Tag, "ball");
	TestEqual("Sphere segments", Restored.Actors[0].SphereSegments, 32);
	TestTrue("Actor mobility", Restored.Actors[0].Mobility == EComponentMobility::Movable);
	TestTrue("Actor bob", Restored.Actors[0].bHasBob);
	if (!TestEqual("Light count", Restored.Lights.Num(), 1))
	{
		return false;
	}
	TestTrue("Light orbit", Restored.Lights[0].bHasOrbit);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLevelFormatDeserializeRejectsBadMagicAndTruncationTest,
	"System.Engine.LevelFormat.DeserializeRejectsBadMagicAndTruncation",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLevelFormatDeserializeRejectsBadMagicAndTruncationTest::RunTest(const FString& Parameters)
{
	// DeserializeLeonLevel fails on a corrupted magic number and on a truncated buffer.
	AddExpectedError("bad magic", 1);
	{
		const FLevelDocument Doc;
		TArray<uint8> Bytes = SerializeLeonLevel(Doc);
		FLevelDocument Restored;

		Bytes[0] = 'X';
		TestFalse("Bad magic rejected", DeserializeLeonLevel(Bytes, Restored));
	}
	{
		const FLevelDocument Doc;
		TArray<uint8> Bytes = SerializeLeonLevel(Doc);
		FLevelDocument Restored;

		Bytes.SetNum(Bytes.Num() / 2);
		TestFalse("Truncated bytes rejected", DeserializeLeonLevel(Bytes, Restored));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLevelFormatWorldRoundTripsThroughLegacyRecordsTest,
	"System.Engine.LevelFormat.WorldRoundTripsThroughLegacyRecords",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLevelFormatWorldRoundTripsThroughLegacyRecordsTest::RunTest(const FString& Parameters)
{
	// A player start, a sun and an orbit camera become legacy records (metres, Y up, legacy angles) and come back the
	// same.
	UGameEngine Engine;
	if (!TestTrue("Headless initialize", Engine.InitializeHeadless()))
	{
		return false;
	}
	ULevel& Level = Engine.GetLevel();
	Level.Clear();
	FPlayerStart Start;
	Start.Transform = FTransform(FRotator(0.0f, 30.0f, 0.0f), FVector(100.0f, 200.0f, 50.0f));
	Level.AddPlayerStart(Start);
	FBasicLight::Directional(FRotator(-45.0f, 20.0f, 0.0f).Quaternion()).AddTo(Level);
	UCameraComponent& Camera = Engine.GetCamera();
	Camera.SetMode(ECameraMode::Orbit);
	Camera.SetTarget(FVector(0.0f, 100.0f, 0.0f));
	Camera.SetDistance(600.0f);
	Camera.SetViewRotation(FRotator(-20.0f, 135.0f, 0.0f));
	const FVector Eye = Camera.GetCameraLocation();

	const FLevelDocument Doc = BuildLevelDocument(Level, Camera);
	if (!TestEqual("One actor", Doc.Actors.Num(), 1) || !TestEqual("One light", Doc.Lights.Num(), 1))
	{
		Engine.Shutdown();
		return false;
	}
	// Legacy: Y up in metres, the start faces legacy yaw 90 - 30, the orbit eye at yaw 135 - 180 and pitch 20 above.
	TestTrue("Legacy position", Doc.Actors[0].Position.Equals(FVector(1.0f, 0.5f, 2.0f), 1.0e-5f));
	TestTrue("Legacy start yaw", Doc.Actors[0].RotationDegrees.Equals(FVector(0.0f, 60.0f, 0.0f), 1.0e-3f));
	TestEqual("Legacy camera yaw", Doc.Camera.Yaw, -45.0f, 1.0e-3f);
	TestEqual("Legacy camera pitch", Doc.Camera.Pitch, 20.0f, 1.0e-3f);
	TestEqual("Legacy light pitch", Doc.Lights[0].RotationDegrees.X, 45.0f, 1.0e-2f);
	TestEqual("Legacy light yaw", Doc.Lights[0].RotationDegrees.Y, 70.0f, 1.0e-2f);

	Level.Clear();
	Camera.SetViewRotation(FRotator::ZeroRotator);
	if (!TestTrue("Applied", ApplyLevelDocument(Engine, Doc, "memory-round-trip")))
	{
		Engine.Shutdown();
		return false;
	}
	const FPlayerStart* Restored = Engine.GetLevel().FindPlayerStart();
	if (TestNotNull("Player start", Restored))
	{
		TestTrue("Start location", Restored->Transform.GetLocation().Equals(FVector(100.0f, 200.0f, 50.0f), 1.0e-3f));
		TestTrue("Start faces yaw 30",
			Restored->Transform.GetRotation().GetForwardVector().Equals(FRotator(0.0f, 30.0f, 0.0f).Vector(), 1.0e-5f));
	}
	TestTrue("Sun direction",
		Engine.GetLevel().GetDirectionalLights()[0].GetDirection().Equals(
			FRotator(-45.0f, 20.0f, 0.0f).Vector(), 1.0e-4f));
	TestTrue("Camera eye", Engine.GetCamera().GetCameraLocation().Equals(Eye, 1.0e-2f));
	TestTrue("Camera rotation", Engine.GetCamera().GetViewRotation().Equals(FRotator(-20.0f, 135.0f, 0.0f), 1.0e-3f));
	Engine.Shutdown();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
