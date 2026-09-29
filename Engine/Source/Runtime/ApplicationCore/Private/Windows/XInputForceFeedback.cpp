#include "Windows/XInputForceFeedback.h"

#include "Windows/WindowsHWrapper.h"

#include <Xinput.h>

DEFINE_LOG_CATEGORY_STATIC(LogXInput, Log, All);

namespace
{
	using FXInputGetState = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);
	using FXInputSetState = DWORD(WINAPI*)(DWORD, XINPUT_VIBRATION*);

	/** XInput's entry points once the library is loaded (one library for the process, as UE's). */
	FXInputGetState GXInputGetState = nullptr;
	FXInputSetState GXInputSetState = nullptr;

	bool IsXInputConnected(uint32 UserIndex)
	{
		XINPUT_STATE State{};
		return GXInputGetState(DWORD(UserIndex), &State) == ERROR_SUCCESS;
	}

	bool SetXInputMotors(uint32 UserIndex, const FXInputMotorSpeeds& Speeds)
	{
		XINPUT_VIBRATION Vibration{};
		Vibration.wLeftMotorSpeed = Speeds.Left;
		Vibration.wRightMotorSpeed = Speeds.Right;
		return GXInputSetState(DWORD(UserIndex), &Vibration) == ERROR_SUCCESS;
	}

	uint16 ToSpeed(float Value)
	{
		return uint16(FMath::RoundToInt(FMath::Clamp(Value, 0.0f, 1.0f) * 65535.0f));
	}
} // namespace

FXInputForceFeedback::FXInputForceFeedback()
{
	// xinput1_4 ships with Windows 8 and later; xinput9_1_0 with every Windows since Vista.
	for (const char* Name : {"xinput1_4.dll", "xinput9_1_0.dll"})
	{
		HMODULE Module = ::LoadLibraryA(Name);
		if (Module == nullptr)
		{
			continue;
		}
		GXInputGetState = reinterpret_cast<FXInputGetState>(::GetProcAddress(Module, "XInputGetState"));
		GXInputSetState = reinterpret_cast<FXInputSetState>(::GetProcAddress(Module, "XInputSetState"));
		if (GXInputGetState != nullptr && GXInputSetState != nullptr)
		{
			Library = Module;
			IsConnected = &IsXInputConnected;
			SetMotors = &SetXInputMotors;
			UE_LOG(LogXInput, Log, "XInput: the pads' motors through %s", Name);
			return;
		}
		::FreeLibrary(Module);
	}
	GXInputGetState = nullptr;
	GXInputSetState = nullptr;
	UE_LOG(LogXInput, Log, "XInput: not found, the pads do not vibrate");
}

FXInputForceFeedback::FXInputForceFeedback(FIsConnectedFunction InIsConnected, FSetMotorsFunction InSetMotors)
	: IsConnected(InIsConnected)
	, SetMotors(InSetMotors)
{
}

FXInputForceFeedback::~FXInputForceFeedback()
{
	// A motor left running would keep running after the game (XInput keeps the last speeds).
	if (IsAvailable())
	{
		for (int32 User = 0; User < MaxUsers; ++User)
		{
			if (bSent[User] && LastSent[User] != FXInputMotorSpeeds())
			{
				(void)SetMotors(uint32(User), FXInputMotorSpeeds());
			}
		}
	}
	if (Library != nullptr)
	{
		::FreeLibrary(static_cast<HMODULE>(Library));
		GXInputGetState = nullptr;
		GXInputSetState = nullptr;
	}
}

FXInputMotorSpeeds FXInputForceFeedback::ToMotorSpeeds(const FForceFeedbackValues& Values)
{
	FXInputMotorSpeeds Speeds;
	Speeds.Left = ToSpeed(FMath::Max(Values.LeftLarge, Values.RightLarge));
	Speeds.Right = ToSpeed(FMath::Max(Values.LeftSmall, Values.RightSmall));
	return Speeds;
}

void FXInputForceFeedback::ScanUsers()
{
	int32 Controller = 0;
	for (int32 User = 0; User < MaxUsers; ++User)
	{
		if (!IsConnected(uint32(User)))
		{
			// Gone: what it was sent is forgotten (a pad plugged back is still).
			bSent[User] = false;
			LastSent[User] = FXInputMotorSpeeds();
			continue;
		}
		if (Controller < IInputInterface::MaxControllers)
		{
			UserOfController[Controller++] = User;
		}
	}
	for (; Controller < IInputInterface::MaxControllers; ++Controller)
	{
		UserOfController[Controller] = INDEX_NONE;
	}
}

void FXInputForceFeedback::Update(const FForceFeedbackValues* Values, int32 NumControllers)
{
	if (!IsAvailable())
	{
		return;
	}
	if (UpdatesUntilScan <= 0)
	{
		ScanUsers();
		UpdatesUntilScan = ScanInterval;
	}
	--UpdatesUntilScan;
	for (int32 Controller = 0; Controller < FMath::Min(NumControllers, IInputInterface::MaxControllers); ++Controller)
	{
		const int32 User = UserOfController[Controller];
		if (User == INDEX_NONE)
		{
			continue;
		}
		const FXInputMotorSpeeds Speeds = ToMotorSpeeds(Values[Controller]);
		if (bSent[User] && Speeds == LastSent[User])
		{
			continue;
		}
		if (!SetMotors(uint32(User), Speeds))
		{
			// Pulled out since the scan: look again at the next Update.
			bSent[User] = false;
			LastSent[User] = FXInputMotorSpeeds();
			UpdatesUntilScan = 0;
			continue;
		}
		bSent[User] = true;
		LastSent[User] = Speeds;
	}
}
