#include "Components/Border.h"

UBorder::UBorder(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FVector2D UBorder::ComputeDesiredSize() const
{
	const UWidget* Content = GetContent();
	return Padding.GetDesiredSize() + (Content != nullptr ? Content->GetDesiredSize() : FVector2D::ZeroVector);
}

void UBorder::OnPaint(FPaintContext& Ctx, const FVector2D& Position, const FVector2D& Size) const
{
	Ctx.DrawRect(Position.X, Position.Y, Size.X, Size.Y, BrushColor);
	if (const UWidget* Content = GetContent())
	{
		Content->Paint(Ctx, Position + FVector2D(Padding.Left, Padding.Top), Size - Padding.GetDesiredSize());
	}
}
