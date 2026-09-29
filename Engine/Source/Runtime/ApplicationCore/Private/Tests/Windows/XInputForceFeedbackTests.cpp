#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Windows/XInputForceFeedback.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** A fake XInput: which slots have a pad, and what each slot was sent. */
	struct FFakeXInput
	{
		bool bConnected[FXInputForceFeedback::MaxUsers] = {};
		FXInputMotorSpeeds Speeds[FXInputForceFeedback::MaxUsers];
		int32 NumSets = 0;
	};
	FFakeXInput GFake;

	bool FakeIsConnected(uint32 UserIndex)
	{
		return GFake.bConnected[UserIndex];
	}

	bool FakeSetMotors(uint32 UserIndex, const FXInputMotorSpeeds& Speeds)
	{
		if (!GFake.bConnected[UserIndex])
		{
			return false;
		}
		GFake.Speeds[UserIndex] = Speeds;
		++GFake.NumSets;
		return true;
	}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FXInputForceFeedbackTest, "System.ApplicationCore.Windows.XInputForceFeedback",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FXInputForceFeedbackTest::RunTest(const FString& Parameters)
{
	// UE's channels on an Xbox pad's motors: the large ones on the heavy left motor, the small ones on the light right
	// one, 0 to 65535 (ps2-shipping N24b).
	FForceFeedbackValues Values;
	Values.LeftLarge = 0.25f;
	Values.RightLarge = 1.0f;
	Values.LeftSmall = 0.5f;
	FXInputMotorSpeeds Speeds = FXInputForceFeedback::ToMotorSpeeds(Values);
	TestEqual("The large channels, the stronger, on the left motor", int32(Speeds.Left), 65535);
	TestEqual("The small channels on the right motor", int32(Speeds.Right), 32768);
	Values = FForceFeedbackValues();
	Values.RightSmall = 2.0f;
	Speeds = FXInputForceFeedback::ToMotorSpeeds(Values);
	TestEqual("Clamped to full", int32(Speeds.Right), 65535);
	TestEqual("Nothing asked: stopped", int32(Speeds.Left), 0);

	// Controller 0 is the first pad XInput has, controller 1 the second; a pad is told when its speeds change.
	GFake = FFakeXInput();
	GFake.bConnected[1] = true;
	GFake.bConnected[3] = true;
	{
		FXInputForceFeedback XInput(&FakeIsConnected, &FakeSetMotors);
		TestTrue("Available through the given functions", XInput.IsAvailable());
		FForceFeedbackValues Controllers[IInputInterface::MaxControllers];
		Controllers[0].LeftLarge = 1.0f;
		Controllers[1].RightSmall = 1.0f;
		XInput.Update(Controllers, IInputInterface::MaxControllers);
		TestEqual("Controller 0 is slot 1", XInput.GetUserIndex(0), 1);
		TestEqual("Controller 1 is slot 3", XInput.GetUserIndex(1), 3);
		TestEqual("Slot 1's heavy motor", int32(GFake.Speeds[1].Left), 65535);
		TestEqual("Slot 3's light motor", int32(GFake.Speeds[3].Right), 65535);
		TestEqual("Two sends", GFake.NumSets, 2);
		XInput.Update(Controllers, IInputInterface::MaxControllers);
		TestEqual("Unchanged: nothing sent", GFake.NumSets, 2);

		// Pulled out: the send fails and is forgotten; plugged back, the pad is told again.
		GFake.bConnected[3] = false;
		Controllers[1].RightSmall = 0.5f;
		XInput.Update(Controllers, IInputInterface::MaxControllers);
		TestEqual("Nothing reached the missing pad", GFake.NumSets, 2);
		XInput.Update(Controllers, IInputInterface::MaxControllers);
		TestEqual("Controller 1 has no pad after the next scan", XInput.GetUserIndex(1), INDEX_NONE);
		GFake.bConnected[3] = true;
		for (int32 Update = 0; Update <= FXInputForceFeedback::ScanInterval; ++Update)
		{
			XInput.Update(Controllers, IInputInterface::MaxControllers);
		}
		TestEqual("Found again", XInput.GetUserIndex(1), 3);
		TestEqual("Told its speeds again", int32(GFake.Speeds[3].Right), 32768);
	}
	TestEqual("The motors stop when the game lets go of the pads", int32(GFake.Speeds[1].Left), 0);
	TestEqual("Both of them", int32(GFake.Speeds[3].Right), 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
