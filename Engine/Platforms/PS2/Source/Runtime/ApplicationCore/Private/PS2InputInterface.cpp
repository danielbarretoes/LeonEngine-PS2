#include "PS2InputInterface.h"

#include "GenericPlatform/GenericApplication.h"

#include <libpad.h>
#include <loadfile.h>
#include <sifrpc.h>

namespace
{
	char GPadBuffer[256] __attribute__((aligned(64)));
	FPS2InputInterface* GPS2InputInterface = nullptr;

	bool bPortOpen = false;
	bool bAnalogRequested = false;
	bool bSampleValid = false;
	padButtonStatus GPad{};
	int32 LastPadState = -1;
	uint16 LastLoggedButtons = 0;

	constexpr float StickDeadZone = 0.18f;

	void LoadPadModules()
	{
		SifInitRpc(0);
		if (SifLoadModule("rom0:SIO2MAN", 0, nullptr) < 0)
		{
			UE_LOG(LogApplicationCore, Error, "PS2InputInterface: SIO2MAN load failed");
		}
		if (SifLoadModule("rom0:PADMAN", 0, nullptr) < 0)
		{
			UE_LOG(LogApplicationCore, Error, "PS2InputInterface: PADMAN load failed");
		}
	}

	/** DualShock axis byte (0..255, centre ~128) to [-1, 1] with a rescaled dead zone. */
	float AxisFromByte(uint8 Raw)
	{
		float Value = (static_cast<float>(Raw) - 128.0f) / 128.0f;
		Value = Value < -1.0f ? -1.0f : (Value > 1.0f ? 1.0f : Value);
		if (Value > -StickDeadZone && Value < StickDeadZone)
		{
			return 0.0f;
		}
		const float Sign = Value < 0.0f ? -1.0f : 1.0f;
		const float Magnitude = (Value < 0.0f ? -Value : Value) - StickDeadZone;
		return Sign * (Magnitude / (1.0f - StickDeadZone));
	}

	bool IsPadStateReadable(int32 State)
	{
		return State == PAD_STATE_STABLE || State == PAD_STATE_FINDCTP1;
	}

	void TryEnableAnalog()
	{
		if (bAnalogRequested || !bPortOpen || !IsPadStateReadable(padGetState(0, 0)))
		{
			return;
		}
		padSetMainMode(0, 0, PAD_MMODE_DUALSHOCK, PAD_MMODE_LOCK);
		bAnalogRequested = true;
		UE_LOG(LogApplicationCore, Log, "PS2InputInterface: DualShock analog mode requested");
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
} // namespace

FPS2InputInterface* FPS2InputInterface::Get()
{
	return GPS2InputInterface;
}

FPS2InputInterface::FPS2InputInterface()
{
	GPS2InputInterface = this;
}

FPS2InputInterface::~FPS2InputInterface()
{
	if (GPS2InputInterface == this)
	{
		GPS2InputInterface = nullptr;
	}
}

bool FPS2InputInterface::Initialize()
{
	LoadPadModules();
	padInit(0);
	bPortOpen = padPortOpen(0, 0, GPadBuffer) != 0;
	bAnalogRequested = false;
	bSampleValid = false;
	UE_LOG(LogApplicationCore, Log, "PS2InputInterface: pad port 0 %s", bPortOpen ? "open" : "failed");
	return bPortOpen;
}

void FPS2InputInterface::SendControllerEvents()
{
	bSampleValid = false;
	if (!bPortOpen)
	{
		return;
	}
	TryEnableAnalog();
	const int32 State = padGetState(0, 0);
	if (State != LastPadState)
	{
		UE_LOG(LogApplicationCore, Log, "PS2InputInterface: port0 state %d", State);
		LastPadState = State;
	}
	if (!IsPadStateReadable(State) || padRead(0, 0, &GPad) == 0)
	{
		return;
	}
	bSampleValid = true;
	const uint16 Buttons = GetRawButtonMask();
	if (Buttons != LastLoggedButtons)
	{
		UE_LOG(LogApplicationCore, Log, "PS2InputInterface: btns 0x%04X", Buttons);
		LastLoggedButtons = Buttons;
	}
}

bool FPS2InputInterface::IsGamepadConnected() const
{
	return bSampleValid;
}

bool FPS2InputInterface::IsGamepadKeyDown(const FKey& Key) const
{
	const uint16 Mask = PadMaskForKey(Key);
	return Mask != 0 && (GetRawButtonMask() & Mask) != 0;
}

float FPS2InputInterface::GetGamepadAnalog(const FKey& Axis) const
{
	if (!bSampleValid)
	{
		return 0.0f;
	}
	if (Axis == EKeys::Gamepad_LeftX)
	{
		return AxisFromByte(GPad.ljoy_h);
	}
	if (Axis == EKeys::Gamepad_LeftY)
	{
		return -AxisFromByte(GPad.ljoy_v); // raw 0 = stick up
	}
	if (Axis == EKeys::Gamepad_RightX)
	{
		return AxisFromByte(GPad.rjoy_h);
	}
	if (Axis == EKeys::Gamepad_RightY)
	{
		return -AxisFromByte(GPad.rjoy_v);
	}
	return 0.0f;
}

bool FPS2InputInterface::IsPortOpen() const
{
	return bPortOpen;
}

uint16 FPS2InputInterface::GetRawButtonMask() const
{
	return bSampleValid ? static_cast<uint16>(GPad.btns ^ 0xFFFF) : 0;
}

void FPS2InputInterface::GetRawSticks(uint8& OutLeftX, uint8& OutLeftY, uint8& OutRightX, uint8& OutRightY) const
{
	OutLeftX = GPad.ljoy_h;
	OutLeftY = GPad.ljoy_v;
	OutRightX = GPad.rjoy_h;
	OutRightY = GPad.rjoy_v;
}
