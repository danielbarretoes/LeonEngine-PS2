#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstring>
#include <filesystem>
#include "Engine/GameEngine.h"
#include "GameplayMinimal.h"
#include "Misc/CString.h"
#include "Level/LeonLevelFormat.h"
#include "Level/LevelLoader.h"
#include "Engine/NetDriver.h"
#include "Net/NetProtocol.h"
#include "Net/NetUtil.h"
#include <string>
#include <thread>
#include <vector>

TEST_CASE("EncodeRpc / DecodeRpc roundtrip Notify payload", "[net][rpc]") {
    const char payload[] = "ping";
    std::vector<std::uint8_t> packet;
    REQUIRE(Leon::Net::EncodeRpc(packet, Leon::Net::ERpcId::Notify, 1,
                                  payload, static_cast<std::uint16_t>(sizeof(payload) - 1)));
    REQUIRE(packet.size() == sizeof(Leon::Net::FRpcHeader) + 4);
    REQUIRE(Leon::Net::AcceptInboundPacket(packet.data(), packet.size()));

    Leon::Net::FRpcHeader header{};
    const std::uint8_t* outPayload = nullptr;
    std::uint16_t outBytes = 0;
    REQUIRE(Leon::Net::DecodeRpc(packet.data(), packet.size(), header, outPayload, outBytes));
    REQUIRE(header.RpcId == static_cast<std::uint8_t>(Leon::Net::ERpcId::Notify));
    REQUIRE(header.TargetSlot == 1);
    REQUIRE(outBytes == 4);
    REQUIRE(std::memcmp(outPayload, "ping", 4) == 0);
}

TEST_CASE("Editor-style level save load apply headless", "[editor][level]") {
#ifdef LEON_ROOT_DIR
    const std::string templateLevel =
        std::string(LEON_ROOT_DIR) + "/Engine/Content/LevelTemplates/Blank.llev";
    UGameEngine engine;
    REQUIRE(engine.InitializeHeadless());
    REQUIRE(LoadLevelFile(engine, templateLevel));

    FLevelDocument doc = BuildLevelDocument(engine.GetLevel(), engine.GetCamera());
    REQUIRE_FALSE(doc.Name.empty());

    const std::vector<std::uint8_t> bytes = SerializeLeonLevel(doc);
    REQUIRE_FALSE(bytes.empty());

    FLevelDocument roundTrip;
    REQUIRE(DeserializeLeonLevel(bytes, roundTrip));
    REQUIRE(roundTrip.Name == doc.Name);

    REQUIRE(ApplyLevelDocument(engine, roundTrip, "memory-editor-smoke"));
    engine.Shutdown();
#else
    SUCCEED("LEON_ROOT_DIR unset");
#endif
}

TEST_CASE("AIChaseBehavior MoveTo when target present", "[gameplay][bt][ai]") {
    UWorld world;
    auto* character = world.SpawnActor<ACharacter>();
    auto* target = world.SpawnActor<ACharacter>();
    target->SetActorLocationAndRotation({5.0f, 0.0f, 0.0f}, 0.0f);

    AAIController ai;
    ai.Possess(character);
    FAIChaseBehavior chase;
    (void)chase.Tick(ai, target, 0.016f);
    REQUIRE(ai.GetLogicState() == EAILogicState::Chase);
    (void)chase.Tick(ai, nullptr, 0.016f);
    REQUIRE(ai.GetLogicState() == EAILogicState::Idle);
}
