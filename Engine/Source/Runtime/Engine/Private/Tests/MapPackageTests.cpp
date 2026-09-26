#include "Camera/CameraActor.h"
#include "CoreMinimal.h"
#include "Engine/BlockingVolume.h"
#include "Engine/DirectionalLight.h"
#include "Engine/GameEngine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/PointLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TargetPoint.h"
#include "Engine/TriggerVolume.h"
#include "Engine/World.h"
#include "GameFramework/GameMode.h"
#include "GameFramework/PainCausingVolume.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/RotatingMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/Material.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Tests/ScopedTestWorld.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{

	/** Where the map tests write their packages: `/MapTest/` is mounted there while a test runs. */
	FString MapTestDir()
	{
		return FPaths::ProjectIntermediateDir() + TEXT("Tests/MapPackage/");
	}

	/** Mounts `/MapTest/` for the length of a scope and deletes what the test wrote. */
	struct FScopedMapTestMount
	{
		FScopedMapTestMount()
		{
			IFileManager::Get().DeleteDirectory(*MapTestDir(), false, true);
			FPackageName::RegisterMountPoint(TEXT("/MapTest/"), MapTestDir());
		}
		~FScopedMapTestMount()
		{
			CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
			FPackageName::UnRegisterMountPoint(TEXT("/MapTest/"), MapTestDir());
			IFileManager::Get().DeleteDirectory(*MapTestDir(), false, true);
		}
	};

	/** A rotation that is not a simple one, so the saved transform is checked on every axis. */
	const FTransform MeshTransform(
		FRotator(12.5f, -33.0f, 7.25f), FVector(120.0f, -45.5f, 30.0f), FVector(2.0f, 0.5f, 1.5f));

	/**
	 * Fills World with one actor of every class a map holds, with values that differ from the class defaults: the world
	 * settings, a basic-shape mesh with a material, collision and a spin and a bob, a blocking, a trigger (with its
	 * interaction data) and a pain-causing volume, a tagged player start and target point, a directional and an
	 * orbiting point light, and the camera actor of a framing.
	 */
	void PopulateMapWorld(UWorld& World)
	{
		AWorldSettings* Settings = World.SpawnActor<AWorldSettings>();
		World.PersistentLevel->SetWorldSettings(Settings);
		Settings->DefaultGameMode = AGameMode::StaticClass();
		Settings->KillZ = -5000.0f;

		AStaticMeshActor* Mesh = World.SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), MeshTransform);
		(void)Mesh->GetStaticMeshComponent()->SetStaticMesh(
			LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
		Mesh->GetStaticMeshComponent()->SetMaterial(0, UMaterial::GetDefaultMaterial(MD_Surface));
		Mesh->Tags.Add(FName(TEXT("Crate")));
		Mesh->SetActorHiddenInGame(true);
		UStaticMeshComponent& MeshComponent = *Mesh->GetStaticMeshComponent();
		MeshComponent.SetMobility(EComponentMobility::Movable);
		MeshComponent.SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		MeshComponent.SetSimulatePhysics(true);
		MeshComponent.SetEnableGravity(false);
		URotatingMovementComponent* Spin = NewObject<URotatingMovementComponent>(Mesh, TEXT("RotatingMovement"));
		Spin->RotationRate = FRotator(0.0f, 45.0f, 0.0f);
		Spin->bRotationInLocalSpace = false;
		Spin->RegisterComponent();

		ABlockingVolume* Clip = World.SpawnActor<ABlockingVolume>(ABlockingVolume::StaticClass(),
			FTransform(FRotator::ZeroRotator, FVector(-300.0f, 0.0f, 100.0f), FVector(1.0f, 4.0f, 2.0f)));
		Clip->Tags.Add(FName(TEXT("Clip")));

		ATriggerVolume* Trigger =
			World.SpawnActor<ATriggerVolume>(ATriggerVolume::StaticClass(), FTransform(FVector(400.0f, 200.0f, 50.0f)));
		Trigger->Tags.Add(FName(TEXT("BombSite")));
		Trigger->Tags.Add(FName(TEXT("A")));

		APainCausingVolume* Pain = World.SpawnActor<APainCausingVolume>(
			APainCausingVolume::StaticClass(), FTransform(FVector(-100.0f, -400.0f, 0.0f)));
		Pain->DamagePerSec = 20.0f;
		Pain->PainInterval = 0.5f;

		APlayerStart* Start = World.SpawnActor<APlayerStart>(FVector(10.0f, 20.0f, 92.0f), FRotator(0.0f, 30.0f, 0.0f));
		Start->PlayerStartTag = FName(TEXT("CT"));

		ATargetPoint* Point =
			World.SpawnActor<ATargetPoint>(FVector(-600.0f, 600.0f, 0.0f), FRotator(0.0f, -90.0f, 0.0f));
		Point->Tags.Add(FName(TEXT("BotSpawn")));

		ADirectionalLight* Sun =
			World.SpawnActor<ADirectionalLight>(FVector::ZeroVector, FRotator(-50.0f, 30.0f, 0.0f));
		UDirectionalLightComponent& SunComponent = *Sun->GetDirectionalLightComponent();
		SunComponent.SetLightColor(FLinearColor(1.0f, 0.9f, 0.8f));
		SunComponent.SetIntensity(1.25f);
		SunComponent.SetCastShadows(false);
		SunComponent.SetLightSourceAngle(1.5f);

		APointLight* Bulb = World.SpawnActor<APointLight>(FVector(0.0f, 0.0f, 300.0f), FRotator::ZeroRotator);
		UPointLightComponent& BulbComponent = *Bulb->GetPointLightComponent();
		BulbComponent.SetLightColor(FLinearColor(0.3f, 0.5f, 1.0f));
		BulbComponent.SetAttenuationRadius(650.0f);

		ACameraActor* Camera = World.SpawnActor<ACameraActor>();
		UCameraComponent& CameraComponent = *Camera->GetCameraComponent();
		CameraComponent.SetTarget(FVector(0.0f, 100.0f, 0.0f));
		CameraComponent.SetDistance(600.0f);
		CameraComponent.SetViewRotation(FRotator(-20.0f, 135.0f, 0.0f));
		CameraComponent.SetEyeLocation(FVector(1.0f, 2.0f, 3.0f));
		CameraComponent.SetMode(ECameraMode::FreeLook);
	}

	/** Saves a new map `/MapTest/Maps/<Name>` made by PopulateMapWorld; the file name, or empty on a failure. */
	FString SaveTestMap(const TCHAR* Name)
	{
		const FString PackageName = FString(TEXT("/MapTest/Maps/")) + Name;
		UPackage* Package = CreatePackage(*PackageName);
		UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false, FName(Name), Package);
		PopulateMapWorld(*World);
		const FString Filename =
			FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetMapPackageExtension());
		const bool bSaved = UPackage::SavePackage(Package, World, RF_Public | RF_Standalone, *Filename);
		World->DestroyWorld(false);
		Package->MarkPendingKill();
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
		return bSaved ? Filename : FString();
	}

	template <typename T>
	T* FindOnly(const UWorld& World)
	{
		TArray<AActor*> Found;
		UGameplayStatics::GetAllActorsOfClass(World, T::StaticClass(), Found);
		return Found.Num() == 1 ? Cast<T>(Found[0]) : nullptr;
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMapPackageSaveLoadRoundTripsEveryActorTest,
	"System.Engine.MapPackage.SaveLoadRoundTripsEveryActor",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FMapPackageSaveLoadRoundTripsEveryActorTest::RunTest(const FString& Parameters)
{
	// A world saved as a `.lmap` package (PKG_ContainsMap) loads back with its level, its world settings and every
	// actor and component in the order they were spawned, with their values.
	FScopedMapTestMount Mount;
	const FString Filename = SaveTestMap(TEXT("RoundTrip"));
	if (!TestFalse("Saved", Filename.IsEmpty()) || !TestTrue("A .lmap file", FPaths::FileExists(Filename)))
	{
		return false;
	}
	UPackage* Package = LoadPackage(nullptr, TEXT("/MapTest/Maps/RoundTrip"), LOAD_None);
	if (!TestNotNull("Loaded", Package))
	{
		return false;
	}
	TestTrue("PKG_ContainsMap", Package->ContainsMap());
	UWorld* World = UWorld::FindWorldInPackage(Package);
	if (!TestNotNull("The world", World))
	{
		return false;
	}
	TestEqual("The world is named after the map", World->GetName(), FString(TEXT("RoundTrip")));
	TestTrue("The world is the package's asset", World->HasAnyFlags(RF_Public));
	World->AddToRoot();
	World->InitWorld();
	ULevel* Level = World->PersistentLevel;
	if (!TestNotNull("The persistent level", Level))
	{
		return false;
	}
	TestTrue("The level knows its world", Level->OwningWorld == World);
	TestEqual("Every actor", Level->Actors.Num(), 10);

	// The spawn order, the world settings first.
	const UClass* const Order[] = {AWorldSettings::StaticClass(), AStaticMeshActor::StaticClass(),
		ABlockingVolume::StaticClass(), ATriggerVolume::StaticClass(), APainCausingVolume::StaticClass(),
		APlayerStart::StaticClass(), ATargetPoint::StaticClass(), ADirectionalLight::StaticClass(),
		APointLight::StaticClass(), ACameraActor::StaticClass()};
	for (int32 Index = 0; Index < static_cast<int32>(UE_ARRAY_COUNT(Order)) && Index < Level->Actors.Num(); ++Index)
	{
		TestTrue(*FString::Printf(TEXT("Actor %d's class"), Index), Level->Actors[Index]->GetClass() == Order[Index]);
	}
	TestTrue("GetWorldSettings", Level->GetWorldSettings() == Level->Actors[0]);
	TestTrue("DefaultGameMode", World->GetWorldSettings()->DefaultGameMode == AGameMode::StaticClass());
	TestEqual("KillZ", World->GetWorldSettings()->KillZ, -5000.0f);

	const AStaticMeshActor* Mesh = FindOnly<AStaticMeshActor>(*World);
	if (TestNotNull("The mesh actor", Mesh))
	{
		const UStaticMeshComponent& Component = *Mesh->GetStaticMeshComponent();
		TestTrue("The basic cube",
			Component.GetStaticMesh() == LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
		TestTrue("The material", Component.GetMaterial(0) == UMaterial::GetDefaultMaterial(MD_Surface));
		TestTrue("Location", Mesh->GetActorLocation().Equals(MeshTransform.GetLocation(), 0.0f));
		TestTrue("Rotation", Mesh->GetActorQuat().Equals(MeshTransform.GetRotation(), 1.0e-6f));
		TestTrue("Scale", Mesh->GetActorScale3D().Equals(MeshTransform.GetScale3D(), 0.0f));
		TestTrue("Tags", Mesh->Tags.Num() == 1 && Mesh->ActorHasTag(FName(TEXT("Crate"))));
		TestTrue("Hidden", Mesh->IsHidden());
		TestTrue("Movable", Component.Mobility == EComponentMobility::Movable);
		TestTrue("Collision", Component.GetCollisionEnabled() == ECollisionEnabled::QueryAndPhysics);
		TestTrue("Simulates", Component.IsSimulatingPhysics());
		TestFalse("No gravity", Component.IsGravityEnabled());
		const URotatingMovementComponent* Spin = Mesh->FindComponentByClass<URotatingMovementComponent>();
		TestTrue("Spin",
			Spin != nullptr && Spin->RotationRate == FRotator(0.0f, 45.0f, 0.0f) && !Spin->bRotationInLocalSpace);
		TestEqual("Components (root, spin)", Mesh->GetComponents().Num(), 2);
	}
	const ABlockingVolume* Clip = FindOnly<ABlockingVolume>(*World);
	if (TestNotNull("The blocking volume", Clip))
	{
		TestTrue("Clip scale", Clip->GetActorScale3D().Equals(FVector(1.0f, 4.0f, 2.0f), 0.0f));
		TestTrue("Clip collision", Clip->GetBrushComponent()->IsCollisionEnabled());
		TestTrue("Clip tag", Clip->ActorHasTag(FName(TEXT("Clip"))));
	}
	const ATriggerVolume* Trigger = FindOnly<ATriggerVolume>(*World);
	if (TestNotNull("The trigger volume", Trigger))
	{
		TestTrue("Trigger tags",
			Trigger->Tags.Num() == 2 && Trigger->Tags[0] == FName(TEXT("BombSite")) &&
				Trigger->Tags[1] == FName(TEXT("A")));
	}
	const APainCausingVolume* Pain = FindOnly<APainCausingVolume>(*World);
	TestTrue("Pain", Pain != nullptr && Pain->DamagePerSec == 20.0f && Pain->PainInterval == 0.5f);
	const APlayerStart* Start = FindOnly<APlayerStart>(*World);
	if (TestNotNull("The player start", Start))
	{
		TestEqual("PlayerStartTag", Start->PlayerStartTag, FName(TEXT("CT")));
		TestTrue("Start location", Start->GetActorLocation().Equals(FVector(10.0f, 20.0f, 92.0f), 0.0f));
		TestTrue("Start rotation", Start->GetActorRotation().Equals(FRotator(0.0f, 30.0f, 0.0f), 0.0f));
	}
	const ATargetPoint* Point = FindOnly<ATargetPoint>(*World);
	TestTrue("Target point",
		Point != nullptr && Point->ActorHasTag(FName(TEXT("BotSpawn"))) &&
			Point->GetActorRotation().Equals(FRotator(0.0f, -90.0f, 0.0f), 0.0f));
	const ADirectionalLight* Sun = FindOnly<ADirectionalLight>(*World);
	if (TestNotNull("The sun", Sun))
	{
		const UDirectionalLightComponent& Component = *Sun->GetDirectionalLightComponent();
		TestTrue("Sun is the light component", Sun->GetLightComponent() == &Component);
		TestTrue("Sun rotation", Sun->GetActorRotation().Equals(FRotator(-50.0f, 30.0f, 0.0f), 0.0f));
		TestTrue("Sun colour", Component.LightColor == FLinearColor(1.0f, 0.9f, 0.8f));
		TestEqual("Sun intensity", Component.Intensity, 1.25f);
		TestFalse("Sun shadows", Component.CastShadows);
		TestEqual("Sun angle", Component.LightSourceAngle, 1.5f);
	}
	const APointLight* Bulb = FindOnly<APointLight>(*World);
	if (TestNotNull("The point light", Bulb))
	{
		TestEqual("Bulb radius", Bulb->GetPointLightComponent()->AttenuationRadius, 650.0f);
	}
	const ACameraActor* Camera = FindOnly<ACameraActor>(*World);
	if (TestNotNull("The camera actor", Camera))
	{
		const UCameraComponent& Component = *Camera->GetCameraComponent();
		TestTrue("Camera mode", Component.GetMode() == ECameraMode::FreeLook);
		TestTrue("Camera target", Component.GetTarget().Equals(FVector(0.0f, 100.0f, 0.0f), 0.0f));
		TestEqual("Camera distance", Component.GetDistance(), 600.0f);
		TestTrue("Camera view", Component.GetViewRotation().Equals(FRotator(-20.0f, 135.0f, 0.0f), 0.0f));
		TestTrue("Camera eye", Component.EyeLocation().Equals(FVector(1.0f, 2.0f, 3.0f), 0.0f));
	}

	// Registered once the loader asks: the colliding components become bodies of the world's physics scene.
	TestEqual("No bodies before registration", World->GetPhysicsScene().GetBodies().Num(), 0);
	World->UpdateWorldComponents();
	TestTrue("Bodies after registration", World->GetPhysicsScene().GetBodies().Num() >= 2);

	World->DestroyWorld(false);
	Package->MarkPendingKill();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMapPackageLoadMapOpensMapPackagesTest,
	"System.Engine.MapPackage.LoadMapOpensMapPackages",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FMapPackageLoadMapOpensMapPackagesTest::RunTest(const FString& Parameters)
{
	// UEngine::LoadMap opens a `.lmap` by its long package name or its file: the world of the package, its components
	// registered, the game mode its world settings name, the player at its player start; opening it again loads it
	// again. A map file outside the mount points mounts its content folder (the folder above `Maps/`).
	FScopedMapTestMount Mount;
	if (!TestFalse("Saved", SaveTestMap(TEXT("Opened")).IsEmpty()))
	{
		return false;
	}
	TStrongObjectPtr<UGameEngine> Engine(NewObject<UGameEngine>());
	Engine->Init(nullptr);
	FWorldContext& Context = *Engine->GameInstance->GetWorldContext();
	FString Error;
	if (!TestEqual("Browse the package",
			static_cast<int32>(
				Engine->Browse(Context, FURL(nullptr, TEXT("/MapTest/Maps/Opened"), TRAVEL_Absolute), Error)),
			static_cast<int32>(EBrowseReturnVal::Success)))
	{
		AddError(Error);
		Engine->PreExit();
		return false;
	}
	UWorld* World = Engine->GetGameWorld();
	TestEqual("The map's world", World->GetName(), FString(TEXT("Opened")));
	TestTrue("In the map package", World->GetOutermost()->GetName() == TEXT("/MapTest/Maps/Opened"));
	TestTrue("Plays", World->HasBegunPlay());
	TestTrue("The world settings' game mode", World->GetAuthGameMode()->GetClass() == AGameMode::StaticClass());
	TestTrue("Bodies", World->GetPhysicsScene().GetBodies().Num() >= 2);
	const APlayerStart* Start = World->FindFirst<APlayerStart>();
	const APlayerController* Controller = Engine->GameInstance->GetFirstGamePlayer()->PlayerController;
	if (TestNotNull("A start", Start) && TestNotNull("Logged in", Controller) &&
		TestNotNull("A pawn", Controller->GetPawn()))
	{
		TestTrue("At the start", Controller->GetPawn()->GetActorLocation().Equals(Start->GetActorLocation(), 0.0f));
	}
	const TWeakObjectPtr<UWorld> OldWorld = World;
	TestEqual("Browse it again",
		static_cast<int32>(
			Engine->Browse(Context, FURL(nullptr, TEXT("/MapTest/Maps/Opened"), TRAVEL_Absolute), Error)),
		static_cast<int32>(EBrowseReturnVal::Success));
	TestFalse("The old world is gone", OldWorld.IsValid());
	TestTrue(
		"A new world from the package", Engine->GetGameWorld() != nullptr && Engine->GetGameWorld()->HasBegunPlay());
	TestTrue("Its actors again", Engine->GetGameWorld()->FindFirst<ACameraActor>() != nullptr);

	// Opened by its file, outside every mount point: `.../MapPackage` (the folder above Maps/) mounts as /MapPackage/.
	FPackageName::UnRegisterMountPoint(TEXT("/MapTest/"), MapTestDir());
	const FString File = MapTestDir() + TEXT("Maps/Opened.lmap");
	TestEqual("Browse the file",
		static_cast<int32>(Engine->Browse(Context, FURL(nullptr, *File, TRAVEL_Absolute), Error)),
		static_cast<int32>(EBrowseReturnVal::Success));
	TestTrue("Mounted", FPackageName::MountPointExists(TEXT("/MapPackage/")));
	TestTrue("The file's package",
		Engine->GetGameWorld() != nullptr &&
			Engine->GetGameWorld()->GetOutermost()->GetName() == TEXT("/MapPackage/Maps/Opened"));
	TestEqual("A missing map",
		static_cast<int32>(
			Engine->Browse(Context, FURL(nullptr, TEXT("/MapPackage/Maps/None"), TRAVEL_Absolute), Error)),
		static_cast<int32>(EBrowseReturnVal::Failure));
	Engine->PreExit();
	Engine.Reset();
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	FPackageName::UnRegisterMountPoint(TEXT("/MapPackage/"), MapTestDir());
	FPackageName::RegisterMountPoint(TEXT("/MapTest/"), MapTestDir());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMapPackageRotatingMovementTurnsTest, "System.Engine.MapPackage.RotatingMovementTurns",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FMapPackageRotatingMovementTurnsTest::RunTest(const FString& Parameters)
{
	// A map's rotating movement turns its actor's root as the world ticks (UE: URotatingMovementComponent).
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	ATargetPoint* Spinner = World.SpawnActor<ATargetPoint>(FVector(100.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
	URotatingMovementComponent* Spin = NewObject<URotatingMovementComponent>(Spinner, TEXT("RotatingMovement"));
	Spin->RotationRate = FRotator(0.0f, 90.0f, 0.0f);
	Spin->RegisterComponent();

	for (int32 Step = 0; Step < 4; ++Step)
	{
		World.Tick(0.125f);
	}
	TestEqual("Spun 45 degrees in half a second", Spinner->GetActorRotation().Yaw, 45.0f, 1.0e-3f);
	TestTrue("The spin keeps the location", Spinner->GetActorLocation().Equals(FVector(100.0f, 0.0f, 0.0f), 1.0e-3f));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
