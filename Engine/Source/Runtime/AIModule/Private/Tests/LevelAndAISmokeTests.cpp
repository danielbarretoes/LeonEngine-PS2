#include "CoreMinimal.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "GameplayMinimal.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Tests/ScopedTestWorld.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLevelAndAISmokeEditorStyleMapResaveTest,
	"System.AIModule.LevelAndAISmoke.EditorStyleMapResaveHeadless",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLevelAndAISmokeEditorStyleMapResaveTest::RunTest(const FString& Parameters)
{
	// A headless tool loads the engine's template maps and saves them again (ResavePackages): a loaded map writes the
	// bytes of its file (plan decision D13), its world settings first and every transform exact.
	for (const TCHAR* MapName : {TEXT("/Engine/Maps/Entry"), TEXT("/Engine/Maps/Template_Default")})
	{
		FString Filename;
		if (!TestTrue(*FString::Printf(TEXT("%s exists"), MapName),
				FPackageName::DoesPackageExist(MapName, nullptr, &Filename)))
		{
			continue;
		}
		TArray<uint8> FileBytes;
		(void)FFileHelper::LoadFileToArray(FileBytes, *Filename);
		UPackage* Package = LoadPackage(nullptr, MapName, LOAD_None);
		UWorld* World = UWorld::FindWorldInPackage(Package);
		if (!TestNotNull(*FString::Printf(TEXT("%s's world"), MapName), World))
		{
			continue;
		}
		TestTrue("The world settings first",
			World->PersistentLevel->Actors.Num() > 0 && World->PersistentLevel->Actors[0] == World->GetWorldSettings());
		TArray<uint8> Resaved;
		TestTrue("Resaved", UPackage::SaveToMemory(Package, World, RF_Public | RF_Standalone, Resaved).IsSuccessful());
		TestTrue(*FString::Printf(TEXT("%s: the same bytes"), MapName), Resaved == FileBytes);
		Package->MarkPendingKill();
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLevelAndAISmokeAIChaseBehaviorMoveToWhenTargetPresentTest,
	"System.AIModule.LevelAndAISmoke.AIChaseBehaviorMoveToWhenTargetPresent",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLevelAndAISmokeAIChaseBehaviorMoveToWhenTargetPresentTest::RunTest(const FString& Parameters)
{
	// The chase behavior tree chases while a target exists and returns the controller to idle without one.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	ACharacter* Character = World.SpawnActor<ACharacter>();
	ACharacter* Target = World.SpawnActor<ACharacter>();
	Target->SetActorLocationAndRotation(FVector(500.0f, 0.0f, 0.0f), FRotator::ZeroRotator);

	AAIController& Ai = *World.SpawnActor<AAIController>();
	Ai.Possess(Character);
	FAIChaseBehavior Chase;
	(void)Chase.Tick(Ai, Target, 0.016f);
	TestTrue("Chasing the target", Ai.GetLogicState() == EAILogicState::Chase);
	(void)Chase.Tick(Ai, nullptr, 0.016f);
	TestTrue("Idle without a target", Ai.GetLogicState() == EAILogicState::Idle);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
