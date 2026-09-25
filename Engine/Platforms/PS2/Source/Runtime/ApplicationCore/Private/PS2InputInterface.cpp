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

	uint16 PadMaskForKey(EKeys Key)
	{
		switch (Key)
		{
			case EKeys::Gamepad_FaceButton_Bottom:
				return PAD_CROSS;
			case EKeys::Gamepad_FaceButton_Right:
				return PAD_CIRCLE;
			case EKeys::Gamepad_FaceButton_Left:
				return PAD_SQUARE;
			case EKeys::Gamepad_FaceButton_Top:
				return PAD_TRIANGLE;
			case EKeys::Gamepad_LeftShoulder:
				return PAD_L1;
			case EKeys::Gamepad_RightShoulder:
				return PAD_R1;
			case EKeys::Gamepad_LeftTrigger:
				return PAD_L2;
			case EKeys::Gamepad_RightTrigger:
				return PAD_R2;
			case EKeys::Gamepad_Special_Left:
				return PAD_SELECT;
			case EKeys::Gamepad_Special_Right:
				return PAD_START;
			case EKeys::Gamepad_LeftThumbstick:
				return PAD_L3;
			case EKeys::Gamepad_RightThumbstick:
				return PAD_R3;
			case EKeys::Gamepad_DPad_Up:
				return PAD_UP;
			case EKeys::Gamepad_DPad_Down:
				return PAD_DOWN;
			case EKeys::Gamepad_DPad_Left:
				return PAD_LEFT;
			case EKeys::Gamepad_DPad_Right:
				return PAD_RIGHT;
			default:
				return 0;
		}
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

bool FPS2InputInterface::IsGamepadKeyDown(EKeys Key) const
{
	const uint16 Mask = PadMaskForKey(Key);
	return Mask != 0 && (GetRawButtonMask() & Mask) != 0;
}

float FPS2InputInterface::GetGamepadAnalog(EKeys Axis) const
{
	if (!bSampleValid)
	{
		return 0.0f;
	}
	switch (Axis)
	{
		case EKeys::Gamepad_LeftX:
			return AxisFromByte(GPad.ljoy_h);
		case EKeys::Gamepad_LeftY:
			return -AxisFromByte(GPad.ljoy_v); // raw 0 = stick up
		case EKeys::Gamepad_RightX:
			return AxisFromByte(GPad.rjoy_h);
		case EKeys::Gamepad_RightY:
			return -AxisFromByte(GPad.rjoy_v);
		default:
			return 0.0f;
	}
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
