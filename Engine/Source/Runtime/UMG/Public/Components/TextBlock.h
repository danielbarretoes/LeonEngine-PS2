#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "Fonts/TextLayout.h"

/** UE-like UTextBlock: simple screen text (status lines, titles). */
class UMG_API UTextBlock : public UUserWidget
{
public:
	void SetText(const FText& InText)
	{
		Text = InText;
	}
	[[nodiscard]] const FText& GetText() const
	{
		return Text;
	}

	void SetColor(const FLinearColor& InColor)
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

	/** Anchor in pixels (top-left origin). For Center justify, X is the screen center of each line. */
	void SetPosition(float InX, float InY)
	{
		X = InX;
		Y = InY;
	}

	/** Places the block in the middle of the viewport (updated each paint from the context size). */
	void SetCenteredOnScreen(bool bEnabled)
	{
		bCenteredOnScreen = bEnabled;
	}

	void NativePaint(FPaintContext& Ctx) override;

private:
	FText Text;
	FLinearColor Color = FLinearColor(1.0f, 0.82f, 0.35f);
	float Scale = HudFontScale;
	ETextJustify Justify = ETextJustify::Center;
	float X = 0.0f;
	float Y = 0.0f;
	bool bCenteredOnScreen = true;
};
