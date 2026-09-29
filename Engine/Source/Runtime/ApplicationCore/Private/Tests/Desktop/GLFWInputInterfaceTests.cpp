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
	// the sticks go through libpad's bytes and dead zone, and the pressure axes are the triggers' travel and the other
	// buttons' all or nothing. Controller 1 is the second pad (ps2-shipping N24).
	FGLFWInputInterface Input;
	TestEqual("Two controllers", Input.GetNumControllers(), 2);
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
	Input.SetState(0, true, State);

	TestTrue("Connected", Input.IsGamepadConnected(0));
	TestFalse("The second pad is not", Input.IsGamepadConnected(1));
	TestTrue("A is Cross", Input.IsGamepadKeyDown(0, EKeys::Gamepad_FaceButton_Bottom));
	TestFalse("Not on the second pad", Input.IsGamepadKeyDown(1, EKeys::Gamepad_FaceButton_Bottom));
	TestFalse("B is not down", Input.IsGamepadKeyDown(0, EKeys::Gamepad_FaceButton_Right));
	TestTrue("Start", Input.IsGamepadKeyDown(0, EKeys::Gamepad_Special_Right));
	TestFalse("L2 released", Input.IsGamepadKeyDown(0, EKeys::Gamepad_LeftTrigger));
	TestTrue("R2 pulled", Input.IsGamepadKeyDown(0, EKeys::Gamepad_RightTrigger));
	TestEqual("The left stick pushed up is +1", Input.GetGamepadAnalog(0, EKeys::Gamepad_LeftY), 1.0f, 1.0e-5f);
	TestEqual("A small tilt is rest", Input.GetGamepadAnalog(0, EKeys::Gamepad_RightX), 0.0f);
	TestEqual("L2's travel", Input.GetGamepadAnalog(0, EKeys::Gamepad_LeftTriggerAxis), 0.0f);
	TestEqual("R2's travel", Input.GetGamepadAnalog(0, EKeys::Gamepad_RightTriggerAxis), 0.8f, 1.0e-5f);
	TestEqual("Cross pressed all the way", Input.GetGamepadAnalog(0, EKeys::Gamepad_FaceButton_BottomAxis), 1.0f);
	TestEqual("Circle not pressed", Input.GetGamepadAnalog(0, EKeys::Gamepad_FaceButton_RightAxis), 0.0f);

	Input.SetState(1, true, State);
	TestTrue("The second pad reads on its own", Input.IsGamepadKeyDown(1, EKeys::Gamepad_FaceButton_Bottom));

	Input.SetState(0, false, State);
	TestFalse("Unplugged: nothing down", Input.IsGamepadKeyDown(0, EKeys::Gamepad_FaceButton_Bottom));
	TestEqual("Unplugged: sticks at rest", Input.GetGamepadAnalog(0, EKeys::Gamepad_LeftY), 0.0f);
	TestEqual("Unplugged: no pressure", Input.GetGamepadAnalog(0, EKeys::Gamepad_RightTriggerAxis), 0.0f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
