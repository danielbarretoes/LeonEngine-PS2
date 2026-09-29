#pragma once

#include "GenericPlatform/IInputInterface.h"
#include "Templates/UniquePtr.h"

#include <GLFW/glfw3.h>

/**
 * The desktop's gamepads as the PS2's DualShocks (Docs/PLANS/ps2-preview.md V1; UE's homologue: XInputInterface): the
 * first two gamepads GLFW knows are controllers 0 and 1 (its standard mapping covers Xbox and PlayStation pads), their
 * buttons as the DualShock's gamepad keys (Cross = A, Circle = B, Square = X, Triangle = Y, L2 / R2 digital past their
 * middle, Select = Back), their sticks through FDualShockAnalog, the bytes and dead zone of libpad, and the pressure
 * axes of FDualShockPressure: the triggers' travel, and 1 or 0 for the other buttons (a desktop pad has no pressure
 * there). FGLFWApplication polls them once a frame. GLFW drives no motor: on Windows the force feedback goes to the
 * Xbox pads through XInput (FXInputForceFeedback, Docs/PLANS/ps2-shipping.md N24b); elsewhere it is kept
 * (GetForceFeedbackValues) and not felt.
 */
class FXInputForceFeedback;

class FGLFWInputInterface final : public IInputInterface
{
public:
	FGLFWInputInterface();
	virtual ~FGLFWInputInterface() override;

	/** Reads the first two connected gamepads (none before GLFW starts with the first window). */
	void Poll();

	int32 GetNumControllers() const override
	{
		return MaxControllers;
	}
	bool IsGamepadConnected(int32 ControllerId) const override;
	bool IsGamepadKeyDown(int32 ControllerId, const FKey& Key) const override;
	float GetGamepadAnalog(int32 ControllerId, const FKey& Axis) const override;

	/** The state as a DualShock reads it (the test's entry point): buttons, then axes, GLFW's layout. */
	void SetState(int32 ControllerId, bool bInConnected, const GLFWgamepadstate& InState)
	{
		if (ControllerId >= 0 && ControllerId < MaxControllers)
		{
			bConnected[ControllerId] = bInConnected;
			State[ControllerId] = InState;
		}
	}

private:
	/** The motors on Windows (made at the first Poll, so a test's interface loads no XInput); null elsewhere. */
	TUniquePtr<FXInputForceFeedback> XInput;
	bool bConnected[MaxControllers] = {};
	GLFWgamepadstate State[MaxControllers]{};
};
