#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Physics/PhysScene.h"
#include "Tests/LegacyGolden.h"

#if WITH_DEV_AUTOMATION_TESTS

	#if defined(LEON_WITH_JOLT) && LEON_WITH_JOLT

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJoltGoldenBoxDropTest, "System.JoltPhysics.Golden.BoxDrop",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FJoltGoldenBoxDropTest::RunTest(const FString& Parameters)
{
	// A dynamic box dropped onto the floor plane in the Jolt backend: its centre every 12 frames for three seconds
	// and its final vertical speed. Gravity, skin and walk bounds keep their metre-tuned step defaults.
	constexpr int32 Frames = 180;
	constexpr int32 SampleEvery = 12;

	FPhysScene Scene(EPhysicsBackend::Jolt);
	const int32 Id = Scene.AddBody({0, EBodyType::Dynamic, 10.0f, true});
	FBodyInstance& Body = Scene.GetBodies()[Id];
	Body.Position = LegacyGolden::ToWorldPosition(FVector(0.3f, 3.0f, -0.2f));
	Body.HalfExtents = LegacyGolden::ToWorldExtent(FVector(0.4f, 0.5f, 0.3f));

	FPhysSceneStepParams Params;
	Params.DeltaTime = 1.0f / 60.0f;
	Params.FloorZ = LegacyGolden::ToWorldLength(0.0f);

	TArray<FVector> Samples;
	for (int32 Frame = 1; Frame <= Frames; ++Frame)
	{
		Scene.Step(Params);
		if (Frame % SampleEvery == 0)
		{
			Samples.Add(Scene.GetBodies()[Id].Position);
		}
	}

	static const FVector ExpectedSamples[Frames / SampleEvery] = {FVector(0.300000012f, 2.48201752f, -0.200000003f),
		FVector(0.300000012f, 1.01437509f, -0.200000003f), FVector(0.299828112f, 0.499993682f, -0.200150296f),
		FVector(0.2998285f, 0.499993652f, -0.200150669f), FVector(0.299828857f, 0.499993652f, -0.200151026f),
		FVector(0.299829215f, 0.499993652f, -0.200151384f), FVector(0.299829572f, 0.499993652f, -0.200151742f),
		FVector(0.29982993f, 0.499993652f, -0.200152099f), FVector(0.299830288f, 0.499993652f, -0.200152457f),
		FVector(0.299830645f, 0.499993652f, -0.200152814f), FVector(0.299831003f, 0.499993652f, -0.200153172f),
		FVector(0.299831361f, 0.499993652f, -0.20015353f), FVector(0.299831718f, 0.499993652f, -0.200153887f),
		FVector(0.299832076f, 0.499993652f, -0.200154245f), FVector(0.299832433f, 0.499993652f, -0.200154603f)};
	static const float ExpectedFinalVerticalSpeed[1] = {-7.15256533e-07f};
	LegacyGolden::CheckPositions(*this, "Samples", Samples, ExpectedSamples, Frames / SampleEvery, 1.0e-3f);
	LegacyGolden::CheckScalars(*this, "FinalVerticalSpeed", TArray<float>{Scene.GetBodies()[Id].VelocityZ},
		ExpectedFinalVerticalSpeed, 1, 1.0e-2f, LegacyGolden::EUnit::Speed);
	return true;
}

	#endif // LEON_WITH_JOLT

#endif // WITH_DEV_AUTOMATION_TESTS
