#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>


class APlayerState;

/// Shared match/session state (Unreal-style `AGameStateBase` / `AGameState`).
/// Owned by GameMode; replicated fields are advanced by the net GameMode on authority.
class AGameStateBase {
public:
    AGameStateBase() = default;
    virtual ~AGameStateBase() = default;

    AGameStateBase(const AGameStateBase&) = delete;
    AGameStateBase& operator=(const AGameStateBase&) = delete;
    AGameStateBase(AGameStateBase&&) = delete;
    AGameStateBase& operator=(AGameStateBase&&) = delete;

    /// Resets match clock / flags / map — not PlayerArray (Unreal: logout removes players).
    virtual void Reset() {
        ElapsedSeconds = 0.0f;
        bMatchInProgress = false;
        bMatchHasEnded = false;
        ReplicatedWorldTimeFrames = 0;
        MapName.clear();
    }

    virtual void Tick(float DeltaTime) {
        if (bMatchInProgress && !bMatchHasEnded) {
            ElapsedSeconds += DeltaTime;
        }
    }

    /// Unreal `HasMatchStarted`.
    [[nodiscard]] bool HasMatchStarted() const { return bMatchInProgress; }
    /// Unreal `HasMatchEnded`.
    [[nodiscard]] bool HasMatchEnded() const { return bMatchHasEnded; }
    /// Unreal `GetServerWorldTimeSeconds` (local elapsed while match is in progress).
    [[nodiscard]] float GetServerWorldTimeSeconds() const { return ElapsedSeconds; }

    /// Unreal `PlayerArray` — PlayerStates registered via PostLogin / Logout.
    [[nodiscard]] const std::vector<APlayerState*>& GetPlayerArray() const { return PlayerArray; }
    /// Unreal `PlayerArray.Num()`.
    [[nodiscard]] int GetNumPlayers() const { return static_cast<int>(PlayerArray.size()); }

    /// Unreal `AGameStateBase::AddPlayerState` (idempotent).
    void AddPlayerState(APlayerState* PlayerState) {
        if (PlayerState == nullptr) {
            return;
        }
        if (std::find(PlayerArray.begin(), PlayerArray.end(), PlayerState) != PlayerArray.end()) {
            return;
        }
        PlayerArray.push_back(PlayerState);
    }

    /// Unreal `AGameStateBase::RemovePlayerState`.
    void RemovePlayerState(APlayerState* PlayerState) {
        if (PlayerState == nullptr) {
            return;
        }
        PlayerArray.erase(std::remove(PlayerArray.begin(), PlayerArray.end(), PlayerState),
                           PlayerArray.end());
    }

    [[nodiscard]] bool HasPlayerState(const APlayerState* PlayerState) const {
        if (PlayerState == nullptr) {
            return false;
        }
        return std::find(PlayerArray.begin(), PlayerArray.end(), PlayerState) !=
               PlayerArray.end();
    }

    /// Authority: mark match in progress (pairs with GameMode::StartMatch).
    virtual void HandleMatchHasStarted() {
        bMatchInProgress = true;
        bMatchHasEnded = false;
    }
    /// Authority: mark match finished (pairs with GameMode::EndMatch).
    virtual void HandleMatchHasEnded() {
        bMatchInProgress = false;
        bMatchHasEnded = true;
    }

    /// Current map identity (Level document name / travel key). Unreal: map package name.
    [[nodiscard]] const std::string& GetMapName() const { return MapName; }
    void SetMapName(std::string Name) { MapName = std::move(Name); }

    /// Replicated simulation frame (host advances; clients apply from Snapshot).
    [[nodiscard]] std::uint32_t GetReplicatedWorldTimeFrames() const {
        return ReplicatedWorldTimeFrames;
    }
    void SetReplicatedWorldTimeFrames(std::uint32_t InTick) { ReplicatedWorldTimeFrames = InTick; }
    void IncrementReplicatedWorldTimeFrames() { ++ReplicatedWorldTimeFrames; }

private:
    float ElapsedSeconds = 0.0f;
    bool bMatchInProgress = false;
    bool bMatchHasEnded = false;
    std::uint32_t ReplicatedWorldTimeFrames = 0;
    std::string MapName;
    std::vector<APlayerState*> PlayerArray;
};

