#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <limits>
#include <cstring>
#include <filesystem>
#include <leon/Engine.h>
#include <leon/Gameplay.h>
#include <leon/audio/AudioDevice.h>
#include <leon/gameplay/NavigationSystem.h>
#include <leon/level/LeonLevelFormat.h>
#include <leon/net/NetProtocol.h>
#include <leon/net/RootReplication.h>
#include <leon/physics/PhysScene.h>
#include <leon/ui/HUD.h>
#include <leon/ui/TextBlockWidget.h>
#include <string>
#include <vector>

using Catch::Matchers::WithinAbs;

TEST_CASE("BehaviorTree Sequence and Selector with Blackboard", "[gameplay][bt]") {
    leon::Blackboard board;
    leon::BTConditionBool hasTarget("HasTarget", true);
    int ran = 0;
    leon::BTAction act([&](leon::Blackboard& b, float) {
        ++ran;
        b.SetBool("DidAct", true);
        return leon::EBTNodeResult::Succeeded;
    });
    leon::BTSequence seq({&hasTarget, &act});

    REQUIRE(seq.Tick(board, 0.016f) == leon::EBTNodeResult::Failed);
    board.SetBool("HasTarget", true);
    REQUIRE(seq.Tick(board, 0.016f) == leon::EBTNodeResult::Succeeded);
    REQUIRE(ran == 1);
    REQUIRE(board.GetBool("DidAct"));

    leon::BTConditionBool never("Never", true);
    leon::BTSelector sel({&never, &act});
    REQUIRE(sel.Tick(board, 0.0f) == leon::EBTNodeResult::Succeeded);
    REQUIRE(ran == 2);
}

TEST_CASE("AIController logic state tracks MoveTo Chase Idle", "[gameplay][ai]") {
    leon::World world;
    auto* character = world.SpawnActor<leon::Character>();
    leon::AIController ai;
    ai.Possess(character);
    REQUIRE(ai.GetLogicState() == leon::EAILogicState::Idle);
    ai.MoveToLocation({3.0f, 0.0f, 0.0f});
    REQUIRE(ai.GetLogicState() == leon::EAILogicState::MoveTo);
    ai.MoveToActor(character);
    REQUIRE(ai.GetLogicState() == leon::EAILogicState::Chase);
    ai.StopMovement();
    REQUIRE(ai.GetLogicState() == leon::EAILogicState::Idle);
}

TEST_CASE("RootReplication relevancy and CaptureCharacterRoot", "[net][replication]") {
    leon::Character character;
    character.SetActorLocationAndRotation({1.0f, 0.0f, 2.0f}, 45.0f);
    const leon::net::PawnSnap snap = leon::net::CaptureCharacterRoot(3, character, 10.0f, -5.0f);
    REQUIRE(snap.slot == 3);
    REQUIRE_THAT(snap.x, WithinAbs(1.0f, 1.0e-5f));
    REQUIRE_THAT(snap.z, WithinAbs(2.0f, 1.0e-5f));
    REQUIRE_THAT(snap.yaw, WithinAbs(45.0f, 1.0e-5f));
    REQUIRE_THAT(snap.boomYaw, WithinAbs(10.0f, 1.0e-5f));

    REQUIRE(leon::net::IsPawnRelevant({0, 0, 0}, {3, 0, 0}, 5.0f));
    REQUIRE_FALSE(leon::net::IsPawnRelevant({0, 0, 0}, {10, 0, 0}, 5.0f));
    REQUIRE(leon::net::IsPawnRelevantXZ({0, 0, 0}, {0, 99, 4}, 5.0f));
}

TEST_CASE("AudioDevice silent mode is safe for Play APIs", "[audio]") {
    leon::AudioDevice audio;
    REQUIRE(audio.Initialize(/*silent=*/true));
    audio.PlaySound2D("does-not-exist.wav");
    audio.PlayUiSound(leon::EUiSound::Click);
    audio.PlayMusic("MenuBed.wav");
    audio.StopMusic();
    audio.Tick();
    audio.Shutdown();
}

TEST_CASE("HUD AddWidget TextBlock and remove", "[ui][hud]") {
    leon::HUD hud;
    auto* text = hud.AddWidget<leon::TextBlockWidget>();
    REQUIRE(text != nullptr);
    text->SetText("Hello");
    REQUIRE(hud.GetWidgetOfClass<leon::TextBlockWidget>() == text);
    hud.Tick(0.016f);
    hud.RemoveWidget(text);
    REQUIRE(hud.GetWidgetOfClass<leon::TextBlockWidget>() == nullptr);
}

TEST_CASE("ServerTravel sibling level load without net", "[gameplay][travel]") {
#ifdef LEON_SOURCE_DIR
    const std::string coopLevels = std::string(LEON_SOURCE_DIR) + "/Projects/CoopTp/Content/Levels";
    const std::string hint = coopLevels + "/Courtyard.llev";
    if (!std::filesystem::exists(hint)) {
        SKIP("Host sample levels not present (Projects/CoopTp removed)");
    }
    leon::Engine engine;
    REQUIRE(engine.InitializeHeadless());
    leon::DefaultGameMode mode;
    // Hint path anchors sibling .llev lookup under the same Levels folder.
    REQUIRE(mode.ServerTravel(engine, "Lobby", hint));
    REQUIRE_FALSE(engine.GetLevel().Name().empty());
    REQUIRE(mode.ClientTravel(engine, "Courtyard", coopLevels + "/Lobby.llev"));
    engine.Shutdown();
#else
    SUCCEED("LEON_SOURCE_DIR unset");
#endif
}

TEST_CASE("DeserializeLeonLevel and InputCmd adversarial inputs", "[content][fuzz][net]") {
    leon::LevelDocument doc;
    std::vector<std::uint8_t> empty;
    REQUIRE_FALSE(leon::DeserializeLeonLevel(empty, doc));

    std::vector<std::uint8_t> junk(64, 0xA5);
    REQUIRE_FALSE(leon::DeserializeLeonLevel(junk, doc));

    std::vector<std::uint8_t> almostMagic = {'L', 'L', 'E', 'V', 1, 0, 0, 0};
    REQUIRE_FALSE(leon::DeserializeLeonLevel(almostMagic, doc));

    leon::net::InputCmdMsg cmd{};
    cmd.moveX = std::numeric_limits<float>::quiet_NaN();
    cmd.buttons = 0xFFFF;
    leon::net::SanitizeInputCmd(cmd);
    REQUIRE_THAT(cmd.moveX, WithinAbs(0.0f, 1.0e-5f));
    REQUIRE(cmd.buttons == leon::net::kInputButtonMask);
    REQUIRE((cmd.buttons & static_cast<std::uint16_t>(~leon::net::kInputButtonMask)) == 0);
}

TEST_CASE("NavigationSystem agent radius dilation shrinks walkable ring", "[gameplay][nav]") {
    leon::PhysScene physics;
    leon::BodyInstance floor{};
    floor.type = leon::EBodyType::Static;
    floor.position = {0.0f, 0.0f, 0.0f};
    floor.halfExtents = {20.0f, 0.5f, 20.0f};
    physics.Bodies().push_back(floor);

    leon::BodyInstance pillar{};
    pillar.type = leon::EBodyType::Static;
    pillar.position = {0.0f, 1.0f, 0.0f};
    pillar.halfExtents = {0.4f, 1.5f, 0.4f};
    physics.Bodies().push_back(pillar);

    leon::NavigationSystem narrow;
    narrow.SetCellSize(0.5f);
    narrow.SetAgentRadius(0.35f);
    narrow.BuildFromPhysScene(physics, 0.0f, 10.0f);

    leon::NavigationSystem wide;
    wide.SetCellSize(0.5f);
    wide.SetAgentRadius(1.5f);
    wide.BuildFromPhysScene(physics, 0.0f, 10.0f);

    REQUIRE(wide.WalkableCellCount() < narrow.WalkableCellCount());
}
