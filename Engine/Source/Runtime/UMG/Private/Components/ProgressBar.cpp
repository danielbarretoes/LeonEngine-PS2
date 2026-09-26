#include "Components/ProgressBar.h"

UProgressBar::UProgressBar(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UProgressBar::OnPaint(FPaintContext& Ctx, const FVector2D& Position, const FVector2D& Size) const
{
	Ctx.DrawRect(Position.X, Position.Y, Size.X, Size.Y, BackgroundColor);
	if (Percent > 0.0f)
	{
		Ctx.DrawRect(Position.X, Position.Y, Size.X * Percent, Size.Y, FillColorAndOpacity);
	}
}
