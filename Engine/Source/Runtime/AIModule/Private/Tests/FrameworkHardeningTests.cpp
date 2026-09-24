#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <limits>
#include <cstring>
#include <filesystem>
#include "Engine/GameEngine.h"
#include "GameplayMinimal.h"
#include "AudioDevice.h"
#include "AI/Navigation/NavigationSystem.h"
#include "Level/LeonLevelFormat.h"
#include "Net/NetProtocol.h"
#include "Net/RootReplication.h"
#include "Physics/PhysScene.h"
#include "GameFramework/HUD.h"
#include "Components/TextBlock.h"
#include <string>
#include <vector>

using Catch::Matchers::WithinAbs;

TEST_CASE("BehaviorTree Sequence and Selector with Blackboard", "[gameplay][bt]") {
    UBlackboardComponent Board;
    UBTDecorator_Bool HasTarget("HasTarget", true);
    int Ran = 0;
    UBTTask_Action Act([&](UBlackboardComponent& B, float) {
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

TEST_CASE("AIController logic state tracks MoveTo Chase Idle", "[gameplay][ai]") {
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

TEST_CASE("RootReplication relevancy and CaptureCharacterRoot", "[net][replication]") {
    ACharacter Character;
    Character.SetActorLocationAndRotation({1.0f, 0.0f, 2.0f}, 45.0f);
    const Leon::Net::FPawnSnap Snap = Leon::Net::CaptureCharacterRoot(3, Character, 10.0f, -5.0f);
    REQUIRE(Snap.Slot == 3);
    REQUIRE_THAT(Snap.X, WithinAbs(1.0f, 1.0e-5f));
    REQUIRE_THAT(Snap.Z, WithinAbs(2.0f, 1.0e-5f));
    REQUIRE_THAT(Snap.Yaw, WithinAbs(45.0f, 1.0e-5f));
    REQUIRE_THAT(Snap.BoomYaw, WithinAbs(10.0f, 1.0e-5f));

    REQUIRE(Leon::Net::IsPawnRelevant({0, 0, 0}, {3, 0, 0}, 5.0f));
    REQUIRE_FALSE(Leon::Net::IsPawnRelevant({0, 0, 0}, {10, 0, 0}, 5.0f));
    REQUIRE(Leon::Net::IsPawnRelevantXZ({0, 0, 0}, {0, 99, 4}, 5.0f));
}

TEST_CASE("AudioDevice silent mode is safe for Play APIs", "[audio]") {
    FAudioDevice Audio;
    REQUIRE(Audio.Initialize(/*silent=*/true));
    Audio.PlaySound2D("does-not-exist.wav");
    Audio.PlayUiSound(EUISound::Click);
    Audio.PlayMusic("MenuBed.wav");
    Audio.StopMusic();
    Audio.Tick();
    Audio.Shutdown();
}

TEST_CASE("HUD AddWidget TextBlock and remove", "[ui][hud]") {
    AHUD Hud;
    auto* Text = Hud.AddWidget<UTextBlock>();
    REQUIRE(Text != nullptr);
    Text->SetText("Hello");
    REQUIRE(Hud.GetWidgetOfClass<UTextBlock>() == Text);
    Hud.Tick(0.016f);
    Hud.RemoveWidget(Text);
    REQUIRE(Hud.GetWidgetOfClass<UTextBlock>() == nullptr);
}

TEST_CASE("DeserializeLeonLevel and InputCmd adversarial inputs", "[content][fuzz][net]") {
    FLevelDocument Doc;
    std::vector<std::uint8_t> Empty;
    REQUIRE_FALSE(DeserializeLeonLevel(Empty, Doc));

    std::vector<std::uint8_t> Junk(64, 0xA5);
    REQUIRE_FALSE(DeserializeLeonLevel(Junk, Doc));

    std::vector<std::uint8_t> AlmostMagic = {'L', 'L', 'E', 'V', 1, 0, 0, 0};
    REQUIRE_FALSE(DeserializeLeonLevel(AlmostMagic, Doc));

    Leon::Net::FInputCmdMsg Cmd{};
    Cmd.MoveX = std::numeric_limits<float>::quiet_NaN();
    Cmd.Buttons = 0xFFFF;
    Leon::Net::SanitizeInputCmd(Cmd);
    REQUIRE_THAT(Cmd.MoveX, WithinAbs(0.0f, 1.0e-5f));
    REQUIRE(Cmd.Buttons == Leon::Net::InputButtonMask);
    REQUIRE((Cmd.Buttons & static_cast<std::uint16_t>(~Leon::Net::InputButtonMask)) == 0);
}

TEST_CASE("NavigationSystem agent radius dilation shrinks walkable ring", "[gameplay][nav]") {
    FPhysScene Physics;
    FBodyInstance Floor{};
    Floor.Type = EBodyType::Static;
    Floor.Position = {0.0f, 0.0f, 0.0f};
    Floor.HalfExtents = {20.0f, 0.5f, 20.0f};
    Physics.GetBodies().push_back(Floor);

    FBodyInstance Pillar{};
    Pillar.Type = EBodyType::Static;
    Pillar.Position = {0.0f, 1.0f, 0.0f};
    Pillar.HalfExtents = {0.4f, 1.5f, 0.4f};
    Physics.GetBodies().push_back(Pillar);

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
