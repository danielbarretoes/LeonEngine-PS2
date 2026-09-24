#include <glm/gtc/matrix_transform.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "SkeletalAnimation.h"

using Catch::Matchers::WithinAbs;

TEST_CASE("Skeleton FindBoneIndex and BoneCount", "[animation][skeleton]") {
    USkeleton sk;
    sk.boneNames = {"root", "hips", "spine"};
    sk.parentIndices = {-1, 0, 1};
    sk.inverseBindPose.assign(3, glm::mat4(1.0f));

    REQUIRE(sk.BoneCount() == 3);
    REQUIRE(sk.FindBoneIndex("hips") == 1);
    REQUIRE(sk.FindBoneIndex("missing") == -1);
}

TEST_CASE("AnimSequence SampleLocalPose lerps mid-frame", "[animation][sequence]") {
    UAnimSequence clip;
    clip.durationSeconds = 1.0f;
    clip.framesPerSecond = 1.0f;
    clip.localPoseFrames.resize(2);
    clip.localPoseFrames[0] = {glm::mat4(1.0f)};
    clip.localPoseFrames[1] = {glm::translate(glm::mat4(1.0f), glm::vec3{2.0f, 0.0f, 0.0f})};

    std::vector<glm::mat4> pose;
    clip.SampleLocalPose(0.5f, pose);
    REQUIRE(pose.size() == 1);
    REQUIRE_THAT(pose[0][3].x, WithinAbs(1.0f, 1.0e-3f));
}

TEST_CASE("AnimInstance without skeleton yields empty skin", "[animation][animinstance]") {
    UAnimInstance anim;
    anim.NativeUpdateAnimation(0.016f);
    std::vector<glm::mat4> skin;
    anim.GetSkinMatrices(skin);
    REQUIRE(skin.empty());
}
