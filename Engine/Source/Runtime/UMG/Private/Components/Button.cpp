#include "Components/Button.h"

void UButton::MeasureDesiredSize(float& OutW, float& OutH) const
{
	float TextW = 0.0f;
	float TextH = 0.0f;
	// Measuring needs no draw target.
	FPaintContext::MeasureTextOnly(Label.IsEmpty() ? FString(" ") : Label.ToString(), HudFontScale, TextW, TextH);
	constexpr float PadX = 24.0f;
	constexpr float PadY = 10.0f;
	OutW = TextW + PadX * 2.0f;
	OutH = FMath::Max(TextH, HudLineHeight) + PadY * 2.0f;
}

bool UButton::Contains(float FbX, float FbY) const
{
	return FbX >= X && FbX <= X + W && FbY >= Y && FbY <= Y + H;
}

void UButton::NativePaint(FPaintContext& Ctx)
{
	if (!IsVisible())
	{
		return;
	}
	FLinearColor Bg = BackgroundColor;
	if (!bEnabled)
	{
		Bg = BackgroundColor * 0.55f;
	}
	else if (bSelected)
	{
		Bg = SelectedBackgroundColor;
	}
	else if (bHovered)
	{
		Bg = HoverBackgroundColor;
	}
	Ctx.DrawRect(X, Y, W, H, Bg);

	// Thin top highlight for selected / hover (Unreal button chrome lite).
	if (bEnabled && (bSelected || bHovered))
	{
		const FLinearColor Edge = bSelected ? FLinearColor(1.0f, 0.82f, 0.35f) : FLinearColor(0.55f, 0.50f, 0.35f);
		Ctx.DrawRect(X, Y, W, 2.0f, Edge);
	}

	float TextW = 0.0f;
	float TextH = 0.0f;
	const FString& LabelString = Label.ToString();
	Ctx.MeasureText(LabelString, HudFontScale, TextW, TextH);
	const float TextX = X + W * 0.5f;
	const float TextY = Y + (H - TextH) * 0.5f;
	const FLinearColor Color = bEnabled ? TextColor : DisabledTextColor;
	Ctx.DrawText(LabelString, TextX, TextY, Color, HudFontScale, ETextJustify::Center);
}
