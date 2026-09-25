#pragma once

#include "CoreMinimal.h"
#include "Fonts/TextLayout.h"

class FDebugOverlay;

/**
 * Immediate screen-space draw for UUserWidget::NativePaint (pixel coords, top-left origin).
 * UE analogy: FPaintContext / Slate draw elements (lite). Colors are linear RGB (alpha unused).
 */
class UMG_API FPaintContext
{
public:
	FPaintContext(FDebugOverlay& InOverlay, int32 FramebufferWidth, int32 FramebufferHeight)
		: Overlay(InOverlay)
		, Width(FramebufferWidth)
		, Height(FramebufferHeight)
	{
	}

	[[nodiscard]] int32 GetWidth() const
	{
		return Width;
	}
	[[nodiscard]] int32 GetHeight() const
	{
		return Height;
	}

	void DrawLine(float X0, float Y0, float X1, float Y1, const FLinearColor& Color, float Thickness = 2.0f);
	void DrawRect(float X, float Y, float W, float H, const FLinearColor& Color);

	/** Draws multiline text; X is the left / center / right of each line per Justify. */
	void DrawText(const FString& Text, float X, float Y, const FLinearColor& Color, float Scale = HudFontScale,
		ETextJustify Justify = ETextJustify::Left);

	void MeasureText(const FString& Text, float Scale, float& OutWidth, float& OutHeight) const;

	/** Measures without a paint context (layout outside NativePaint). */
	static void MeasureTextOnly(const FString& Text, float Scale, float& OutWidth, float& OutHeight);

private:
	FDebugOverlay& Overlay;
	int32 Width = 0;
	int32 Height = 0;
};
