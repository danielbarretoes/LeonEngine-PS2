#pragma once

#include <leon/animation/SkeletalAnimation.h>

namespace game {

/// Project AnimBP for the Bot pack (`assets/characters/bot/Bot.lchar`).
/// Wires BlendSpace1D (BreathingIdle↔Running) + jump/fall/land sequences.
class ThirdPersonAnimInstance final : public leon::CharacterAnimInstance {
public:
    void NativeInitializeAnimation() override;
};

} // namespace game
