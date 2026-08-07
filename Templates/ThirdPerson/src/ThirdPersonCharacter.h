#pragma once

#include <leon/Gameplay.h>
#include <leon/gameplay/SpringArmComponent.h>

#include "ThirdPersonAnimInstance.h"

namespace game {

/// Third-person Character: capsule + SpringArm + Bot skeletal mesh / AnimInstance.
class ThirdPersonCharacter final : public leon::Character {
public:
    ThirdPersonCharacter();

    [[nodiscard]] leon::SpringArmComponent& SpringArm() { return springArm_; }
    [[nodiscard]] const leon::SpringArmComponent& SpringArm() const { return springArm_; }

    [[nodiscard]] ThirdPersonAnimInstance* GetAnimInstance() {
        return GetMesh().GetAnimInstance<ThirdPersonAnimInstance>();
    }

private:
    leon::SpringArmComponent springArm_{};
};

} // namespace game
