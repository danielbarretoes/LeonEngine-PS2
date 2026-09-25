#include "Components/TextBlock.h"

void UTextBlock::NativePaint(FPaintContext& Ctx)
{
	if (Text.IsEmpty())
	{
		return;
	}
	const FString& String = Text.ToString();
	float LocalX = X;
	float LocalY = Y;
	if (bCenteredOnScreen)
	{
		float W = 0.0f;
		float H = 0.0f;
		Ctx.MeasureText(String, Scale, W, H);
		LocalX = static_cast<float>(Ctx.GetWidth()) * 0.5f;
		LocalY = FMath::Clamp((static_cast<float>(Ctx.GetHeight()) - H) * 0.5f, 10.0f,
			FMath::Max(10.0f, static_cast<float>(Ctx.GetHeight()) - H - 10.0f));
		Justify = ETextJustify::Center;
	}
	Ctx.DrawText(String, LocalX, LocalY, Color, Scale, Justify);
}
