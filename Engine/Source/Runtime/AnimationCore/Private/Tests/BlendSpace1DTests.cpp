#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "SkeletalAnimation.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{

	UAnimSequence MakeNamedClip(const TCHAR* Name)
	{
		UAnimSequence Clip;
		Clip.Name = FName(Name);
		Clip.DurationSeconds = 1.0f;
		Clip.FramesPerSecond = 1.0f;
		Clip.LocalPoseFrames.SetNum(1);
		Clip.LocalPoseFrames[0].Init(FMatrix::Identity, 2);
		return Clip;
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlendSpace1DEvaluateTest, "System.AnimationCore.BlendSpace1D.Evaluate",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FBlendSpace1DEvaluateTest::RunTest(const FString& Parameters)
{
	const UAnimSequence Idle = MakeNamedClip("Idle");
	const UAnimSequence Run = MakeNamedClip("Run");

	UBlendSpace1D Bs;
	Bs.Name = FName("Locomotion");
	Bs.AxisMin = 0.0f;
	Bs.AxisMax = 1.0f;
	Bs.AddSample(&Idle, 0.0f);
	Bs.AddSample(&Run, 1.0f);

	const UAnimSequence* A = nullptr;
	const UAnimSequence* B = nullptr;
	float Alpha = -1.0f;

	// At idle.
	Bs.Evaluate(0.0f, A, B, Alpha);
	TestTrue("Idle: both samples idle", A == &Idle && B == &Idle);
	TestEqual("Idle: alpha", Alpha, 0.0f, 1.0e-5f);

	// Mid blend.
	Bs.Evaluate(0.5f, A, B, Alpha);
	TestTrue("Mid: idle to run", A == &Idle && B == &Run);
	TestEqual("Mid: alpha", Alpha, 0.5f, 1.0e-5f);

	// At run.
	Bs.Evaluate(1.0f, A, B, Alpha);
	TestTrue("Run: both samples run", A == &Run && B == &Run);
	TestEqual("Run: alpha", Alpha, 0.0f, 1.0e-5f);

	// Clamped below and above the axis.
	Bs.Evaluate(-2.0f, A, B, Alpha);
	TestTrue("Below the axis: idle", A == &Idle && B == &Idle);
	Bs.Evaluate(3.0f, A, B, Alpha);
	TestTrue("Above the axis: run", A == &Run && B == &Run);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlendSpace1DEmptyTest, "System.AnimationCore.BlendSpace1D.Empty",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FBlendSpace1DEmptyTest::RunTest(const FString& Parameters)
{
	UBlendSpace1D Bs;
	const UAnimSequence* A = reinterpret_cast<const UAnimSequence*>(1);
	const UAnimSequence* B = reinterpret_cast<const UAnimSequence*>(1);
	float Alpha = 1.0f;
	Bs.Evaluate(0.5f, A, B, Alpha);
	TestTrue("No samples", A == nullptr && B == nullptr);
	TestEqual("Alpha", Alpha, 0.0f, 1.0e-5f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
