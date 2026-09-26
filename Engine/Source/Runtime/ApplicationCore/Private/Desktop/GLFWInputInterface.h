#pragma once

#include "GenericPlatform/IInputInterface.h"

#include <GLFW/glfw3.h>

/**
 * The desktop's gamepad as the PS2's DualShock (Docs/PLANS/ps2-preview.md V1; UE's homologue: XInputInterface): the
 * first gamepad GLFW knows (its standard mapping covers Xbox and PlayStation pads), its buttons as the DualShock's
 * gamepad keys (Cross = A, Circle = B, Square = X, Triangle = Y, L2 / R2 digital, Select = Back), and its sticks
 * through FDualShockAnalog, the bytes and dead zone of libpad. FGLFWApplication polls it once a frame.
 */
class FGLFWInputInterface final : public IInputInterface
{
public:
	/** Reads the first connected gamepad (none before GLFW starts with the first window). */
	void Poll();

	bool IsGamepadConnected() const override
	{
		return bConnected;
	}
	bool IsGamepadKeyDown(const FKey& Key) const override;
	float GetGamepadAnalog(const FKey& Axis) const override;

	/** The state as a DualShock reads it (the test's entry point): buttons, then axes, GLFW's layout. */
	void SetState(bool bInConnected, const GLFWgamepadstate& InState)
	{
		bConnected = bInConnected;
		State = InState;
	}

private:
	bool bConnected = false;
	GLFWgamepadstate State{};
};
