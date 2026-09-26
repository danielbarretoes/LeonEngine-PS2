#pragma once

#include "Components/Widget.h"
#include "CoreMinimal.h"
#include "Fonts/TextLayout.h"
#include "TextBlock.generated.h"

/** Text in Leon's HUD font, justified in its rectangle (UE: UTextBlock). */
UCLASS()
class UMG_API UTextBlock : public UWidget
{
	GENERATED_BODY()

public:
	UTextBlock(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** UE: SetText / GetText. */
	void SetText(const FText& InText)
	{
		Text = InText;
	}
	[[nodiscard]] const FText& GetText() const
	{
		return Text;
	}
	/** UE: SetColorAndOpacity (an FSlateColor there; the alpha is not used). */
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

protected:
	FVector2D ComputeDesiredSize() const override;
	void OnPaint(FPaintContext& Ctx, const FVector2D& Position, const FVector2D& Size) const override;

private:
	FText Text;
	FLinearColor ColorAndOpacity = FLinearColor::White;
	ETextJustify Justification = ETextJustify::Left;
};
