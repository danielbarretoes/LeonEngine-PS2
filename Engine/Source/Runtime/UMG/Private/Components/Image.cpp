#include "Components/Image.h"

UImage::UImage(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UImage::OnPaint(FPaintContext& Ctx, const FVector2D& Position, const FVector2D& Size) const
{
	Ctx.DrawRect(Position.X, Position.Y, Size.X, Size.Y, ColorAndOpacity);
}
