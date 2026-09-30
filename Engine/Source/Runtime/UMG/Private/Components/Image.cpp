#include "Components/Image.h"

#include "Engine/Texture2D.h"

UImage::UImage(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UImage::SetBrushFromTexture(UTexture2D* Texture, bool bMatchSize)
{
	Brush.SetResourceObject(Texture);
	if (bMatchSize && Texture != nullptr)
	{
		Brush.SetImageSize(FVector2D(float(Texture->GetSizeX()), float(Texture->GetSizeY())));
	}
}

void UImage::OnPaint(FPaintContext& Ctx, const FVector2D& Position, const FVector2D& Size) const
{
	Ctx.DrawBrush(Brush, Position.X, Position.Y, Size.X, Size.Y, ColorAndOpacity);
}
