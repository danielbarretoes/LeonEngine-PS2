#pragma once

#include "Blueprint/UserWidget.h"
#include "Fonts/TextLayout.h"

#include <glm/vec3.hpp>

#include <string>

/// Centered outlined interact hint ("[F] Open Door [750]"). Empty Prompt skips paint.
class UMG_API UInteractionPromptWidget : public UUserWidget
{
public:
	std::string Prompt;

	glm::vec3 Color{0.95f, 0.9f, 0.45f};
	float Scale = 2.4f;
	/// Vertical placement as a fraction of viewport height (0 = top, 1 = bottom).
	float NormalizedY = 0.62f;
	ETextJustify Justify = ETextJustify::Center;

	void NativePaint(FPaintContext& Ctx) override;
};
