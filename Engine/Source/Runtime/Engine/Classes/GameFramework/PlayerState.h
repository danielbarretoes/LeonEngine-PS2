#pragma once

#include <string>


/// Per-player session data (Unreal-style `APlayerState`). Typically owned by PlayerController.
class PlayerState {
public:
    PlayerState() = default;
    virtual ~PlayerState() = default;

    PlayerState(const PlayerState&) = delete;
    PlayerState& operator=(const PlayerState&) = delete;
    PlayerState(PlayerState&&) = delete;
    PlayerState& operator=(PlayerState&&) = delete;

    virtual void Reset() {
        score_ = 0.0f;
        lives_ = 0;
        playerName_.clear();
    }

    virtual void Tick(float /*deltaTime*/) {}

    /// Unreal `GetPlayerId`.
    [[nodiscard]] int GetPlayerId() const { return playerId_; }
    void SetPlayerId(int id) { playerId_ = id; }

    /// Unreal `GetPlayerName` / `SetPlayerName`.
    [[nodiscard]] const std::string& GetPlayerName() const { return playerName_; }
    void SetPlayerName(std::string name) { playerName_ = std::move(name); }

    [[nodiscard]] float GetScore() const { return score_; }
    void SetScore(float score) { score_ = score; }
    void AddScore(float delta) { score_ += delta; }

    /// Stocks / lives (Unreal-like). Default 0 — packs call SetLives at match start.
    [[nodiscard]] int GetLives() const { return lives_; }
    void SetLives(int lives) { lives_ = lives; }
    /// Decrements one life if any remain. Returns true if a life was consumed.
    [[nodiscard]] bool ConsumeLife() {
        if (lives_ <= 0) {
            return false;
        }
        --lives_;
        return true;
    }

private:
    int playerId_ = 0;
    float score_ = 0.0f;
    int lives_ = 0;
    std::string playerName_;
};

