#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/Info.h"
#include "GameFramework/Pawn.h"
#include "Misc/AutomationTest.h"
#include "Tests/EngineTestTypes.h"
#include "Tests/ScopedTestWorld.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/WeakObjectPtrTemplates.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldCreateWorldOwnsItsLevelTest, "System.Engine.World.CreateWorldOwnsItsLevel",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FWorldCreateWorldOwnsItsLevelTest::RunTest(const FString& Parameters)
{
	// A new world lives in a transient package, owns its persistent level and plays at once; destroying it and
	// collecting frees the world, the level and the actors.
	TWeakObjectPtr<UWorld> WeakWorld;
	TWeakObjectPtr<ULevel> WeakLevel;
	TWeakObjectPtr<AActor> WeakActor;
	{
		FScopedTestWorld TestWorld;
		UWorld& World = *TestWorld;
		TestTrue("Game world", World.GetWorldType() == EWorldType::Game);
		TestTrue("Package outer", World.GetOuter() != nullptr && World.GetOuter()->IsA<UPackage>());
		TestTrue("Transient package", World.GetOutermost()->HasAnyFlags(RF_Transient));
		if (!TestNotNull("Persistent level", World.PersistentLevel))
		{
			return false;
		}
		TestTrue("Level outer is the world", World.PersistentLevel->GetOuter() == &World);
		TestTrue("Level owning world", World.PersistentLevel->OwningWorld == &World);
		TestTrue("Plays from the start", World.HasBegunPlay());
		TestTrue("Rooted", World.IsRooted());

		AActor* Actor = World.SpawnActor<AActor>();
		TestTrue("Actor outer is the level", Actor->GetOuter() == World.PersistentLevel);
		TestTrue("Actor in the level", World.PersistentLevel->Actors.Contains(Actor));
		WeakWorld = &World;
		WeakLevel = World.PersistentLevel;
		WeakActor = Actor;
	}
	TestFalse("World collected", WeakWorld.IsValid());
	TestFalse("Level collected", WeakLevel.IsValid());
	TestFalse("Actor collected", WeakActor.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldSpawnActorParametersTest, "System.Engine.World.SpawnActorParameters",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FWorldSpawnActorParametersTest::RunTest(const FString& Parameters)
{
	// SpawnActor places the root, names the actor, sets owner and instigator, and refuses null or abstract classes.
	// The refused spawns log a LogSpawn warning each (as in UE).
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;

	APawn* Instigator = World.SpawnActor<APawn>();
	AActor* Owner = World.SpawnActor<AActor>();
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Name = FName(TEXT("NamedActor"));
	SpawnInfo.Owner = Owner;
	SpawnInfo.Instigator = Instigator;
	AActor* Actor = World.SpawnActor<AActor>(FVector(100.0f, 200.0f, 300.0f), FRotator(0.0f, 90.0f, 0.0f), SpawnInfo);
	if (!TestNotNull("Spawned", Actor))
	{
		return false;
	}
	TestEqual("Name", Actor->GetName(), FString(TEXT("NamedActor")));
	TestTrue("Owner", Actor->GetOwner() == Owner);
	TestTrue("Instigator", Actor->GetInstigator() == Instigator);
	TestTrue("Location", Actor->GetActorLocation().Equals(FVector(100.0f, 200.0f, 300.0f), 0.0f));
	TestTrue("Rotation", Actor->GetActorRotation().Equals(FRotator(0.0f, 90.0f, 0.0f), 0.0f));
	TestTrue("Serial after the first two", Actor->GetUniqueID() > Owner->GetUniqueID());

	FTransform Transform(FRotator(0.0f, 45.0f, 0.0f), FVector(1.0f, 2.0f, 3.0f), FVector(2.0f, 2.0f, 2.0f));
	AActor* Scaled = World.SpawnActor<AActor>(AActor::StaticClass(), Transform);
	TestTrue("Scale from the transform", Scaled->GetActorScale3D().Equals(FVector(2.0f, 2.0f, 2.0f), 1.0e-5f));

	TestNull("Null class refused", World.SpawnActor(nullptr));
	TestNull("Abstract class refused", World.SpawnActor(AInfo::StaticClass()));
	TestEqual("Four actors", World.ActorCount(), static_cast<SIZE_T>(4));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldForEachVisitsEveryActorWhenOneIsDestroyedTest,
	"System.Engine.World.ForEachVisitsEveryActorWhenOneIsDestroyed",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FWorldForEachVisitsEveryActorWhenOneIsDestroyedTest::RunTest(const FString& Parameters)
{
	// Outside the tick (BeginPlay walks the actors with ForEach): an actor destroyed during the visit leaves its slot
	// null until the visit ends, so the actor after it is still visited, and the level is compacted afterwards.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AActor* First = World.SpawnActor<AActor>(FVector::ZeroVector, FRotator::ZeroRotator);
	AActor* Second = World.SpawnActor<AActor>(FVector::ZeroVector, FRotator::ZeroRotator);
	AActor* Third = World.SpawnActor<AActor>(FVector::ZeroVector, FRotator::ZeroRotator);
	TArray<AActor*> Visited;
	World.ForEach<AActor>(
		[&Visited, First](AActor& Actor)
		{
			Visited.Add(&Actor);
			if (&Actor == First)
			{
				(void)Actor.Destroy();
			}
		});
	TestTrue("The first", Visited.Contains(First));
	TestTrue("The one after the destroyed one", Visited.Contains(Second));
	TestTrue("The last", Visited.Contains(Third));
	TestFalse("Compacted", World.PersistentLevel->Actors.Contains(nullptr));
	TestFalse("The destroyed one left", World.PersistentLevel->Actors.Contains(First));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldSpawnDuringTickJoinsAfterTheTickTest,
	"System.Engine.World.SpawnDuringTickJoinsAfterTheTick",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FWorldSpawnDuringTickJoinsAfterTheTickTest::RunTest(const FString& Parameters)
{
	// An actor spawned during the world tick joins the level and begins play once the tick ends; one destroyed during
	// the tick ends play at once and its slot is removed once the tick ends.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AActor* Victim = World.SpawnActor<AActor>();
	AEngineTestTickSpawner* Spawner = World.SpawnActor<AEngineTestTickSpawner>();
	Spawner->Victim = Victim;

	World.Tick(1.0f / 60.0f);
	if (!TestNotNull("Spawned during the tick", Spawner->Spawned))
	{
		return false;
	}
	TestFalse("Not in the level during the tick", Spawner->bSpawnedInLevelDuringTick);
	TestFalse("Not begun during the tick", Spawner->bSpawnedBegunDuringTick);
	TestTrue("Victim slot nulled during the tick", Spawner->bVictimSlotNulledDuringTick);
	TestTrue("In the level after the tick", World.PersistentLevel->Actors.Contains(Spawner->Spawned));
	TestTrue("Begun after the tick", Spawner->Spawned->HasActorBegunPlay());
	TestTrue("Victim pending kill", Victim->IsPendingKillPending());
	TestFalse("Null slots compacted", World.PersistentLevel->Actors.Contains(nullptr));
	TestEqual("Spawner and spawned", World.ActorCount(), static_cast<SIZE_T>(2));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldDestroyActorEndsPlayAndCollectsTest,
	"System.Engine.World.DestroyActorEndsPlayAndCollects",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FWorldDestroyActorEndsPlayAndCollectsTest::RunTest(const FString& Parameters)
{
	// Destroy ends play and unregisters the components; the next collection frees the actor and its components and
	// clears the references to it.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	ACharacter* Character = World.SpawnActor<ACharacter>();
	TestTrue("Begun play", Character->HasActorBegunPlay());
	USceneComponent* Root = Character->GetRootComponent();
	TestTrue("Root registered", Root->IsRegistered());

	TWeakObjectPtr<ACharacter> WeakCharacter = Character;
	TWeakObjectPtr<USceneComponent> WeakRoot = Root;
	TestTrue("Destroy", Character->Destroy());
	TestFalse("Ended play", Character->HasActorBegunPlay());
	TestFalse("Root unregistered", Root->IsRegistered());
	TestTrue("Pending kill", Character->IsPendingKill());
	TestEqual("No actors", World.ActorCount(), static_cast<SIZE_T>(0));

	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	TestFalse("Actor collected", WeakCharacter.IsValid());
	TestFalse("Root collected", WeakRoot.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldGameInstanceOwnsTheWorldContextTest,
	"System.Engine.World.GameInstanceOwnsTheWorldContext",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FWorldGameInstanceOwnsTheWorldContextTest::RunTest(const FString& Parameters)
{
	// InitializeStandalone creates the context's world; destroying it clears the context and the collection frees it.
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	GameInstance->AddToRoot();
	GameInstance->InitializeStandalone();
	UWorld* World = GameInstance->GetWorld();
	if (!TestNotNull("Context world", World))
	{
		GameInstance->RemoveFromRoot();
		return false;
	}
	TestTrue("World knows its game instance", World->GetGameInstance() == GameInstance);
	TestTrue("Context owner", GameInstance->GetWorldContext()->OwningGameInstance == GameInstance);

	TWeakObjectPtr<UWorld> WeakWorld = World;
	GameInstance->DestroyWorldContextWorld();
	TestNull("Context cleared", GameInstance->GetWorld());
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	TestFalse("World collected", WeakWorld.IsValid());
	GameInstance->RemoveFromRoot();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
