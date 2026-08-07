#pragma once

#include <leon/gameplay/PlayerState.h>

namespace game {

/// Per-player coop session data (Unreal-style `APlayerState` subclass).
class CoopTpPlayerState final : public leon::PlayerState {
public:
    void Reset() override {
        leon::PlayerState::Reset();
        bReady_ = false;
    }

    [[nodiscard]] bool IsReady() const { return bReady_; }
    void SetReady(bool ready) { bReady_ = ready; }

private:
    bool bReady_ = false;
};

} // namespace game
