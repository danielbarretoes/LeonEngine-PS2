#include "Blueprint/PaintContext.h"
#include "CanvasTypes.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateBrush.h"

const UFont* FSlateFontInfo::GetFont() const
{
	if (const UFont* Font = Cast<UFont>(FontObject))
	{
		return Font;
	}
	return Size <= 10 ? UEngine::GetTinyFont()
		: Size <= 14  ? UEngine::GetSmallFont()
		: Size <= 20  ? UEngine::GetMediumFont()
					  : UEngine::GetLargeFont();
}

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

void FPaintContext::DrawBrush(const FSlateBrush& Brush, float X, float Y, float W, float H, const FLinearColor& Color)
{
	if (Brush.DrawAs == ESlateBrushDrawType::NoDrawType)
	{
		return;
	}
	const FLinearColor Tint = Brush.TintColor * Color;
	if (Tint.A <= 0.0f)
	{
		return;
	}
	const UTexture2D* Texture = Cast<UTexture2D>(Brush.GetResourceObject());
	if (Texture == nullptr)
	{
		Canvas.DrawTile(X, Y, W, H, Tint);
		return;
	}
	const FBox2D& UV = Brush.GetUVRegion();
	Canvas.DrawTile(X, Y, W, H, UV.Min.X, UV.Min.Y, UV.Max.X - UV.Min.X, UV.Max.Y - UV.Min.Y, Tint, Texture);
}

void FPaintContext::DrawText(const FSlateFontInfo& Font, const FString& Text, float X, float Y,
	const FLinearColor& Color, ETextJustify Justify, const FVector2D& ShadowOffset, const FLinearColor& ShadowColor)
{
	// Straight to the canvas: the text is copied onto the frame's stack only (no FText, no heap).
	const bool bShadow = ShadowOffset.X != 0.0f || ShadowOffset.Y != 0.0f;
	Canvas.DrawText(Font.GetFont(), Text, X, Y, Color, Justify, ShadowOffset,
		bShadow ? ShadowColor : FLinearColor::Transparent,
		Font.OutlineSettings.OutlineSize > 0 ? Font.OutlineSettings.OutlineColor : FLinearColor::Transparent);
}

void FPaintContext::MeasureText(const FSlateFontInfo& Font, const FString& Text, float& OutWidth, float& OutHeight)
{
	FCanvas::MeasureText(Font.GetFont(), Text, OutWidth, OutHeight);
}
