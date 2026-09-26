#include "CoreMinimal.h"
#include "Desktop/GLFWInputInterface.h"
#include "GenericPlatform/DualShockAnalog.h"
#include "InputCoreTypes.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLFWInputInterfaceTest, "System.ApplicationCore.Desktop.GamepadAsDualShock",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGLFWInputInterfaceTest::RunTest(const FString& Parameters)
{
	// A desktop gamepad reads as the DualShock: A is Cross, a trigger past its middle is L2 / R2, the Y axes grow up,
	// and the sticks go through libpad's bytes and dead zone.
	FGLFWInputInterface Input;
	GLFWgamepadstate State{};
	for (float& Axis : State.axes)
	{
		Axis = 0.0f;
	}
	State.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER] = -1.0f;
	State.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] = 0.6f;
	State.buttons[GLFW_GAMEPAD_BUTTON_A] = GLFW_PRESS;
	State.buttons[GLFW_GAMEPAD_BUTTON_START] = GLFW_PRESS;
	State.axes[GLFW_GAMEPAD_AXIS_LEFT_Y] = -1.0f;
	State.axes[GLFW_GAMEPAD_AXIS_RIGHT_X] = 0.1f;
	Input.SetState(true, State);

	TestTrue("Connected", Input.IsGamepadConnected());
	TestTrue("A is Cross", Input.IsGamepadKeyDown(EKeys::Gamepad_FaceButton_Bottom));
	TestFalse("B is not down", Input.IsGamepadKeyDown(EKeys::Gamepad_FaceButton_Right));
	TestTrue("Start", Input.IsGamepadKeyDown(EKeys::Gamepad_Special_Right));
	TestFalse("L2 released", Input.IsGamepadKeyDown(EKeys::Gamepad_LeftTrigger));
	TestTrue("R2 pulled", Input.IsGamepadKeyDown(EKeys::Gamepad_RightTrigger));
	TestEqual("The left stick pushed up is +1", Input.GetGamepadAnalog(EKeys::Gamepad_LeftY), 1.0f, 1.0e-5f);
	TestEqual("A small tilt is rest", Input.GetGamepadAnalog(EKeys::Gamepad_RightX), 0.0f);

	Input.SetState(false, State);
	TestFalse("Unplugged: nothing down", Input.IsGamepadKeyDown(EKeys::Gamepad_FaceButton_Bottom));
	TestEqual("Unplugged: sticks at rest", Input.GetGamepadAnalog(EKeys::Gamepad_LeftY), 0.0f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
