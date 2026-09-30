#include "Components/Widget.h"

#include "Blueprint/UserWidget.h"
#include "Components/PanelSlot.h"
#include "Components/PanelWidget.h"
#include "Input/HittestGrid.h"

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

bool UWidget::GetIsEnabled() const
{
	for (const UWidget* Widget = this; Widget != nullptr; Widget = Widget->GetParent())
	{
		if (!Widget->bIsEnabled)
		{
			return false;
		}
	}
	return true;
}

FVector2D UWidget::GetDesiredSize() const
{
	return Visibility == ESlateVisibility::Collapsed ? FVector2D::ZeroVector : ComputeDesiredSize();
}

void UWidget::Paint(FPaintContext& Ctx, const FVector2D& Position, const FVector2D& Size) const
{
	if (!IsVisible())
	{
		return;
	}
	CachedGeometry = FGeometry(Position, Size);
	// The hit-test grid takes what the mouse and the focus can reach.
	if (FHittestGrid* Grid = Ctx.GetHittestGrid())
	{
		if (Visibility == ESlateVisibility::Visible && IsInteractable() && GetIsEnabled())
		{
			Grid->AddWidget(const_cast<UWidget*>(this), CachedGeometry);
		}
	}
	OnPaint(Ctx, Position, Size);
}

UUserWidget* UWidget::GetOwningUserWidget() const
{
	// Up the outers (a tree's widgets are its objects, a tree is its user widget's) to the outermost user widget.
	UUserWidget* Owner = nullptr;
	for (UObject* Outer = const_cast<UWidget*>(this); Outer != nullptr; Outer = Outer->GetOuter())
	{
		if (UUserWidget* UserWidget = Cast<UUserWidget>(Outer))
		{
			Owner = UserWidget;
		}
	}
	return Owner;
}

void UWidget::SetKeyboardFocus()
{
	if (UUserWidget* Owner = GetOwningUserWidget())
	{
		Owner->SetFocusedWidget(this);
	}
}

bool UWidget::HasKeyboardFocus() const
{
	const UUserWidget* Owner = GetOwningUserWidget();
	return Owner != nullptr && Owner->GetFocusedWidget() == this;
}

bool UWidget::IsHovered() const
{
	const UUserWidget* Owner = GetOwningUserWidget();
	return Owner != nullptr && Owner->GetHoveredWidget() == this;
}

void UWidget::SetNavigationRuleExplicit(EUINavigation Direction, UWidget* InWidget)
{
	const int32 Index = int32(Direction);
	if (Index < 0 || Index >= int32(EUINavigation::Num))
	{
		return;
	}
	if (ExplicitNavigation.Num() < int32(EUINavigation::Num))
	{
		ExplicitNavigation.SetNumZeroed(int32(EUINavigation::Num));
	}
	ExplicitNavigation[Index] = InWidget;
}

UWidget* UWidget::GetExplicitNavigation(EUINavigation Direction) const
{
	const int32 Index = int32(Direction);
	return Index >= 0 && Index < ExplicitNavigation.Num() ? ExplicitNavigation[Index] : nullptr;
}
