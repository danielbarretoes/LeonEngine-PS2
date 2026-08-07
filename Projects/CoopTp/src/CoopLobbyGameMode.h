#pragma once

#include <leon/Gameplay.h>
#include <string>

namespace game {

/// Pre-match Lobby (pick map; host ServerTravelToMatchMap — not GameMode::StartMatch).
class CoopLobbyGameMode final : public leon::GameMode {
public:
    [[nodiscard]] const char* Id() const override { return "coop-lobby"; }

    [[nodiscard]] bool Matches(const leon::LevelEntry& /*entry*/,
                               const std::string& gameModeId) const override {
        return gameModeId == Id() || gameModeId == "Lobby";
    }

    void OnEnter(leon::Engine& engine, const std::string& levelPath) override;
    void OnExit(leon::Engine& engine) override;
    void Tick(leon::Engine& engine, float deltaTime) override;

private:
    void setupNetCallbacks(leon::Engine& engine);
    void rebuildMenu(leon::Engine& engine);
    void activate(leon::Engine& engine, const std::string& itemId);
    /// Host → clients: net Travel msg before ServerTravel (Unreal ClientTravel notify lite).
    void NotifyClientsServerTravel(leon::Engine& engine, std::string_view mapName);
    /// Lobby host: travel everyone to the selected match map (Start Match UI).
    void ServerTravelToMatchMap(leon::Engine& engine);
    void handlePacket(leon::Engine& engine, int peerSlot, const std::uint8_t* data,
                      std::size_t size);

    /// Unreal-like client Hello after CONNECT (or if Lobby callbacks missed the connect event).
    void SendClientHello(leon::Engine& engine);

    leon::Engine* engine_ = nullptr;
    std::string levelPath_;
    leon::VerticalBoxWidget* menu_ = nullptr;
    int lastPeerCount_ = -1;
    bool applyingTravel_ = false;
    bool backKeyWasDown_ = false;
    float clientConnectSeconds_ = 0.0f;
};

} // namespace game
