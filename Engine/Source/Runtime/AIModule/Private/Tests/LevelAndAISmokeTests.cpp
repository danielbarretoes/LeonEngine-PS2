#include "CoreMinimal.h"
#include "GameFramework/WorldSettings.h"
#include "GameplayMinimal.h"
#include "Level/LegacyLevelDataComponent.h"
#include "Level/LeonLevelFormat.h"
#include "Level/LevelLoader.h"
#include "Misc/AutomationTest.h"
#include "ResourceCache.h"
#include "Tests/ScopedTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLevelAndAISmokeEditorStyleLevelSaveLoadApplyTest,
	"System.AIModule.LevelAndAISmoke.EditorStyleLevelSaveLoadApplyHeadless",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLevelAndAISmokeEditorStyleLevelSaveLoadApplyTest::RunTest(const FString& Parameters)
{
	// A headless engine loads the blank template, and its level document survives a byte round trip and re-apply.
	#ifdef LEON_ROOT_DIR
	const FString TemplateLevel = FString(LEON_ROOT_DIR) + "/Engine/Content/LevelTemplates/Blank.llev";
	FScopedTestWorld TestWorld;
	FResourceCache Resources;
	Resources.SetTextureLoadingEnabled(false);
	if (!TestTrue("Template level loaded", LoadLevelFile(*TestWorld, Resources, TemplateLevel)))
	{
		return false;
	}

	// The camera the level opens with (its framing on the world settings).
	UCameraComponent& Camera = *NewObject<UCameraComponent>();
	TestWorld->GetWorldSettings()->FindComponentByClass<ULegacyLevelDataComponent>()->ApplyCameraFraming(Camera);
	FLevelDocument Doc = BuildLevelDocument(*TestWorld->PersistentLevel, Camera);
	TestFalse("Document has a name", Doc.Name.IsEmpty());

	const TArray<uint8> Bytes = SerializeLeonLevel(Doc);
	TestTrue("Bytes written", Bytes.Num() > 0);

	FLevelDocument RoundTrip;
	if (!TestTrue("Deserialized", DeserializeLeonLevel(Bytes, RoundTrip)))
	{
		return false;
	}
	TestEqual("Name kept", RoundTrip.Name, Doc.Name);

	TestTrue("Document applied", ApplyLevelDocument(*TestWorld, Resources, RoundTrip, "memory-editor-smoke"));
	#else
	AddInfo("LEON_ROOT_DIR unset");
	#endif
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
