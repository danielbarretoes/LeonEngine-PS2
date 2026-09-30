#include "Debug/DebugOverlay.h"

#include "CanvasTypes.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"

namespace
{

	constexpr float MarginX = 10.0f;
	constexpr float MarginY = 10.0f;
	constexpr float FadeTailSeconds = 0.5f;
	constexpr int32 MaxOnScreenMessages = 12;
	/** The overlay draws behind the HUD's widgets (higher depth sort keys are drawn first). */
	constexpr int32 OverlayDepthSortKey = 1;

	[[nodiscard]] FColor ColorWithAlpha(const FLinearColor& Rgb, float Alpha)
	{
		return FColor(static_cast<uint8>(FMath::Clamp(Rgb.R, 0.0f, 1.0f) * 255.0f),
			static_cast<uint8>(FMath::Clamp(Rgb.G, 0.0f, 1.0f) * 255.0f),
			static_cast<uint8>(FMath::Clamp(Rgb.B, 0.0f, 1.0f) * 255.0f),
			static_cast<uint8>(FMath::Clamp(Alpha, 0.0f, 1.0f) * 255.0f));
	}

} // namespace

void FDebugOverlay::SetText(const FString& InText)
{
	Text = InText;
}

void FDebugOverlay::SetBottomLeftText(const FString& InText)
{
	BottomLeftText = InText;
}

void FDebugOverlay::SetCenterText(const FString& InText)
{
	CenterText = InText;
}

void FDebugOverlay::SetRightText(const FString& InText)
{
	RightText = InText;
}

void FDebugOverlay::SetRightTextOriginY(float OriginY)
{
	RightTextOriginY = OriginY;
}

void FDebugOverlay::AddOnScreenDebugMessage(const FString& Message, float DisplaySeconds, const FLinearColor& InColor)
{
	if (Message.IsEmpty())
	{
		return;
	}
	const float LocalDuration = DisplaySeconds > 0.0f ? DisplaySeconds : 0.01f;
	OnScreenMessages.Add(FOnScreenMessage{Message, LocalDuration, LocalDuration, InColor});
	if (OnScreenMessages.Num() > MaxOnScreenMessages)
	{
		OnScreenMessages.RemoveAt(0, OnScreenMessages.Num() - MaxOnScreenMessages);
	}
}

void FDebugOverlay::TickOnScreenMessages(float DeltaTime)
{
	if (OnScreenMessages.Num() == 0)
	{
		return;
	}
	for (FOnScreenMessage& Msg : OnScreenMessages)
	{
		Msg.TimeRemaining -= DeltaTime;
	}
	OnScreenMessages.RemoveAll([](const FOnScreenMessage& Msg) { return Msg.TimeRemaining <= 0.0f; });
}

void FDebugOverlay::Clear()
{
	Text.Empty();
	BottomLeftText.Empty();
	CenterText.Empty();
	RightText.Empty();
	RightTextOriginY = MarginY;
	OnScreenMessages.Empty();
}

float FDebugOverlay::GetRightTextBottom() const
{
	if (RightText.IsEmpty())
	{
		return RightTextOriginY;
	}
	float Width = 0.0f;
	float Height = 0.0f;
	FCanvas::MeasureText(UEngine::GetSmallFont(), RightText, Width, Height);
	return RightTextOriginY + Height;
}

void FDebugOverlay::Draw(FCanvas& Canvas) const
{
	const float FramebufferWidth = static_cast<float>(Canvas.GetSizeX());
	const float FramebufferHeight = static_cast<float>(Canvas.GetSizeY());
	constexpr FColor LeftColor(230, 235, 240);
	constexpr FColor BottomLeftColor(200, 210, 220);
	constexpr FColor CenterColor(255, 210, 90);
	constexpr FColor RightColor(240, 240, 245);
	// The overlay's text is in the engine's small font, its lines the font's line height apart.
	const UFont* Font = UEngine::GetSmallFont();
	if (Font == nullptr)
	{
		return;
	}
	const float LineStepY = Font->GetLineHeight();

	Canvas.PushDepthSortKey(OverlayDepthSortKey);

	// Top-left HUD block (FPS / tools).
	Canvas.DrawText(Font, Text, MarginX, MarginY, LeftColor);

	if (!BottomLeftText.IsEmpty())
	{
		int32 LineCount = 1;
		for (const ANSICHAR* C = *BottomLeftText; *C != '\0'; ++C)
		{
			if (*C == '\n')
			{
				++LineCount;
			}
		}
		const float OriginY = FramebufferHeight - MarginY - (LineStepY * static_cast<float>(LineCount));
		Canvas.DrawText(Font, BottomLeftText, MarginX, OriginY, BottomLeftColor);
	}

	if (!CenterText.IsEmpty())
	{
		float BlockWidth = 0.0f;
		float BlockH = 0.0f;
		FCanvas::MeasureText(Font, CenterText, BlockWidth, BlockH);
		// Vertically center; clamp so short windows still keep the block on-screen.
		float OriginY = (FramebufferHeight - BlockH) * 0.5f;
		OriginY = FMath::Clamp(OriginY, MarginY, FMath::Max(MarginY, FramebufferHeight - BlockH - MarginY));
		// Each line centered — long Main Menu hints must not left-bias short rows.
		Canvas.DrawText(Font, CenterText, FramebufferWidth * 0.5f, OriginY, CenterColor, ETextJustify::Center);
	}

	// Right-aligned block (stats top-right / level chrome bottom-right).
	Canvas.DrawText(Font, RightText, FramebufferWidth - MarginX, RightTextOriginY, RightColor, ETextJustify::Right);

	// Top-left debug console: newest at the fixed top slot; older lines shift down (+Y).
	float LocalY = MarginY;
	for (int32 Index = OnScreenMessages.Num() - 1; Index >= 0; --Index)
	{
		const FOnScreenMessage& Msg = OnScreenMessages[Index];
		float Alpha = 1.0f;
		if (Msg.TimeRemaining < FadeTailSeconds)
		{
			Alpha = FMath::Clamp(Msg.TimeRemaining / FadeTailSeconds, 0.0f, 1.0f);
		}
		Canvas.DrawText(Font, Msg.Text, MarginX, LocalY, ColorWithAlpha(Msg.Color, Alpha));
		LocalY += LineStepY;
		if (LocalY > FramebufferHeight - MarginY)
		{
			break;
		}
	}

	Canvas.PopDepthSortKey();
}
