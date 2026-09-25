#pragma once

#include "GenericPlatform/IInputInterface.h"

/**
 * DualShock on pad port 0 through libpad (UE homologue: XInputInterface).
 * Owned by FPS2Application; SendControllerEvents() polls once per frame.
 */
class APPLICATIONCORE_API FPS2InputInterface final : public IInputInterface
{
public:
	/** The single pad interface (nullptr before the application created it). */
	static FPS2InputInterface* Get();

	FPS2InputInterface();
	virtual ~FPS2InputInterface() override;

	/** Loads the IOP pad modules and opens port 0. */
	bool Initialize();

	/** Reads the pad (UE: SendControllerEvents); called by PollGameDeviceState. */
	void SendControllerEvents();

	virtual bool IsGamepadConnected() const override;
	virtual bool IsGamepadKeyDown(const FKey& Key) const override;
	virtual float GetGamepadAnalog(const FKey& Axis) const override;

	// Raw state for the engine debug widget.
	bool IsPortOpen() const;
	uint16 GetRawButtonMask() const;
	void GetRawSticks(uint8& OutLeftX, uint8& OutLeftY, uint8& OutRightX, uint8& OutRightY) const;
};
