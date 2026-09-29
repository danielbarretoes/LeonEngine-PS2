#pragma once

#include "CoreTypes.h"
#include "InputCoreTypes.h"

/** A force feedback motor (UE: FForceFeedbackChannelType). */
enum class FForceFeedbackChannelType : uint8
{
	LEFT_LARGE,
	LEFT_SMALL,
	RIGHT_LARGE,
	RIGHT_SMALL
};

/** Every motor's strength, 0 to 1 (UE: FForceFeedbackValues). */
struct FForceFeedbackValues
{
	float LeftLarge = 0.0f;
	float LeftSmall = 0.0f;
	float RightLarge = 0.0f;
	float RightSmall = 0.0f;

	[[nodiscard]] bool operator==(const FForceFeedbackValues& Other) const
	{
		return LeftLarge == Other.LeftLarge && LeftSmall == Other.LeftSmall && RightLarge == Other.RightLarge &&
			RightSmall == Other.RightSmall;
	}
	[[nodiscard]] bool operator!=(const FForceFeedbackValues& Other) const
	{
		return !(*this == Other);
	}
};

/**
 * Game controller access owned by the platform application (UE: IInputInterface + the platform's gamepad polling, e.g.
 * XInputInterface). Leon exposes the polled state of each controller (ControllerId 0 and 1: the PS2's two pad ports,
 * Docs/PLANS/ps2-shipping.md N24) and takes the force feedback UE's way: the player controllers set each controller's
 * motors (SetForceFeedbackChannelValues) and the platform sends them at its next poll.
 */
class APPLICATIONCORE_API IInputInterface
{
public:
	/** The controllers any platform has at most (the PS2's two ports). */
	static constexpr int32 MaxControllers = 2;

	virtual ~IInputInterface() = default;

	/** The controllers this platform polls (1 to MaxControllers). */
	[[nodiscard]] virtual int32 GetNumControllers() const
	{
		return 1;
	}

	virtual bool IsGamepadConnected(int32 ControllerId) const = 0;

	/** Gamepad button state from the last GenericApplication::PollGameDeviceState(). */
	virtual bool IsGamepadKeyDown(int32 ControllerId, const FKey& Key) const = 0;

	/**
	 * Gamepad axis in [-1, 1] (the sticks, EKeys::Gamepad_LeftX ... Gamepad_RightY, dead zone applied) or [0, 1] (a
	 * button's pressure, EKeys::Gamepad_LeftTriggerAxis, Gamepad_FaceButton_BottomAxis, ...: FDualShockPressure).
	 */
	virtual float GetGamepadAnalog(int32 ControllerId, const FKey& Axis) const = 0;

	/** Sets one motor of a controller, 0 to 1 (UE). */
	virtual void SetForceFeedbackChannelValue(int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value);

	/** Sets every motor of a controller (UE); the platform sends them at its next poll. */
	virtual void SetForceFeedbackChannelValues(int32 ControllerId, const FForceFeedbackValues& Values);

	/** What the motors of a controller were last asked for (Leon: the platforms and the tests read it). */
	[[nodiscard]] const FForceFeedbackValues& GetForceFeedbackValues(int32 ControllerId) const;

protected:
	/** Each controller's motors as the game asked. */
	FForceFeedbackValues ForceFeedbackValues[MaxControllers];
};
