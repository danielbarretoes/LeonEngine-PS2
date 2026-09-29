#pragma once

#include "CoreMinimal.h"
#include "GenericPlatform/IInputInterface.h"

/**
 * An XInput pad's two motors (XINPUT_VIBRATION): the left one is the heavy, low-frequency motor and the right one the
 * light, high-frequency motor, each 0 to 65535.
 */
struct FXInputMotorSpeeds
{
	uint16 Left = 0;
	uint16 Right = 0;

	[[nodiscard]] bool operator==(const FXInputMotorSpeeds& Other) const
	{
		return Left == Other.Left && Right == Other.Right;
	}
	[[nodiscard]] bool operator!=(const FXInputMotorSpeeds& Other) const
	{
		return !(*this == Other);
	}
};

/**
 * The force feedback of the desktop's Xbox pads through XInput (UE: XInputInterface's UpdateForceFeedback;
 * Docs/PLANS/ps2-shipping.md N24b). GLFW reads the pads but drives no motor, so the motors go through
 * `XInputSetState` of `xinput1_4.dll` (or `xinput9_1_0.dll`), loaded at run time: without it nothing vibrates and
 * nothing fails. UE's four channels fold on the pad's two motors as on the DualShock (FDualShockForceFeedback): the
 * large channels drive the heavy left motor, the small ones the light right motor.
 *
 * Controller N is the N-th connected XInput pad (GLFW lists the XInput pads first, in their slot order). A pad is sent
 * its speeds when they change; a pad that goes away forgets them, and the destructor stops every motor it started.
 */
class FXInputForceFeedback
{
public:
	/** Whether XInput slot UserIndex (0 to 3) has a pad. */
	using FIsConnectedFunction = bool (*)(uint32 UserIndex);
	/** Sets a slot's motors; false when no pad is there. */
	using FSetMotorsFunction = bool (*)(uint32 UserIndex, const FXInputMotorSpeeds& Speeds);

	/** XInput's slots. */
	static constexpr int32 MaxUsers = 4;
	/** How many Updates a slot scan lasts: XInput is slow to answer for an empty slot (UE polls it rarely too). */
	static constexpr int32 ScanInterval = 60;

	/** Loads XInput (none on a system without it). */
	FXInputForceFeedback();
	/** Through the given functions instead of XInput (the tests). */
	FXInputForceFeedback(FIsConnectedFunction InIsConnected, FSetMotorsFunction InSetMotors);
	~FXInputForceFeedback();

	FXInputForceFeedback(const FXInputForceFeedback&) = delete;
	FXInputForceFeedback& operator=(const FXInputForceFeedback&) = delete;

	/** Whether XInput was found. */
	[[nodiscard]] bool IsAvailable() const
	{
		return IsConnected != nullptr && SetMotors != nullptr;
	}

	/** UE's channels as the pad's motor speeds. */
	[[nodiscard]] static FXInputMotorSpeeds ToMotorSpeeds(const FForceFeedbackValues& Values);

	/** Once a frame: sends each controller's values (Values[ControllerId]) to its pad when they changed. */
	void Update(const FForceFeedbackValues* Values, int32 NumControllers);

	/** The XInput slot of a controller after the last Update, or INDEX_NONE. */
	[[nodiscard]] int32 GetUserIndex(int32 ControllerId) const
	{
		return ControllerId >= 0 && ControllerId < IInputInterface::MaxControllers ? UserOfController[ControllerId]
																				   : INDEX_NONE;
	}

	/** What a slot was last sent (all 0 before). */
	[[nodiscard]] const FXInputMotorSpeeds& GetLastSent(int32 UserIndex) const
	{
		return LastSent[UserIndex];
	}

private:
	void ScanUsers();

	void* Library = nullptr;
	FIsConnectedFunction IsConnected = nullptr;
	FSetMotorsFunction SetMotors = nullptr;
	int32 UserOfController[IInputInterface::MaxControllers] = {INDEX_NONE, INDEX_NONE};
	FXInputMotorSpeeds LastSent[MaxUsers];
	bool bSent[MaxUsers] = {};
	int32 UpdatesUntilScan = 0;
};
