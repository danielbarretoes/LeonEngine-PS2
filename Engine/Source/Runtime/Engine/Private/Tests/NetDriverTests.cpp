#include "Engine/NetDriver.h"
#include "Net/NetProtocol.h"
#include "Net/SnapshotCodec.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <chrono>
#include <cstring>
#include <limits>
#include <thread>
#include <vector>

using Catch::Matchers::WithinAbs;

TEST_CASE("AcceptInboundPacket rejects unknown, short, and bad Hello magic", "[net][protocol]")
{
	using Leon::Net::AcceptInboundPacket;
	using Leon::Net::ENetMsg;
	using Leon::Net::FHelloMsg;
	using Leon::Net::FInputCmdMsg;
	using Leon::Net::ProtocolMagic;

	REQUIRE_FALSE(AcceptInboundPacket(nullptr, 0));

	const std::uint8_t Unknown[] = {99};
	REQUIRE_FALSE(AcceptInboundPacket(Unknown, sizeof(Unknown)));

	const std::uint8_t ShortHello[] = {static_cast<std::uint8_t>(ENetMsg::Hello)};
	REQUIRE_FALSE(AcceptInboundPacket(ShortHello, sizeof(ShortHello)));

	FHelloMsg BadMagic{};
	BadMagic.Magic = 0;
	REQUIRE_FALSE(AcceptInboundPacket(reinterpret_cast<const std::uint8_t*>(&BadMagic), sizeof(BadMagic)));

	FHelloMsg Good{};
	REQUIRE(AcceptInboundPacket(reinterpret_cast<const std::uint8_t*>(&Good), sizeof(Good)));

	FInputCmdMsg Cmd{};
	REQUIRE(AcceptInboundPacket(reinterpret_cast<const std::uint8_t*>(&Cmd), sizeof(Cmd)));
	REQUIRE_FALSE(AcceptInboundPacket(reinterpret_cast<const std::uint8_t*>(&Cmd), 1));

	(void)ProtocolMagic;
}

TEST_CASE("SanitizeInputCmd clamps axes and clears non-finite floats", "[net][protocol]")
{
	Leon::Net::FInputCmdMsg Cmd{};
	Cmd.MoveX = 4.0f;
	Cmd.MoveZ = -2.5f;
	Cmd.LookYaw = std::numeric_limits<float>::quiet_NaN();
	Cmd.LookPitch = std::numeric_limits<float>::infinity();
	Cmd.Jump = 7;
	Cmd.Buttons = 0xFFFF;

	Leon::Net::SanitizeInputCmd(Cmd);

	REQUIRE_THAT(Cmd.MoveX, WithinAbs(1.0f, 1.0e-5f));
	REQUIRE_THAT(Cmd.MoveZ, WithinAbs(-1.0f, 1.0e-5f));
	REQUIRE_THAT(Cmd.LookYaw, WithinAbs(0.0f, 1.0e-5f));
	REQUIRE_THAT(Cmd.LookPitch, WithinAbs(0.0f, 1.0e-5f));
	REQUIRE(Cmd.Jump == 1);
	REQUIRE(Cmd.Buttons == Leon::Net::InputButtonMask);
	REQUIRE(Cmd.Type == static_cast<std::uint8_t>(Leon::Net::ENetMsg::InputCmd));
}

TEST_CASE("InputButtons helpers set and test game aliases", "[net][protocol]")
{
	Leon::Net::FInputCmdMsg Cmd{};
	Leon::Net::SetInputButton(Cmd, Leon::Net::InputButtons::Fire, true);
	REQUIRE(Leon::Net::HasInputButton(Cmd, Leon::Net::InputButtons::Fire));
	REQUIRE(Leon::Net::HasInputButton(Cmd, Leon::Net::InputButtons::Primary)); // same bit
	Leon::Net::SetInputButton(Cmd, Leon::Net::InputButtons::Reload, true);
	REQUIRE(Leon::Net::HasInputButton(Cmd, Leon::Net::InputButtons::Secondary));
	Leon::Net::SetInputButton(Cmd, Leon::Net::InputButtons::Fire, false);
	REQUIRE_FALSE(Leon::Net::HasInputButton(Cmd, Leon::Net::InputButtons::Fire));
	REQUIRE(Leon::Net::HasInputButton(Cmd, Leon::Net::InputButtons::Reload));
}

TEST_CASE("PeerPacketWindow disconnects after accepted flood", "[net][protocol][ratelimit]")
{
	using Action = Leon::Net::FPeerPacketWindow::EAction;
	Leon::Net::FPeerPacketWindow Window{};
	constexpr int Max = 5;
	for (int I = 0; I < Max; ++I)
	{
		REQUIRE(Window.Observe(1000, true, Max, 10) == Action::Allow);
	}
	REQUIRE(Window.Observe(1000, true, Max, 10) == Action::Disconnect);
}

TEST_CASE("PeerPacketWindow drops then disconnects on reject flood", "[net][protocol][ratelimit]")
{
	using Action = Leon::Net::FPeerPacketWindow::EAction;
	Leon::Net::FPeerPacketWindow Window{};
	constexpr int MaxReject = 3;
	for (int I = 0; I < MaxReject; ++I)
	{
		REQUIRE(Window.Observe(50, false, 100, MaxReject) == Action::Drop);
	}
	REQUIRE(Window.Observe(50, false, 100, MaxReject) == Action::Disconnect);
}

TEST_CASE("PeerPacketWindow resets after one second", "[net][protocol][ratelimit]")
{
	using Action = Leon::Net::FPeerPacketWindow::EAction;
	Leon::Net::FPeerPacketWindow Window{};
	REQUIRE(Window.Observe(0, true, 1, 10) == Action::Allow);
	REQUIRE(Window.Observe(0, true, 1, 10) == Action::Disconnect);
	REQUIRE(Window.Observe(1000, true, 1, 10) == Action::Allow);
}

TEST_CASE("EncodeSnapshot / DecodeSnapshot roundtrip", "[net][snapshot]")
{
	Leon::Net::FPawnSnap Pawns[2]{};
	Pawns[0].Slot = 0;
	Pawns[0].X = 1.0f;
	Pawns[0].Y = 2.0f;
	Pawns[0].Z = 3.0f;
	Pawns[0].Yaw = 45.0f;
	Pawns[0].Grounded = 1;
	Pawns[1].Slot = 1;
	Pawns[1].X = -1.0f;
	Pawns[1].AnimBlend = 0.5f;

	Leon::Net::FBodySnap Bodies[1]{};
	Bodies[0].LevelMeshIndex = 7;
	Bodies[0].X = 9.0f;
	Bodies[0].VelY = -1.5f;

	std::vector<std::uint8_t> Packet;
	REQUIRE(Leon::Net::EncodeSnapshot(Packet, 42, Pawns, 2, Bodies, 1));
	REQUIRE(Packet.size() > sizeof(Leon::Net::FSnapshotHeader));

	Leon::Net::FDecodedSnapshot Decoded;
	REQUIRE(Leon::Net::DecodeSnapshot(Packet.data(), Packet.size(), Decoded));
	REQUIRE(Decoded.Tick == 42);
	REQUIRE(Decoded.Pawns.size() == 2);
	REQUIRE(Decoded.Bodies.size() == 1);
	REQUIRE_THAT(Decoded.Pawns[0].X, WithinAbs(1.0f, 1.0e-5f));
	REQUIRE_THAT(Decoded.Pawns[1].AnimBlend, WithinAbs(0.5f, 1.0e-5f));
	REQUIRE(Decoded.Bodies[0].LevelMeshIndex == 7);
	REQUIRE_THAT(Decoded.Bodies[0].VelY, WithinAbs(-1.5f, 1.0e-5f));
}

TEST_CASE("EncodeSnapshot accepts AI pawn slots beyond MaxPlayers", "[net][snapshot][ai]")
{
	Leon::Net::FPawnSnap Pawns[Leon::Net::MaxSnapshotPawns]{};
	Pawns[0].Slot = 0;
	Pawns[1].Slot = 1;
	Pawns[2].Slot = static_cast<std::uint8_t>(Leon::Net::MaxPlayers);
	Pawns[2].X = 4.0f;
	Pawns[3].Slot = static_cast<std::uint8_t>(Leon::Net::MaxPlayers + 1);
	Pawns[3].X = 5.0f;

	std::vector<std::uint8_t> Packet;
	REQUIRE(Leon::Net::EncodeSnapshot(Packet, 7, Pawns, Leon::Net::MaxSnapshotPawns, nullptr, 0));

	Leon::Net::FDecodedSnapshot Decoded;
	REQUIRE(Leon::Net::DecodeSnapshot(Packet.data(), Packet.size(), Decoded));
	REQUIRE(Decoded.Pawns.size() == Leon::Net::MaxSnapshotPawns);
	REQUIRE(Decoded.Pawns[2].Slot == Leon::Net::MaxPlayers);
	REQUIRE_THAT(Decoded.Pawns[2].X, WithinAbs(4.0f, 1.0e-5f));
	REQUIRE_THAT(Decoded.Pawns[3].X, WithinAbs(5.0f, 1.0e-5f));
}

TEST_CASE("EncodeSnapshot roundtrips match meta and pawn health", "[net][snapshot]")
{
	Leon::Net::FPawnSnap Pawns[1]{};
	Pawns[0].Slot = 0;
	Pawns[0].Health = 73.5f;
	Pawns[0].Flags = Leon::Net::PawnSnapAlive;

	Leon::Net::FSnapshotMatchMeta Meta{};
	Meta.RoundIndex = 4;
	Meta.UnitsAlive = 9;
	Meta.RemainingSeconds = 3;
	Meta.PlayerScore[0] = 1200;
	Meta.PlayerLives[0] = 2;
	Meta.PlayerElims[0] = 7;

	std::vector<std::uint8_t> Packet;
	REQUIRE(Leon::Net::EncodeSnapshot(Packet, 11, Pawns, 1, nullptr, 0, &Meta));

	Leon::Net::FDecodedSnapshot Decoded;
	REQUIRE(Leon::Net::DecodeSnapshot(Packet.data(), Packet.size(), Decoded));
	REQUIRE(Decoded.bHasMatchMeta);
	REQUIRE(Decoded.RoundIndex() == 4);
	REQUIRE(Decoded.UnitsAlive() == 9);
	REQUIRE(Decoded.RemainingSeconds() == 3);
	REQUIRE(Decoded.MatchMeta.PlayerScore[0] == 1200);
	REQUIRE(Decoded.MatchMeta.PlayerLives[0] == 2);
	REQUIRE(Decoded.MatchMeta.PlayerElims[0] == 7);
	REQUIRE_THAT(Decoded.Pawns[0].Health, WithinAbs(73.5f, 1.0e-5f));
	REQUIRE(Decoded.Pawns[0].Flags == Leon::Net::PawnSnapAlive);
}

TEST_CASE("EncodeSnapshot without match meta leaves extension empty", "[net][snapshot]")
{
	Leon::Net::FPawnSnap Pawns[1]{};
	Pawns[0].Slot = 0;
	std::vector<std::uint8_t> Packet;
	REQUIRE(Leon::Net::EncodeSnapshot(Packet, 3, Pawns, 1, nullptr, 0, nullptr));
	REQUIRE(Packet.size() == sizeof(Leon::Net::FSnapshotHeader) + sizeof(Leon::Net::FPawnSnap));

	Leon::Net::FDecodedSnapshot Decoded;
	REQUIRE(Leon::Net::DecodeSnapshot(Packet.data(), Packet.size(), Decoded));
	REQUIRE_FALSE(Decoded.bHasMatchMeta);
	REQUIRE(Decoded.RoundIndex() == 0);
}

TEST_CASE("AcceptInboundPacket rejects Hello with wrong protocol version", "[net][protocol]")
{
	Leon::Net::FHelloMsg Hello{};
	Hello.ProtocolVersion = static_cast<std::uint16_t>(Leon::Net::CurrentProtocolVersion + 1);
	REQUIRE_FALSE(Leon::Net::AcceptInboundPacket(reinterpret_cast<const std::uint8_t*>(&Hello), sizeof(Hello)));
	Hello.ProtocolVersion = Leon::Net::CurrentProtocolVersion;
	REQUIRE(Leon::Net::AcceptInboundPacket(reinterpret_cast<const std::uint8_t*>(&Hello), sizeof(Hello)));
}

TEST_CASE("NetDriver listen + client Hello/Welcome on localhost", "[net][enet]")
{
	constexpr std::uint16_t Port = 17991;

	UNetDriver Host;
	UNetDriver Client;

	std::uint8_t WelcomedSlot = 255;
	bool bHostSawHello = false;

	Host.SetOnPacket(
		[&](int PeerSlot, const std::uint8_t* Data, std::size_t Size)
		{
			if (Data == nullptr || Size < sizeof(Leon::Net::FHelloMsg))
			{
				return;
			}
			if (static_cast<Leon::Net::ENetMsg>(Data[0]) != Leon::Net::ENetMsg::Hello)
			{
				return;
			}
			Leon::Net::FHelloMsg Hello{};
			std::memcpy(&Hello, Data, sizeof(Hello));
			if (!Leon::Net::IsValidHello(Hello))
			{
				return;
			}
			bHostSawHello = true;
			Leon::Net::FWelcomeMsg Welcome{};
			Welcome.Slot = 1;
			Leon::Net::WriteLevelKey(Welcome.LevelKey, "Main");
			Host.SendToPeer(PeerSlot, &Welcome, sizeof(Welcome), true);
		});

	Client.SetOnPeerConnected(
		[&](int /*peerSlot*/)
		{
			Leon::Net::FHelloMsg Hello{};
			Client.SendToPeer(&Hello, sizeof(Hello), true);
		});
	Client.SetOnPacket(
		[&](int /*peerSlot*/, const std::uint8_t* Data, std::size_t Size)
		{
			if (Data == nullptr || Size < sizeof(Leon::Net::FWelcomeMsg))
			{
				return;
			}
			if (static_cast<Leon::Net::ENetMsg>(Data[0]) != Leon::Net::ENetMsg::Welcome)
			{
				return;
			}
			Leon::Net::FWelcomeMsg Welcome{};
			std::memcpy(&Welcome, Data, sizeof(Welcome));
			WelcomedSlot = Welcome.Slot;
			REQUIRE(Leon::Net::ReadLevelKey(Welcome.LevelKey) == "Main");
		});

	REQUIRE(Host.StartHost(Port));
	REQUIRE(Client.Connect("127.0.0.1", Port));

	const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
	while (std::chrono::steady_clock::now() < Deadline && !(bHostSawHello && WelcomedSlot != 255))
	{
		Host.Poll();
		Client.Poll();
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}

	REQUIRE(bHostSawHello);
	REQUIRE(WelcomedSlot == 1);

	Client.Shutdown();
	Host.Shutdown();
}
