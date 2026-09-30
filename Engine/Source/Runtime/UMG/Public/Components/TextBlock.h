#pragma once

#include "Components/Widget.h"
#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Fonts/TextLayout.h"
#include "TextBlock.generated.h"

/**
 * Text in a font, justified in its rectangle (UE: UTextBlock), with an optional drop shadow and the font's outline.
 * Its size is measured once per change of text or font (the cached layout), not per frame.
 */
UCLASS()
class UMG_API UTextBlock : public UWidget
{
	GENERATED_BODY()

public:
	UTextBlock(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** UE: SetText / GetText. */
	void SetText(const FText& InText);
	[[nodiscard]] const FText& GetText() const
	{
		return Text;
	}
	/** UE: SetColorAndOpacity (an FSlateColor there); the alpha blends the text. */
	void SetColorAndOpacity(const FLinearColor& InColor)
	{
		ColorAndOpacity = InColor;
	}
	[[nodiscard]] const FLinearColor& GetColorAndOpacity() const
	{
		return ColorAndOpacity;
	}
	/** UE: SetJustification. */
	void SetJustification(ETextJustify InJustification)
	{
		Justification = InJustification;
	}
	/** UE: SetFont / GetFont: the engine's small font (14 pixels) by default. */
	void SetFont(const FSlateFontInfo& InFontInfo);
	[[nodiscard]] const FSlateFontInfo& GetFont() const
	{
		return Font;
	}
	/** A drop shadow ShadowOffset away (UE: SetShadowOffset, SetShadowColorAndOpacity: its alpha 0, none). */
	void SetShadowOffset(const FVector2D& InShadowOffset)
	{
		ShadowOffset = InShadowOffset;
	}
	void SetShadowColorAndOpacity(const FLinearColor& InShadowColorAndOpacity)
	{
		ShadowColorAndOpacity = InShadowColorAndOpacity;
	}

protected:
	FVector2D ComputeDesiredSize() const override;
	void OnPaint(FPaintContext& Ctx, const FVector2D& Position, const FVector2D& Size) const override;

private:
	UPROPERTY()
	FSlateFontInfo Font;
	FText Text;
	FLinearColor ColorAndOpacity = FLinearColor::White;
	ETextJustify Justification = ETextJustify::Left;
	FVector2D ShadowOffset = FVector2D(1.0f, 1.0f);
	FLinearColor ShadowColorAndOpacity = FLinearColor::Transparent;
	/** The text's size in the font, measured when either changed (the text layout's cache). */
	mutable FVector2D CachedSize = FVector2D::ZeroVector;
	mutable bool bLayoutDirty = true;
};
