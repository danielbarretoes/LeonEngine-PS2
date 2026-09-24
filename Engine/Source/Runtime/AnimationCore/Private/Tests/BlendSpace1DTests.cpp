#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "SkeletalAnimation.h"

using Catch::Matchers::WithinAbs;

namespace {

leon::AnimSequence makeNamedClip(const char* name) {
    leon::AnimSequence clip;
    clip.name = name;
    clip.durationSeconds = 1.0f;
    clip.framesPerSecond = 1.0f;
    clip.localPoseFrames.resize(1);
    clip.localPoseFrames[0].assign(2, glm::mat4(1.0f));
    return clip;
}

} // namespace

TEST_CASE("BlendSpace1D evaluates idle/run axis", "[animation][blendspace]") {
    const leon::AnimSequence idle = makeNamedClip("Idle");
    const leon::AnimSequence run = makeNamedClip("Run");

    leon::BlendSpace1D bs;
    bs.name = "Locomotion";
    bs.axisMin = 0.0f;
    bs.axisMax = 1.0f;
    bs.AddSample(&idle, 0.0f);
    bs.AddSample(&run, 1.0f);

    const leon::AnimSequence* a = nullptr;
    const leon::AnimSequence* b = nullptr;
    float alpha = -1.0f;

    SECTION("at idle") {
        bs.Evaluate(0.0f, a, b, alpha);
        REQUIRE(a == &idle);
        REQUIRE(b == &idle);
        REQUIRE_THAT(alpha, WithinAbs(0.0f, 1.0e-5f));
    }

    SECTION("mid blend") {
        bs.Evaluate(0.5f, a, b, alpha);
        REQUIRE(a == &idle);
        REQUIRE(b == &run);
        REQUIRE_THAT(alpha, WithinAbs(0.5f, 1.0e-5f));
    }

    SECTION("at run") {
        bs.Evaluate(1.0f, a, b, alpha);
        REQUIRE(a == &run);
        REQUIRE(b == &run);
        REQUIRE_THAT(alpha, WithinAbs(0.0f, 1.0e-5f));
    }

    SECTION("clamps below axis") {
        bs.Evaluate(-2.0f, a, b, alpha);
        REQUIRE(a == &idle);
        REQUIRE(b == &idle);
    }

    SECTION("clamps above axis") {
        bs.Evaluate(3.0f, a, b, alpha);
        REQUIRE(a == &run);
        REQUIRE(b == &run);
    }
}

TEST_CASE("BlendSpace1D empty samples are safe", "[animation][blendspace]") {
    leon::BlendSpace1D bs;
    const leon::AnimSequence* a = reinterpret_cast<const leon::AnimSequence*>(1);
    const leon::AnimSequence* b = reinterpret_cast<const leon::AnimSequence*>(1);
    float alpha = 1.0f;
    bs.Evaluate(0.5f, a, b, alpha);
    REQUIRE(a == nullptr);
    REQUIRE(b == nullptr);
    REQUIRE_THAT(alpha, WithinAbs(0.0f, 1.0e-5f));
}
