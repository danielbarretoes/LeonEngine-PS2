#pragma once

#include "CoreTypes.h"
#include "InputCoreTypes.h"

/**
 * Game controller access owned by the platform application (UE: IInputInterface + the
 * platform's gamepad polling, e.g. XInputInterface). Leon exposes polled state directly.
 */
class APPLICATIONCORE_API IInputInterface
{
public:
	virtual ~IInputInterface() = default;

	virtual bool IsGamepadConnected() const = 0;

	/** Gamepad button state from the last GenericApplication::PollGameDeviceState(). */
	virtual bool IsGamepadKeyDown(const FKey& Key) const = 0;

	/** Gamepad axis (EKeys::Gamepad_LeftX ... Gamepad_RightY) in [-1, 1], dead zone applied. */
	virtual float GetGamepadAnalog(const FKey& Axis) const = 0;
};
