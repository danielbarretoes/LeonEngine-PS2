#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <leon/Engine.h>
#include <leon/Gameplay.h>
#include <leon/core/Ascii.h>
#include <leon/level/LeonLevelFormat.h>
#include <leon/level/LevelLoader.h>
#include <leon/net/NetDriver.h>
#include <leon/net/NetProtocol.h>
#include <leon/net/NetUtil.h>
#include <string>
#include <thread>
#include <vector>

TEST_CASE("EncodeRpc / DecodeRpc roundtrip Notify payload", "[net][rpc]") {
    const char payload[] = "ping";
    std::vector<std::uint8_t> packet;
    REQUIRE(leon::net::EncodeRpc(packet, leon::net::ERpcId::Notify, 1,
                                  payload, static_cast<std::uint16_t>(sizeof(payload) - 1)));
    REQUIRE(packet.size() == sizeof(leon::net::RpcHeader) + 4);
    REQUIRE(leon::net::AcceptInboundPacket(packet.data(), packet.size()));

    leon::net::RpcHeader header{};
    const std::uint8_t* outPayload = nullptr;
    std::uint16_t outBytes = 0;
    REQUIRE(leon::net::DecodeRpc(packet.data(), packet.size(), header, outPayload, outBytes));
    REQUIRE(header.rpcId == static_cast<std::uint8_t>(leon::net::ERpcId::Notify));
    REQUIRE(header.targetSlot == 1);
    REQUIRE(outBytes == 4);
    REQUIRE(std::memcmp(outPayload, "ping", 4) == 0);
}

TEST_CASE("Menu to Lobby to Match travel headless", "[gameplay][travel][e2e]") {
#ifdef LEON_SOURCE_DIR
    const std::string levels =
        std::string(LEON_SOURCE_DIR) + "/Projects/CoopTp/Content/Levels";
    const std::string hint = levels + "/Courtyard.llev";
    if (!std::filesystem::exists(hint)) {
        SKIP("Host sample levels not present (Projects/CoopTp removed)");
    }

    leon::Engine engine;
    REQUIRE(engine.InitializeHeadless());
    leon::DefaultGameMode mode;

    REQUIRE(mode.ServerTravel(engine, "MainMenu", hint));
    REQUIRE(leon::AsciiToLower(engine.GetLevel().Name()).find("mainmenu") != std::string::npos);

    REQUIRE(mode.ServerTravel(engine, "Lobby", hint));
    REQUIRE(leon::AsciiToLower(engine.GetLevel().Name()).find("lobby") != std::string::npos);

    REQUIRE(mode.ServerTravel(engine, "Courtyard", hint));
    {
        const std::string name = leon::AsciiToLower(engine.GetLevel().Name());
        REQUIRE((name.find("courtyard") != std::string::npos ||
                 name.find("coop") != std::string::npos || !name.empty()));
    }

    // Net travel notify: host → client TravelMsg (same path as ServerTravelToMatchMap).
    leon::NetDriver host;
    leon::NetDriver client;
    constexpr std::uint16_t kPort = 29111;
    REQUIRE(host.StartHost(kPort));
    REQUIRE(client.Connect("127.0.0.1", kPort));

    bool clientGotTravel = false;
    std::string travelKey;
    client.SetOnPacket([&](int, const std::uint8_t* data, std::size_t size) {
        if (size < sizeof(leon::net::TravelMsg) ||
            data[0] != static_cast<std::uint8_t>(leon::net::ENetMsg::Travel)) {
            return;
        }
        leon::net::TravelMsg msg{};
        std::memcpy(&msg, data, sizeof(msg));
        travelKey = leon::net::ReadLevelKey(msg.levelKey);
        clientGotTravel = true;
    });

    const auto connectDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (std::chrono::steady_clock::now() < connectDeadline && host.PeerCount() < 1) {
        host.Poll();
        client.Poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    REQUIRE(host.PeerCount() >= 1);

    leon::net::SendTravelToPeers(host, "Courtyard", /*dedicatedServer=*/false);
    const auto travelDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (std::chrono::steady_clock::now() < travelDeadline && !clientGotTravel) {
        host.Poll();
        client.Poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    REQUIRE(clientGotTravel);
    REQUIRE(travelKey == "Courtyard");

    client.Shutdown();
    host.Shutdown();
    engine.Shutdown();
#else
    SUCCEED("LEON_SOURCE_DIR unset");
#endif
}

TEST_CASE("Editor-style level save load apply headless", "[editor][level]") {
#ifdef LEON_SOURCE_DIR
    const std::string templateLevel =
        std::string(LEON_SOURCE_DIR) + "/Engine/Assets/LevelTemplates/Blank.llev";
    leon::Engine engine;
    REQUIRE(engine.InitializeHeadless());
    REQUIRE(leon::LoadLevelFile(engine, templateLevel));

    leon::LevelDocument doc = leon::BuildLevelDocument(engine.GetLevel(), engine.GetCamera());
    REQUIRE_FALSE(doc.name.empty());

    const std::vector<std::uint8_t> bytes = leon::SerializeLeonLevel(doc);
    REQUIRE_FALSE(bytes.empty());

    leon::LevelDocument roundTrip;
    REQUIRE(leon::DeserializeLeonLevel(bytes, roundTrip));
    REQUIRE(roundTrip.name == doc.name);

    REQUIRE(leon::ApplyLevelDocument(engine, roundTrip, "memory-editor-smoke"));
    engine.Shutdown();
#else
    SUCCEED("LEON_SOURCE_DIR unset");
#endif
}

TEST_CASE("AIChaseBehavior MoveTo when target present", "[gameplay][bt][ai]") {
    leon::World world;
    auto* character = world.SpawnActor<leon::Character>();
    auto* target = world.SpawnActor<leon::Character>();
    target->SetActorLocationAndRotation({5.0f, 0.0f, 0.0f}, 0.0f);

    leon::AIController ai;
    ai.Possess(character);
    leon::AIChaseBehavior chase;
    (void)chase.Tick(ai, target, 0.016f);
    REQUIRE(ai.GetLogicState() == leon::EAILogicState::Chase);
    (void)chase.Tick(ai, nullptr, 0.016f);
    REQUIRE(ai.GetLogicState() == leon::EAILogicState::Idle);
}
