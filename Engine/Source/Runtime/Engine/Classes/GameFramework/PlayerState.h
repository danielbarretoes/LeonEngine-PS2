#pragma once

#include <string>


/// Per-player session data (Unreal-style `APlayerState`). Typically owned by APlayerController.
class APlayerState {
public:
    APlayerState() = default;
    virtual ~APlayerState() = default;

    APlayerState(const APlayerState&) = delete;
    APlayerState& operator=(const APlayerState&) = delete;
    APlayerState(APlayerState&&) = delete;
    APlayerState& operator=(APlayerState&&) = delete;

    virtual void Reset() {
        Score = 0.0f;
        Lives = 0;
        PlayerName.clear();
    }

    virtual void Tick(float /*deltaTime*/) {}

    /// Unreal `GetPlayerId`.
    [[nodiscard]] int GetPlayerId() const { return PlayerId; }
    void SetPlayerId(int Id) { PlayerId = Id; }

    /// Unreal `GetPlayerName` / `SetPlayerName`.
    [[nodiscard]] const std::string& GetPlayerName() const { return PlayerName; }
    void SetPlayerName(std::string Name) { PlayerName = std::move(Name); }

    [[nodiscard]] float GetScore() const { return Score; }
    void SetScore(float InScore) { Score = InScore; }
    void AddScore(float Delta) { Score += Delta; }

    /// Stocks / lives (Unreal-like). Default 0 — packs call SetLives at match start.
    [[nodiscard]] int GetLives() const { return Lives; }
    void SetLives(int InLives) { Lives = InLives; }
    /// Decrements one life if any remain. Returns true if a life was consumed.
    [[nodiscard]] bool ConsumeLife() {
        if (Lives <= 0) {
            return false;
        }
        --Lives;
        return true;
    }

private:
    int PlayerId = 0;
    float Score = 0.0f;
    int Lives = 0;
    std::string PlayerName;
};

