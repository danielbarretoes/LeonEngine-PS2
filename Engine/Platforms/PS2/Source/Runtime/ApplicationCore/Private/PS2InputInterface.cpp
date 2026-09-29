#include "PS2InputInterface.h"

#include "GenericPlatform/DualShockAnalog.h"
#include "GenericPlatform/DualShockConnection.h"
#include "GenericPlatform/DualShockForceFeedback.h"
#include "GenericPlatform/DualShockPressure.h"
#include "GenericPlatform/GenericApplication.h"
#include "HAL/PlatformMisc.h"

#include <libpad.h>

// The DualShocks on the PS2's two pad ports (Docs/PLANS/ps2-shipping.md N5, N24): libpad over the ROM's SIO2MAN and
// PADMAN. Each port's pad is read every frame; FDualShockConnection says which command it needs (the analog mode, the
// motors' alignment, the pressure mode: one a frame, when the pad is stable), and FDualShockActuators when its motors
// are sent (padSetActDirect).

namespace
{
	constexpr int32 NumPorts = IInputInterface::MaxControllers;

	/** libpad's buffer of each port: 256 bytes, 64-byte aligned. */
	char GPadBuffers[NumPorts][256] __attribute__((aligned(64)));

	/** One port's pad. */
	struct FPadPort
	{
		bool bOpen = false;
		bool bSampleValid = false;
		FDualShockConnection Connection;
		FDualShockActuators Actuators;
		padButtonStatus Status{};
		int32 LastState = -1;
		/** What the pad has (padInfoAct, padInfoPressMode: asked once a connection, in DualShock mode). */
		bool bCapsKnown = false;
		bool bHasActuators = false;
		bool bHasPressure = false;
	};
	FPadPort GPorts[NumPorts];

	/** padSetActAlign: the small motor on byte 0, the large one on byte 1, the rest unused. */
	const char GActuatorAlign[6] = {0, 1, char(0xff), char(0xff), char(0xff), char(0xff)};

	bool IsPadStateReadable(int32 State)
	{
		return State == PAD_STATE_STABLE || State == PAD_STATE_FINDCTP1;
	}

	/** The DualShock button of a gamepad key (the pad masks of libpad), or 0. */
	uint16 PadMaskForKey(const FKey& Key)
	{
		struct FPadButton
		{
			const FKey* Key;
			uint16 Mask;
		};
		static const FPadButton Buttons[] = {
			{&EKeys::Gamepad_FaceButton_Bottom, PAD_CROSS},
			{&EKeys::Gamepad_FaceButton_Right, PAD_CIRCLE},
			{&EKeys::Gamepad_FaceButton_Left, PAD_SQUARE},
			{&EKeys::Gamepad_FaceButton_Top, PAD_TRIANGLE},
			{&EKeys::Gamepad_LeftShoulder, PAD_L1},
			{&EKeys::Gamepad_RightShoulder, PAD_R1},
			{&EKeys::Gamepad_LeftTrigger, PAD_L2},
			{&EKeys::Gamepad_RightTrigger, PAD_R2},
			{&EKeys::Gamepad_Special_Left, PAD_SELECT},
			{&EKeys::Gamepad_Special_Right, PAD_START},
			{&EKeys::Gamepad_LeftThumbstick, PAD_L3},
			{&EKeys::Gamepad_RightThumbstick, PAD_R3},
			{&EKeys::Gamepad_DPad_Up, PAD_UP},
			{&EKeys::Gamepad_DPad_Down, PAD_DOWN},
			{&EKeys::Gamepad_DPad_Left, PAD_LEFT},
			{&EKeys::Gamepad_DPad_Right, PAD_RIGHT},
		};
		for (const FPadButton& Button : Buttons)
		{
			if (*Button.Key == Key)
			{
				return Button.Mask;
			}
		}
		return 0;
	}

	/** The pressed buttons of a port's last sample (libpad's bits are 0 when pressed). */
	uint16 GetButtonMask(const FPadPort& Port)
	{
		return Port.bSampleValid ? static_cast<uint16>(Port.Status.btns ^ 0xFFFF) : 0;
	}

	/** A port's pressure bytes in padButtonStatus's order (FDualShockPressure). */
	uint8 GetPressureByte(const FPadPort& Port, int32 Index)
	{
		const unsigned char* Bytes = &Port.Status.right_p;
		return Bytes[Index];
	}

	/** Sends the command FDualShockConnection asks for. */
	void UpdateConnection(int32 PortIndex, FPadPort& Port, int32 State)
	{
		const bool bConnected = State != PAD_STATE_DISCONN;
		const bool bReadable = IsPadStateReadable(State);
		const bool bDualShockMode = bReadable && padInfoMode(PortIndex, 0, PAD_MODECURID, 0) == PAD_TYPE_DUALSHOCK;
		if (!bDualShockMode && (!bConnected || bReadable))
		{
			Port.bCapsKnown = false;
		}
		else if (bDualShockMode && !Port.bCapsKnown)
		{
			Port.bCapsKnown = true;
			Port.bHasActuators = padInfoAct(PortIndex, 0, -1, 0) > 0;
			Port.bHasPressure = padInfoPressMode(PortIndex, 0) == 1;
			UE_LOG(LogApplicationCore, Log, "PS2InputInterface: port %d DualShock%s%s", PortIndex,
				Port.bHasActuators ? ", motors" : "", Port.bHasPressure ? ", pressure" : "");
		}
		switch (Port.Connection.Update(bConnected, bReadable, bDualShockMode, bDualShockMode && Port.bHasActuators,
			bDualShockMode && Port.bHasPressure))
		{
			case EDualShockCommand::SetAnalogMode:
				padSetMainMode(PortIndex, 0, PAD_MMODE_DUALSHOCK, PAD_MMODE_LOCK);
				UE_LOG(LogApplicationCore, Log, "PS2InputInterface: port %d DualShock analog mode requested (%d)",
					PortIndex, Port.Connection.GetNumRequests());
				break;
			case EDualShockCommand::SetActuatorAlign:
				padSetActAlign(PortIndex, 0, GActuatorAlign);
				UE_LOG(LogApplicationCore, Log, "PS2InputInterface: port %d motors aligned", PortIndex);
				break;
			case EDualShockCommand::EnterPressureMode:
				padEnterPressMode(PortIndex, 0);
				UE_LOG(LogApplicationCore, Log, "PS2InputInterface: port %d pressure mode", PortIndex);
				break;
			case EDualShockCommand::None:
				break;
		}
	}
} // namespace

FPS2InputInterface::~FPS2InputInterface()
{
	// The motors stop with the game.
	for (int32 PortIndex = 0; PortIndex < NumPorts; ++PortIndex)
	{
		if (GPorts[PortIndex].bOpen && GPorts[PortIndex].Connection.AreActuatorsAligned())
		{
			const char Stop[6] = {0, 0, 0, 0, 0, 0};
			padSetActDirect(PortIndex, 0, Stop);
		}
	}
}

bool FPS2InputInterface::Initialize()
{
	bool bAnyOpen = false;
	const bool bModules = FPlatformMisc::LoadIopModule("rom0:SIO2MAN") && FPlatformMisc::LoadIopModule("rom0:PADMAN");
	if (bModules)
	{
		// libpad's padInit waits for PADMAN forever when it is missing: only with the modules in.
		padInit(0);
	}
	for (int32 PortIndex = 0; PortIndex < NumPorts; ++PortIndex)
	{
		FPadPort& Port = GPorts[PortIndex];
		Port = FPadPort();
		Port.bOpen = bModules && padPortOpen(PortIndex, 0, GPadBuffers[PortIndex]) != 0;
		bAnyOpen |= Port.bOpen;
		UE_LOG(LogApplicationCore, Log, "PS2InputInterface: pad port %d %s", PortIndex, Port.bOpen ? "open" : "failed");
	}
	return bAnyOpen;
}

void FPS2InputInterface::SendControllerEvents()
{
	for (int32 PortIndex = 0; PortIndex < NumPorts; ++PortIndex)
	{
		FPadPort& Port = GPorts[PortIndex];
		Port.bSampleValid = false;
		if (!Port.bOpen)
		{
			continue;
		}
		const int32 State = padGetState(PortIndex, 0);
		UpdateConnection(PortIndex, Port, State);
		if (State != Port.LastState)
		{
			UE_LOG(LogApplicationCore, Log, "PS2InputInterface: port %d state %d", PortIndex, State);
			Port.LastState = State;
		}
		if (State == PAD_STATE_DISCONN)
		{
			// A pad pulled out stops, and the game's last request does not start it again when it comes back.
			ForceFeedbackValues[PortIndex] = FForceFeedbackValues();
		}
		const bool bReadable = IsPadStateReadable(State);
		FDualShockMotors Motors;
		if (Port.Actuators.Update(
				bReadable && Port.Connection.AreActuatorsAligned(), ForceFeedbackValues[PortIndex], Motors))
		{
			const char Direct[6] = {char(Motors.Small), char(Motors.Large), 0, 0, 0, 0};
			padSetActDirect(PortIndex, 0, Direct);
		}
		if (bReadable && padRead(PortIndex, 0, &Port.Status) != 0)
		{
			Port.bSampleValid = true;
		}
	}
}

bool FPS2InputInterface::IsGamepadConnected(int32 ControllerId) const
{
	return ControllerId >= 0 && ControllerId < NumPorts && GPorts[ControllerId].bSampleValid;
}

bool FPS2InputInterface::IsGamepadKeyDown(int32 ControllerId, const FKey& Key) const
{
	if (!IsGamepadConnected(ControllerId))
	{
		return false;
	}
	const uint16 Mask = PadMaskForKey(Key);
	return Mask != 0 && (GetButtonMask(GPorts[ControllerId]) & Mask) != 0;
}

float FPS2InputInterface::GetGamepadAnalog(int32 ControllerId, const FKey& Axis) const
{
	if (!IsGamepadConnected(ControllerId))
	{
		return 0.0f;
	}
	const FPadPort& Port = GPorts[ControllerId];
	if (Axis == EKeys::Gamepad_LeftX)
	{
		return FDualShockAnalog::FromByte(Port.Status.ljoy_h);
	}
	if (Axis == EKeys::Gamepad_LeftY)
	{
		return -FDualShockAnalog::FromByte(Port.Status.ljoy_v); // raw 0 = stick up
	}
	if (Axis == EKeys::Gamepad_RightX)
	{
		return FDualShockAnalog::FromByte(Port.Status.rjoy_h);
	}
	if (Axis == EKeys::Gamepad_RightY)
	{
		return -FDualShockAnalog::FromByte(Port.Status.rjoy_v);
	}
	const int32 Pressure = FDualShockPressure::IndexOfAxis(Axis);
	if (Pressure != INDEX_NONE)
	{
		// Without the pressure mode a button is all or nothing.
		if (Port.Connection.IsPressureMode())
		{
			return FDualShockPressure::FromByte(GetPressureByte(Port, Pressure));
		}
		return IsGamepadKeyDown(ControllerId, FDualShockPressure::GetButtonKey(Pressure)) ? 1.0f : 0.0f;
	}
	return 0.0f;
}
