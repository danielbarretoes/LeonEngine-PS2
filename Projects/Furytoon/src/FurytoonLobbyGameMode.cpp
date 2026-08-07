#include "FurytoonLobbyGameMode.h"

#include <cstring>
#include <filesystem>
#include <iostream>
#include <string_view>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <leon/core/Ascii.h>
#include <leon/core/Window.h>
#include <leon/Engine.h>
#include <leon/net/NetDriver.h>
#include <leon/net/NetProtocol.h>
#include <leon/net/NetUtil.h>

#include "FurytoonGameInstance.h"

namespace game {
namespace {

[[nodiscard]] FurytoonGameInstance& Session(leon::Engine& engine) {
    return FurytoonGameInstance::Get(engine.GetGameInstance());
}

[[nodiscard]] bool LevelKeysEqual(std::string_view a, std::string_view b) {
    return leon::AsciiToLower(a) == leon::AsciiToLower(b);
}

[[nodiscard]] std::string CurrentLevelKey(const leon::Engine& engine,
                                          const std::string& levelPath) {
    if (!engine.GetLevel().Name().empty()) {
        return engine.GetLevel().Name();
    }
    if (!levelPath.empty()) {
        return std::filesystem::path(levelPath).stem().string();
    }
    return {};
}

} // namespace

void FurytoonLobbyGameMode::rebuildMenu(leon::Engine& engine) {
    if (menu_ == nullptr) {
        return;
    }
    FurytoonGameInstance& session = Session(engine);
    const bool host = engine.GetGameInstance().IsNetHost();
    const bool client = engine.GetGameInstance().IsClient();
    const leon::NetDriver& net = engine.GetGameInstance().GetNetDriver();
    const int peers = net.PeerCount();
    const int maxRemote = net.MaxClients();

    std::string title = "LOBBY";
    if (host) {
        title += engine.GetGameInstance().IsDedicatedServer() ? "  (Dedicated Server)"
                                                              : "  (Listen Server)";
        title += "\nPlayers connected: " + std::to_string(peers) + "/" + std::to_string(maxRemote);
        if (!session.GetLanAddress().empty()) {
            title += "\nAdvertise: " + session.GetLanAddress() + ":7777";
        }
    } else if (client) {
        title += session.IsClientWelcomed() ? "  (Client)" : "  (Connecting...)";
    } else {
        title += "  (Standalone)";
    }
    title += "\nMap: " + session.GetMatchMapName();

    menu_->SetTitle(std::move(title));
    menu_->SetHint("Arrows / click  |  Enter  |  Esc / Backspace Back");
    menu_->ClearChildren();
    if (host) {
        menu_->AddButton("travel_match", "Start Match (Kitchen)");
        menu_->AddButton("menu", "Return to Main Menu");
    } else if (client) {
        if (session.IsClientWelcomed()) {
            menu_->AddButton("menu", "Disconnect");
        } else {
            menu_->AddButton("", "Connecting...");
        }
    } else {
        menu_->AddButton("menu", "Return to Main Menu");
    }
}

void FurytoonLobbyGameMode::NotifyClientsServerTravel(leon::Engine& engine,
                                                      std::string_view mapName) {
    leon::net::SendTravelToPeers(engine.GetGameInstance().GetNetDriver(), mapName,
                                 engine.GetGameInstance().IsDedicatedServer());
}

void FurytoonLobbyGameMode::ServerTravelToMatchMap(leon::Engine& engine) {
    FurytoonGameInstance& session = Session(engine);
    const std::string mapName = FurytoonGameInstance::kDefaultMatchMap;
    session.SetMatchMapName(mapName);
    NotifyClientsServerTravel(engine, mapName);
    (void)ServerTravel(engine, mapName, levelPath_);
}

void FurytoonLobbyGameMode::SendClientHello(leon::Engine& engine) {
    leon::net::HelloMsg hello{};
    engine.GetGameInstance().GetNetDriver().SendToPeer(&hello, sizeof(hello), true);
    std::cout << "FurytoonLobby: client Hello sent\n";
}

void FurytoonLobbyGameMode::setupNetCallbacks(leon::Engine& engine) {
    leon::NetDriver& net = engine.GetGameInstance().GetNetDriver();
    net.SetOnPacket([this, &engine](int peerSlot, const std::uint8_t* data, std::size_t size) {
        handlePacket(engine, peerSlot, data, size);
    });
    net.SetOnPeerConnected([this, &engine](int /*peerSlot*/) {
        if (engine.GetGameInstance().IsClient()) {
            SendClientHello(engine);
            return;
        }
        rebuildMenu(engine);
        engine.AddOnScreenDebugMessage("Player connected", 2.0f, {0.4f, 1.0f, 0.55f});
    });
    net.SetOnPeerDisconnected([this, &engine](int /*peerSlot*/) {
        if (engine.GetGameInstance().IsClient()) {
            Session(engine).ResetMatchTravelState();
            std::cout << "FurytoonLobby: disconnected -> MainMenu\n";
            engine.AddOnScreenDebugMessage("Disconnected", 3.0f, {1.0f, 0.4f, 0.3f});
            (void)ClientTravel(engine, FurytoonGameInstance::kMainMenuMap, levelPath_);
            return;
        }
        rebuildMenu(engine);
        engine.AddOnScreenDebugMessage("Player left", 2.0f, {1.0f, 0.7f, 0.3f});
    });
}

void FurytoonLobbyGameMode::handlePacket(leon::Engine& engine, int peerSlot,
                                         const std::uint8_t* data, std::size_t size) {
    if (data == nullptr || size < 1) {
        return;
    }
    const auto type = static_cast<leon::net::ENetMsg>(data[0]);
    leon::GameInstance& gi = engine.GetGameInstance();
    FurytoonGameInstance& session = Session(engine);

    if (type == leon::net::ENetMsg::Hello && gi.IsNetHost() &&
        size >= sizeof(leon::net::HelloMsg)) {
        leon::net::HelloMsg hello{};
        std::memcpy(&hello, data, sizeof(hello));
        if (!leon::net::IsValidHello(hello)) {
            return;
        }
        leon::net::WelcomeMsg welcome{};
        // Dedicated: peer == slot. Listen: host is 0, remotes are peer+1 (up to kMaxPlayers-1).
        const int assign = gi.IsDedicatedServer() ? peerSlot : peerSlot + 1;
        if (assign < 0 || assign >= leon::net::kMaxPlayers) {
            return;
        }
        welcome.slot = static_cast<std::uint8_t>(assign);
        leon::net::WriteLevelKey(welcome.levelKey, FurytoonGameInstance::kLobbyMap);
        gi.GetNetDriver().SendToPeer(peerSlot, &welcome, sizeof(welcome), true);
        return;
    }

    if (type == leon::net::ENetMsg::Welcome && gi.IsClient() &&
        size >= sizeof(leon::net::WelcomeMsg)) {
        leon::net::WelcomeMsg welcome{};
        std::memcpy(&welcome, data, sizeof(welcome));
        session.SetLocalPlayerId(welcome.slot);
        session.SetClientWelcomed(true);
        const std::string hostLevel = leon::net::ReadLevelKey(welcome.levelKey);
        const std::string here = CurrentLevelKey(engine, levelPath_);
        if (!hostLevel.empty() && !LevelKeysEqual(hostLevel, here)) {
            if (!LevelKeysEqual(hostLevel, FurytoonGameInstance::kLobbyMap) &&
                !LevelKeysEqual(hostLevel, FurytoonGameInstance::kMainMenuMap)) {
                session.SetMatchMapName(hostLevel);
            }
            std::cout << "FurytoonLobby: Welcome travel '" << here << "' -> '" << hostLevel
                      << "'\n";
            applyingTravel_ = true;
            (void)ClientTravel(engine, hostLevel, levelPath_);
            applyingTravel_ = false;
        }
        rebuildMenu(engine);
        if (menu_ != nullptr) {
            menu_->ResetEdges();
        }
        engine.AddOnScreenDebugMessage("Joined as slot " + std::to_string(welcome.slot) + " @ '" +
                                           (hostLevel.empty() ? here : hostLevel) + "'",
                                       3.0f, {0.4f, 1.0f, 0.6f});
        return;
    }

    if (type == leon::net::ENetMsg::Travel && gi.IsClient() &&
        size >= sizeof(leon::net::TravelMsg)) {
        leon::net::TravelMsg travel{};
        std::memcpy(&travel, data, sizeof(travel));
        session.SetLocalPlayerId(travel.slot);
        session.SetClientWelcomed(true);
        session.SetMatchMapName(leon::net::ReadLevelKey(travel.levelKey));
        const std::string here = CurrentLevelKey(engine, levelPath_);
        if (session.GetMatchMapName().empty() || LevelKeysEqual(session.GetMatchMapName(), here)) {
            return;
        }
        std::cout << "FurytoonLobby: Travel '" << here << "' -> '" << session.GetMatchMapName()
                  << "'\n";
        applyingTravel_ = true;
        (void)ClientTravel(engine, session.GetMatchMapName(), levelPath_);
        applyingTravel_ = false;
    }
}

void FurytoonLobbyGameMode::OnEnter(leon::Engine& engine, const std::string& levelPath) {
    if (applyingTravel_) {
        return;
    }
    // Flow: Lobby -- wait for players; dedicated headless auto ServerTravelToMatchMap.
    engine_ = &engine;
    levelPath_ = levelPath;
    engine.GetGameInstance().SetLevelBrowserVisible(false);
    engine.SetKeyboardOrbitEnabled(false);
    engine.SetOrbitMouseEnabled(false);
    engine.SetPlayMouseLookActive(false);
    engine.SetCursorCaptured(false);

    FurytoonGameInstance& session = Session(engine);
    if (session.GetMatchMapName().empty()) {
        session.SetMatchMapName(FurytoonGameInstance::kDefaultMatchMap);
    }
    session.SetLanAddress(leon::net::DetectPrimaryLanIPv4());

    setupNetCallbacks(engine);
    clientConnectSeconds_ = 0.0f;

    // CONNECT may have completed on MainMenu before Lobby callbacks existed -- Hello now.
    if (engine.GetGameInstance().IsClient() &&
        engine.GetGameInstance().GetNetDriver().IsConnected() && !session.IsClientWelcomed()) {
        SendClientHello(engine);
    }

    if (engine.GetGameInstance().IsDedicatedServer() && engine.IsHeadless()) {
        engine.GetGameInstance().NotifyLevelOpened();
        ServerTravelToMatchMap(engine);
        return;
    }

    engine.ClearCenterHudText();
    engine.GetHUD().Clear();
    menu_ = engine.GetHUD().AddWidget<leon::VerticalBoxWidget>();
    rebuildMenu(engine);
    menu_->ResetEdges();
    backKeyWasDown_ = true;
    engine.GetGameInstance().NotifyLevelOpened();
    engine.GetAudioDevice().PlayMusic("assets/Audio/Music/MenuBed.wav", 0.28f);
    std::cout << "FurytoonLobby: map='" << session.GetMatchMapName() << "' mode="
              << (engine.GetGameInstance().IsNetHost()
                      ? "host"
                      : (engine.GetGameInstance().IsClient() ? "client" : "standalone"))
              << '\n';
}

void FurytoonLobbyGameMode::OnExit(leon::Engine& engine) {
    engine.GetAudioDevice().StopMusic();
    leon::NetDriver& net = engine.GetGameInstance().GetNetDriver();
    net.SetOnPacket(nullptr);
    net.SetOnPeerConnected(nullptr);
    net.SetOnPeerDisconnected(nullptr);
    if (menu_ != nullptr) {
        engine.GetHUD().RemoveWidget(menu_);
        menu_ = nullptr;
    }
    engine.ClearCenterHudText();
    engine_ = nullptr;
}

void FurytoonLobbyGameMode::activate(leon::Engine& engine, const std::string& itemId) {
    FurytoonGameInstance& session = Session(engine);
    if (itemId == "menu") {
        std::cout << "FurytoonLobby: leave session -> MainMenu\n";
        engine.GetGameInstance().CloseNetSession();
        session.ResetSession();
        // CloseNetSession clears client/host flags -- always load MainMenu via ServerTravel.
        (void)ServerTravel(engine, FurytoonGameInstance::kMainMenuMap, levelPath_);
        return;
    }
    if (itemId == "travel_match" && engine.GetGameInstance().IsNetHost()) {
        ServerTravelToMatchMap(engine);
    }
}

void FurytoonLobbyGameMode::Tick(leon::Engine& engine, float deltaTime) {
    engine.GetGameInstance().GetNetDriver().Poll();
    const int peers = engine.GetGameInstance().GetNetDriver().PeerCount();
    if (peers != lastPeerCount_ && engine.GetGameInstance().IsNetHost()) {
        lastPeerCount_ = peers;
        rebuildMenu(engine);
    }

    FurytoonGameInstance& session = Session(engine);
    if (engine.GetGameInstance().IsClient() && !session.IsClientWelcomed()) {
        clientConnectSeconds_ += deltaTime;
        constexpr float kWelcomeTimeoutSeconds = 8.0f;
        if (clientConnectSeconds_ >= kWelcomeTimeoutSeconds) {
            std::cout << "FurytoonLobby: Welcome timeout -> MainMenu\n";
            engine.GetGameInstance().CloseNetSession();
            session.ResetSession();
            engine.AddOnScreenDebugMessage("Join failed -- no Welcome from host", 4.0f,
                                           {1.0f, 0.4f, 0.3f});
            (void)ServerTravel(engine, FurytoonGameInstance::kMainMenuMap, levelPath_);
            return;
        }
    }

    if (!engine.IsHeadless()) {
        leon::Window& window = engine.GetPlayInputWindow();
        const bool backDown = window.IsKeyPressed(GLFW_KEY_ESCAPE) ||
                              window.IsKeyPressed(GLFW_KEY_BACKSPACE) ||
                              window.IsKeyPressed(GLFW_KEY_DELETE);
        if (backDown && !backKeyWasDown_) {
            activate(engine, "menu");
            backKeyWasDown_ = backDown;
            return;
        }
        backKeyWasDown_ = backDown;
    }

    if (menu_ != nullptr) {
        const std::string id =
            menu_->TickInput(engine.GetPlayInputWindow(), engine.IsCursorCaptured(), deltaTime);
        if (!id.empty()) {
            if (id == "menu") {
                engine.GetAudioDevice().PlayUiSound(leon::EUiSound::Back);
            } else if (id == "travel_match") {
                engine.GetAudioDevice().PlayUiSound(leon::EUiSound::Confirm);
            } else {
                engine.GetAudioDevice().PlayUiSound(leon::EUiSound::Click);
            }
            activate(engine, id);
        }
    }
}

} // namespace game
