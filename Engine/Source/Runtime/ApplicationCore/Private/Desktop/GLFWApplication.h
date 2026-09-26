#pragma once

#include "Desktop/GLFWInputInterface.h"
#include "GenericPlatform/GenericApplication.h"

/** Desktop application: GLFW windows, and the first gamepad as the PS2's DualShock (FGLFWInputInterface). */
class FGLFWApplication final : public GenericApplication
{
public:
	virtual TSharedRef<FGenericWindow> MakeWindow() override;

	/** Reads the gamepad once a frame (UE: PollGameDeviceState). */
	virtual void PollGameDeviceState() override
	{
		InputInterface.Poll();
	}

	virtual IInputInterface* GetInputInterface() override
	{
		return &InputInterface;
	}

private:
	FGLFWInputInterface InputInterface;
};
