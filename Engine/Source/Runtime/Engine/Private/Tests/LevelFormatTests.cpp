#include "CoreMinimal.h"
#include "Engine/DirectionalLight.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "LeonMeshFormat.h"
#include "Level/BasicLight.h"
#include "Level/LegacyLevelDataComponent.h"
#include "Level/LeonLevelFormat.h"
#include "Level/LevelLoader.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Primitives.h"
#include "Tests/ScopedTestWorld.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{

	/** A camera with the level's camera framing (kept on its world settings), as the level opens. */
	UCameraComponent& MakeFramingCamera(const UWorld& World)
	{
		UCameraComponent& Camera = *NewObject<UCameraComponent>();
		World.GetWorldSettings()->FindComponentByClass<ULegacyLevelDataComponent>()->ApplyCameraFraming(Camera);
		return Camera;
	}

	/** MD5 of the bytes the level saver writes for the world's level and its camera framing, with the byte count. */
	FString SavedHash(const UWorld& World)
	{
		const FLevelDocument Doc = BuildLevelDocument(*World.PersistentLevel, MakeFramingCamera(World));
		const TArray<uint8> Bytes = SerializeLeonLevel(Doc);
		return FMD5::HashBytes(Bytes.GetData(), Bytes.Num()) + FString::Printf(" (%d bytes)", Bytes.Num());
	}

	/** SavedHash after loading a level file into a test world. */
	FString LoadAndHash(const FString& Path)
	{
		FScopedTestWorld TestWorld;
		if (!LoadLevelFile(*TestWorld, Path))
		{
			return "LOAD FAILED";
		}
		return SavedHash(*TestWorld);
	}

	FLevelActorRecord MakeRecord(ELevelActorClass Class, const FVector& Position,
		const FVector& Rotation = FVector::ZeroVector, const FVector& Scale = FVector::OneVector)
	{
		FLevelActorRecord Record;
		Record.ActorClass = Class;
		Record.Position = Position;
		Record.RotationDegrees = Rotation;
		Record.Scale = Scale;
		return Record;
	}

} // namespace

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
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	ULevel& Level = *World.PersistentLevel;
	World.SpawnActor<APlayerStart>(
		APlayerStart::StaticClass(), FTransform(FRotator(0.0f, 30.0f, 0.0f), FVector(100.0f, 200.0f, 50.0f)));
	FBasicLight::Directional(FRotator(-45.0f, 20.0f, 0.0f).Quaternion()).SpawnIn(World);
	// The camera that frames the level (held: the level load collects garbage).
	TStrongObjectPtr<UCameraComponent> CameraPtr(NewObject<UCameraComponent>());
	UCameraComponent& Camera = *CameraPtr;
	Camera.SetMode(ECameraMode::Orbit);
	Camera.SetTarget(FVector(0.0f, 100.0f, 0.0f));
	Camera.SetDistance(600.0f);
	Camera.SetViewRotation(FRotator(-20.0f, 135.0f, 0.0f));
	const FVector Eye = Camera.GetCameraLocation();

	const FLevelDocument Doc = BuildLevelDocument(Level, Camera);
	if (!TestEqual("One actor", Doc.Actors.Num(), 1) || !TestEqual("One light", Doc.Lights.Num(), 1))
	{
		return false;
	}
	// Legacy: Y up in metres, the start faces legacy yaw 90 - 30, the orbit eye at yaw 135 - 180 and pitch 20 above.
	TestTrue("Legacy position", Doc.Actors[0].Position.Equals(FVector(1.0f, 0.5f, 2.0f), 1.0e-5f));
	TestTrue("Legacy start yaw", Doc.Actors[0].RotationDegrees.Equals(FVector(0.0f, 60.0f, 0.0f), 1.0e-3f));
	TestEqual("Legacy camera yaw", Doc.Camera.Yaw, -45.0f, 1.0e-3f);
	TestEqual("Legacy camera pitch", Doc.Camera.Pitch, 20.0f, 1.0e-3f);
	TestEqual("Legacy light pitch", Doc.Lights[0].RotationDegrees.X, 45.0f, 1.0e-2f);
	TestEqual("Legacy light yaw", Doc.Lights[0].RotationDegrees.Y, 70.0f, 1.0e-2f);

	World.Clear();
	Camera.SetViewRotation(FRotator::ZeroRotator);
	if (!TestTrue("Applied", ApplyLevelDocument(World, Doc, "memory-round-trip")))
	{
		return false;
	}
	// The framing comes back on the world settings; the camera takes it as the level opens.
	World.GetWorldSettings()->FindComponentByClass<ULegacyLevelDataComponent>()->ApplyCameraFraming(Camera);
	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(World, APlayerStart::StaticClass(), Found);
	const AActor* Restored = Found.Num() > 0 ? Found[0] : nullptr;
	if (TestNotNull("Player start", Restored))
	{
		TestTrue("Start location", Restored->GetActorLocation().Equals(FVector(100.0f, 200.0f, 50.0f), 1.0e-3f));
		TestTrue("Start faces yaw 30",
			Restored->GetActorQuat().GetForwardVector().Equals(FRotator(0.0f, 30.0f, 0.0f).Vector(), 1.0e-5f));
	}
	UGameplayStatics::GetAllActorsOfClass(World, ADirectionalLight::StaticClass(), Found);
	TestTrue("Sun direction",
		Found.Num() > 0 &&
			CastChecked<ADirectionalLight>(Found[0])->GetLightComponent()->GetDirection().Equals(
				FRotator(-45.0f, 20.0f, 0.0f).Vector(), 1.0e-4f));
	TestTrue("Camera eye", Camera.GetCameraLocation().Equals(Eye, 1.0e-2f));
	TestTrue("Camera rotation", Camera.GetViewRotation().Equals(FRotator(-20.0f, 135.0f, 0.0f), 1.0e-3f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLevelFormatSaveWritesTheSameBytesTest,
	"System.Engine.LevelFormat.SaveWritesTheSameBytes",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLevelFormatSaveWritesTheSameBytesTest::RunTest(const FString& Parameters)
{
	// Loading a level and saving it back writes the bytes the level saver wrote before levels became actors (hashes
	// recorded at P12 from the POD level): the templates, and a document with every record class, the legacy-only
	// fields, a fit height, a loaded .lmesh and more lights than the renderer keeps.
	#ifdef LEON_ROOT_DIR
	const FString Root = FString(LEON_ROOT_DIR);
	TestEqual("Starter", LoadAndHash(Root + "/Engine/Content/LevelTemplates/Starter.llev"),
		FString("4c9d53d8d45c4a6295f8ab5c5f8935f8 (292 bytes)"));
	TestEqual("Blank", LoadAndHash(Root + "/Engine/Content/LevelTemplates/Blank.llev"),
		FString("700599a4fffc3b1fcb3392341c70bf0d (163 bytes)"));

	const FString Dir = FPaths::ProjectIntermediateDir() + TEXT("Tests/LevelFormat/");
	const FString MeshPath = Dir + TEXT("Meshes/HashCube.lmesh");
	const FString LevelPath = Dir + TEXT("Levels/SaveBytes.llev");
	TestTrue("Mesh written", SaveLeonMeshFile(MeshPath, MakeCube()));

	FLevelDocument Doc;
	Doc.Name = "SaveBytes";
	Doc.GameMode = "Default";
	Doc.EnvironmentPath = "hdr/sky.hdr";
	Doc.EnvironmentExposure = 1.5f;
	Doc.Camera.Mode = ECameraMode::FreeLook;
	Doc.Camera.Target = FVector(0.0f, 1.0f, 0.0f);
	Doc.Camera.Eye = FVector(1.0f, 2.0f, 3.0f);
	Doc.Camera.Distance = 7.0f;
	Doc.Camera.Yaw = 30.0f;
	Doc.Camera.Pitch = -10.0f;

	Doc.Actors.Add(MakeRecord(ELevelActorClass::PlayerStart, FVector(1.0f, 0.0f, 2.0f), FVector(0.0f, 30.0f, 0.0f)));
	{
		FLevelActorRecord Record = MakeRecord(
			ELevelActorClass::Cube, FVector(0.0f, 0.5f, 0.0f), FVector(10.0f, 20.0f, 30.0f), FVector(1.0f, 2.0f, 1.0f));
		Record.MaterialPath = "Materials/M_WorldGrid.lmat";
		Record.Tag = "crate";
		Record.bCollisionEnabled = true;
		Record.bSimulatePhysics = true;
		Record.Mobility = EComponentMobility::Movable;
		Record.bHasSpinYaw = true;
		Record.SpinYaw = 45.0f;
		Record.bHasBob = true;
		Record.BobBaseY = 0.25f;
		Record.BobAmplitude = 0.3f;
		Record.BobSpeed = 2.0f;
		Record.LightmapId = "lm1";
		Doc.Actors.Add(Record);
	}
	{
		FLevelActorRecord Record = MakeRecord(ELevelActorClass::Sphere, FVector(2.0f, 1.0f, -1.0f));
		Record.SphereSegments = 12;
		Record.SphereRings = 8;
		Record.bHidden = true;
		Record.Tag = "NavBlocker";
		Record.bEnableGravity = false;
		Doc.Actors.Add(Record);
	}
	{
		FLevelActorRecord Record =
			MakeRecord(ELevelActorClass::Plane, FVector::ZeroVector, FVector::ZeroVector, FVector(20.0f, 1.0f, 20.0f));
		Record.bCollisionEnabled = true;
		Doc.Actors.Add(Record);
	}
	{
		FLevelActorRecord Record = MakeRecord(ELevelActorClass::BlockingVolume, FVector(-3.0f, 1.0f, 0.0f),
			FVector(0.0f, 15.0f, 0.0f), FVector(1.0f, 2.0f, 4.0f));
		Record.bCollisionEnabled = true;
		Record.bHidden = true;
		Record.MaterialPath = "Materials/M_SolidMetal.lmat";
		Doc.Actors.Add(Record);
	}
	{
		FLevelActorRecord Record =
			MakeRecord(ELevelActorClass::StaticMesh, FVector(4.0f, 0.0f, 4.0f), FVector(0.0f, 90.0f, 0.0f));
		Record.MeshPath = "Meshes/HashCube.lmesh";
		Record.bHasFitHeight = true;
		Record.FitHeight = 1.5f;
		Record.bCollisionEnabled = true;
		Doc.Actors.Add(Record);
	}
	{
		FLevelActorRecord Record = MakeRecord(
			ELevelActorClass::TriggerVolume, FVector(5.0f, 0.0f, 1.0f), FVector::ZeroVector, FVector(2.0f, 2.0f, 2.0f));
		Record.InteractCost = 500;
		Record.InteractRadius = 3.0f;
		Record.Payload = "WallBuy:M14";
		Record.bConsumeOnUse = true;
		Record.Tag = "buy";
		Doc.Actors.Add(Record);
	}
	{
		FLevelActorRecord Record = MakeRecord(ELevelActorClass::PainCausingVolume, FVector(-5.0f, 0.0f, 1.0f),
			FVector(0.0f, 0.0f, 0.0f), FVector(3.0f, 1.0f, 3.0f));
		Record.DamagePerSecond = 20.0f;
		Record.DamageInterval = 0.5f;
		Record.Tag = "lava";
		Doc.Actors.Add(Record);
	}
	{
		FLevelActorRecord Record =
			MakeRecord(ELevelActorClass::AISpawnPoint, FVector(6.0f, 0.0f, -6.0f), FVector(0.0f, 90.0f, 0.0f));
		Record.Tag = "zombie";
		Doc.Actors.Add(Record);
	}
	Doc.Actors.Add(MakeRecord(ELevelActorClass::PlayerStart, FVector(-1.0f, 0.0f, -2.0f), FVector(0.0f, -45.0f, 0.0f)));

	auto MakeLight = [](ELevelLightClass Class, bool bCast, const FVector& Position, const FVector& Rotation)
	{
		FLevelLightRecord Light;
		Light.LightClass = Class;
		Light.bCastShadows = bCast;
		Light.Position = Position;
		Light.RotationDegrees = Rotation;
		Light.LightColor = FVector(1.0f, 0.9f, 0.8f);
		Light.Intensity = 1.25f;
		Light.Range = 6.0f;
		Light.SourceAngle = 1.0f;
		return Light;
	};
	Doc.Lights.Add(
		MakeLight(ELevelLightClass::DirectionalLight, true, FVector::ZeroVector, FVector(50.0f, -30.0f, 0.0f)));
	{
		FLevelLightRecord Light =
			MakeLight(ELevelLightClass::PointLight, false, FVector(1.0f, 2.0f, 1.0f), FVector::ZeroVector);
		Light.bHasOrbit = true;
		Light.OrbitRadius = 2.0f;
		Light.OrbitHeight = 1.5f;
		Light.OrbitHeightAmp = 0.25f;
		Light.OrbitSpeed = 0.5f;
		Doc.Lights.Add(Light);
	}
	Doc.Lights.Add(
		MakeLight(ELevelLightClass::DirectionalLight, false, FVector::ZeroVector, FVector(20.0f, 100.0f, 0.0f)));
	Doc.Lights.Add(
		MakeLight(ELevelLightClass::DirectionalLight, true, FVector::ZeroVector, FVector(70.0f, 10.0f, 0.0f)));
	for (int32 I = 0; I < 5; ++I)
	{
		Doc.Lights.Add(MakeLight(
			ELevelLightClass::PointLight, I == 2, FVector(static_cast<float>(I), 3.0f, 0.0f), FVector::ZeroVector));
	}

	{
		FScopedTestWorld TestWorld;
		if (TestTrue("Applied", ApplyLevelDocument(*TestWorld, Doc, LevelPath)))
		{
			TestEqual(
				"Every record class", SavedHash(*TestWorld), FString("9c048faf15eeb14d5fd9d408fc14dcbf (1165 bytes)"));
		}
	}
	IFileManager::Get().DeleteDirectory(*Dir, false, true);
	#else
	AddInfo("LEON_ROOT_DIR unset");
	#endif
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
