#include "SkeletalAnimation.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using Catch::Matchers::WithinAbs;

namespace
{

	UAnimSequence MakeNamedClip(const char* Name)
	{
		UAnimSequence Clip;
		Clip.Name = Name;
		Clip.DurationSeconds = 1.0f;
		Clip.FramesPerSecond = 1.0f;
		Clip.LocalPoseFrames.resize(1);
		Clip.LocalPoseFrames[0].assign(2, glm::mat4(1.0f));
		return Clip;
	}

} // namespace

TEST_CASE("BlendSpace1D evaluates idle/run axis", "[animation][blendspace]")
{
	const UAnimSequence Idle = MakeNamedClip("Idle");
	const UAnimSequence Run = MakeNamedClip("Run");

	UBlendSpace1D Bs;
	Bs.Name = "Locomotion";
	Bs.AxisMin = 0.0f;
	Bs.AxisMax = 1.0f;
	Bs.AddSample(&Idle, 0.0f);
	Bs.AddSample(&Run, 1.0f);

	const UAnimSequence* A = nullptr;
	const UAnimSequence* B = nullptr;
	float Alpha = -1.0f;

	SECTION("at idle")
	{
		Bs.Evaluate(0.0f, A, B, Alpha);
		REQUIRE(A == &Idle);
		REQUIRE(B == &Idle);
		REQUIRE_THAT(Alpha, WithinAbs(0.0f, 1.0e-5f));
	}

	SECTION("mid blend")
	{
		Bs.Evaluate(0.5f, A, B, Alpha);
		REQUIRE(A == &Idle);
		REQUIRE(B == &Run);
		REQUIRE_THAT(Alpha, WithinAbs(0.5f, 1.0e-5f));
	}

	SECTION("at run")
	{
		Bs.Evaluate(1.0f, A, B, Alpha);
		REQUIRE(A == &Run);
		REQUIRE(B == &Run);
		REQUIRE_THAT(Alpha, WithinAbs(0.0f, 1.0e-5f));
	}

	SECTION("clamps below axis")
	{
		Bs.Evaluate(-2.0f, A, B, Alpha);
		REQUIRE(A == &Idle);
		REQUIRE(B == &Idle);
	}

	SECTION("clamps above axis")
	{
		Bs.Evaluate(3.0f, A, B, Alpha);
		REQUIRE(A == &Run);
		REQUIRE(B == &Run);
	}
}

TEST_CASE("BlendSpace1D empty samples are safe", "[animation][blendspace]")
{
	UBlendSpace1D Bs;
	const UAnimSequence* A = reinterpret_cast<const UAnimSequence*>(1);
	const UAnimSequence* B = reinterpret_cast<const UAnimSequence*>(1);
	float Alpha = 1.0f;
	Bs.Evaluate(0.5f, A, B, Alpha);
	REQUIRE(A == nullptr);
	REQUIRE(B == nullptr);
	REQUIRE_THAT(Alpha, WithinAbs(0.0f, 1.0e-5f));
}
