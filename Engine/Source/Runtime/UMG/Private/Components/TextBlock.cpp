#include "Components/TextBlock.h"

UTextBlock::UTextBlock(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UTextBlock::SetText(const FText& InText)
{
	if (InText.ToString() != Text.ToString())
	{
		Text = InText;
		bLayoutDirty = true;
	}
}

void UTextBlock::SetFont(const FSlateFontInfo& InFontInfo)
{
	if (!(InFontInfo == Font))
	{
		Font = InFontInfo;
		bLayoutDirty = true;
	}
}

FVector2D UTextBlock::ComputeDesiredSize() const
{
	if (bLayoutDirty)
	{
		CachedSize = FVector2D::ZeroVector;
		if (!Text.IsEmpty())
		{
			FPaintContext::MeasureText(Font, Text.ToString(), CachedSize.X, CachedSize.Y);
		}
		bLayoutDirty = false;
	}
	return CachedSize;
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
	Ctx.DrawText(
		Font, Text.ToString(), X, Position.Y, ColorAndOpacity, Justification, ShadowOffset, ShadowColorAndOpacity);
}
