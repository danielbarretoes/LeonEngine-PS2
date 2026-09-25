#include "Blueprint/PaintContext.h"
#include "CanvasTypes.h"

FPaintContext::FPaintContext(FCanvas& InCanvas)
	: Canvas(InCanvas)
	, Width(InCanvas.GetSizeX())
	, Height(InCanvas.GetSizeY())
{
}

void FPaintContext::DrawLine(float X0, float Y0, float X1, float Y1, const FLinearColor& Color, float Thickness)
{
	Canvas.DrawLine(X0, Y0, X1, Y1, Color, Thickness);
}

void FPaintContext::DrawRect(float X, float Y, float W, float H, const FLinearColor& Color)
{
	Canvas.DrawTile(X, Y, W, H, Color);
}

void FPaintContext::DrawText(
	const FString& Text, float X, float Y, const FLinearColor& Color, float Scale, ETextJustify Justify)
{
	Canvas.DrawText(Text, X, Y, Color, Scale, Justify);
}

void FPaintContext::MeasureText(const FString& Text, float Scale, float& OutWidth, float& OutHeight) const
{
	MeasureTextOnly(Text, Scale, OutWidth, OutHeight);
}

void FPaintContext::MeasureTextOnly(const FString& Text, float Scale, float& OutWidth, float& OutHeight)
{
	FCanvas::MeasureText(Text, Scale, OutWidth, OutHeight);
}
