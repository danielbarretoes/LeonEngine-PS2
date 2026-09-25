#include "Components/Image.h"

UImage::UImage(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UImage::NativePaint(FPaintContext& Ctx)
{
	if (!IsVisible())
	{
		return;
	}

	float LocalX = X;
	float LocalY = Y;
	float LocalW = W;
	float LocalH = H;
	if (bFillScreen)
	{
		LocalX = 0.0f;
		LocalY = 0.0f;
		LocalW = static_cast<float>(Ctx.GetWidth());
		LocalH = static_cast<float>(Ctx.GetHeight());
	}

	Ctx.DrawRect(LocalX, LocalY, LocalW, LocalH, Color);
	if (bDrawBorder && !bFillScreen)
	{
		Ctx.DrawRect(LocalX, LocalY, LocalW, 2.0f, BorderColor);
		Ctx.DrawRect(LocalX, LocalY + LocalH - 2.0f, LocalW, 2.0f, BorderColor);
		Ctx.DrawRect(LocalX, LocalY, 2.0f, LocalH, BorderColor);
		Ctx.DrawRect(LocalX + LocalW - 2.0f, LocalY, 2.0f, LocalH, BorderColor);
	}
}
