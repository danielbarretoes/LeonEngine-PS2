#pragma once

#include "Components/PanelWidget.h"
#include "CoreMinimal.h"
#include "VerticalBox.generated.h"

class UVerticalBoxSlot;

/**
 * Stacks its children top to bottom, each at its desired height plus its slot's padding, as wide as the box (UE:
 * UVerticalBox; every slot sizes Auto: UE's Fill sizing is not in Leon).
 */
UCLASS()
class UMG_API UVerticalBox : public UPanelWidget
{
	GENERATED_BODY()

public:
	UVerticalBox(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** UE: AddChildToVerticalBox. */
	UVerticalBoxSlot* AddChildToVerticalBox(UWidget* Content);

protected:
	UClass* GetSlotClass() const override;
	FVector2D ComputeDesiredSize() const override;
	void OnPaint(FPaintContext& Ctx, const FVector2D& Position, const FVector2D& Size) const override;
};
