#include "CoreMinimal.h"
#include "GenericPlatform/DualShockConnection.h"
#include "GenericPlatform/DualShockForceFeedback.h"
#include "GenericPlatform/DualShockPressure.h"
#include "GenericPlatform/IInputInterface.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDualShockConnectionTest, "System.ApplicationCore.DualShock.Reconnect",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FDualShockConnectionTest::RunTest(const FString& Parameters)
{
	// A pad found digital is asked for the analog mode once, and not while the command is in flight (not readable).
	FDualShockConnection Connection;
	TestTrue("Nothing before the pad is readable",
		Connection.Update(true, false, false, true, true) == EDualShockCommand::None);
	TestTrue("Asked when readable and digital",
		Connection.Update(true, true, false, true, true) == EDualShockCommand::SetAnalogMode);
	TestTrue(
		"Not while the command runs", Connection.Update(true, false, false, true, true) == EDualShockCommand::None);
	TestTrue("Not again at once", Connection.Update(true, true, false, true, true) == EDualShockCommand::None);
	TestEqual("One request", Connection.GetNumRequests(), 1);

	// In DualShock mode: the motors, then the pressure, one command a frame, each once (ps2-shipping N24).
	TestTrue(
		"The motors first", Connection.Update(true, true, true, true, true) == EDualShockCommand::SetActuatorAlign);
	TestTrue("Not while it runs", Connection.Update(true, false, true, true, true) == EDualShockCommand::None);
	TestTrue(
		"Then the pressure", Connection.Update(true, true, true, true, true) == EDualShockCommand::EnterPressureMode);
	TestTrue("Then nothing", Connection.Update(true, true, true, true, true) == EDualShockCommand::None);
	TestTrue("Aligned", Connection.AreActuatorsAligned());
	TestTrue("Pressure mode", Connection.IsPressureMode());

	// Pulled out and plugged back: the pad starts digital, unaligned and without pressure, and is asked again (N5's
	// bug: it stayed digital).
	TestTrue("Disconnected", Connection.Update(false, false, false) == EDualShockCommand::None);
	TestFalse("The motors are forgotten", Connection.AreActuatorsAligned());
	TestFalse("The pressure is forgotten", Connection.IsPressureMode());
	TestTrue("Reconnecting", Connection.Update(true, false, false, true, true) == EDualShockCommand::None);
	TestTrue("Asked again after the reconnection",
		Connection.Update(true, true, false, true, true) == EDualShockCommand::SetAnalogMode);
	TestEqual("Two requests", Connection.GetNumRequests(), 2);
	TestTrue(
		"The motors again", Connection.Update(true, true, true, true, true) == EDualShockCommand::SetActuatorAlign);
	TestTrue(
		"The pressure again", Connection.Update(true, true, true, true, true) == EDualShockCommand::EnterPressureMode);

	// A pad without motors or pressure (a first DualShock) is only asked for its mode.
	FDualShockConnection Plain;
	TestTrue("Mode", Plain.Update(true, true, false) == EDualShockCommand::SetAnalogMode);
	TestTrue("Nothing more", Plain.Update(true, true, true) == EDualShockCommand::None);

	// A request that did not take is repeated after RetryFrames readable frames, not every frame.
	int32 Requests = 0;
	for (int32 Frame = 0; Frame < FDualShockConnection::RetryFrames * 3; ++Frame)
	{
		Requests += Connection.Update(true, true, false) == EDualShockCommand::SetAnalogMode ? 1 : 0;
	}
	TestEqual("One retry each RetryFrames", Requests, 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDualShockForceFeedbackTest, "System.ApplicationCore.DualShock.ForceFeedback",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FDualShockForceFeedbackTest::RunTest(const FString& Parameters)
{
	// UE's four channels on the two motors: the large ones (the stronger) are the large motor's speed from its minimum,
	// the small ones switch the small motor on from half.
	FForceFeedbackValues Values;
	TestTrue("Still", FDualShockForceFeedback::ToMotors(Values) == FDualShockMotors());
	Values.RightSmall = 0.4f;
	TestEqual("A light small channel", FDualShockForceFeedback::ToMotors(Values).Small, uint8(0));
	Values.LeftSmall = 0.5f;
	TestEqual("Half turns the small motor on", FDualShockForceFeedback::ToMotors(Values).Small, uint8(1));
	TestEqual("The large motor stays still", FDualShockForceFeedback::ToMotors(Values).Large, uint8(0));
	Values.LeftLarge = 0.01f;
	TestEqual("A touch starts the large motor at its minimum", FDualShockForceFeedback::ToMotors(Values).Large,
		uint8(FDualShockForceFeedback::LargeMinimumSpeed + 2));
	Values.RightLarge = 1.0f;
	TestEqual("The stronger large channel, full", FDualShockForceFeedback::ToMotors(Values).Large, uint8(255));

	// The input interface keeps each controller's request, clamped; one channel at a time too.
	struct FTestInput final : public IInputInterface
	{
		bool IsGamepadConnected(int32) const override
		{
			return true;
		}
		bool IsGamepadKeyDown(int32, const FKey&) const override
		{
			return false;
		}
		float GetGamepadAnalog(int32, const FKey&) const override
		{
			return 0.0f;
		}
	} Input;
	Input.SetForceFeedbackChannelValue(1, FForceFeedbackChannelType::LEFT_LARGE, 2.0f);
	Input.SetForceFeedbackChannelValue(1, FForceFeedbackChannelType::RIGHT_SMALL, 0.75f);
	TestEqual("Clamped", Input.GetForceFeedbackValues(1).LeftLarge, 1.0f);
	TestEqual("Kept", Input.GetForceFeedbackValues(1).RightSmall, 0.75f);
	TestEqual("Per controller", Input.GetForceFeedbackValues(0).LeftLarge, 0.0f);
	Input.SetForceFeedbackChannelValue(7, FForceFeedbackChannelType::LEFT_LARGE, 1.0f);
	TestEqual("No such controller", Input.GetForceFeedbackValues(7).LeftLarge, 0.0f);

	// The pad is told when the motors change, only when it can take them, and again after a reconnection; a pad that
	// goes away is not told anything (and the motors of a pad pulled out stop).
	FDualShockActuators Actuators;
	FDualShockMotors Sent;
	FForceFeedbackValues Fire;
	Fire.LeftSmall = 1.0f;
	TestFalse("Not before the motors are aligned", Actuators.Update(false, Fire, Sent));
	TestTrue("Sent once aligned", Actuators.Update(true, Fire, Sent));
	TestEqual("The small motor", Sent.Small, uint8(1));
	TestFalse("Not sent again unchanged", Actuators.Update(true, Fire, Sent));
	TestTrue("Stopped when the game stops", Actuators.Update(true, FForceFeedbackValues(), Sent));
	TestTrue("Stopped", Sent == FDualShockMotors());
	TestFalse("Disconnected: nothing sent", Actuators.Update(false, Fire, Sent));
	TestTrue("Reconnected: sent again", Actuators.Update(true, Fire, Sent));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDualShockPressureTest, "System.ApplicationCore.DualShock.Pressure",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FDualShockPressureTest::RunTest(const FString& Parameters)
{
	// libpad's pressure bytes in padButtonStatus's order, as analog keys: L2 / R2 are UE's trigger axes.
	TestTrue("Byte 0 is right", FDualShockPressure::GetAxisKey(0) == EKeys::Gamepad_DPad_RightAxis);
	TestTrue("Byte 6 is Cross", FDualShockPressure::GetAxisKey(6) == EKeys::Gamepad_FaceButton_BottomAxis);
	TestTrue("Byte 10 is L2", FDualShockPressure::GetAxisKey(10) == EKeys::Gamepad_LeftTriggerAxis);
	TestTrue("Byte 11 is R2", FDualShockPressure::GetAxisKey(11) == EKeys::Gamepad_RightTriggerAxis);
	TestTrue("Its button", FDualShockPressure::GetButtonKey(6) == EKeys::Gamepad_FaceButton_Bottom);
	for (int32 Index = 0; Index < FDualShockPressure::NumButtons; ++Index)
	{
		const FKey& Axis = FDualShockPressure::GetAxisKey(Index);
		TestEqual(*FString::Printf(TEXT("%s's index"), *Axis.ToString()), FDualShockPressure::IndexOfAxis(Axis), Index);
		TestTrue(
			*FString::Printf(TEXT("%s is a gamepad axis"), *Axis.ToString()), Axis.IsGamepadKey() && Axis.IsAxis1D());
	}
	TestEqual(
		"A button is no axis", FDualShockPressure::IndexOfAxis(EKeys::Gamepad_FaceButton_Bottom), int32(INDEX_NONE));
	TestEqual("Released", FDualShockPressure::FromByte(0), 0.0f);
	TestEqual("Pressed hard", FDualShockPressure::FromByte(255), 1.0f);
	TestEqual("Half", FDualShockPressure::FromByte(128), 128.0f / 255.0f, 1.0e-6f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
