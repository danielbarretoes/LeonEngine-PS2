#include "Components/Widget.h"

#include "Components/PanelSlot.h"
#include "Components/PanelWidget.h"

UWidget::UWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UPanelWidget* UWidget::GetParent() const
{
	return Slot != nullptr ? Slot->Parent : nullptr;
}

void UWidget::RemoveFromParent()
{
	if (UPanelWidget* Parent = GetParent())
	{
		(void)Parent->RemoveChild(this);
	}
}

FVector2D UWidget::GetDesiredSize() const
{
	return Visibility == ESlateVisibility::Collapsed ? FVector2D::ZeroVector : ComputeDesiredSize();
}

void UWidget::Paint(FPaintContext& Ctx, const FVector2D& Position, const FVector2D& Size) const
{
	if (IsVisible())
	{
		OnPaint(Ctx, Position, Size);
	}
}
