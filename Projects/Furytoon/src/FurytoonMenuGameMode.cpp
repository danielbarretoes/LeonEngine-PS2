#include "FurytoonMenuGameMode.h"

#include <algorithm>
#include <iostream>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <leon/core/Window.h>
#include <leon/Engine.h>
#include <leon/net/NetDriver.h>
#include <leon/net/NetUtil.h>

#include "FurytoonGameInstance.h"

namespace game {
namespace {

[[nodiscard]] FurytoonGameInstance& Session(leon::Engine& engine) {
    return FurytoonGameInstance::Get(engine.GetGameInstance());
}

[[nodiscard]] std::string JoinAddressOrLocalhost(const FurytoonGameInstance& session) {
    return session.GetJoinAddress().empty() ? std::string("127.0.0.1") : session.GetJoinAddress();
}

} // namespace

void FurytoonMenuGameMode::rebuildMenu() {
    if (menu_ == nullptr || engine_ == nullptr) {
        return;
    }
    FurytoonGameInstance& session = Session(*engine_);
    const std::string addr = JoinAddressOrLocalhost(session);
    std::string joinLabel = "Join Listen Host (" + addr + ")";
    std::string joinDedicatedLabel = "Join Dedicated (" + addr + ")";
    if (pendingJoin_) {
        joinLabel = "Connecting to " + addr + "...";
        joinDedicatedLabel = joinLabel;
    }
    // Flow: Host = listen on this PC; Join Listen = peer Host; Join Dedicated = headless server.
    // Dedicated host binary: leon-Furytoon-server (or leon-Furytoon.exe --dedicated).
    std::string hint = "Arrows / click  |  Enter  |  Esc Quit";
    hint += "\nHost: friends Join Listen your LAN IP";
    hint += "\nJoin Dedicated: needs leon-Furytoon-server (or --dedicated) already running";
    if (!session.GetLanAddress().empty()) {
        hint += "\nThis PC LAN: " + session.GetLanAddress() + ":7777";
    }
    hint += "  |  --join <ip>";
    menu_->SetTitle("Furytoon");
    menu_->SetHint(std::move(hint));
    menu_->ClearChildren();
    if (pendingJoin_) {
        menu_->AddButton("", joinLabel);
        menu_->AddButton("cancel_join", "Cancel");
        menu_->AddButton("quit", "Quit Game");
        menu_->SetSelectedIndex(1); // Cancel
    } else {
        menu_->AddButton("listen", "Host Game");
        menu_->AddButton("join", joinLabel);
        menu_->AddButton("join_dedicated", joinDedicatedLabel);
        menu_->AddButton("quit", "Quit Game");
        menu_->SetSelectedIndex(0); // Host
    }
    syncJoinProgressBar();
}

void FurytoonMenuGameMode::syncJoinProgressBar() {
    if (joinProgress_ == nullptr) {
        return;
    }
    joinProgress_->SetVisibility(pendingJoin_);
    if (!pendingJoin_) {
        joinProgress_->SetPercent(0.0f);
        return;
    }
    constexpr float kJoinTimeoutSeconds = 6.0f;
    joinProgress_->SetPercent(std::clamp(pendingJoinSeconds_ / kJoinTimeoutSeconds, 0.0f, 1.0f));
}

void FurytoonMenuGameMode::OnEnter(leon::Engine& engine, const std::string& levelPath) {
    engine_ = &engine;
    levelPath_ = levelPath;
    engine.GetGameInstance().SetLevelBrowserVisible(false);
    engine.SetKeyboardOrbitEnabled(false);
    engine.SetOrbitMouseEnabled(false);
    engine.SetPlayMouseLookActive(false);
    engine.SetCursorCaptured(false);
    engine.ClearCenterHudText();

    FurytoonGameInstance& session = Session(engine);
    session.SetLanAddress(leon::net::DetectPrimaryLanIPv4());
    if (std::string pendingJoin = engine.GetGameInstance().ConsumePendingJoinAddress();
        !pendingJoin.empty()) {
        session.SetJoinAddress(std::move(pendingJoin));
    } else if (session.GetJoinAddress().empty()) {
        session.SetJoinAddress("127.0.0.1");
    }

    if (engine.GetGameInstance().HasPendingDedicatedStart()) {
        const std::uint16_t port = engine.GetGameInstance().PendingDedicatedPort();
        if (!engine.GetGameInstance().HostDedicated(port)) {
            engine.AddOnScreenDebugMessage("Dedicated Server bind failed", 4.0f,
                                           {1.0f, 0.3f, 0.3f});
            engine.GetHUD().Clear();
            menuBackdrop_ = engine.GetHUD().AddWidget<leon::ImageWidget>();
            menuBackdrop_->SetFillScreen(true);
            menuBackdrop_->SetColor({0.04f, 0.04f, 0.055f});
            menu_ = engine.GetHUD().AddWidget<leon::VerticalBoxWidget>();
            rebuildMenu();
            menu_->ResetEdges();
            engine.GetGameInstance().NotifyLevelOpened();
            return;
        }
        engine.GetGameInstance().ClearPendingDedicatedStart();
        session.ResetMatchTravelState();
        const char* dest = engine.IsHeadless() ? FurytoonGameInstance::kDefaultMatchMap
                                               : FurytoonGameInstance::kLobbyMap;
        (void)ServerTravel(engine, dest, levelPath_);
        engine.GetGameInstance().NotifyLevelOpened();
        return;
    }

    pendingJoin_ = false;
    pendingJoinSeconds_ = 0.0f;
    engine.GetHUD().Clear();
    menuBackdrop_ = engine.GetHUD().AddWidget<leon::ImageWidget>();
    menuBackdrop_->SetFillScreen(true);
    menuBackdrop_->SetColor({0.04f, 0.04f, 0.055f});
    menu_ = engine.GetHUD().AddWidget<leon::VerticalBoxWidget>();
    joinProgress_ = engine.GetHUD().AddWidget<leon::ProgressBarWidget>();
    joinProgress_->SetSize(320.0f, 14.0f);
    joinProgress_->SetAnchoredBottomCenter(true);
    joinProgress_->SetShowPercentText(true);
    joinProgress_->SetFillColor({0.35f, 0.75f, 1.0f});
    joinProgress_->SetVisibility(false);
    rebuildMenu();
    menu_->ResetEdges();
    backKeyWasDown_ = true;
    engine.GetGameInstance().NotifyLevelOpened();
    engine.GetAudioDevice().PlayMusic("assets/Audio/Music/MenuBed.wav", 0.28f);
    std::cout << "MainMenu: LAN advertise "
              << (session.GetLanAddress().empty() ? "(none)" : session.GetLanAddress())
              << " | Join " << JoinAddressOrLocalhost(session) << '\n';
}

void FurytoonMenuGameMode::OnExit(leon::Engine& engine) {
    pendingJoin_ = false;
    pendingJoinSeconds_ = 0.0f;
    engine.GetAudioDevice().StopMusic();
    if (joinProgress_ != nullptr) {
        engine.GetHUD().RemoveWidget(joinProgress_);
        joinProgress_ = nullptr;
    }
    if (menu_ != nullptr) {
        engine.GetHUD().RemoveWidget(menu_);
        menu_ = nullptr;
    }
    if (menuBackdrop_ != nullptr) {
        engine.GetHUD().RemoveWidget(menuBackdrop_);
        menuBackdrop_ = nullptr;
    }
    engine.ClearCenterHudText();
    engine_ = nullptr;
}

void FurytoonMenuGameMode::cancelPendingJoin(leon::Engine& engine, const std::string& reason) {
    pendingJoin_ = false;
    pendingJoinSeconds_ = 0.0f;
    engine.GetGameInstance().CloseNetSession();
    if (!reason.empty()) {
        engine.AddOnScreenDebugMessage(reason, 4.0f, {1.0f, 0.45f, 0.3f});
        std::cout << "MainMenu: " << reason << '\n';
    }
    rebuildMenu();
    if (menu_ != nullptr) {
        menu_->ResetEdges();
    }
}

void FurytoonMenuGameMode::activate(leon::Engine& engine, const std::string& itemId) {
    FurytoonGameInstance& session = Session(engine);
    if (itemId == "quit") {
        engine.RequestQuit();
        return;
    }
    if (itemId == "cancel_join") {
        cancelPendingJoin(engine, "Join cancelled");
        return;
    }
    if (pendingJoin_) {
        return;
    }
    if (itemId == "listen") {
        // Flow: bind ListenServer -> ServerTravel Lobby (host waits for Join there).
        pendingJoin_ = false;
        pendingJoinSeconds_ = 0.0f;
        engine.GetGameInstance().CloseNetSession();
        if (!engine.GetGameInstance().HostListen(leon::net::kDefaultPort)) {
            engine.AddOnScreenDebugMessage("Host failed (port 7777 in use?)", 4.0f,
                                           {1.0f, 0.3f, 0.3f});
            engine.GetAudioDevice().PlayUiSound(leon::EUiSound::Error);
            std::cout << "MainMenu: Host Listen bind failed\n";
            return;
        }
        session.ResetMatchTravelState();
        session.SetLocalPlayerId(0);
        const std::string tip =
            session.GetLanAddress().empty() ? "127.0.0.1" : session.GetLanAddress();
        engine.AddOnScreenDebugMessage("Hosting -- friends: Join Listen Host " + tip, 6.0f,
                                       {0.4f, 1.0f, 0.55f});
        std::cout << "MainMenu: Host -> Lobby (advertise " << tip << ":7777)\n";
        if (!ServerTravel(engine, FurytoonGameInstance::kLobbyMap, levelPath_)) {
            engine.AddOnScreenDebugMessage("Host OK but Lobby travel failed", 4.0f,
                                           {1.0f, 0.3f, 0.3f});
            std::cout << "MainMenu: ServerTravel Lobby failed\n";
        }
        return;
    }
    if (itemId == "join" || itemId == "join_dedicated") {
        // Same Connect API for listen host or dedicated -- client does not distinguish.
        const bool dedicatedJoin = (itemId == "join_dedicated");
        const std::string addr = JoinAddressOrLocalhost(session);
        session.SetJoinAddress(addr);
        engine.GetGameInstance().CloseNetSession();
        if (!engine.GetGameInstance().Join(addr, leon::net::kDefaultPort)) {
            engine.AddOnScreenDebugMessage("Join failed (" + addr + ")", 3.0f, {1.0f, 0.3f, 0.3f});
            rebuildMenu();
            return;
        }
        session.ResetMatchTravelState();
        pendingJoin_ = true;
        pendingJoinSeconds_ = 0.0f;
        rebuildMenu();
        if (menu_ != nullptr) {
            menu_->ResetEdges();
        }
        const char* kind = dedicatedJoin ? "dedicated" : "listen host";
        engine.AddOnScreenDebugMessage(std::string("Connecting to ") + kind + " " + addr +
                                           ":7777...",
                                       3.0f, {0.55f, 0.85f, 1.0f});
        std::cout << "MainMenu: Join " << kind << " pending -> " << addr << ":7777\n";
    }
}

void FurytoonMenuGameMode::Tick(leon::Engine& engine, float deltaTime) {
    engine.GetGameInstance().GetNetDriver().Poll();

    if (pendingJoin_) {
        pendingJoinSeconds_ += deltaTime;
        syncJoinProgressBar();
        constexpr float kJoinTimeoutSeconds = 6.0f;
        leon::NetDriver& net = engine.GetGameInstance().GetNetDriver();
        if (!engine.GetGameInstance().IsClient()) {
            cancelPendingJoin(engine, "Join lost (not a client)");
        } else if (net.IsConnected()) {
            pendingJoin_ = false;
            pendingJoinSeconds_ = 0.0f;
            std::cout << "MainMenu: connected -- ClientTravel Lobby\n";
            engine.AddOnScreenDebugMessage("Connected -- entering Lobby...", 2.0f,
                                           {0.4f, 1.0f, 0.55f});
            (void)ClientTravel(engine, FurytoonGameInstance::kLobbyMap, levelPath_);
            return;
        } else if (pendingJoinSeconds_ >= kJoinTimeoutSeconds) {
            cancelPendingJoin(engine, "Join timed out -- is a Host listening on that IP:7777?");
        }
    }

    if (!engine.IsHeadless()) {
        leon::Window& window = engine.GetPlayInputWindow();
        const bool backDown = window.IsKeyPressed(GLFW_KEY_ESCAPE) ||
                              window.IsKeyPressed(GLFW_KEY_BACKSPACE) ||
                              window.IsKeyPressed(GLFW_KEY_DELETE);
        if (backDown && !backKeyWasDown_) {
            if (pendingJoin_) {
                cancelPendingJoin(engine, "Join cancelled");
            } else {
                activate(engine, "quit");
            }
            backKeyWasDown_ = backDown;
            return;
        }
        backKeyWasDown_ = backDown;
    }

    if (menu_ != nullptr) {
        const std::string id =
            menu_->TickInput(engine.GetPlayInputWindow(), engine.IsCursorCaptured(), deltaTime);
        if (!id.empty()) {
            if (id == "quit" || id == "cancel_join" || id == "menu") {
                engine.GetAudioDevice().PlayUiSound(leon::EUiSound::Back);
            } else if (id == "listen" || id == "join" || id == "join_dedicated") {
                engine.GetAudioDevice().PlayUiSound(leon::EUiSound::Confirm);
            } else {
                engine.GetAudioDevice().PlayUiSound(leon::EUiSound::Click);
            }
            activate(engine, id);
        }
    }
}

} // namespace game
