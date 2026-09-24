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
    using Leon::Net::AcceptInboundPacket;
    using Leon::Net::ENetMsg;
    using Leon::Net::FHelloMsg;
    using Leon::Net::FInputCmdMsg;
    using Leon::Net::ProtocolMagic;

    REQUIRE_FALSE(AcceptInboundPacket(nullptr, 0));

    const std::uint8_t unknown[] = {99};
    REQUIRE_FALSE(AcceptInboundPacket(unknown, sizeof(unknown)));

    const std::uint8_t shortHello[] = {static_cast<std::uint8_t>(ENetMsg::Hello)};
    REQUIRE_FALSE(AcceptInboundPacket(shortHello, sizeof(shortHello)));

    FHelloMsg badMagic{};
    badMagic.Magic = 0;
    REQUIRE_FALSE(
        AcceptInboundPacket(reinterpret_cast<const std::uint8_t*>(&badMagic), sizeof(badMagic)));

    FHelloMsg good{};
    REQUIRE(AcceptInboundPacket(reinterpret_cast<const std::uint8_t*>(&good), sizeof(good)));

    FInputCmdMsg cmd{};
    REQUIRE(AcceptInboundPacket(reinterpret_cast<const std::uint8_t*>(&cmd), sizeof(cmd)));
    REQUIRE_FALSE(AcceptInboundPacket(reinterpret_cast<const std::uint8_t*>(&cmd), 1));

    (void)ProtocolMagic;
}

TEST_CASE("SanitizeInputCmd clamps axes and clears non-finite floats", "[net][protocol]") {
    Leon::Net::FInputCmdMsg cmd{};
    cmd.MoveX = 4.0f;
    cmd.MoveZ = -2.5f;
    cmd.LookYaw = std::numeric_limits<float>::quiet_NaN();
    cmd.LookPitch = std::numeric_limits<float>::infinity();
    cmd.Jump = 7;
    cmd.Buttons = 0xFFFF;

    Leon::Net::SanitizeInputCmd(cmd);

    REQUIRE_THAT(cmd.MoveX, WithinAbs(1.0f, 1.0e-5f));
    REQUIRE_THAT(cmd.MoveZ, WithinAbs(-1.0f, 1.0e-5f));
    REQUIRE_THAT(cmd.LookYaw, WithinAbs(0.0f, 1.0e-5f));
    REQUIRE_THAT(cmd.LookPitch, WithinAbs(0.0f, 1.0e-5f));
    REQUIRE(cmd.Jump == 1);
    REQUIRE(cmd.Buttons == Leon::Net::InputButtonMask);
    REQUIRE(cmd.Type == static_cast<std::uint8_t>(Leon::Net::ENetMsg::InputCmd));
}

TEST_CASE("InputButtons helpers set and test game aliases", "[net][protocol]") {
    Leon::Net::FInputCmdMsg cmd{};
    Leon::Net::SetInputButton(cmd, Leon::Net::InputButtons::Fire, true);
    REQUIRE(Leon::Net::HasInputButton(cmd, Leon::Net::InputButtons::Fire));
    REQUIRE(Leon::Net::HasInputButton(cmd, Leon::Net::InputButtons::Primary)); // same bit
    Leon::Net::SetInputButton(cmd, Leon::Net::InputButtons::Reload, true);
    REQUIRE(Leon::Net::HasInputButton(cmd, Leon::Net::InputButtons::Secondary));
    Leon::Net::SetInputButton(cmd, Leon::Net::InputButtons::Fire, false);
    REQUIRE_FALSE(Leon::Net::HasInputButton(cmd, Leon::Net::InputButtons::Fire));
    REQUIRE(Leon::Net::HasInputButton(cmd, Leon::Net::InputButtons::Reload));
}

TEST_CASE("PeerPacketWindow disconnects after accepted flood", "[net][protocol][ratelimit]") {
    using Action = Leon::Net::FPeerPacketWindow::EAction;
    Leon::Net::FPeerPacketWindow window{};
    constexpr int kMax = 5;
    for (int i = 0; i < kMax; ++i) {
        REQUIRE(window.Observe(1000, true, kMax, 10) == Action::Allow);
    }
    REQUIRE(window.Observe(1000, true, kMax, 10) == Action::Disconnect);
}

TEST_CASE("PeerPacketWindow drops then disconnects on reject flood", "[net][protocol][ratelimit]") {
    using Action = Leon::Net::FPeerPacketWindow::EAction;
    Leon::Net::FPeerPacketWindow window{};
    constexpr int kMaxReject = 3;
    for (int i = 0; i < kMaxReject; ++i) {
        REQUIRE(window.Observe(50, false, 100, kMaxReject) == Action::Drop);
    }
    REQUIRE(window.Observe(50, false, 100, kMaxReject) == Action::Disconnect);
}

TEST_CASE("PeerPacketWindow resets after one second", "[net][protocol][ratelimit]") {
    using Action = Leon::Net::FPeerPacketWindow::EAction;
    Leon::Net::FPeerPacketWindow window{};
    REQUIRE(window.Observe(0, true, 1, 10) == Action::Allow);
    REQUIRE(window.Observe(0, true, 1, 10) == Action::Disconnect);
    REQUIRE(window.Observe(1000, true, 1, 10) == Action::Allow);
}

TEST_CASE("EncodeSnapshot / DecodeSnapshot roundtrip", "[net][snapshot]") {
    Leon::Net::FPawnSnap pawns[2]{};
    pawns[0].Slot = 0;
    pawns[0].X = 1.0f;
    pawns[0].Y = 2.0f;
    pawns[0].Z = 3.0f;
    pawns[0].Yaw = 45.0f;
    pawns[0].Grounded = 1;
    pawns[1].Slot = 1;
    pawns[1].X = -1.0f;
    pawns[1].AnimBlend = 0.5f;

    Leon::Net::FBodySnap bodies[1]{};
    bodies[0].LevelMeshIndex = 7;
    bodies[0].X = 9.0f;
    bodies[0].VelY = -1.5f;

    std::vector<std::uint8_t> packet;
    REQUIRE(Leon::Net::EncodeSnapshot(packet, 42, pawns, 2, bodies, 1));
    REQUIRE(packet.size() > sizeof(Leon::Net::FSnapshotHeader));

    Leon::Net::FDecodedSnapshot decoded;
    REQUIRE(Leon::Net::DecodeSnapshot(packet.data(), packet.size(), decoded));
    REQUIRE(decoded.Tick == 42);
    REQUIRE(decoded.Pawns.size() == 2);
    REQUIRE(decoded.Bodies.size() == 1);
    REQUIRE_THAT(decoded.Pawns[0].X, WithinAbs(1.0f, 1.0e-5f));
    REQUIRE_THAT(decoded.Pawns[1].AnimBlend, WithinAbs(0.5f, 1.0e-5f));
    REQUIRE(decoded.Bodies[0].LevelMeshIndex == 7);
    REQUIRE_THAT(decoded.Bodies[0].VelY, WithinAbs(-1.5f, 1.0e-5f));
}

TEST_CASE("EncodeSnapshot accepts AI pawn slots beyond kMaxPlayers", "[net][snapshot][ai]") {
    Leon::Net::FPawnSnap pawns[Leon::Net::MaxSnapshotPawns]{};
    pawns[0].Slot = 0;
    pawns[1].Slot = 1;
    pawns[2].Slot = static_cast<std::uint8_t>(Leon::Net::MaxPlayers);
    pawns[2].X = 4.0f;
    pawns[3].Slot = static_cast<std::uint8_t>(Leon::Net::MaxPlayers + 1);
    pawns[3].X = 5.0f;

    std::vector<std::uint8_t> packet;
    REQUIRE(Leon::Net::EncodeSnapshot(packet, 7, pawns, Leon::Net::MaxSnapshotPawns, nullptr, 0));

    Leon::Net::FDecodedSnapshot decoded;
    REQUIRE(Leon::Net::DecodeSnapshot(packet.data(), packet.size(), decoded));
    REQUIRE(decoded.Pawns.size() == Leon::Net::MaxSnapshotPawns);
    REQUIRE(decoded.Pawns[2].Slot == Leon::Net::MaxPlayers);
    REQUIRE_THAT(decoded.Pawns[2].X, WithinAbs(4.0f, 1.0e-5f));
    REQUIRE_THAT(decoded.Pawns[3].X, WithinAbs(5.0f, 1.0e-5f));
}

TEST_CASE("EncodeSnapshot roundtrips match meta and pawn health", "[net][snapshot]") {
    Leon::Net::FPawnSnap pawns[1]{};
    pawns[0].Slot = 0;
    pawns[0].Health = 73.5f;
    pawns[0].Flags = Leon::Net::PawnSnapAlive;

    Leon::Net::FSnapshotMatchMeta meta{};
    meta.RoundIndex = 4;
    meta.UnitsAlive = 9;
    meta.RemainingSeconds = 3;
    meta.PlayerScore[0] = 1200;
    meta.PlayerLives[0] = 2;
    meta.PlayerElims[0] = 7;

    std::vector<std::uint8_t> packet;
    REQUIRE(Leon::Net::EncodeSnapshot(packet, 11, pawns, 1, nullptr, 0, &meta));

    Leon::Net::FDecodedSnapshot decoded;
    REQUIRE(Leon::Net::DecodeSnapshot(packet.data(), packet.size(), decoded));
    REQUIRE(decoded.bHasMatchMeta);
    REQUIRE(decoded.RoundIndex() == 4);
    REQUIRE(decoded.UnitsAlive() == 9);
    REQUIRE(decoded.RemainingSeconds() == 3);
    REQUIRE(decoded.MatchMeta.PlayerScore[0] == 1200);
    REQUIRE(decoded.MatchMeta.PlayerLives[0] == 2);
    REQUIRE(decoded.MatchMeta.PlayerElims[0] == 7);
    REQUIRE_THAT(decoded.Pawns[0].Health, WithinAbs(73.5f, 1.0e-5f));
    REQUIRE(decoded.Pawns[0].Flags == Leon::Net::PawnSnapAlive);
}

TEST_CASE("EncodeSnapshot without match meta leaves extension empty", "[net][snapshot]") {
    Leon::Net::FPawnSnap pawns[1]{};
    pawns[0].Slot = 0;
    std::vector<std::uint8_t> packet;
    REQUIRE(Leon::Net::EncodeSnapshot(packet, 3, pawns, 1, nullptr, 0, nullptr));
    REQUIRE(packet.size() == sizeof(Leon::Net::FSnapshotHeader) + sizeof(Leon::Net::FPawnSnap));

    Leon::Net::FDecodedSnapshot decoded;
    REQUIRE(Leon::Net::DecodeSnapshot(packet.data(), packet.size(), decoded));
    REQUIRE_FALSE(decoded.bHasMatchMeta);
    REQUIRE(decoded.RoundIndex() == 0);
}

TEST_CASE("AcceptInboundPacket rejects Hello with wrong protocol version", "[net][protocol]") {
    Leon::Net::FHelloMsg hello{};
    hello.ProtocolVersion = static_cast<std::uint16_t>(Leon::Net::CurrentProtocolVersion + 1);
    REQUIRE_FALSE(Leon::Net::AcceptInboundPacket(reinterpret_cast<const std::uint8_t*>(&hello),
                                                 sizeof(hello)));
    hello.ProtocolVersion = Leon::Net::CurrentProtocolVersion;
    REQUIRE(Leon::Net::AcceptInboundPacket(reinterpret_cast<const std::uint8_t*>(&hello),
                                           sizeof(hello)));
}

TEST_CASE("NetDriver listen + client Hello/Welcome on localhost", "[net][enet]") {
    constexpr std::uint16_t kPort = 17991;

    UNetDriver host;
    UNetDriver client;

    std::uint8_t welcomedSlot = 255;
    bool hostSawHello = false;

    host.SetOnPacket([&](int peerSlot, const std::uint8_t* data, std::size_t size) {
        if (data == nullptr || size < sizeof(Leon::Net::FHelloMsg)) {
            return;
        }
        if (static_cast<Leon::Net::ENetMsg>(data[0]) != Leon::Net::ENetMsg::Hello) {
            return;
        }
        Leon::Net::FHelloMsg hello{};
        std::memcpy(&hello, data, sizeof(hello));
        if (!Leon::Net::IsValidHello(hello)) {
            return;
        }
        hostSawHello = true;
        Leon::Net::FWelcomeMsg welcome{};
        welcome.Slot = 1;
        Leon::Net::WriteLevelKey(welcome.LevelKey, "Main");
        host.SendToPeer(peerSlot, &welcome, sizeof(welcome), true);
    });

    client.SetOnPeerConnected([&](int /*peerSlot*/) {
        Leon::Net::FHelloMsg hello{};
        client.SendToPeer(&hello, sizeof(hello), true);
    });
    client.SetOnPacket([&](int /*peerSlot*/, const std::uint8_t* data, std::size_t size) {
        if (data == nullptr || size < sizeof(Leon::Net::FWelcomeMsg)) {
            return;
        }
        if (static_cast<Leon::Net::ENetMsg>(data[0]) != Leon::Net::ENetMsg::Welcome) {
            return;
        }
        Leon::Net::FWelcomeMsg welcome{};
        std::memcpy(&welcome, data, sizeof(welcome));
        welcomedSlot = welcome.Slot;
        REQUIRE(Leon::Net::ReadLevelKey(welcome.LevelKey) == "Main");
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
