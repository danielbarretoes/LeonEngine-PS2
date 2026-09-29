#include "Desktop/GLFWInputInterface.h"

#include "Desktop/GLFWWindow.h"
#include "GenericPlatform/DualShockAnalog.h"
#include "GenericPlatform/DualShockPressure.h"
#include "InputCoreTypes.h"

#if PLATFORM_WINDOWS
	#include "Windows/XInputForceFeedback.h"
#else
/** No motors off Windows (GLFW drives none). */
class FXInputForceFeedback
{
};
#endif

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

FGLFWInputInterface::FGLFWInputInterface() = default;

FGLFWInputInterface::~FGLFWInputInterface() = default;

void FGLFWInputInterface::Poll()
{
	int32 ControllerId = 0;
	if (IsGLFWInitialized())
	{
		for (int Joystick = GLFW_JOYSTICK_1; Joystick <= GLFW_JOYSTICK_LAST && ControllerId < MaxControllers;
			++Joystick)
		{
			if (glfwJoystickIsGamepad(Joystick) == GLFW_TRUE &&
				glfwGetGamepadState(Joystick, &State[ControllerId]) == GLFW_TRUE)
			{
				bConnected[ControllerId++] = true;
			}
		}
	}
	for (; ControllerId < MaxControllers; ++ControllerId)
	{
		bConnected[ControllerId] = false;
		// A pad that went away does not start again with the game's last request (as the PS2's).
		ForceFeedbackValues[ControllerId] = FForceFeedbackValues();
	}
#if PLATFORM_WINDOWS
	if (!XInput)
	{
		XInput = MakeUnique<FXInputForceFeedback>();
	}
	XInput->Update(ForceFeedbackValues, MaxControllers);
#endif
}

bool FGLFWInputInterface::IsGamepadConnected(int32 ControllerId) const
{
	return ControllerId >= 0 && ControllerId < MaxControllers && bConnected[ControllerId];
}

bool FGLFWInputInterface::IsGamepadKeyDown(int32 ControllerId, const FKey& Key) const
{
	if (!IsGamepadConnected(ControllerId))
	{
		return false;
	}
	const GLFWgamepadstate& Pad = State[ControllerId];
	if (Key == EKeys::Gamepad_LeftTrigger)
	{
		return Pad.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER] > TriggerThreshold;
	}
	if (Key == EKeys::Gamepad_RightTrigger)
	{
		return Pad.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] > TriggerThreshold;
	}
	const int32 Button = ButtonForKey(Key);
	return Button >= 0 && Pad.buttons[Button] == GLFW_PRESS;
}

float FGLFWInputInterface::GetGamepadAnalog(int32 ControllerId, const FKey& Axis) const
{
	if (!IsGamepadConnected(ControllerId))
	{
		return 0.0f;
	}
	const GLFWgamepadstate& Pad = State[ControllerId];
	// GLFW's Y grows downwards; the game's grows up (as FPS2InputInterface's).
	if (Axis == EKeys::Gamepad_LeftX)
	{
		return FDualShockAnalog::FromAxis(Pad.axes[GLFW_GAMEPAD_AXIS_LEFT_X]);
	}
	if (Axis == EKeys::Gamepad_LeftY)
	{
		return -FDualShockAnalog::FromAxis(Pad.axes[GLFW_GAMEPAD_AXIS_LEFT_Y]);
	}
	if (Axis == EKeys::Gamepad_RightX)
	{
		return FDualShockAnalog::FromAxis(Pad.axes[GLFW_GAMEPAD_AXIS_RIGHT_X]);
	}
	if (Axis == EKeys::Gamepad_RightY)
	{
		return -FDualShockAnalog::FromAxis(Pad.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y]);
	}
	// The pressures: a trigger's travel (-1 released, 1 pulled), the other buttons all or nothing.
	if (Axis == EKeys::Gamepad_LeftTriggerAxis)
	{
		return FMath::Clamp((Pad.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER] + 1.0f) * 0.5f, 0.0f, 1.0f);
	}
	if (Axis == EKeys::Gamepad_RightTriggerAxis)
	{
		return FMath::Clamp((Pad.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] + 1.0f) * 0.5f, 0.0f, 1.0f);
	}
	const int32 Pressure = FDualShockPressure::IndexOfAxis(Axis);
	if (Pressure != INDEX_NONE)
	{
		return IsGamepadKeyDown(ControllerId, FDualShockPressure::GetButtonKey(Pressure)) ? 1.0f : 0.0f;
	}
	return 0.0f;
}
