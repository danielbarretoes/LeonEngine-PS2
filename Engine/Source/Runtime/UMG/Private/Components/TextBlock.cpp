#include "Components/TextBlock.h"

#include <algorithm>

void UTextBlock::NativePaint(FPaintContext& Ctx)
{
	if (Text.empty())
	{
		return;
	}
	float LocalX = X;
	float LocalY = Y;
	if (bCenteredOnScreen)
	{
		float W = 0.0f;
		float H = 0.0f;
		Ctx.MeasureText(Text, Scale, W, H);
		LocalX = static_cast<float>(Ctx.GetWidth()) * 0.5f;
		LocalY = std::clamp((static_cast<float>(Ctx.GetHeight()) - H) * 0.5f, 10.0f,
			std::max(10.0f, static_cast<float>(Ctx.GetHeight()) - H - 10.0f));
		Justify = ETextJustify::Center;
	}
	Ctx.DrawText(Text, LocalX, LocalY, Color, Scale, Justify);
}
