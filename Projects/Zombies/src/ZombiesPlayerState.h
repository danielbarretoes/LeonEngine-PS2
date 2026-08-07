#pragma once

#include <leon/gameplay/PlayerState.h>

#include "ZombiesInteract.h"

namespace game {

/// Per-player Zombies session: score, lives, kills, Town perks.
class ZombiesPlayerState final : public leon::PlayerState {
public:
    static constexpr int kStartingLives = 3;

    ZombiesPlayerState() { SetLives(kStartingLives); }

    void Reset() override {
        leon::PlayerState::Reset();
        kills_ = 0;
        SetLives(kStartingLives);
        deaths_ = 0;
        ClearPerks();
        bQuickReviveUsed_ = false;
    }

    [[nodiscard]] int GetKills() const { return kills_; }
    void SetKills(int kills) { kills_ = kills; }
    void AddKill() { ++kills_; }

    // Lives: leon::PlayerState GetLives / SetLives / ConsumeLife.

    [[nodiscard]] int GetDeaths() const { return deaths_; }
    void AddDeath() { ++deaths_; }

    /// On death: consume one life stock. Returns true if the player still has lives left.
    [[nodiscard]] bool ConsumeLifeOnDeath() {
        AddDeath();
        (void)ConsumeLife();
        return GetLives() > 0;
    }

    void ClearPerks() {
        hasJugg_ = false;
        hasSpeed_ = false;
        hasDoubleTap_ = false;
        hasQuickRevive_ = false;
    }

    [[nodiscard]] bool HasPerk(EZombiesPerk perk) const {
        switch (perk) {
        case EZombiesPerk::Juggernog:
            return hasJugg_;
        case EZombiesPerk::SpeedCola:
            return hasSpeed_;
        case EZombiesPerk::DoubleTap:
            return hasDoubleTap_;
        case EZombiesPerk::QuickRevive:
            return hasQuickRevive_;
        default:
            return false;
        }
    }

    void GrantPerk(EZombiesPerk perk) {
        switch (perk) {
        case EZombiesPerk::Juggernog:
            hasJugg_ = true;
            break;
        case EZombiesPerk::SpeedCola:
            hasSpeed_ = true;
            break;
        case EZombiesPerk::DoubleTap:
            hasDoubleTap_ = true;
            break;
        case EZombiesPerk::QuickRevive:
            hasQuickRevive_ = true;
            break;
        default:
            break;
        }
    }

    [[nodiscard]] bool TryConsumeQuickRevive() {
        if (!hasQuickRevive_ || bQuickReviveUsed_) {
            return false;
        }
        bQuickReviveUsed_ = true;
        return true;
    }

    [[nodiscard]] bool SpendScore(int cost) {
        if (cost <= 0) {
            return true;
        }
        if (GetScore() + 0.5f < static_cast<float>(cost)) {
            return false;
        }
        SetScore(GetScore() - static_cast<float>(cost));
        return true;
    }

private:
    int kills_ = 0;
    int deaths_ = 0;
    bool hasJugg_ = false;
    bool hasSpeed_ = false;
    bool hasDoubleTap_ = false;
    bool hasQuickRevive_ = false;
    bool bQuickReviveUsed_ = false;
};

} // namespace game
