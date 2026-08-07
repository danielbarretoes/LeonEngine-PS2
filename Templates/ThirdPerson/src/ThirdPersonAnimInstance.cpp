#include "ThirdPersonAnimInstance.h"

#include <leon/gameplay/SkeletalMeshComponent.h>

namespace game {

void ThirdPersonAnimInstance::NativeInitializeAnimation() {
    leon::SkeletalMeshComponent* mesh = GetOwningMeshComponent();
    if (mesh == nullptr) {
        return;
    }

    leon::BlendSpace1D& blend = mesh->GetBlendSpace();
    blend.name = "Locomotion_BS_1D";
    blend.axisMin = 0.0f;
    blend.axisMax = 1.0f;
    blend.ClearSamples();

    if (leon::AnimSequence* idle = mesh->FindSequence("BreathingIdle")) {
        blend.AddSample(idle, 0.0f);
    }
    if (leon::AnimSequence* run = mesh->FindSequence("Running")) {
        blend.AddSample(run, 1.0f);
    }
    SetBlendSpace(&blend);

    leon::AnimJumpClips clips{};
    if (leon::AnimSequence* jump = mesh->FindSequence("JumpingUp")) {
        jump->bLooping = false;
        clips.jumpStart = jump;
    }
    if (leon::AnimSequence* fall = mesh->FindSequence("FallingIdle")) {
        fall->bLooping = true;
        clips.fallLoop = fall;
    }
    if (leon::AnimSequence* land = mesh->FindSequence("FallingToLanding")) {
        land->bLooping = false;
        clips.land = land;
    }
    SetJumpClips(clips);

    SetJumpPlayRates(4.0f, 1.15f, 4.5f);
    SetCrossfadeDuration(0.12f);
    SetLandToLocomotionCrossfade(0.18f);
    SetLocomotionBlendInterpSpeed(12.0f);
}

} // namespace game
