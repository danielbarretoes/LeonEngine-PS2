#pragma once

#include "Components/PanelWidget.h"
#include "CoreMinimal.h"
#include "CanvasPanel.generated.h"

class UCanvasPanelSlot;

/** Places each child at its slot's position and size (UE: UCanvasPanel, the usual root of a user widget). */
UCLASS()
class UMG_API UCanvasPanel : public UPanelWidget
{
	GENERATED_BODY()

public:
	UCanvasPanel(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** UE: AddChildToCanvas. */
	UCanvasPanelSlot* AddChildToCanvas(UWidget* Content);

protected:
	UClass* GetSlotClass() const override;
	/** The rectangle holding every child's (Slate: SConstraintCanvas). */
	FVector2D ComputeDesiredSize() const override;
	void OnPaint(FPaintContext& Ctx, const FVector2D& Position, const FVector2D& Size) const override;
};
