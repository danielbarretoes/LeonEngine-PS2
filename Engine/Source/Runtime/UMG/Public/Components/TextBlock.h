#pragma once

#include "Blueprint/UserWidget.h"
#include "Fonts/TextLayout.h"

#include <glm/vec3.hpp>

#include <string>

/// Unreal-like UTextBlock: simple screen text (status lines, titles).
class UMG_API UTextBlock : public UUserWidget
{
public:
	void SetText(std::string InText)
	{
		Text = std::move(InText);
	}
	[[nodiscard]] const std::string& GetText() const
	{
		return Text;
	}

	void SetColor(const glm::vec3& InColor)
	{
		Color = InColor;
	}
	void SetScale(float InScale)
	{
		Scale = InScale;
	}
	void SetJustify(ETextJustify InJustify)
	{
		Justify = InJustify;
	}

	/// Anchor in pixels (top-left origin). For Center justify, X is screen center of each line.
	void SetPosition(float InX, float InY)
	{
		X = InX;
		Y = InY;
	}

	/// Place block in the middle of the viewport (updates each paint from ctx size).
	void SetCenteredOnScreen(bool bEnabled)
	{
		bCenteredOnScreen = bEnabled;
	}

	void NativePaint(FPaintContext& Ctx) override;

private:
	std::string Text;
	glm::vec3 Color{1.0f, 0.82f, 0.35f};
	float Scale = HudFontScale;
	ETextJustify Justify = ETextJustify::Center;
	float X = 0.0f;
	float Y = 0.0f;
	bool bCenteredOnScreen = true;
};
