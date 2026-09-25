#include "CoreMinimal.h"
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

#endif // WITH_DEV_AUTOMATION_TESTS
