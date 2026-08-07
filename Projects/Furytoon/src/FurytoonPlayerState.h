#pragma once

#include <leon/gameplay/PlayerState.h>

namespace game {

class FurytoonPlayerState final : public leon::PlayerState {
public:
    void Reset() override {
        PlayerState::Reset();
        kos_ = 0;
    }

    [[nodiscard]] int GetKOs() const { return kos_; }
    void AddKO() { ++kos_; }
    void SetKOs(int kos) { kos_ = kos; }

    /// Stocks alias PlayerState lives (stocks == lives).
    [[nodiscard]] int GetStocks() const { return GetLives(); }
    void SetStocks(int stocks) { SetLives(stocks); }
    /// Consumes one stock. Returns true if stocks remain (fighter can respawn).
    [[nodiscard]] bool ConsumeStock() {
        if (!ConsumeLife()) {
            return false;
        }
        return GetLives() > 0;
    }

private:
    int kos_ = 0;
};

} // namespace game
