#include "SkeletalAnimation.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <glm/gtc/matrix_transform.hpp>

using Catch::Matchers::WithinAbs;

namespace
{

	USkeleton MakeTwoBoneSkeleton()
	{
		USkeleton Sk;
		Sk.BoneNames = {"root", "child"};
		Sk.ParentIndices = {-1, 0};
		Sk.InverseBindPose = {
			glm::mat4(1.0f), glm::inverse(glm::translate(glm::mat4(1.0f), glm::vec3{0.0f, 1.0f, 0.0f}))};
		return Sk;
	}

	UAnimSequence MakeTranslatedClip(const char* Name, const glm::vec3& ChildLocalTranslation)
	{
		UAnimSequence Clip;
		Clip.Name = Name;
		Clip.DurationSeconds = 1.0f;
		Clip.FramesPerSecond = 1.0f;
		Clip.LocalPoseFrames.resize(1);
		Clip.LocalPoseFrames[0].resize(2);
		Clip.LocalPoseFrames[0][0] = glm::mat4(1.0f);
		Clip.LocalPoseFrames[0][1] = glm::translate(glm::mat4(1.0f), ChildLocalTranslation);
		return Clip;
	}

} // namespace

TEST_CASE("AnimSequence SampleLocalPose loops duration", "[animation][sequence]")
{
	UAnimSequence Clip;
	Clip.DurationSeconds = 2.0f;
	Clip.FramesPerSecond = 1.0f;
	Clip.LocalPoseFrames.resize(2);
	Clip.LocalPoseFrames[0] = {glm::mat4(1.0f)};
	Clip.LocalPoseFrames[1] = {glm::translate(glm::mat4(1.0f), glm::vec3{1.0f, 0.0f, 0.0f})};

	std::vector<glm::mat4> Pose;
	Clip.SampleLocalPose(0.0f, Pose);
	REQUIRE(Pose.size() == 1);
	REQUIRE_THAT(Pose[0][3].x, WithinAbs(0.0f, 1.0e-4f));

	Clip.SampleLocalPose(2.0f, Pose); // wraps to start
	REQUIRE_THAT(Pose[0][3].x, WithinAbs(0.0f, 1.0e-4f));
}

TEST_CASE("AnimSequence one-shot clamps and reports finished", "[animation][sequence]")
{
	UAnimSequence Clip;
	Clip.DurationSeconds = 1.0f;
	Clip.FramesPerSecond = 1.0f;
	Clip.bLooping = false;
	Clip.LocalPoseFrames.resize(2);
	Clip.LocalPoseFrames[0] = {glm::mat4(1.0f)};
	Clip.LocalPoseFrames[1] = {glm::translate(glm::mat4(1.0f), glm::vec3{2.0f, 0.0f, 0.0f})};

	REQUIRE_FALSE(Clip.IsFinished(0.0f));
	REQUIRE(Clip.IsFinished(1.0f));

	std::vector<glm::mat4> Pose;
	Clip.SampleLocalPose(5.0f, Pose);
	REQUIRE_THAT(Pose[0][3].x, WithinAbs(2.0f, 1.0e-4f));
}

TEST_CASE("AnimInstance BlendSpace produces skin matrices", "[animation][animinstance]")
{
	const USkeleton Skeleton = MakeTwoBoneSkeleton();
	const UAnimSequence Idle = MakeTranslatedClip("Idle", {0.0f, 1.0f, 0.0f});
	const UAnimSequence Run = MakeTranslatedClip("Run", {0.0f, 2.0f, 0.0f});

	UBlendSpace1D Bs;
	Bs.AddSample(&Idle, 0.0f);
	Bs.AddSample(&Run, 1.0f);

	UAnimInstance Anim;
	Anim.SetSkeleton(&Skeleton);
	Anim.SetBlendSpace(&Bs);
	Anim.SetLocomotionBlendInterpSpeed(0.0f); // snap for unit tests

	SECTION("idle input → near bind child")
	{
		Anim.SetBlendSpaceInput(0.0f);
		Anim.NativeUpdateAnimation(0.016f);
		std::vector<glm::mat4> Skin;
		Anim.GetSkinMatrices(Skin);
		REQUIRE(Skin.size() == 2);
		// child global = translate(0,1,0); * inverseBind ≈ identity
		REQUIRE_THAT(Skin[1][3].y, WithinAbs(0.0f, 1.0e-3f));
		REQUIRE_THAT(Anim.GetBlendAlpha(), WithinAbs(0.0f, 1.0e-5f));
	}

	SECTION("mid input blends")
	{
		Anim.SetBlendSpaceInput(0.5f);
		Anim.NativeUpdateAnimation(0.016f);
		REQUIRE_THAT(Anim.GetBlendAlpha(), WithinAbs(0.5f, 1.0e-5f));
		std::vector<glm::mat4> Skin;
		Anim.GetSkinMatrices(Skin);
		REQUIRE(Skin.size() == 2);
	}
}

TEST_CASE("AnimInstance eases locomotion blend input", "[animation][animinstance]")
{
	const USkeleton Skeleton = MakeTwoBoneSkeleton();
	const UAnimSequence Idle = MakeTranslatedClip("Idle", {0.0f, 1.0f, 0.0f});
	const UAnimSequence Run = MakeTranslatedClip("Run", {0.0f, 2.0f, 0.0f});
	UBlendSpace1D Bs;
	Bs.AddSample(&Idle, 0.0f);
	Bs.AddSample(&Run, 1.0f);

	UAnimInstance Anim;
	Anim.SetSkeleton(&Skeleton);
	Anim.SetBlendSpace(&Bs);
	Anim.SetLocomotionBlendInterpSpeed(8.0f);
	Anim.SetBlendSpaceInput(1.0f);
	Anim.NativeUpdateAnimation(0.016f);
	REQUIRE(Anim.GetBlendSpaceInput() > 0.0f);
	REQUIRE(Anim.GetBlendSpaceInput() < 1.0f);
	REQUIRE_THAT(Anim.GetBlendSpaceInputTarget(), WithinAbs(1.0f, 1.0e-5f));
}

TEST_CASE("CharacterAnimInstance jump state machine with crossfade", "[animation][animinstance][jump]")
{
	const USkeleton Skeleton = MakeTwoBoneSkeleton();
	UAnimSequence Idle = MakeTranslatedClip("Idle", {0.0f, 1.0f, 0.0f});
	UAnimSequence Run = MakeTranslatedClip("Run", {0.0f, 2.0f, 0.0f});
	UAnimSequence Jump = MakeTranslatedClip("Jump", {0.0f, 3.0f, 0.0f});
	UAnimSequence Fall = MakeTranslatedClip("Fall", {0.0f, 4.0f, 0.0f});
	UAnimSequence Land = MakeTranslatedClip("Land", {0.0f, 1.5f, 0.0f});
	Jump.bLooping = false;
	Jump.DurationSeconds = 0.2f;
	Fall.bLooping = true;
	Land.bLooping = false;
	Land.DurationSeconds = 0.2f;

	UBlendSpace1D Bs;
	Bs.AddSample(&Idle, 0.0f);
	Bs.AddSample(&Run, 1.0f);

	UCharacterAnimInstance Anim;
	Anim.SetSkeleton(&Skeleton);
	Anim.SetBlendSpace(&Bs);
	Anim.SetJumpClips({&Jump, &Fall, &Land});
	Anim.SetCrossfadeDuration(0.1f);
	Anim.SetJumpPlayRates(1.0f, 1.0f, 1.0f);
	Anim.SetLocomotionBlendInterpSpeed(0.0f);
	Anim.SetBlendSpaceInput(0.0f);

	REQUIRE(Anim.GetJumpState() == EAnimJumpState::Locomotion);

	Anim.NotifyJumped();
	Anim.SetMovementState(true, 5.0f, false);
	Anim.NativeUpdateAnimation(0.016f);
	REQUIRE(Anim.GetJumpState() == EAnimJumpState::JumpStart);
	REQUIRE(Anim.GetCrossfadeAlpha() < 1.0f);

	Anim.SetMovementState(true, -1.0f, false);
	Anim.NativeUpdateAnimation(0.016f);
	REQUIRE(Anim.GetJumpState() == EAnimJumpState::FallLoop);

	Anim.SetMovementState(false, 0.0f, true);
	Anim.NativeUpdateAnimation(0.016f);
	REQUIRE(Anim.GetJumpState() == EAnimJumpState::Land);

	for (int I = 0; I < 20; ++I)
	{
		Anim.SetMovementState(false, 0.0f, false);
		Anim.NativeUpdateAnimation(0.05f);
	}
	REQUIRE(Anim.GetJumpState() == EAnimJumpState::Locomotion);

	std::vector<glm::mat4> Skin;
	Anim.GetSkinMatrices(Skin);
	REQUIRE(Skin.size() == 2);
}

TEST_CASE("CharacterAnimInstance jump play rate finishes one-shot sooner", "[animation][animinstance][jump]")
{
	const USkeleton Skeleton = MakeTwoBoneSkeleton();
	UAnimSequence Idle = MakeTranslatedClip("Idle", {0.0f, 1.0f, 0.0f});
	UAnimSequence Jump = MakeTranslatedClip("Jump", {0.0f, 3.0f, 0.0f});
	UAnimSequence Fall = MakeTranslatedClip("Fall", {0.0f, 4.0f, 0.0f});
	Jump.bLooping = false;
	Jump.DurationSeconds = 1.0f;
	Fall.bLooping = true;

	UBlendSpace1D Bs;
	Bs.AddSample(&Idle, 0.0f);

	UCharacterAnimInstance Anim;
	Anim.SetSkeleton(&Skeleton);
	Anim.SetBlendSpace(&Bs);
	Anim.SetJumpClips({&Jump, &Fall, nullptr});
	Anim.SetCrossfadeDuration(0.0f);
	Anim.SetJumpPlayRates(4.0f, 1.0f, 1.0f);
	Anim.SetLocomotionBlendInterpSpeed(0.0f);

	Anim.NotifyJumped();
	Anim.SetMovementState(true, 5.0f, false);
	Anim.NativeUpdateAnimation(0.0f);
	REQUIRE(Anim.GetJumpState() == EAnimJumpState::JumpStart);

	Anim.SetMovementState(true, 5.0f, false);
	Anim.NativeUpdateAnimation(0.3f);
	REQUIRE(Anim.GetJumpState() == EAnimJumpState::FallLoop);
}
