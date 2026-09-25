#include "PS2StatsOverlay.h"

#include "DynamicRHI.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"
#include "Misc/CString.h"
#include "PS2InputInterface.h"
#include "PS2RHI.h"
#include "Stats/StatsOverlay.h"

namespace
{
	uint64 FrameStartCycles = 0;
	uint64 PrevDrawCycles = 0;
	uint64 AccumMicroseconds = 0;
	uint32 AccumFrames = 0;
	float WorkSumMs = 0.0f;
	bool bPrevSelect = false;

	constexpr uint32 LineChars = 32;
	char LineFps[LineChars] = "FPS --";
	char LineRam[LineChars] = "RAM --";
	char LineVram[LineChars] = "VRAM --";
	char LineRes[LineChars] = "RES --";

	// Gamepad widget: 150 x 78 units, 4 units of padding on every side, drawn at 0.75 scale.
	constexpr float GamepadWidgetWidth = 150.0f;
	constexpr float GamepadWidgetPadding = 4.0f;
	constexpr float GamepadWidgetScale = 0.75f;

	// Shared layout (screen px). The gamepad widget scale sets the common padding.
	constexpr float Margin = 10.0f;
	constexpr float Padding = GamepadWidgetPadding * GamepadWidgetScale;
	constexpr float TextScale = 0.5f;
	constexpr float GlyphWidth = 6.0f * TextScale * 2.0f; // advance: 6 cells x 1 px
	constexpr float GlyphHeight = 7.0f * TextScale * 2.0f; // 7 cells x 1 px
	constexpr float LineGap = 2.0f;
	constexpr float StatsMinChars = 16.0f;
	constexpr float PanelAlpha = 0.5f;
	constexpr float IdleAlpha = 0.5f;
	constexpr float LitAlpha = 0.9f;
	constexpr uint64 StatsPeriodMicroseconds = 250000u;

	/** Integer tenths: the EE has no hardware double, keep Snprintf off the float path. */
	void FormatMegabytes(char* Out, uint32 OutSize, const char* Label, uint64 Used, uint64 Total)
	{
		const uint32 UsedTenths = static_cast<uint32>((Used * 10u + 512u * 1024u) / (1024u * 1024u));
		const uint32 TotalTenths = static_cast<uint32>((Total * 10u + 512u * 1024u) / (1024u * 1024u));
		FCString::Snprintf(Out, static_cast<int32>(OutSize), "%s %u.%u/%u.%u MB", Label, UsedTenths / 10u,
			UsedTenths % 10u, TotalTenths / 10u, TotalTenths % 10u);
	}

	void RefreshStats(int32 ScreenWidth, int32 ScreenHeight, float WorkMs)
	{
		const uint64 NowCycles = FPlatformTime::Cycles64();
		if (PrevDrawCycles != 0)
		{
			AccumMicroseconds += FPlatformTime::CyclesToMicroseconds(NowCycles - PrevDrawCycles);
			WorkSumMs += WorkMs;
			++AccumFrames;
		}
		PrevDrawCycles = NowCycles;
		if (AccumMicroseconds < StatsPeriodMicroseconds || AccumFrames == 0)
		{
			return;
		}

		const float Seconds = static_cast<float>(AccumMicroseconds) / 1000000.0f;
		const int32 Fps = static_cast<int32>(static_cast<float>(AccumFrames) / Seconds + 0.5f);
		int32 Tenths = static_cast<int32>(WorkSumMs / static_cast<float>(AccumFrames) * 10.0f + 0.5f);
		if (Tenths < 0)
		{
			Tenths = 0;
		}
		FCString::Sprintf(LineFps, "FPS %d  %d.%d ms", Fps, Tenths / 10, Tenths % 10);

		const FPlatformMemoryStats MemoryStats = FPlatformMemory::GetStats();
		FormatMegabytes(LineRam, sizeof(LineRam), "RAM", MemoryStats.UsedPhysical, MemoryStats.TotalPhysical);
		if (GDynamicRHI != nullptr)
		{
			const FRHIGPUMemoryStats GPUStats = GDynamicRHI->GetGPUMemoryStats();
			if (GPUStats.bValid && GPUStats.bReportsUsage)
			{
				FormatMegabytes(LineVram, sizeof(LineVram), "VRAM", GPUStats.UsedBytes, GPUStats.BudgetBytes);
			}
		}
		FCString::Sprintf(LineRes, "RES %dX%d", ScreenWidth, ScreenHeight);

		AccumMicroseconds = 0;
		AccumFrames = 0;
		WorkSumMs = 0.0f;
	}

	struct FHudLine
	{
		const char* Text;
		float R;
		float G;
		float B;
	};

	void DrawStatsPanel(int32 ScreenWidth, int32 ScreenHeight)
	{
		FHudLine Lines[4 + FStatsOverlay::MaxOnScreenMessages] = {
			{LineFps, 0.95f, 0.95f, 0.75f},
			{LineRam, 0.75f, 0.95f, 0.80f},
			{LineVram, 0.95f, 0.80f, 0.90f},
			{LineRes, 0.80f, 0.90f, 0.95f},
		};
		uint32 Count = 4;
		for (int32 Key = 0; Key < FStatsOverlay::MaxOnScreenMessages; ++Key)
		{
			const char* Message = FStatsOverlay::GetOnScreenDebugMessage(Key);
			if (Message[0] != '\0')
			{
				Lines[Count++] = {Message, 0.70f, 0.85f, 0.85f};
			}
		}

		int32 Chars = static_cast<int32>(StatsMinChars);
		for (uint32 Index = 0; Index < Count; ++Index)
		{
			const int32 Length = FCString::Strlen(Lines[Index].Text);
			Chars = Length > Chars ? Length : Chars;
		}

		const float Left = -static_cast<float>(ScreenWidth) * 0.5f + Margin;
		const float Top = -static_cast<float>(ScreenHeight) * 0.5f + Margin;
		const float Width = Padding * 2.0f + static_cast<float>(Chars) * GlyphWidth - 1.0f;
		const float Height =
			Padding * 2.0f + static_cast<float>(Count) * GlyphHeight + static_cast<float>(Count - 1u) * LineGap;
		(void)FPS2RHI::DrawUnlitRectAlpha(Left, Top, Left + Width, Top + Height, 0.02f, 0.03f, 0.05f, PanelAlpha);

		float Y = Top + Padding;
		for (uint32 Index = 0; Index < Count; ++Index)
		{
			FPS2RHI::DrawDebugText(
				Left + Padding, Y, Lines[Index].Text, Lines[Index].R, Lines[Index].G, Lines[Index].B, TextScale);
			Y += GlyphHeight + LineGap;
		}
	}

	/** DualShock widget: buttons light while held, sticks from raw bytes (no dead zone). */
	void DrawGamepadWidget(float X, float Y, float Scale, const FPS2InputInterface* Pad)
	{
		const bool bLive = Pad != nullptr && Pad->IsGamepadConnected();
		const bool bPortOpen = Pad != nullptr && Pad->IsPortOpen();
		uint8 LeftX = 128;
		uint8 LeftY = 128;
		uint8 RightX = 128;
		uint8 RightY = 128;
		if (bLive)
		{
			Pad->GetRawSticks(LeftX, LeftY, RightX, RightY);
		}

		const auto Box = [&](float X0, float Y0, float X1, float Y1, float R, float G, float B, float A)
		{
			(void)FPS2RHI::DrawUnlitRectAlpha(
				X + X0 * Scale, Y + Y0 * Scale, X + X1 * Scale, Y + Y1 * Scale, R, G, B, A);
		};
		const auto IsDown = [&](EKeys Key) { return bLive && Pad->IsGamepadKeyDown(Key); };
		// Lit colour while held, dim grey otherwise.
		const auto Button = [&](EKeys Key, float X0, float Y0, float X1, float Y1, float R, float G, float B)
		{
			if (IsDown(Key))
			{
				Box(X0, Y0, X1, Y1, R, G, B, LitAlpha);
			}
			else
			{
				Box(X0, Y0, X1, Y1, 0.55f, 0.58f, 0.64f, IdleAlpha);
			}
		};
		// Screen y grows down like the raw vertical axis.
		const auto Stick = [&](uint8 RawX, uint8 RawY, EKeys ClickKey, float CenterX, float CenterY)
		{
			if (IsDown(ClickKey))
			{
				Box(CenterX - 9.0f, CenterY - 9.0f, CenterX + 9.0f, CenterY + 9.0f, 0.95f, 0.75f, 0.2f, IdleAlpha);
			}
			else
			{
				Box(CenterX - 9.0f, CenterY - 9.0f, CenterX + 9.0f, CenterY + 9.0f, 0.3f, 0.32f, 0.38f, IdleAlpha);
			}
			const float DeltaX = (static_cast<float>(RawX) - 128.0f) / 128.0f * 7.0f;
			const float DeltaY = (static_cast<float>(RawY) - 128.0f) / 128.0f * 7.0f;
			const bool bMoved = DeltaX * DeltaX + DeltaY * DeltaY > 1.0f;
			Box(CenterX + DeltaX - 2.0f, CenterY + DeltaY - 2.0f, CenterX + DeltaX + 2.0f, CenterY + DeltaY + 2.0f,
				bMoved ? 1.0f : 0.8f, bMoved ? 0.85f : 0.8f, bMoved ? 0.2f : 0.85f, LitAlpha);
		};

		// Layout in widget units (150 x 78), mirrored around x = 75.
		Box(0.0f, 0.0f, 150.0f, 78.0f, 0.02f, 0.03f, 0.05f, PanelAlpha);

		// Status LED (top centre): green = reading, orange = port open, red = no pad.
		if (bLive)
		{
			Box(71.0f, 4.0f, 79.0f, 12.0f, 0.2f, 0.95f, 0.3f, LitAlpha);
		}
		else if (bPortOpen)
		{
			Box(71.0f, 4.0f, 79.0f, 12.0f, 1.0f, 0.6f, 0.1f, LitAlpha);
		}
		else
		{
			Box(71.0f, 4.0f, 79.0f, 12.0f, 0.95f, 0.15f, 0.15f, LitAlpha);
		}

		constexpr float WhiteR = 0.95f;
		constexpr float WhiteG = 0.9f;
		constexpr float WhiteB = 0.4f;
		Button(EKeys::Gamepad_LeftTrigger, 4.0f, 4.0f, 30.0f, 8.0f, WhiteR, WhiteG, WhiteB);
		Button(EKeys::Gamepad_LeftShoulder, 4.0f, 10.0f, 30.0f, 14.0f, WhiteR, WhiteG, WhiteB);
		Button(EKeys::Gamepad_RightTrigger, 120.0f, 4.0f, 146.0f, 8.0f, WhiteR, WhiteG, WhiteB);
		Button(EKeys::Gamepad_RightShoulder, 120.0f, 10.0f, 146.0f, 14.0f, WhiteR, WhiteG, WhiteB);

		Button(EKeys::Gamepad_DPad_Up, 13.0f, 20.0f, 21.0f, 28.0f, WhiteR, WhiteG, WhiteB);
		Button(EKeys::Gamepad_DPad_Down, 13.0f, 38.0f, 21.0f, 46.0f, WhiteR, WhiteG, WhiteB);
		Button(EKeys::Gamepad_DPad_Left, 4.0f, 29.0f, 12.0f, 37.0f, WhiteR, WhiteG, WhiteB);
		Button(EKeys::Gamepad_DPad_Right, 22.0f, 29.0f, 30.0f, 37.0f, WhiteR, WhiteG, WhiteB);

		Button(EKeys::Gamepad_Special_Left, 57.0f, 31.0f, 69.0f, 35.0f, WhiteR, WhiteG, WhiteB);
		Button(EKeys::Gamepad_Special_Right, 81.0f, 31.0f, 93.0f, 35.0f, WhiteR, WhiteG, WhiteB);

		Button(EKeys::Gamepad_FaceButton_Top, 129.0f, 20.0f, 137.0f, 28.0f, 0.2f, 0.9f, 0.5f);
		Button(EKeys::Gamepad_FaceButton_Bottom, 129.0f, 38.0f, 137.0f, 46.0f, 0.35f, 0.55f, 1.0f);
		Button(EKeys::Gamepad_FaceButton_Left, 120.0f, 29.0f, 128.0f, 37.0f, 0.95f, 0.45f, 0.85f);
		Button(EKeys::Gamepad_FaceButton_Right, 138.0f, 29.0f, 146.0f, 37.0f, 0.95f, 0.3f, 0.3f);

		Stick(LeftX, LeftY, EKeys::Gamepad_LeftThumbstick, 55.0f, 65.0f);
		Stick(RightX, RightY, EKeys::Gamepad_RightThumbstick, 95.0f, 65.0f);
	}
} // namespace

void FPS2StatsOverlay::MarkFrameStart()
{
	FrameStartCycles = FPlatformTime::Cycles64();
	FPS2RHI::BeginDraw3DStatsFrame();
}

void FPS2StatsOverlay::Draw(int32 ScreenWidth, int32 ScreenHeight, IInputInterface* InputInterface)
{
	// Game work this frame, measured before the overlay adds its own draws.
	const uint64 NowCycles = FPlatformTime::Cycles64();
	const float WorkMs = FrameStartCycles != 0
		? static_cast<float>(FPlatformTime::CyclesToMicroseconds(NowCycles - FrameStartCycles)) / 1000.0f
		: 0.0f;

	const bool bSelect = InputInterface != nullptr && InputInterface->IsGamepadKeyDown(EKeys::Gamepad_Special_Left);
	if (bSelect && !bPrevSelect)
	{
		FStatsOverlay::CycleVisibility();
	}
	bPrevSelect = bSelect;

	RefreshStats(ScreenWidth, ScreenHeight, WorkMs);
	if (FStatsOverlay::IsStatsVisible())
	{
		DrawStatsPanel(ScreenWidth, ScreenHeight);
	}
	if (FStatsOverlay::IsGamepadWidgetVisible())
	{
		const float X = static_cast<float>(ScreenWidth) * 0.5f - Margin - GamepadWidgetWidth * GamepadWidgetScale;
		const float Y = -static_cast<float>(ScreenHeight) * 0.5f + Margin;
		DrawGamepadWidget(X, Y, GamepadWidgetScale, FPS2InputInterface::Get());
	}
}
