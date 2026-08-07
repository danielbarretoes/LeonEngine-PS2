#pragma once

#include <leon/Gameplay.h>
#include <leon/gameplay/SpringArmComponent.h>

#include "CoopTpAnimInstance.h"

namespace game {

/// Coop LAN Character — third-person boom + Bot skeletal mesh.
class CoopTpCharacter final : public leon::Character {
public:
    CoopTpCharacter();

    [[nodiscard]] leon::SpringArmComponent& SpringArm() { return springArm_; }
    [[nodiscard]] const leon::SpringArmComponent& SpringArm() const { return springArm_; }

private:
    leon::SpringArmComponent springArm_{};
};

} // namespace game
