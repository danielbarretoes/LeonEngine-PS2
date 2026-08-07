#pragma once

#include <leon/gameplay/GameState.h>
#include <string>

namespace game {

class FurytoonGameState final : public leon::GameState {
public:
    void Reset() override {
        leon::GameState::Reset();
        expectedMapName_.clear();
        fightersAlive_ = 0;
    }

    [[nodiscard]] int GetFightersAlive() const { return fightersAlive_; }
    void SetFightersAlive(int count) { fightersAlive_ = count; }

    [[nodiscard]] const std::string& GetExpectedMapName() const {
        return expectedMapName_.empty() ? GetMapName() : expectedMapName_;
    }
    void SetExpectedMapName(std::string name) { expectedMapName_ = std::move(name); }

private:
    int fightersAlive_ = 0;
    std::string expectedMapName_;
};

} // namespace game
