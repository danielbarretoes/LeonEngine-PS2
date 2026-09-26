#pragma once

#include "Components/ContentWidget.h"
#include "CoreMinimal.h"
#include "Layout/Margin.h"
#include "Border.generated.h"

/** A filled rectangle around its child, with padding (UE: UBorder; its brush is a plain colour in Leon). */
UCLASS()
class UMG_API UBorder : public UContentWidget
{
	GENERATED_BODY()

public:
	UBorder(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** UE: SetBrushColor. */
	void SetBrushColor(const FLinearColor& InBrushColor)
	{
		BrushColor = InBrushColor;
	}
	[[nodiscard]] const FLinearColor& GetBrushColor() const
	{
		return BrushColor;
	}
	/** The space between the border's edge and the child (UE: SetPadding). */
	void SetPadding(const FMargin& InPadding)
	{
		Padding = InPadding;
	}
	[[nodiscard]] const FMargin& GetPadding() const
	{
		return Padding;
	}

protected:
	FVector2D ComputeDesiredSize() const override;
	void OnPaint(FPaintContext& Ctx, const FVector2D& Position, const FVector2D& Size) const override;

private:
	FLinearColor BrushColor = FLinearColor::White;
	/** UE's default: 4 by 2. */
	FMargin Padding = FMargin(4.0f, 2.0f);
};
