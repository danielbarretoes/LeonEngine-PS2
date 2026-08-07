#pragma once

#include <leon/gameplay/GameState.h>
#include <string>

namespace game {

/// Coop match GameState (Unreal-style `AGameState` subclass).
/// PlayerArray / GetNumPlayers live on `leon::GameState`; this adds travel target map.
class CoopTpGameState final : public leon::GameState {
public:
    void Reset() override {
        leon::GameState::Reset();
        expectedMapName_.clear();
    }

    /// Map the host wants clients on (travel target). Mirrors UE map URL package name.
    [[nodiscard]] const std::string& GetExpectedMapName() const {
        return expectedMapName_.empty() ? GetMapName() : expectedMapName_;
    }
    void SetExpectedMapName(std::string name) { expectedMapName_ = std::move(name); }

private:
    std::string expectedMapName_;
};

} // namespace game
