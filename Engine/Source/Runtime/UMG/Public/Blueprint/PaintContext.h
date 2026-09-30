#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Fonts/TextLayout.h"

class FCanvas;
class FHittestGrid;
struct FSlateBrush;

/**
 * Screen-space drawing for the widgets' paint (pixel coords, top-left origin), into the frame's FCanvas
 * (Engine's CanvasTypes.h). UE analogy: FPaintContext / Slate draw elements (lite). Colours are linear RGBA, blended by
 * their alpha. While a user widget paints, it also collects the interactable widgets and where they went (the hit-test
 * grid: hover, clicks and focus navigation).
 */
class UMG_API FPaintContext
{
public:
	explicit FPaintContext(FCanvas& InCanvas);

	[[nodiscard]] int32 GetWidth() const
	{
		return Width;
	}
	[[nodiscard]] int32 GetHeight() const
	{
		return Height;
	}
	[[nodiscard]] FCanvas& GetCanvas() const
	{
		return Canvas;
	}

	void DrawLine(float X0, float Y0, float X1, float Y1, const FLinearColor& Color, float Thickness = 2.0f);
	void DrawRect(float X, float Y, float W, float H, const FLinearColor& Color);
	/** A brush over the rectangle: its texture's UV region tinted by its tint times Color, or the tint alone. */
	void DrawBrush(
		const FSlateBrush& Brush, float X, float Y, float W, float H, const FLinearColor& Color = FLinearColor::White);

	/**
	 * Multiline text in Font; X is the left / center / right of each line per Justify. A shadow when ShadowColor's
	 * alpha is above 0 (ShadowOffset away), an outline when the font's OutlineSettings ask for one.
	 */
	void DrawText(const FSlateFontInfo& Font, const FString& Text, float X, float Y, const FLinearColor& Color,
		ETextJustify Justify = ETextJustify::Left, const FVector2D& ShadowOffset = FVector2D::ZeroVector,
		const FLinearColor& ShadowColor = FLinearColor::Transparent);

	/** The size of multiline text in Font (layout outside a paint too). */
	static void MeasureText(const FSlateFontInfo& Font, const FString& Text, float& OutWidth, float& OutHeight);

	/** The grid the interactable widgets painted now go to; null outside a user widget's paint. */
	[[nodiscard]] FHittestGrid* GetHittestGrid() const
	{
		return HittestGrid;
	}
	void SetHittestGrid(FHittestGrid* InHittestGrid)
	{
		HittestGrid = InHittestGrid;
	}

private:
	FCanvas& Canvas;
	int32 Width = 0;
	int32 Height = 0;
	FHittestGrid* HittestGrid = nullptr;
};
