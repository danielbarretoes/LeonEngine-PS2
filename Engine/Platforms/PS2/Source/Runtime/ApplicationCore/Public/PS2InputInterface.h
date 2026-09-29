#pragma once

#include "GenericPlatform/IInputInterface.h"

/**
 * The DualShocks on pad ports 0 and 1 through libpad (UE homologue: XInputInterface): controller ids 0 and 1
 * (Docs/PLANS/ps2-shipping.md N24). Each pad is put in analog mode, gets its motors aligned and its pressure mode
 * (FDualShockConnection) and is sent its motors (FDualShockActuators) as the game asks. Owned by FPS2Application;
 * SendControllerEvents() polls once per frame.
 */
class APPLICATIONCORE_API FPS2InputInterface final : public IInputInterface
{
public:
	FPS2InputInterface() = default;
	/** Stops the motors. */
	virtual ~FPS2InputInterface() override;

	/** Loads the IOP pad modules and opens both ports. */
	bool Initialize();

	/** Reads the pads and sends their commands and motors (UE: SendControllerEvents); called by PollGameDeviceState. */
	void SendControllerEvents();

	virtual int32 GetNumControllers() const override
	{
		return MaxControllers;
	}
	virtual bool IsGamepadConnected(int32 ControllerId) const override;
	virtual bool IsGamepadKeyDown(int32 ControllerId, const FKey& Key) const override;
	virtual float GetGamepadAnalog(int32 ControllerId, const FKey& Axis) const override;
};
