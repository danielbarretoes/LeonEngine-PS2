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
    UBlackboardComponent board;
    UBTDecorator_Bool hasTarget("HasTarget", true);
    int ran = 0;
    UBTTask_Action act([&](UBlackboardComponent& b, float) {
        ++ran;
        b.SetBool("DidAct", true);
        return EBTNodeResult::Succeeded;
    });
    UBTComposite_Sequence seq({&hasTarget, &act});

    REQUIRE(seq.Tick(board, 0.016f) == EBTNodeResult::Failed);
    board.SetBool("HasTarget", true);
    REQUIRE(seq.Tick(board, 0.016f) == EBTNodeResult::Succeeded);
    REQUIRE(ran == 1);
    REQUIRE(board.GetBool("DidAct"));

    UBTDecorator_Bool never("Never", true);
    UBTComposite_Selector sel({&never, &act});
    REQUIRE(sel.Tick(board, 0.0f) == EBTNodeResult::Succeeded);
    REQUIRE(ran == 2);
}

TEST_CASE("AIController logic state tracks MoveTo Chase Idle", "[gameplay][ai]") {
    UWorld world;
    auto* character = world.SpawnActor<ACharacter>();
    AAIController ai;
    ai.Possess(character);
    REQUIRE(ai.GetLogicState() == EAILogicState::Idle);
    ai.MoveToLocation({3.0f, 0.0f, 0.0f});
    REQUIRE(ai.GetLogicState() == EAILogicState::MoveTo);
    ai.MoveToActor(character);
    REQUIRE(ai.GetLogicState() == EAILogicState::Chase);
    ai.StopMovement();
    REQUIRE(ai.GetLogicState() == EAILogicState::Idle);
}

TEST_CASE("RootReplication relevancy and CaptureCharacterRoot", "[net][replication]") {
    ACharacter character;
    character.SetActorLocationAndRotation({1.0f, 0.0f, 2.0f}, 45.0f);
    const Leon::Net::FPawnSnap snap = Leon::Net::CaptureCharacterRoot(3, character, 10.0f, -5.0f);
    REQUIRE(snap.slot == 3);
    REQUIRE_THAT(snap.x, WithinAbs(1.0f, 1.0e-5f));
    REQUIRE_THAT(snap.z, WithinAbs(2.0f, 1.0e-5f));
    REQUIRE_THAT(snap.yaw, WithinAbs(45.0f, 1.0e-5f));
    REQUIRE_THAT(snap.boomYaw, WithinAbs(10.0f, 1.0e-5f));

    REQUIRE(Leon::Net::IsPawnRelevant({0, 0, 0}, {3, 0, 0}, 5.0f));
    REQUIRE_FALSE(Leon::Net::IsPawnRelevant({0, 0, 0}, {10, 0, 0}, 5.0f));
    REQUIRE(Leon::Net::IsPawnRelevantXZ({0, 0, 0}, {0, 99, 4}, 5.0f));
}

TEST_CASE("AudioDevice silent mode is safe for Play APIs", "[audio]") {
    FAudioDevice audio;
    REQUIRE(audio.Initialize(/*silent=*/true));
    audio.PlaySound2D("does-not-exist.wav");
    audio.PlayUiSound(EUISound::Click);
    audio.PlayMusic("MenuBed.wav");
    audio.StopMusic();
    audio.Tick();
    audio.Shutdown();
}

TEST_CASE("HUD AddWidget TextBlock and remove", "[ui][hud]") {
    AHUD hud;
    auto* text = hud.AddWidget<UTextBlock>();
    REQUIRE(text != nullptr);
    text->SetText("Hello");
    REQUIRE(hud.GetWidgetOfClass<UTextBlock>() == text);
    hud.Tick(0.016f);
    hud.RemoveWidget(text);
    REQUIRE(hud.GetWidgetOfClass<UTextBlock>() == nullptr);
}

TEST_CASE("DeserializeLeonLevel and InputCmd adversarial inputs", "[content][fuzz][net]") {
    FLevelDocument doc;
    std::vector<std::uint8_t> empty;
    REQUIRE_FALSE(DeserializeLeonLevel(empty, doc));

    std::vector<std::uint8_t> junk(64, 0xA5);
    REQUIRE_FALSE(DeserializeLeonLevel(junk, doc));

    std::vector<std::uint8_t> almostMagic = {'L', 'L', 'E', 'V', 1, 0, 0, 0};
    REQUIRE_FALSE(DeserializeLeonLevel(almostMagic, doc));

    Leon::Net::FInputCmdMsg cmd{};
    cmd.moveX = std::numeric_limits<float>::quiet_NaN();
    cmd.buttons = 0xFFFF;
    Leon::Net::SanitizeInputCmd(cmd);
    REQUIRE_THAT(cmd.moveX, WithinAbs(0.0f, 1.0e-5f));
    REQUIRE(cmd.buttons == Leon::Net::kInputButtonMask);
    REQUIRE((cmd.buttons & static_cast<std::uint16_t>(~Leon::Net::kInputButtonMask)) == 0);
}

TEST_CASE("NavigationSystem agent radius dilation shrinks walkable ring", "[gameplay][nav]") {
    FPhysScene physics;
    FBodyInstance floor{};
    floor.Type = EBodyType::Static;
    floor.Position = {0.0f, 0.0f, 0.0f};
    floor.HalfExtents = {20.0f, 0.5f, 20.0f};
    physics.GetBodies().push_back(floor);

    FBodyInstance pillar{};
    pillar.Type = EBodyType::Static;
    pillar.Position = {0.0f, 1.0f, 0.0f};
    pillar.HalfExtents = {0.4f, 1.5f, 0.4f};
    physics.GetBodies().push_back(pillar);

    UNavigationSystem narrow;
    narrow.SetCellSize(0.5f);
    narrow.SetAgentRadius(0.35f);
    narrow.BuildFromPhysScene(physics, 0.0f, 10.0f);

    UNavigationSystem wide;
    wide.SetCellSize(0.5f);
    wide.SetAgentRadius(1.5f);
    wide.BuildFromPhysScene(physics, 0.0f, 10.0f);

    REQUIRE(wide.WalkableCellCount() < narrow.WalkableCellCount());
}
