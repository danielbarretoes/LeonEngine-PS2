#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <chrono>
#include <cstring>
#include "Engine/NetDriver.h"
#include "Net/NetProtocol.h"
#include "Net/SnapshotCodec.h"
#include <limits>
#include <thread>
#include <vector>

using Catch::Matchers::WithinAbs;

TEST_CASE("AcceptInboundPacket rejects unknown, short, and bad Hello magic", "[net][protocol]") {
    using leon::net::AcceptInboundPacket;
    using leon::net::ENetMsg;
    using leon::net::HelloMsg;
    using leon::net::InputCmdMsg;
    using leon::net::kProtocolMagic;

    REQUIRE_FALSE(AcceptInboundPacket(nullptr, 0));

    const std::uint8_t unknown[] = {99};
    REQUIRE_FALSE(AcceptInboundPacket(unknown, sizeof(unknown)));

    const std::uint8_t shortHello[] = {static_cast<std::uint8_t>(ENetMsg::Hello)};
    REQUIRE_FALSE(AcceptInboundPacket(shortHello, sizeof(shortHello)));

    HelloMsg badMagic{};
    badMagic.magic = 0;
    REQUIRE_FALSE(
        AcceptInboundPacket(reinterpret_cast<const std::uint8_t*>(&badMagic), sizeof(badMagic)));

    HelloMsg good{};
    REQUIRE(AcceptInboundPacket(reinterpret_cast<const std::uint8_t*>(&good), sizeof(good)));

    InputCmdMsg cmd{};
    REQUIRE(AcceptInboundPacket(reinterpret_cast<const std::uint8_t*>(&cmd), sizeof(cmd)));
    REQUIRE_FALSE(AcceptInboundPacket(reinterpret_cast<const std::uint8_t*>(&cmd), 1));

    (void)kProtocolMagic;
}

TEST_CASE("SanitizeInputCmd clamps axes and clears non-finite floats", "[net][protocol]") {
    leon::net::InputCmdMsg cmd{};
    cmd.moveX = 4.0f;
    cmd.moveZ = -2.5f;
    cmd.lookYaw = std::numeric_limits<float>::quiet_NaN();
    cmd.lookPitch = std::numeric_limits<float>::infinity();
    cmd.jump = 7;
    cmd.buttons = 0xFFFF;

    leon::net::SanitizeInputCmd(cmd);

    REQUIRE_THAT(cmd.moveX, WithinAbs(1.0f, 1.0e-5f));
    REQUIRE_THAT(cmd.moveZ, WithinAbs(-1.0f, 1.0e-5f));
    REQUIRE_THAT(cmd.lookYaw, WithinAbs(0.0f, 1.0e-5f));
    REQUIRE_THAT(cmd.lookPitch, WithinAbs(0.0f, 1.0e-5f));
    REQUIRE(cmd.jump == 1);
    REQUIRE(cmd.buttons == leon::net::kInputButtonMask);
    REQUIRE(cmd.type == static_cast<std::uint8_t>(leon::net::ENetMsg::InputCmd));
}

TEST_CASE("InputButtons helpers set and test game aliases", "[net][protocol]") {
    leon::net::InputCmdMsg cmd{};
    leon::net::SetInputButton(cmd, leon::net::InputButtons::Fire, true);
    REQUIRE(leon::net::HasInputButton(cmd, leon::net::InputButtons::Fire));
    REQUIRE(leon::net::HasInputButton(cmd, leon::net::InputButtons::Primary)); // same bit
    leon::net::SetInputButton(cmd, leon::net::InputButtons::Reload, true);
    REQUIRE(leon::net::HasInputButton(cmd, leon::net::InputButtons::Secondary));
    leon::net::SetInputButton(cmd, leon::net::InputButtons::Fire, false);
    REQUIRE_FALSE(leon::net::HasInputButton(cmd, leon::net::InputButtons::Fire));
    REQUIRE(leon::net::HasInputButton(cmd, leon::net::InputButtons::Reload));
}

TEST_CASE("PeerPacketWindow disconnects after accepted flood", "[net][protocol][ratelimit]") {
    using Action = leon::net::PeerPacketWindow::EAction;
    leon::net::PeerPacketWindow window{};
    constexpr int kMax = 5;
    for (int i = 0; i < kMax; ++i) {
        REQUIRE(window.Observe(1000, true, kMax, 10) == Action::Allow);
    }
    REQUIRE(window.Observe(1000, true, kMax, 10) == Action::Disconnect);
}

TEST_CASE("PeerPacketWindow drops then disconnects on reject flood", "[net][protocol][ratelimit]") {
    using Action = leon::net::PeerPacketWindow::EAction;
    leon::net::PeerPacketWindow window{};
    constexpr int kMaxReject = 3;
    for (int i = 0; i < kMaxReject; ++i) {
        REQUIRE(window.Observe(50, false, 100, kMaxReject) == Action::Drop);
    }
    REQUIRE(window.Observe(50, false, 100, kMaxReject) == Action::Disconnect);
}

TEST_CASE("PeerPacketWindow resets after one second", "[net][protocol][ratelimit]") {
    using Action = leon::net::PeerPacketWindow::EAction;
    leon::net::PeerPacketWindow window{};
    REQUIRE(window.Observe(0, true, 1, 10) == Action::Allow);
    REQUIRE(window.Observe(0, true, 1, 10) == Action::Disconnect);
    REQUIRE(window.Observe(1000, true, 1, 10) == Action::Allow);
}

TEST_CASE("EncodeSnapshot / DecodeSnapshot roundtrip", "[net][snapshot]") {
    leon::net::PawnSnap pawns[2]{};
    pawns[0].slot = 0;
    pawns[0].x = 1.0f;
    pawns[0].y = 2.0f;
    pawns[0].z = 3.0f;
    pawns[0].yaw = 45.0f;
    pawns[0].grounded = 1;
    pawns[1].slot = 1;
    pawns[1].x = -1.0f;
    pawns[1].animBlend = 0.5f;

    leon::net::BodySnap bodies[1]{};
    bodies[0].levelMeshIndex = 7;
    bodies[0].x = 9.0f;
    bodies[0].velY = -1.5f;

    std::vector<std::uint8_t> packet;
    REQUIRE(leon::net::EncodeSnapshot(packet, 42, pawns, 2, bodies, 1));
    REQUIRE(packet.size() > sizeof(leon::net::SnapshotHeader));

    leon::net::DecodedSnapshot decoded;
    REQUIRE(leon::net::DecodeSnapshot(packet.data(), packet.size(), decoded));
    REQUIRE(decoded.tick == 42);
    REQUIRE(decoded.pawns.size() == 2);
    REQUIRE(decoded.bodies.size() == 1);
    REQUIRE_THAT(decoded.pawns[0].x, WithinAbs(1.0f, 1.0e-5f));
    REQUIRE_THAT(decoded.pawns[1].animBlend, WithinAbs(0.5f, 1.0e-5f));
    REQUIRE(decoded.bodies[0].levelMeshIndex == 7);
    REQUIRE_THAT(decoded.bodies[0].velY, WithinAbs(-1.5f, 1.0e-5f));
}

TEST_CASE("EncodeSnapshot accepts AI pawn slots beyond kMaxPlayers", "[net][snapshot][ai]") {
    leon::net::PawnSnap pawns[leon::net::kMaxSnapshotPawns]{};
    pawns[0].slot = 0;
    pawns[1].slot = 1;
    pawns[2].slot = static_cast<std::uint8_t>(leon::net::kMaxPlayers);
    pawns[2].x = 4.0f;
    pawns[3].slot = static_cast<std::uint8_t>(leon::net::kMaxPlayers + 1);
    pawns[3].x = 5.0f;

    std::vector<std::uint8_t> packet;
    REQUIRE(leon::net::EncodeSnapshot(packet, 7, pawns, leon::net::kMaxSnapshotPawns, nullptr, 0));

    leon::net::DecodedSnapshot decoded;
    REQUIRE(leon::net::DecodeSnapshot(packet.data(), packet.size(), decoded));
    REQUIRE(decoded.pawns.size() == leon::net::kMaxSnapshotPawns);
    REQUIRE(decoded.pawns[2].slot == leon::net::kMaxPlayers);
    REQUIRE_THAT(decoded.pawns[2].x, WithinAbs(4.0f, 1.0e-5f));
    REQUIRE_THAT(decoded.pawns[3].x, WithinAbs(5.0f, 1.0e-5f));
}

TEST_CASE("EncodeSnapshot roundtrips match meta and pawn health", "[net][snapshot]") {
    leon::net::PawnSnap pawns[1]{};
    pawns[0].slot = 0;
    pawns[0].health = 73.5f;
    pawns[0].flags = leon::net::kPawnSnapAlive;

    leon::net::SnapshotMatchMeta meta{};
    meta.roundIndex = 4;
    meta.unitsAlive = 9;
    meta.remainingSeconds = 3;
    meta.playerScore[0] = 1200;
    meta.playerLives[0] = 2;
    meta.playerElims[0] = 7;

    std::vector<std::uint8_t> packet;
    REQUIRE(leon::net::EncodeSnapshot(packet, 11, pawns, 1, nullptr, 0, &meta));

    leon::net::DecodedSnapshot decoded;
    REQUIRE(leon::net::DecodeSnapshot(packet.data(), packet.size(), decoded));
    REQUIRE(decoded.hasMatchMeta);
    REQUIRE(decoded.RoundIndex() == 4);
    REQUIRE(decoded.UnitsAlive() == 9);
    REQUIRE(decoded.RemainingSeconds() == 3);
    REQUIRE(decoded.matchMeta.playerScore[0] == 1200);
    REQUIRE(decoded.matchMeta.playerLives[0] == 2);
    REQUIRE(decoded.matchMeta.playerElims[0] == 7);
    REQUIRE_THAT(decoded.pawns[0].health, WithinAbs(73.5f, 1.0e-5f));
    REQUIRE(decoded.pawns[0].flags == leon::net::kPawnSnapAlive);
}

TEST_CASE("EncodeSnapshot without match meta leaves extension empty", "[net][snapshot]") {
    leon::net::PawnSnap pawns[1]{};
    pawns[0].slot = 0;
    std::vector<std::uint8_t> packet;
    REQUIRE(leon::net::EncodeSnapshot(packet, 3, pawns, 1, nullptr, 0, nullptr));
    REQUIRE(packet.size() == sizeof(leon::net::SnapshotHeader) + sizeof(leon::net::PawnSnap));

    leon::net::DecodedSnapshot decoded;
    REQUIRE(leon::net::DecodeSnapshot(packet.data(), packet.size(), decoded));
    REQUIRE_FALSE(decoded.hasMatchMeta);
    REQUIRE(decoded.RoundIndex() == 0);
}

TEST_CASE("AcceptInboundPacket rejects Hello with wrong protocol version", "[net][protocol]") {
    leon::net::HelloMsg hello{};
    hello.protocolVersion = static_cast<std::uint16_t>(leon::net::kProtocolVersion + 1);
    REQUIRE_FALSE(leon::net::AcceptInboundPacket(reinterpret_cast<const std::uint8_t*>(&hello),
                                                 sizeof(hello)));
    hello.protocolVersion = leon::net::kProtocolVersion;
    REQUIRE(leon::net::AcceptInboundPacket(reinterpret_cast<const std::uint8_t*>(&hello),
                                           sizeof(hello)));
}

TEST_CASE("NetDriver listen + client Hello/Welcome on localhost", "[net][enet]") {
    constexpr std::uint16_t kPort = 17991;

    leon::NetDriver host;
    leon::NetDriver client;

    std::uint8_t welcomedSlot = 255;
    bool hostSawHello = false;

    host.SetOnPacket([&](int peerSlot, const std::uint8_t* data, std::size_t size) {
        if (data == nullptr || size < sizeof(leon::net::HelloMsg)) {
            return;
        }
        if (static_cast<leon::net::ENetMsg>(data[0]) != leon::net::ENetMsg::Hello) {
            return;
        }
        leon::net::HelloMsg hello{};
        std::memcpy(&hello, data, sizeof(hello));
        if (!leon::net::IsValidHello(hello)) {
            return;
        }
        hostSawHello = true;
        leon::net::WelcomeMsg welcome{};
        welcome.slot = 1;
        leon::net::WriteLevelKey(welcome.levelKey, "Main");
        host.SendToPeer(peerSlot, &welcome, sizeof(welcome), true);
    });

    client.SetOnPeerConnected([&](int /*peerSlot*/) {
        leon::net::HelloMsg hello{};
        client.SendToPeer(&hello, sizeof(hello), true);
    });
    client.SetOnPacket([&](int /*peerSlot*/, const std::uint8_t* data, std::size_t size) {
        if (data == nullptr || size < sizeof(leon::net::WelcomeMsg)) {
            return;
        }
        if (static_cast<leon::net::ENetMsg>(data[0]) != leon::net::ENetMsg::Welcome) {
            return;
        }
        leon::net::WelcomeMsg welcome{};
        std::memcpy(&welcome, data, sizeof(welcome));
        welcomedSlot = welcome.slot;
        REQUIRE(leon::net::ReadLevelKey(welcome.levelKey) == "Main");
    });

    REQUIRE(host.StartHost(kPort));
    REQUIRE(client.Connect("127.0.0.1", kPort));

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (std::chrono::steady_clock::now() < deadline && !(hostSawHello && welcomedSlot != 255)) {
        host.Poll();
        client.Poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    REQUIRE(hostSawHello);
    REQUIRE(welcomedSlot == 1);

    client.Shutdown();
    host.Shutdown();
}
