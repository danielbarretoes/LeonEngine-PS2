#pragma once

#include <leon/animation/SkeletalAnimation.h>

namespace game {

/// Bot AnimBP for CoopTp (same locomotion setup as ThirdPerson template).
class CoopTpAnimInstance final : public leon::CharacterAnimInstance {
public:
    void NativeInitializeAnimation() override;
};

} // namespace game
