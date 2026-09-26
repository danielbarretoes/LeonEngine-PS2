#pragma once

#include "Components/Widget.h"
#include "CoreMinimal.h"
#include "ProgressBar.generated.h"

/** A bar filled left to right by Percent (UE: UProgressBar; its style's images are plain colours in Leon). */
UCLASS()
class UMG_API UProgressBar : public UWidget
{
	GENERATED_BODY()

public:
	UProgressBar(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The filled part, 0 to 1 (UE: SetPercent / GetPercent). */
	void SetPercent(float InPercent)
	{
		Percent = FMath::Clamp(InPercent, 0.0f, 1.0f);
	}
	[[nodiscard]] float GetPercent() const
	{
		return Percent;
	}
	/** UE: SetFillColorAndOpacity (the alpha is not used). */
	void SetFillColorAndOpacity(const FLinearColor& InColor)
	{
		FillColorAndOpacity = InColor;
	}

protected:
	/** The style's background image size in UE. */
	FVector2D ComputeDesiredSize() const override
	{
		return FVector2D(280.0f, 18.0f);
	}
	void OnPaint(FPaintContext& Ctx, const FVector2D& Position, const FVector2D& Size) const override;

private:
	float Percent = 0.0f;
	FLinearColor FillColorAndOpacity = FLinearColor(0.85f, 0.65f, 0.20f);
	FLinearColor BackgroundColor = FLinearColor(0.10f, 0.10f, 0.12f);
};
