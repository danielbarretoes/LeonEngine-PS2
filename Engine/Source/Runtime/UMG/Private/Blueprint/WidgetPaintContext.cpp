#include "Blueprint/PaintContext.h"
#include "Debug/DebugOverlay.h"

#include <glm/vec3.hpp>

#include <string>

namespace
{
	/** The debug overlay still takes glm colors and std::string text (until the renderer migrates, P6). */
	glm::vec3 ToOverlayColor(const FLinearColor& Color)
	{
		return glm::vec3(Color.R, Color.G, Color.B);
	}
} // namespace

void FPaintContext::DrawLine(float X0, float Y0, float X1, float Y1, const FLinearColor& Color, float Thickness)
{
	Overlay.AddScreenLine(X0, Y0, X1, Y1, ToOverlayColor(Color), Thickness);
}

void FPaintContext::DrawRect(float X, float Y, float W, float H, const FLinearColor& Color)
{
	Overlay.AddScreenRect(X, Y, W, H, ToOverlayColor(Color));
}

void FPaintContext::DrawText(
	const FString& Text, float X, float Y, const FLinearColor& Color, float Scale, ETextJustify Justify)
{
	Overlay.AddScreenText(std::string(*Text), X, Y, ToOverlayColor(Color), Scale, Justify);
}

void FPaintContext::MeasureText(const FString& Text, float Scale, float& OutWidth, float& OutHeight) const
{
	MeasureTextOnly(Text, Scale, OutWidth, OutHeight);
}

void FPaintContext::MeasureTextOnly(const FString& Text, float Scale, float& OutWidth, float& OutHeight)
{
	FDebugOverlay::MeasureText(std::string(*Text), Scale, OutWidth, OutHeight);
}
