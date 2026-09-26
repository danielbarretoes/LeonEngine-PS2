#include "Components/CanvasPanel.h"

#include "Components/CanvasPanelSlot.h"

namespace
{

	/** The child's size in its canvas slot: its desired size with AutoSize, the slot's otherwise. */
	FVector2D GetChildSize(const UCanvasPanelSlot& CanvasSlot)
	{
		return CanvasSlot.GetAutoSize() ? CanvasSlot.Content->GetDesiredSize() : CanvasSlot.GetSize();
	}

} // namespace

UCanvasPanel::UCanvasPanel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UClass* UCanvasPanel::GetSlotClass() const
{
	return UCanvasPanelSlot::StaticClass();
}

UCanvasPanelSlot* UCanvasPanel::AddChildToCanvas(UWidget* Content)
{
	return Cast<UCanvasPanelSlot>(AddChild(Content));
}

FVector2D UCanvasPanel::ComputeDesiredSize() const
{
	FVector2D Desired = FVector2D::ZeroVector;
	for (const UPanelSlot* PanelSlot : Slots)
	{
		const UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(PanelSlot);
		const FVector2D Corner = CanvasSlot->GetPosition() + GetChildSize(*CanvasSlot);
		Desired.X = FMath::Max(Desired.X, Corner.X);
		Desired.Y = FMath::Max(Desired.Y, Corner.Y);
	}
	return Desired;
}

void UCanvasPanel::OnPaint(FPaintContext& Ctx, const FVector2D& Position, const FVector2D& /*Size*/) const
{
	// In slot order: a later child paints over an earlier one (UE: the ZOrder, left at 0).
	for (const UPanelSlot* PanelSlot : Slots)
	{
		const UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(PanelSlot);
		CanvasSlot->Content->Paint(Ctx, Position + CanvasSlot->GetPosition(), GetChildSize(*CanvasSlot));
	}
}
