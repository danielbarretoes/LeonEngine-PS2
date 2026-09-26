#pragma once

#include "Components/Widget.h"
#include "CoreMinimal.h"
#include "Image.generated.h"

/** A tinted rectangle (UE: UImage; Leon's brush has no texture, so the image is its colour). */
UCLASS()
class UMG_API UImage : public UWidget
{
	GENERATED_BODY()

public:
	UImage(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** UE: SetColorAndOpacity (the alpha is not used). */
	void SetColorAndOpacity(const FLinearColor& InColor)
	{
		ColorAndOpacity = InColor;
	}
	[[nodiscard]] const FLinearColor& GetColorAndOpacity() const
	{
		return ColorAndOpacity;
	}
	/** The size the image asks for (UE: SetDesiredSizeOverride; the brush's image size otherwise). */
	void SetDesiredSizeOverride(const FVector2D& InSize)
	{
		DesiredSizeOverride = InSize;
	}

protected:
	FVector2D ComputeDesiredSize() const override
	{
		return DesiredSizeOverride;
	}
	void OnPaint(FPaintContext& Ctx, const FVector2D& Position, const FVector2D& Size) const override;

private:
	FLinearColor ColorAndOpacity = FLinearColor::White;
	/** UE's default brush: 32 by 32. */
	FVector2D DesiredSizeOverride = FVector2D(32.0f, 32.0f);
};
