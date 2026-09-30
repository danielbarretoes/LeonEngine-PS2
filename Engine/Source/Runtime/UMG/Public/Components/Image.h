#pragma once

#include "Components/Widget.h"
#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "Image.generated.h"

class UTexture2D;

/**
 * A brush stretched over the image's rectangle (UE: UImage): a texture's region tinted by the brush's tint and the
 * image's colour, or a plain colour without a texture. It asks for the brush's image size (or the override).
 */
UCLASS()
class UMG_API UImage : public UWidget
{
	GENERATED_BODY()

public:
	UImage(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** UE: SetColorAndOpacity; the alpha blends the image. */
	void SetColorAndOpacity(const FLinearColor& InColor)
	{
		ColorAndOpacity = InColor;
	}
	[[nodiscard]] const FLinearColor& GetColorAndOpacity() const
	{
		return ColorAndOpacity;
	}
	/** UE: SetBrush / GetBrush. */
	void SetBrush(const FSlateBrush& InBrush)
	{
		Brush = InBrush;
	}
	[[nodiscard]] const FSlateBrush& GetBrush() const
	{
		return Brush;
	}
	/** The brush's texture; bMatchSize takes its size as the image size (UE: SetBrushFromTexture). */
	void SetBrushFromTexture(UTexture2D* Texture, bool bMatchSize = false);
	/** UE: SetBrushResourceObject. */
	void SetBrushResourceObject(UObject* ResourceObject)
	{
		Brush.SetResourceObject(ResourceObject);
	}
	/** The size the image asks for instead of the brush's (UE: SetDesiredSizeOverride). */
	void SetDesiredSizeOverride(const FVector2D& InSize)
	{
		DesiredSizeOverride = InSize;
		bHasDesiredSizeOverride = true;
	}

protected:
	FVector2D ComputeDesiredSize() const override
	{
		return bHasDesiredSizeOverride ? DesiredSizeOverride : Brush.GetImageSize();
	}
	void OnPaint(FPaintContext& Ctx, const FVector2D& Position, const FVector2D& Size) const override;

private:
	UPROPERTY()
	FSlateBrush Brush;
	FLinearColor ColorAndOpacity = FLinearColor::White;
	FVector2D DesiredSizeOverride = FVector2D::ZeroVector;
	bool bHasDesiredSizeOverride = false;
};
