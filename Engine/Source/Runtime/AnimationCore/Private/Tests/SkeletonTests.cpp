#include "SkeletalAnimation.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <glm/gtc/matrix_transform.hpp>

using Catch::Matchers::WithinAbs;

TEST_CASE("Skeleton FindBoneIndex and BoneCount", "[animation][skeleton]")
{
	USkeleton Sk;
	Sk.BoneNames = {"root", "hips", "spine"};
	Sk.ParentIndices = {-1, 0, 1};
	Sk.InverseBindPose.assign(3, glm::mat4(1.0f));

	REQUIRE(Sk.BoneCount() == 3);
	REQUIRE(Sk.FindBoneIndex("hips") == 1);
	REQUIRE(Sk.FindBoneIndex("missing") == -1);
}

TEST_CASE("AnimSequence SampleLocalPose lerps mid-frame", "[animation][sequence]")
{
	UAnimSequence Clip;
	Clip.DurationSeconds = 1.0f;
	Clip.FramesPerSecond = 1.0f;
	Clip.LocalPoseFrames.resize(2);
	Clip.LocalPoseFrames[0] = {glm::mat4(1.0f)};
	Clip.LocalPoseFrames[1] = {glm::translate(glm::mat4(1.0f), glm::vec3{2.0f, 0.0f, 0.0f})};

	std::vector<glm::mat4> Pose;
	Clip.SampleLocalPose(0.5f, Pose);
	REQUIRE(Pose.size() == 1);
	REQUIRE_THAT(Pose[0][3].x, WithinAbs(1.0f, 1.0e-3f));
}

TEST_CASE("AnimInstance without skeleton yields empty skin", "[animation][animinstance]")
{
	UAnimInstance Anim;
	Anim.NativeUpdateAnimation(0.016f);
	std::vector<glm::mat4> Skin;
	Anim.GetSkinMatrices(Skin);
	REQUIRE(Skin.empty());
}
