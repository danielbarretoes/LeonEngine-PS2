#include "Desktop/GLFWInputInterface.h"

#include "Desktop/GLFWWindow.h"
#include "GenericPlatform/DualShockAnalog.h"
#include "InputCoreTypes.h"

namespace
{

	/** A trigger (-1 released, 1 pulled) counts as pressed past its middle, as libpad reports L2 / R2. */
	constexpr float TriggerThreshold = 0.0f;

	/** GLFW's button for a DualShock key, or -1. */
	int32 ButtonForKey(const FKey& Key)
	{
		struct FMapping
		{
			const FKey* Key;
			int32 Button;
		};
		static const FMapping Mappings[] = {
			{&EKeys::Gamepad_FaceButton_Bottom, GLFW_GAMEPAD_BUTTON_CROSS},
			{&EKeys::Gamepad_FaceButton_Right, GLFW_GAMEPAD_BUTTON_CIRCLE},
			{&EKeys::Gamepad_FaceButton_Left, GLFW_GAMEPAD_BUTTON_SQUARE},
			{&EKeys::Gamepad_FaceButton_Top, GLFW_GAMEPAD_BUTTON_TRIANGLE},
			{&EKeys::Gamepad_LeftShoulder, GLFW_GAMEPAD_BUTTON_LEFT_BUMPER},
			{&EKeys::Gamepad_RightShoulder, GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER},
			{&EKeys::Gamepad_Special_Left, GLFW_GAMEPAD_BUTTON_BACK},
			{&EKeys::Gamepad_Special_Right, GLFW_GAMEPAD_BUTTON_START},
			{&EKeys::Gamepad_LeftThumbstick, GLFW_GAMEPAD_BUTTON_LEFT_THUMB},
			{&EKeys::Gamepad_RightThumbstick, GLFW_GAMEPAD_BUTTON_RIGHT_THUMB},
			{&EKeys::Gamepad_DPad_Up, GLFW_GAMEPAD_BUTTON_DPAD_UP},
			{&EKeys::Gamepad_DPad_Right, GLFW_GAMEPAD_BUTTON_DPAD_RIGHT},
			{&EKeys::Gamepad_DPad_Down, GLFW_GAMEPAD_BUTTON_DPAD_DOWN},
			{&EKeys::Gamepad_DPad_Left, GLFW_GAMEPAD_BUTTON_DPAD_LEFT},
		};
		for (const FMapping& Mapping : Mappings)
		{
			if (*Mapping.Key == Key)
			{
				return Mapping.Button;
			}
		}
		return -1;
	}

} // namespace

void FGLFWInputInterface::Poll()
{
	bConnected = false;
	if (!IsGLFWInitialized())
	{
		return;
	}
	for (int Joystick = GLFW_JOYSTICK_1; Joystick <= GLFW_JOYSTICK_LAST; ++Joystick)
	{
		if (glfwJoystickIsGamepad(Joystick) == GLFW_TRUE && glfwGetGamepadState(Joystick, &State) == GLFW_TRUE)
		{
			bConnected = true;
			return;
		}
	}
}

bool FGLFWInputInterface::IsGamepadKeyDown(const FKey& Key) const
{
	if (!bConnected)
	{
		return false;
	}
	if (Key == EKeys::Gamepad_LeftTrigger)
	{
		return State.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER] > TriggerThreshold;
	}
	if (Key == EKeys::Gamepad_RightTrigger)
	{
		return State.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] > TriggerThreshold;
	}
	const int32 Button = ButtonForKey(Key);
	return Button >= 0 && State.buttons[Button] == GLFW_PRESS;
}

float FGLFWInputInterface::GetGamepadAnalog(const FKey& Axis) const
{
	if (!bConnected)
	{
		return 0.0f;
	}
	// GLFW's Y grows downwards; the game's grows up (as FPS2InputInterface's).
	if (Axis == EKeys::Gamepad_LeftX)
	{
		return FDualShockAnalog::FromAxis(State.axes[GLFW_GAMEPAD_AXIS_LEFT_X]);
	}
	if (Axis == EKeys::Gamepad_LeftY)
	{
		return -FDualShockAnalog::FromAxis(State.axes[GLFW_GAMEPAD_AXIS_LEFT_Y]);
	}
	if (Axis == EKeys::Gamepad_RightX)
	{
		return FDualShockAnalog::FromAxis(State.axes[GLFW_GAMEPAD_AXIS_RIGHT_X]);
	}
	if (Axis == EKeys::Gamepad_RightY)
	{
		return -FDualShockAnalog::FromAxis(State.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y]);
	}
	return 0.0f;
}
