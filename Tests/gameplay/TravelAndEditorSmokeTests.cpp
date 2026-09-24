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
