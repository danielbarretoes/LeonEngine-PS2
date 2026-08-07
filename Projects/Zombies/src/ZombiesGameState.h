#pragma once

#include <algorithm>
#include <leon/gameplay/GameState.h>
#include <string>

namespace game {

/// Zombies match GameState (Unreal-style `AGameState` subclass).
/// PlayerArray / GetNumPlayers live on `leon::GameState`; this adds wave/round bookkeeping that
/// mirrors `leon::net::SnapshotMatchMeta` (RoundIndex, UnitsAlive) plus travel target map.
class ZombiesGameState final : public leon::GameState {
public:
    void Reset() override {
        leon::GameState::Reset();
        roundIndex_ = 0;
        zombiesRemaining_ = 0;
        intermissionSeconds_ = 0;
        bRoundActive_ = false;
        expectedMapName_.clear();
    }

    /// 1-based wave number (Unreal-ish "Round 1", "Round 2", ...).
    [[nodiscard]] int GetRoundIndex() const { return roundIndex_; }
    void SetRoundIndex(int roundIndex) { roundIndex_ = roundIndex; }
    void IncrementRoundIndex() { ++roundIndex_; }

    [[nodiscard]] int GetZombiesRemaining() const { return zombiesRemaining_; }
    void SetZombiesRemaining(int count) { zombiesRemaining_ = count; }
    void DecrementZombiesRemaining() {
        if (zombiesRemaining_ > 0) {
            --zombiesRemaining_;
        }
    }

    /// True while a wave is actively spawned (between "Round Start" and last zombie killed).
    [[nodiscard]] bool IsRoundActive() const { return bRoundActive_; }
    void SetRoundActive(bool active) { bRoundActive_ = active; }

    /// Host intermission countdown (ceiling seconds); 0 = not in intermission. Replicated in snap.
    [[nodiscard]] int GetIntermissionSeconds() const { return intermissionSeconds_; }
    void SetIntermissionSeconds(int seconds) { intermissionSeconds_ = std::max(0, seconds); }

    /// Map the host wants clients on (travel target). Mirrors UE map URL package name.
    [[nodiscard]] const std::string& GetExpectedMapName() const {
        return expectedMapName_.empty() ? GetMapName() : expectedMapName_;
    }
    void SetExpectedMapName(std::string name) { expectedMapName_ = std::move(name); }

private:
    int roundIndex_ = 0;
    int zombiesRemaining_ = 0;
    int intermissionSeconds_ = 0;
    bool bRoundActive_ = false;
    std::string expectedMapName_;
};

} // namespace game
