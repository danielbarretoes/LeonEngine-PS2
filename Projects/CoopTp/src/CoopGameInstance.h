#pragma once

#include <cstdint>
#include <leon/gameplay/GameInstance.h>
#include <string>

namespace game {

/// Pack GameInstance (Unreal-style `UGameInstance` subclass).
/// Holds travel / join options that survive GameMode switches (Menu → Lobby → Match).
class CoopGameInstance final : public leon::GameInstance {
public:
    static constexpr const char* kMainMenuMap = "MainMenu";
    static constexpr const char* kLobbyMap = "Lobby";
    static constexpr const char* kDefaultMatchMap = "Courtyard";
    static constexpr const char* kAltMatchMap = "Rooftops";

    void Init() override {
        leon::GameInstance::Init();
        ResetSession();
    }

    /// Full disconnect / quit-to-menu reset.
    void ResetSession() {
        ResetMatchTravelState();
        joinAddress_ = "127.0.0.1";
        lanAddress_.clear();
    }

    /// Clears Welcome / local slot / soft-enter after leaving a match (keeps join/LAN).
    void ResetMatchTravelState() {
        bClientWelcomed_ = false;
        localPlayerId_ = 0;
        matchMapName_ = kDefaultMatchMap;
        bSkipNextSoftEnter_ = false;
    }

    [[nodiscard]] bool IsClientWelcomed() const { return bClientWelcomed_; }
    void SetClientWelcomed(bool welcomed) { bClientWelcomed_ = welcomed; }

    /// Local PlayerState id / net slot (Unreal UniqueId lite).
    [[nodiscard]] std::uint8_t GetLocalPlayerId() const { return localPlayerId_; }
    void SetLocalPlayerId(std::uint8_t playerId) { localPlayerId_ = playerId; }

    [[nodiscard]] const std::string& GetMatchMapName() const { return matchMapName_; }
    void SetMatchMapName(std::string mapName) { matchMapName_ = std::move(mapName); }

    [[nodiscard]] const std::string& GetJoinAddress() const { return joinAddress_; }
    void SetJoinAddress(std::string address) { joinAddress_ = std::move(address); }

    [[nodiscard]] const std::string& GetLanAddress() const { return lanAddress_; }
    void SetLanAddress(std::string address) { lanAddress_ = std::move(address); }

    void SetSkipNextSoftEnter(bool skip) { bSkipNextSoftEnter_ = skip; }
    [[nodiscard]] bool ConsumeSkipNextSoftEnter() {
        const bool skip = bSkipNextSoftEnter_;
        bSkipNextSoftEnter_ = false;
        return skip;
    }

    /// Dynamic cast helper (pack always installs CoopGameInstance in main).
    [[nodiscard]] static CoopGameInstance& Get(leon::GameInstance& gameInstance) {
        return static_cast<CoopGameInstance&>(gameInstance);
    }
    [[nodiscard]] static const CoopGameInstance& Get(const leon::GameInstance& gameInstance) {
        return static_cast<const CoopGameInstance&>(gameInstance);
    }

private:
    bool bClientWelcomed_ = false;
    std::uint8_t localPlayerId_ = 0;
    std::string matchMapName_ = kDefaultMatchMap;
    std::string joinAddress_ = "127.0.0.1";
    std::string lanAddress_;
    bool bSkipNextSoftEnter_ = false;
};

} // namespace game
