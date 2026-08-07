#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace leon {

class PlayerState;

/// Shared match/session state (Unreal-style `AGameStateBase` / `AGameState`).
/// Owned by GameMode; replicated fields are advanced by the net GameMode on authority.
class GameState {
public:
    GameState() = default;
    virtual ~GameState() = default;

    GameState(const GameState&) = delete;
    GameState& operator=(const GameState&) = delete;
    GameState(GameState&&) = delete;
    GameState& operator=(GameState&&) = delete;

    /// Resets match clock / flags / map — not PlayerArray (Unreal: logout removes players).
    virtual void Reset() {
        elapsedSeconds_ = 0.0f;
        matchInProgress_ = false;
        matchHasEnded_ = false;
        replicatedWorldTimeFrames_ = 0;
        mapName_.clear();
    }

    virtual void Tick(float deltaTime) {
        if (matchInProgress_ && !matchHasEnded_) {
            elapsedSeconds_ += deltaTime;
        }
    }

    /// Unreal `HasMatchStarted`.
    [[nodiscard]] bool HasMatchStarted() const { return matchInProgress_; }
    /// Unreal `HasMatchEnded`.
    [[nodiscard]] bool HasMatchEnded() const { return matchHasEnded_; }
    /// Unreal `GetServerWorldTimeSeconds` (local elapsed while match is in progress).
    [[nodiscard]] float GetServerWorldTimeSeconds() const { return elapsedSeconds_; }

    /// Unreal `PlayerArray` — PlayerStates registered via PostLogin / Logout.
    [[nodiscard]] const std::vector<PlayerState*>& GetPlayerArray() const { return playerArray_; }
    /// Unreal `PlayerArray.Num()`.
    [[nodiscard]] int GetNumPlayers() const { return static_cast<int>(playerArray_.size()); }

    /// Unreal `AGameStateBase::AddPlayerState` (idempotent).
    void AddPlayerState(PlayerState* playerState) {
        if (playerState == nullptr) {
            return;
        }
        if (std::find(playerArray_.begin(), playerArray_.end(), playerState) != playerArray_.end()) {
            return;
        }
        playerArray_.push_back(playerState);
    }

    /// Unreal `AGameStateBase::RemovePlayerState`.
    void RemovePlayerState(PlayerState* playerState) {
        if (playerState == nullptr) {
            return;
        }
        playerArray_.erase(std::remove(playerArray_.begin(), playerArray_.end(), playerState),
                           playerArray_.end());
    }

    [[nodiscard]] bool HasPlayerState(const PlayerState* playerState) const {
        if (playerState == nullptr) {
            return false;
        }
        return std::find(playerArray_.begin(), playerArray_.end(), playerState) !=
               playerArray_.end();
    }

    /// Authority: mark match in progress (pairs with GameMode::StartMatch).
    virtual void HandleMatchHasStarted() {
        matchInProgress_ = true;
        matchHasEnded_ = false;
    }
    /// Authority: mark match finished (pairs with GameMode::EndMatch).
    virtual void HandleMatchHasEnded() {
        matchInProgress_ = false;
        matchHasEnded_ = true;
    }

    /// Current map identity (Level document name / travel key). Unreal: map package name.
    [[nodiscard]] const std::string& GetMapName() const { return mapName_; }
    void SetMapName(std::string name) { mapName_ = std::move(name); }

    /// Replicated simulation frame (host advances; clients apply from Snapshot).
    [[nodiscard]] std::uint32_t GetReplicatedWorldTimeFrames() const {
        return replicatedWorldTimeFrames_;
    }
    void SetReplicatedWorldTimeFrames(std::uint32_t tick) { replicatedWorldTimeFrames_ = tick; }
    void IncrementReplicatedWorldTimeFrames() { ++replicatedWorldTimeFrames_; }

private:
    float elapsedSeconds_ = 0.0f;
    bool matchInProgress_ = false;
    bool matchHasEnded_ = false;
    std::uint32_t replicatedWorldTimeFrames_ = 0;
    std::string mapName_;
    std::vector<PlayerState*> playerArray_;
};

} // namespace leon
