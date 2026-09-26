#include "BehaviorTree/BehaviorTree.h"
#include "CoreMinimal.h"
#include "Engine/BlockingVolume.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Perception/PawnSensingComponent.h"
#include "Tests/GameplayTestTypes.h"
#include "Tests/ScopedTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

// P20's AI pieces: the typed blackboard and the pawn's senses (sight in a cone with a line of sight, hearing of
// AActor::MakeNoise through the module's noise delegate).

namespace
{

	/** A pawn at Location facing Yaw, with a sensing component that senses every pawn. */
	UPawnSensingComponent* SpawnObserver(UWorld& World, const FVector& Location, float Yaw, ATestPawn*& OutPawn)
	{
		OutPawn = World.SpawnActor<ATestPawn>(Location, FRotator(0.0f, Yaw, 0.0f));
		UPawnSensingComponent* Sensing = NewObject<UPawnSensingComponent>(OutPawn);
		Sensing->RegisterComponent();
		Sensing->bOnlySensePlayers = false;
		Sensing->SightRadius = 2000.0f;
		Sensing->SetPeripheralVisionAngle(60.0f);
		return Sensing;
	}

	/** A wall 3 m tall across Y at X, 4 m wide. */
	void SpawnSensingWall(UWorld& World, float X)
	{
		(void)World.SpawnActor<ABlockingVolume>(ABlockingVolume::StaticClass(),
			FTransform(FQuat::Identity, FVector(X, 0.0f, 150.0f), FVector(0.2f, 4.0f, 3.0f)));
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAIBlackboardTypedKeysTest, "System.AIModule.Blackboard.TypedKeys",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FAIBlackboardTypedKeysTest::RunTest(const FString& Parameters)
{
	// A key takes the type of its first value; a get of another type or an unset key reads the default; a set of
	// another type is refused; ClearValue keeps the type.
	FScopedTestWorld TestWorld;
	UBlackboardComponent Board;
	Board.SetValueAsBool(TEXT("HasTarget"), true);
	Board.SetValueAsVector(TEXT("Goal"), FVector(1.0f, 2.0f, 3.0f));
	Board.SetValueAsFloat(TEXT("Aggression"), 0.5f);
	Board.SetValueAsInt(TEXT("Round"), 3);
	Board.SetValueAsName(TEXT("Site"), FName(TEXT("A")));
	ATestActor* Actor = TestWorld->SpawnActor<ATestActor>();
	Board.SetValueAsObject(TEXT("Enemy"), Actor);
	TestTrue("A bool", Board.GetValueAsBool(TEXT("HasTarget")));
	TestTrue("A vector", Board.GetValueAsVector(TEXT("Goal")) == FVector(1.0f, 2.0f, 3.0f));
	TestEqual("A float", Board.GetValueAsFloat(TEXT("Aggression")), 0.5f);
	TestEqual("An int", Board.GetValueAsInt(TEXT("Round")), 3);
	TestTrue("A name", Board.GetValueAsName(TEXT("Site")) == FName(TEXT("A")));
	TestTrue("An object", Board.GetValueAsObject<ATestActor>(TEXT("Enemy")) == Actor);
	TestTrue("The key's type", Board.GetKeyType(TEXT("Goal")) == EBlackboardKeyType::Vector);

	TestFalse("The wrong type reads the default", Board.GetValueAsBool(TEXT("Goal")));
	TestTrue(
		"Unset", Board.GetKeyType(TEXT("Nothing")) == EBlackboardKeyType::None && !Board.IsValueSet(TEXT("Nothing")));
	Board.SetValueAsFloat(TEXT("HasTarget"), 2.0f);
	TestTrue("Another type is refused",
		Board.GetValueAsBool(TEXT("HasTarget")) && Board.GetKeyType(TEXT("HasTarget")) == EBlackboardKeyType::Bool);
	Board.ClearValue(TEXT("Goal"));
	TestFalse("Cleared", Board.IsValueSet(TEXT("Goal")));
	TestTrue("It keeps its type", Board.GetKeyType(TEXT("Goal")) == EBlackboardKeyType::Vector);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAIPawnSensingSightTest, "System.AIModule.PawnSensing.Sight",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FAIPawnSensingSightTest::RunTest(const FString& Parameters)
{
	// A pawn ahead within range is seen; one behind, one beyond SightRadius and one behind a wall are not;
	// bOnlySensePlayers keeps the pawns without a player controller out.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	ATestPawn* Observer = nullptr;
	UPawnSensingComponent* Sensing = SpawnObserver(World, FVector::ZeroVector, 0.0f, Observer);
	ATestPawn* Ahead = World.SpawnActor<ATestPawn>(FVector(800.0f, 100.0f, 0.0f), FRotator::ZeroRotator);
	ATestPawn* Behind = World.SpawnActor<ATestPawn>(FVector(-800.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
	ATestPawn* Far = World.SpawnActor<ATestPawn>(FVector(3000.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
	ATestPawn* Hidden = World.SpawnActor<ATestPawn>(FVector(700.0f, -1200.0f, 0.0f), FRotator::ZeroRotator);
	(void)World.SpawnActor<ABlockingVolume>(ABlockingVolume::StaticClass(),
		FTransform(FQuat::Identity, FVector(350.0f, -600.0f, 150.0f), FVector(0.2f, 4.0f, 3.0f)));

	TArray<APawn*> Seen;
	Sensing->OnSeePawn.AddLambda([&Seen](APawn* Pawn) { Seen.AddUnique(Pawn); });
	Sensing->UpdateAISensing();
	TestTrue("Ahead: seen", Seen.Contains(Ahead));
	TestFalse("Behind: not", Seen.Contains(Behind));
	TestFalse("Too far: not", Seen.Contains(Far));
	TestFalse("Behind a wall: not", Seen.Contains(Hidden));
	TestFalse("Not itself", Seen.Contains(Observer));
	TestTrue("A line of sight to the one ahead", Sensing->HasLineOfSightTo(Ahead));
	TestFalse("None through the wall", Sensing->HasLineOfSightTo(Hidden));

	Seen.Reset();
	Sensing->bOnlySensePlayers = true;
	Sensing->UpdateAISensing();
	TestEqual("Players only: no player here", Seen.Num(), 0);

	// The component's own tick looks every SensingInterval.
	Sensing->bOnlySensePlayers = false;
	Sensing->SensingInterval = 0.25f;
	Seen.Reset();
	World.Tick(0.1f);
	World.Tick(0.3f);
	TestTrue("Seen by the tick", Seen.Contains(Ahead));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAIPawnSensingHearingTest, "System.AIModule.PawnSensing.Hearing",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FAIPawnSensingHearingTest::RunTest(const FString& Parameters)
{
	// AActor::MakeNoise reaches the sensing components: within HearingThreshold through a wall, within
	// LOSHearingThreshold only in the open, never beyond, and not the listener's own noise; Loudness scales the reach.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	ATestPawn* Listener = nullptr;
	UPawnSensingComponent* Sensing = SpawnObserver(World, FVector::ZeroVector, 0.0f, Listener);
	Sensing->HearingThreshold = 1000.0f;
	Sensing->LOSHearingThreshold = 2000.0f;
	SpawnSensingWall(World, -600.0f);
	int32 Heard = 0;
	APawn* LastInstigator = nullptr;
	Sensing->OnHearNoise.AddLambda(
		[&Heard, &LastInstigator](APawn* Instigator, const FVector&, float)
		{
			++Heard;
			LastInstigator = Instigator;
		});

	ATestPawn* Near = World.SpawnActor<ATestPawn>(FVector(-900.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
	ATestPawn* OpenFar = World.SpawnActor<ATestPawn>(FVector(1500.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
	ATestPawn* WalledFar = World.SpawnActor<ATestPawn>(FVector(-1500.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
	ATestPawn* TooFar = World.SpawnActor<ATestPawn>(FVector(2500.0f, 0.0f, 0.0f), FRotator::ZeroRotator);

	Near->MakeNoise();
	TestEqual("Near, through the wall", Heard, 1);
	TestTrue("Its instigator", LastInstigator == Near);
	OpenFar->MakeNoise();
	TestEqual("Far in the open", Heard, 2);
	WalledFar->MakeNoise();
	TestEqual("Far behind the wall: not", Heard, 2);
	TooFar->MakeNoise();
	TestEqual("Too far: not", Heard, 2);
	TooFar->MakeNoise(2.0f);
	TestEqual("Louder carries further", Heard, 3);
	Listener->MakeNoise();
	TestEqual("Not its own", Heard, 3);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
