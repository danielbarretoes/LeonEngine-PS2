#pragma once

#include "Components/PanelWidget.h"
#include "CoreMinimal.h"
#include "HorizontalBox.generated.h"

class UHorizontalBoxSlot;

/**
 * Lays its children left to right, each in its slot's padding (UE: UHorizontalBox): the Automatic slots at their
 * desired widths, the Fill slots sharing the width left by their values; as tall as the box, each aligned in it by its
 * slot.
 */
UCLASS()
class UMG_API UHorizontalBox : public UPanelWidget
{
	GENERATED_BODY()

public:
	UHorizontalBox(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** UE: AddChildToHorizontalBox. */
	UHorizontalBoxSlot* AddChildToHorizontalBox(UWidget* Content);

protected:
	UClass* GetSlotClass() const override;
	FVector2D ComputeDesiredSize() const override;
	void OnPaint(FPaintContext& Ctx, const FVector2D& Position, const FVector2D& Size) const override;
};
