#include "Components/WidgetSwitcher.h"

#include "Components/PanelSlot.h"

UWidgetSwitcher::UWidgetSwitcher(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

int32 UWidgetSwitcher::GetActiveWidgetIndex() const
{
	return GetChildrenCount() > 0 ? FMath::Clamp(ActiveWidgetIndex, 0, GetChildrenCount() - 1) : 0;
}

void UWidgetSwitcher::SetActiveWidgetIndex(int32 Index)
{
	ActiveWidgetIndex = FMath::Max(0, Index);
}

void UWidgetSwitcher::SetActiveWidget(UWidget* Widget)
{
	const int32 Index = GetChildIndex(Widget);
	if (Index != INDEX_NONE)
	{
		ActiveWidgetIndex = Index;
	}
}

UWidget* UWidgetSwitcher::GetActiveWidget() const
{
	return GetChildAt(GetActiveWidgetIndex());
}

FVector2D UWidgetSwitcher::ComputeDesiredSize() const
{
	FVector2D Desired = FVector2D::ZeroVector;
	for (const UPanelSlot* PanelSlot : Slots)
	{
		const FVector2D Child = PanelSlot->Content->GetDesiredSize();
		Desired.X = FMath::Max(Desired.X, Child.X);
		Desired.Y = FMath::Max(Desired.Y, Child.Y);
	}
	return Desired;
}

void UWidgetSwitcher::OnPaint(FPaintContext& Ctx, const FVector2D& Position, const FVector2D& Size) const
{
	if (const UWidget* Active = GetActiveWidget())
	{
		Active->Paint(Ctx, Position, Size);
	}
}
