#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <filesystem>
#include <leon/animation/SkeletalAnimation.h>
#include <string>

namespace {

[[nodiscard]] std::string sourceAsset(const char* relative) {
#ifdef LEON_SOURCE_DIR
    return (std::filesystem::path(LEON_SOURCE_DIR) / relative).lexically_normal().string();
#else
    return relative;
#endif
}

} // namespace

TEST_CASE("LoadSkeletalMeshFromFbx loads Mixamo bot idle", "[animation][fbx]") {
    const std::string path = sourceAsset("Templates/ThirdPerson/Content/assets/characters/bot/BreathingIdle.fbx");
    REQUIRE(std::filesystem::exists(path));

    leon::SkeletalMeshData data;
    REQUIRE(leon::LoadSkeletalMeshFromFbx(path, data));
    REQUIRE_FALSE(data.empty());
    REQUIRE(data.skeleton.BoneCount() > 0);
    REQUIRE(data.skeleton.BoneCount() <= leon::kMaxSkinBones);
    REQUIRE(data.skeleton.parentIndices.size() ==
            static_cast<std::size_t>(data.skeleton.BoneCount()));
    REQUIRE(data.skeleton.inverseBindPose.size() ==
            static_cast<std::size_t>(data.skeleton.BoneCount()));
    REQUIRE(data.embeddedAnim.FrameCount() > 0);
    REQUIRE(data.embeddedAnim.durationSeconds >
            2.0f); // real Mixamo idle ≈ 10s, not static Take 001

    // Skin weights should be present on at least some vertices.
    bool anyWeighted = false;
    for (const leon::SkeletalVertex& v : data.vertices) {
        if (v.boneWeights.x > 0.0f) {
            anyWeighted = true;
            break;
        }
    }
    REQUIRE(anyWeighted);
}

TEST_CASE("LoadAnimSequenceFromFbx loads Mixamo run against bot skeleton", "[animation][fbx]") {
    leon::SkeletalMeshData meshData;
    REQUIRE(leon::LoadSkeletalMeshFromFbx(sourceAsset("Templates/ThirdPerson/Content/assets/characters/bot/BreathingIdle.fbx"),
                                          meshData));

    leon::AnimSequence run;
    REQUIRE(leon::LoadAnimSequenceFromFbx(sourceAsset("Templates/ThirdPerson/Content/assets/characters/bot/Running.fbx"),
                                          meshData.skeleton, run));
    REQUIRE(run.FrameCount() > 0);
    REQUIRE(run.durationSeconds > 0.0f);
    REQUIRE(run.localPoseFrames[0].size() ==
            static_cast<std::size_t>(meshData.skeleton.BoneCount()));
}

TEST_CASE("Bot idle+run BlendSpace wires like Character::GetMesh", "[animation][fbx][blendspace]") {
    leon::SkeletalMeshData meshData;
    REQUIRE(leon::LoadSkeletalMeshFromFbx(sourceAsset("Templates/ThirdPerson/Content/assets/characters/bot/BreathingIdle.fbx"),
                                          meshData));

    leon::AnimSequence idle = std::move(meshData.embeddedAnim);
    leon::AnimSequence run;
    REQUIRE(leon::LoadAnimSequenceFromFbx(sourceAsset("Templates/ThirdPerson/Content/assets/characters/bot/Running.fbx"),
                                          meshData.skeleton, run));

    leon::BlendSpace1D bs;
    bs.AddSample(&idle, 0.0f);
    bs.AddSample(&run, 1.0f);

    leon::AnimInstance anim;
    anim.SetSkeleton(&meshData.skeleton);
    anim.SetBlendSpace(&bs);
    anim.SetBlendSpaceInput(0.0f);
    anim.NativeUpdateAnimation(0.5f); // advance into the breathing clip

    std::vector<glm::mat4> skin;
    anim.GetSkinMatrices(skin);
    REQUIRE(static_cast<int>(skin.size()) == meshData.skeleton.BoneCount());

    // Idle must deform away from pure bind (identity skin = T-pose).
    bool anyDeform = false;
    for (const glm::mat4& m : skin) {
        const float dx = std::abs(m[3].x);
        const float dy = std::abs(m[3].y);
        const float dz = std::abs(m[3].z);
        const float offDiag = std::abs(m[0][1]) + std::abs(m[1][0]) + std::abs(m[2][0]);
        if (dx + dy + dz + offDiag > 1.0e-2f) {
            anyDeform = true;
            break;
        }
    }
    REQUIRE(anyDeform);
}
