#include <algorithm>
#include <cmath>
#include "SkeletalAnimation.h"

namespace leon {

void CharacterAnimInstance::SetJumpPlayRates(float jumpStart, float fallLoop, float land) {
    jumpStartPlayRate_ = std::max(jumpStart, 0.01f);
    fallLoopPlayRate_ = std::max(fallLoop, 0.01f);
    landPlayRate_ = std::max(land, 0.01f);
}

void CharacterAnimInstance::NotifyJumped() {
    jumpRequested_ = true;
}

void CharacterAnimInstance::SetMovementState(bool falling, float velocityY, bool justLanded) {
    falling_ = falling;
    velocityY_ = velocityY;
    justLanded_ = justLanded;
}

float CharacterAnimInstance::playRateForState(EAnimJumpState state) const {
    switch (state) {
    case EAnimJumpState::JumpStart:
        return jumpStartPlayRate_;
    case EAnimJumpState::FallLoop:
        return fallLoopPlayRate_;
    case EAnimJumpState::Land:
        return landPlayRate_;
    case EAnimJumpState::Locomotion:
    default:
        return 1.0f;
    }
}

void CharacterAnimInstance::advancePlayer(PosePlayer& player, float deltaTime,
                                          float playRate) const {
    if (player.sequence == nullptr) {
        return;
    }
    player.time += deltaTime * playRate;
    if (!player.sequence->bLooping && player.sequence->durationSeconds > 1.0e-4f) {
        player.time = std::min(player.time, player.sequence->durationSeconds);
    }
}

void CharacterAnimInstance::enterState(EAnimJumpState next) {
    if (next == jumpState_) {
        return;
    }

    const EAnimJumpState from = jumpState_;
    previousState_ = from;
    previous_ = active_;

    jumpState_ = next;
    active_ = {};
    switch (next) {
    case EAnimJumpState::JumpStart:
        active_.sequence = jumpClips_.jumpStart;
        break;
    case EAnimJumpState::FallLoop:
        active_.sequence = jumpClips_.fallLoop;
        break;
    case EAnimJumpState::Land:
        active_.sequence = jumpClips_.land;
        break;
    case EAnimJumpState::Locomotion:
    default:
        active_.sequence = nullptr;
        break;
    }
    active_.time = 0.0f;

    float fade = crossfadeDuration_;
    if (from == EAnimJumpState::Land && next == EAnimJumpState::Locomotion) {
        fade = std::max(crossfadeDuration_, landToLocomotionCrossfade_);
    }
    crossfadeElapsed_ = 0.0f;
    crossfadeAlpha_ = (fade <= 1.0e-6f) ? 1.0f : 0.0f;
    activeCrossfadeDuration_ = fade;
}

void CharacterAnimInstance::updateJumpStateMachine() {
    const bool hasJumpStart =
        jumpClips_.jumpStart != nullptr && jumpClips_.jumpStart->FrameCount() > 0;
    const bool hasFallLoop =
        jumpClips_.fallLoop != nullptr && jumpClips_.fallLoop->FrameCount() > 0;
    const bool hasLand = jumpClips_.land != nullptr && jumpClips_.land->FrameCount() > 0;

    switch (jumpState_) {
    case EAnimJumpState::Locomotion:
        if (jumpRequested_) {
            jumpRequested_ = false;
            if (hasJumpStart) {
                enterState(EAnimJumpState::JumpStart);
            } else if (hasFallLoop) {
                enterState(EAnimJumpState::FallLoop);
            }
        } else if (falling_) {
            if (velocityY_ > 0.0f && hasJumpStart) {
                enterState(EAnimJumpState::JumpStart);
            } else if (hasFallLoop) {
                enterState(EAnimJumpState::FallLoop);
            }
        }
        break;

    case EAnimJumpState::JumpStart:
        jumpRequested_ = false;
        if (justLanded_ || !falling_) {
            if (hasLand) {
                enterState(EAnimJumpState::Land);
            } else {
                enterState(EAnimJumpState::Locomotion);
            }
        } else if (velocityY_ <= 0.0f ||
                   (active_.sequence != nullptr && active_.sequence->IsFinished(active_.time))) {
            if (hasFallLoop) {
                enterState(EAnimJumpState::FallLoop);
            }
        }
        break;

    case EAnimJumpState::FallLoop:
        jumpRequested_ = false;
        if (justLanded_ || !falling_) {
            if (hasLand) {
                enterState(EAnimJumpState::Land);
            } else {
                enterState(EAnimJumpState::Locomotion);
            }
        }
        break;

    case EAnimJumpState::Land:
        if (jumpRequested_ && falling_) {
            jumpRequested_ = false;
            if (hasJumpStart) {
                enterState(EAnimJumpState::JumpStart);
            } else if (hasFallLoop) {
                enterState(EAnimJumpState::FallLoop);
            }
        } else if (falling_ && !justLanded_) {
            jumpRequested_ = false;
            if (hasFallLoop) {
                enterState(EAnimJumpState::FallLoop);
            }
        } else if (active_.sequence == nullptr || active_.sequence->IsFinished(active_.time) ||
                   !hasLand) {
            jumpRequested_ = false;
            enterState(EAnimJumpState::Locomotion);
        } else {
            jumpRequested_ = false;
        }
        break;
    }

    justLanded_ = false;
}

void CharacterAnimInstance::samplePlayerBoneWorld(const PosePlayer& player,
                                                  std::vector<glm::mat4>& outBoneWorld) const {
    outBoneWorld.clear();
    const Skeleton* skeleton = GetSkeleton();
    if (skeleton == nullptr) {
        return;
    }
    if (player.sequence == nullptr || player.sequence->FrameCount() <= 0) {
        outBoneWorld.assign(static_cast<std::size_t>(skeleton->BoneCount()), glm::mat4(1.0f));
        return;
    }
    player.sequence->SampleLocalPose(player.time, outBoneWorld);
}

void CharacterAnimInstance::NativeUpdateAnimation(float deltaTime) {
    UpdateLocomotion(deltaTime);

    if (jumpState_ != EAnimJumpState::Locomotion) {
        advancePlayer(active_, deltaTime, playRateForState(jumpState_));
    }
    if (crossfadeAlpha_ < 1.0f && previousState_ != EAnimJumpState::Locomotion) {
        advancePlayer(previous_, deltaTime, playRateForState(previousState_));
    }

    updateJumpStateMachine();

    if (crossfadeAlpha_ < 1.0f) {
        const float fadeDur =
            activeCrossfadeDuration_ > 1.0e-6f ? activeCrossfadeDuration_ : crossfadeDuration_;
        if (fadeDur <= 1.0e-6f) {
            crossfadeAlpha_ = 1.0f;
            crossfadeElapsed_ = fadeDur;
        } else {
            crossfadeElapsed_ += deltaTime;
            const float t = std::clamp(crossfadeElapsed_ / fadeDur, 0.0f, 1.0f);
            crossfadeAlpha_ = t * t * (3.0f - (2.0f * t));
        }
    }
}

void CharacterAnimInstance::GetBoneWorldMatrices(std::vector<glm::mat4>& outBoneWorld) const {
    const Skeleton* skeleton = GetSkeleton();
    if (skeleton == nullptr || skeleton->BoneCount() <= 0) {
        outBoneWorld.clear();
        return;
    }

    const int boneCount = skeleton->BoneCount();
    std::vector<glm::mat4> worldCurrent;
    if (jumpState_ == EAnimJumpState::Locomotion) {
        SampleLocomotionBoneWorld(worldCurrent);
    } else {
        samplePlayerBoneWorld(active_, worldCurrent);
    }

    outBoneWorld = worldCurrent;
    if (crossfadeAlpha_ < 0.999f) {
        std::vector<glm::mat4> worldPrev;
        if (previousState_ == EAnimJumpState::Locomotion) {
            SampleLocomotionBoneWorld(worldPrev);
        } else {
            samplePlayerBoneWorld(previous_, worldPrev);
        }
        if (worldPrev.size() == static_cast<std::size_t>(boneCount) &&
            worldCurrent.size() == static_cast<std::size_t>(boneCount)) {
            outBoneWorld.resize(static_cast<std::size_t>(boneCount));
            for (int i = 0; i < boneCount; ++i) {
                outBoneWorld[static_cast<std::size_t>(i)] =
                    worldPrev[static_cast<std::size_t>(i)] * (1.0f - crossfadeAlpha_) +
                    worldCurrent[static_cast<std::size_t>(i)] * crossfadeAlpha_;
            }
        }
    }
}

void CharacterAnimInstance::GetSkinMatrices(std::vector<glm::mat4>& outSkin) const {
    std::vector<glm::mat4> worldBlended;
    GetBoneWorldMatrices(worldBlended);
    SkinFromBoneWorld(worldBlended, outSkin);
}

} // namespace leon
