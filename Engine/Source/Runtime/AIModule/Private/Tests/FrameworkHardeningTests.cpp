#include "AI/Navigation/NavigationSystem.h"
#include "AudioDevice.h"
#include "Components/TextBlock.h"
#include "Engine/GameEngine.h"
#include "GameFramework/HUD.h"
#include "GameplayMinimal.h"
#include "Level/LeonLevelFormat.h"
#include "Physics/PhysScene.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cstring>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>

using Catch::Matchers::WithinAbs;

TEST_CASE("BehaviorTree Sequence and Selector with Blackboard", "[gameplay][bt]")
{
	UBlackboardComponent Board;
	UBTDecorator_Bool HasTarget("HasTarget", true);
	int Ran = 0;
	UBTTask_Action Act(
		[&](UBlackboardComponent& B, float)
		{
			++Ran;
			B.SetBool("DidAct", true);
			return EBTNodeResult::Succeeded;
		});
	UBTComposite_Sequence Seq({&HasTarget, &Act});

	REQUIRE(Seq.Tick(Board, 0.016f) == EBTNodeResult::Failed);
	Board.SetBool("HasTarget", true);
	REQUIRE(Seq.Tick(Board, 0.016f) == EBTNodeResult::Succeeded);
	REQUIRE(Ran == 1);
	REQUIRE(Board.GetBool("DidAct"));

	UBTDecorator_Bool Never("Never", true);
	UBTComposite_Selector Sel({&Never, &Act});
	REQUIRE(Sel.Tick(Board, 0.0f) == EBTNodeResult::Succeeded);
	REQUIRE(Ran == 2);
}

TEST_CASE("AIController logic state tracks MoveTo Chase Idle", "[gameplay][ai]")
{
	UWorld World;
	auto* Character = World.SpawnActor<ACharacter>();
	AAIController Ai;
	Ai.Possess(Character);
	REQUIRE(Ai.GetLogicState() == EAILogicState::Idle);
	Ai.MoveToLocation({3.0f, 0.0f, 0.0f});
	REQUIRE(Ai.GetLogicState() == EAILogicState::MoveTo);
	Ai.MoveToActor(Character);
	REQUIRE(Ai.GetLogicState() == EAILogicState::Chase);
	Ai.StopMovement();
	REQUIRE(Ai.GetLogicState() == EAILogicState::Idle);
}

TEST_CASE("AudioDevice silent mode is safe for Play APIs", "[audio]")
{
	FAudioDevice Audio;
	REQUIRE(Audio.Initialize(/*silent=*/true));
	Audio.PlaySound2D("does-not-exist.wav");
	Audio.PlayUiSound(EUISound::Click);
	Audio.PlayMusic("MenuBed.wav");
	Audio.StopMusic();
	Audio.Tick();
	Audio.Shutdown();
}

TEST_CASE("HUD AddWidget TextBlock and remove", "[ui][hud]")
{
	AHUD Hud;
	auto* Text = Hud.AddWidget<UTextBlock>();
	REQUIRE(Text != nullptr);
	Text->SetText(FText::FromString("Hello"));
	REQUIRE(Hud.GetWidgetOfClass<UTextBlock>() == Text);
	Hud.Tick(0.016f);
	Hud.RemoveWidget(Text);
	REQUIRE(Hud.GetWidgetOfClass<UTextBlock>() == nullptr);
}

TEST_CASE("DeserializeLeonLevel adversarial inputs", "[content][fuzz]")
{
	FLevelDocument Doc;
	std::vector<std::uint8_t> Empty;
	REQUIRE_FALSE(DeserializeLeonLevel(Empty, Doc));

	std::vector<std::uint8_t> Junk(64, 0xA5);
	REQUIRE_FALSE(DeserializeLeonLevel(Junk, Doc));

	std::vector<std::uint8_t> AlmostMagic = {'L', 'L', 'E', 'V', 1, 0, 0, 0};
	REQUIRE_FALSE(DeserializeLeonLevel(AlmostMagic, Doc));
}

TEST_CASE("NavigationSystem agent radius dilation shrinks walkable ring", "[gameplay][nav]")
{
	FPhysScene Physics;
	FBodyInstance Floor{};
	Floor.Type = EBodyType::Static;
	Floor.Position = {0.0f, 0.0f, 0.0f};
	Floor.HalfExtents = {20.0f, 0.5f, 20.0f};
	Physics.GetBodies().Add(Floor);

	FBodyInstance Pillar{};
	Pillar.Type = EBodyType::Static;
	Pillar.Position = {0.0f, 1.0f, 0.0f};
	Pillar.HalfExtents = {0.4f, 1.5f, 0.4f};
	Physics.GetBodies().Add(Pillar);

	UNavigationSystem Narrow;
	Narrow.SetCellSize(0.5f);
	Narrow.SetAgentRadius(0.35f);
	Narrow.BuildFromPhysScene(Physics, 0.0f, 10.0f);

	UNavigationSystem Wide;
	Wide.SetCellSize(0.5f);
	Wide.SetAgentRadius(1.5f);
	Wide.BuildFromPhysScene(Physics, 0.0f, 10.0f);

	REQUIRE(Wide.GetWalkableCellCount() < Narrow.GetWalkableCellCount());
}
