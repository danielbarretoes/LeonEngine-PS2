#include "Components/TextBlock.h"

UTextBlock::UTextBlock(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FVector2D UTextBlock::ComputeDesiredSize() const
{
	if (Text.IsEmpty())
	{
		return FVector2D::ZeroVector;
	}
	float Width = 0.0f;
	float Height = 0.0f;
	FPaintContext::MeasureTextOnly(Text.ToString(), HudFontScale, Width, Height);
	return FVector2D(Width, Height);
}

void UTextBlock::OnPaint(FPaintContext& Ctx, const FVector2D& Position, const FVector2D& Size) const
{
	if (Text.IsEmpty())
	{
		return;
	}
	const float X = Justification == ETextJustify::Center ? Position.X + (Size.X * 0.5f)
		: Justification == ETextJustify::Right            ? Position.X + Size.X
														  : Position.X;
	Ctx.DrawText(Text.ToString(), X, Position.Y, ColorAndOpacity, HudFontScale, Justification);
}
