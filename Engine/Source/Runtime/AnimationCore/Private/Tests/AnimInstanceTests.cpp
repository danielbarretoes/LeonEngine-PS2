#include <glm/gtc/matrix_transform.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "SkeletalAnimation.h"

using Catch::Matchers::WithinAbs;

namespace {

leon::Skeleton makeTwoBoneSkeleton() {
    leon::Skeleton sk;
    sk.boneNames = {"root", "child"};
    sk.parentIndices = {-1, 0};
    sk.inverseBindPose = {glm::mat4(1.0f), glm::inverse(glm::translate(
                                               glm::mat4(1.0f), glm::vec3{0.0f, 1.0f, 0.0f}))};
    return sk;
}

leon::AnimSequence makeTranslatedClip(const char* name, const glm::vec3& childLocalTranslation) {
    leon::AnimSequence clip;
    clip.name = name;
    clip.durationSeconds = 1.0f;
    clip.framesPerSecond = 1.0f;
    clip.localPoseFrames.resize(1);
    clip.localPoseFrames[0].resize(2);
    clip.localPoseFrames[0][0] = glm::mat4(1.0f);
    clip.localPoseFrames[0][1] = glm::translate(glm::mat4(1.0f), childLocalTranslation);
    return clip;
}

} // namespace

TEST_CASE("AnimSequence SampleLocalPose loops duration", "[animation][sequence]") {
    leon::AnimSequence clip;
    clip.durationSeconds = 2.0f;
    clip.framesPerSecond = 1.0f;
    clip.localPoseFrames.resize(2);
    clip.localPoseFrames[0] = {glm::mat4(1.0f)};
    clip.localPoseFrames[1] = {glm::translate(glm::mat4(1.0f), glm::vec3{1.0f, 0.0f, 0.0f})};

    std::vector<glm::mat4> pose;
    clip.SampleLocalPose(0.0f, pose);
    REQUIRE(pose.size() == 1);
    REQUIRE_THAT(pose[0][3].x, WithinAbs(0.0f, 1.0e-4f));

    clip.SampleLocalPose(2.0f, pose); // wraps to start
    REQUIRE_THAT(pose[0][3].x, WithinAbs(0.0f, 1.0e-4f));
}

TEST_CASE("AnimSequence one-shot clamps and reports finished", "[animation][sequence]") {
    leon::AnimSequence clip;
    clip.durationSeconds = 1.0f;
    clip.framesPerSecond = 1.0f;
    clip.bLooping = false;
    clip.localPoseFrames.resize(2);
    clip.localPoseFrames[0] = {glm::mat4(1.0f)};
    clip.localPoseFrames[1] = {glm::translate(glm::mat4(1.0f), glm::vec3{2.0f, 0.0f, 0.0f})};

    REQUIRE_FALSE(clip.IsFinished(0.0f));
    REQUIRE(clip.IsFinished(1.0f));

    std::vector<glm::mat4> pose;
    clip.SampleLocalPose(5.0f, pose);
    REQUIRE_THAT(pose[0][3].x, WithinAbs(2.0f, 1.0e-4f));
}

TEST_CASE("AnimInstance BlendSpace produces skin matrices", "[animation][animinstance]") {
    const leon::Skeleton skeleton = makeTwoBoneSkeleton();
    const leon::AnimSequence idle = makeTranslatedClip("Idle", {0.0f, 1.0f, 0.0f});
    const leon::AnimSequence run = makeTranslatedClip("Run", {0.0f, 2.0f, 0.0f});

    leon::BlendSpace1D bs;
    bs.AddSample(&idle, 0.0f);
    bs.AddSample(&run, 1.0f);

    leon::AnimInstance anim;
    anim.SetSkeleton(&skeleton);
    anim.SetBlendSpace(&bs);
    anim.SetLocomotionBlendInterpSpeed(0.0f); // snap for unit tests

    SECTION("idle input → near bind child") {
        anim.SetBlendSpaceInput(0.0f);
        anim.NativeUpdateAnimation(0.016f);
        std::vector<glm::mat4> skin;
        anim.GetSkinMatrices(skin);
        REQUIRE(skin.size() == 2);
        // child global = translate(0,1,0); * inverseBind ≈ identity
        REQUIRE_THAT(skin[1][3].y, WithinAbs(0.0f, 1.0e-3f));
        REQUIRE_THAT(anim.GetBlendAlpha(), WithinAbs(0.0f, 1.0e-5f));
    }

    SECTION("mid input blends") {
        anim.SetBlendSpaceInput(0.5f);
        anim.NativeUpdateAnimation(0.016f);
        REQUIRE_THAT(anim.GetBlendAlpha(), WithinAbs(0.5f, 1.0e-5f));
        std::vector<glm::mat4> skin;
        anim.GetSkinMatrices(skin);
        REQUIRE(skin.size() == 2);
    }
}

TEST_CASE("AnimInstance eases locomotion blend input", "[animation][animinstance]") {
    const leon::Skeleton skeleton = makeTwoBoneSkeleton();
    const leon::AnimSequence idle = makeTranslatedClip("Idle", {0.0f, 1.0f, 0.0f});
    const leon::AnimSequence run = makeTranslatedClip("Run", {0.0f, 2.0f, 0.0f});
    leon::BlendSpace1D bs;
    bs.AddSample(&idle, 0.0f);
    bs.AddSample(&run, 1.0f);

    leon::AnimInstance anim;
    anim.SetSkeleton(&skeleton);
    anim.SetBlendSpace(&bs);
    anim.SetLocomotionBlendInterpSpeed(8.0f);
    anim.SetBlendSpaceInput(1.0f);
    anim.NativeUpdateAnimation(0.016f);
    REQUIRE(anim.GetBlendSpaceInput() > 0.0f);
    REQUIRE(anim.GetBlendSpaceInput() < 1.0f);
    REQUIRE_THAT(anim.GetBlendSpaceInputTarget(), WithinAbs(1.0f, 1.0e-5f));
}

TEST_CASE("CharacterAnimInstance jump state machine with crossfade",
          "[animation][animinstance][jump]") {
    const leon::Skeleton skeleton = makeTwoBoneSkeleton();
    leon::AnimSequence idle = makeTranslatedClip("Idle", {0.0f, 1.0f, 0.0f});
    leon::AnimSequence run = makeTranslatedClip("Run", {0.0f, 2.0f, 0.0f});
    leon::AnimSequence jump = makeTranslatedClip("Jump", {0.0f, 3.0f, 0.0f});
    leon::AnimSequence fall = makeTranslatedClip("Fall", {0.0f, 4.0f, 0.0f});
    leon::AnimSequence land = makeTranslatedClip("Land", {0.0f, 1.5f, 0.0f});
    jump.bLooping = false;
    jump.durationSeconds = 0.2f;
    fall.bLooping = true;
    land.bLooping = false;
    land.durationSeconds = 0.2f;

    leon::BlendSpace1D bs;
    bs.AddSample(&idle, 0.0f);
    bs.AddSample(&run, 1.0f);

    leon::CharacterAnimInstance anim;
    anim.SetSkeleton(&skeleton);
    anim.SetBlendSpace(&bs);
    anim.SetJumpClips({&jump, &fall, &land});
    anim.SetCrossfadeDuration(0.1f);
    anim.SetJumpPlayRates(1.0f, 1.0f, 1.0f);
    anim.SetLocomotionBlendInterpSpeed(0.0f);
    anim.SetBlendSpaceInput(0.0f);

    REQUIRE(anim.GetJumpState() == leon::EAnimJumpState::Locomotion);

    anim.NotifyJumped();
    anim.SetMovementState(true, 5.0f, false);
    anim.NativeUpdateAnimation(0.016f);
    REQUIRE(anim.GetJumpState() == leon::EAnimJumpState::JumpStart);
    REQUIRE(anim.GetCrossfadeAlpha() < 1.0f);

    anim.SetMovementState(true, -1.0f, false);
    anim.NativeUpdateAnimation(0.016f);
    REQUIRE(anim.GetJumpState() == leon::EAnimJumpState::FallLoop);

    anim.SetMovementState(false, 0.0f, true);
    anim.NativeUpdateAnimation(0.016f);
    REQUIRE(anim.GetJumpState() == leon::EAnimJumpState::Land);

    for (int i = 0; i < 20; ++i) {
        anim.SetMovementState(false, 0.0f, false);
        anim.NativeUpdateAnimation(0.05f);
    }
    REQUIRE(anim.GetJumpState() == leon::EAnimJumpState::Locomotion);

    std::vector<glm::mat4> skin;
    anim.GetSkinMatrices(skin);
    REQUIRE(skin.size() == 2);
}

TEST_CASE("CharacterAnimInstance jump play rate finishes one-shot sooner",
          "[animation][animinstance][jump]") {
    const leon::Skeleton skeleton = makeTwoBoneSkeleton();
    leon::AnimSequence idle = makeTranslatedClip("Idle", {0.0f, 1.0f, 0.0f});
    leon::AnimSequence jump = makeTranslatedClip("Jump", {0.0f, 3.0f, 0.0f});
    leon::AnimSequence fall = makeTranslatedClip("Fall", {0.0f, 4.0f, 0.0f});
    jump.bLooping = false;
    jump.durationSeconds = 1.0f;
    fall.bLooping = true;

    leon::BlendSpace1D bs;
    bs.AddSample(&idle, 0.0f);

    leon::CharacterAnimInstance anim;
    anim.SetSkeleton(&skeleton);
    anim.SetBlendSpace(&bs);
    anim.SetJumpClips({&jump, &fall, nullptr});
    anim.SetCrossfadeDuration(0.0f);
    anim.SetJumpPlayRates(4.0f, 1.0f, 1.0f);
    anim.SetLocomotionBlendInterpSpeed(0.0f);

    anim.NotifyJumped();
    anim.SetMovementState(true, 5.0f, false);
    anim.NativeUpdateAnimation(0.0f);
    REQUIRE(anim.GetJumpState() == leon::EAnimJumpState::JumpStart);

    anim.SetMovementState(true, 5.0f, false);
    anim.NativeUpdateAnimation(0.3f);
    REQUIRE(anim.GetJumpState() == leon::EAnimJumpState::FallLoop);
}
