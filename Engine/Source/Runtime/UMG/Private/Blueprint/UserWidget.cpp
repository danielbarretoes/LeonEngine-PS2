#include "Blueprint/UserWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Framework/Application/NavigationConfig.h"

UUserWidget::UUserWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UUserWidget::Initialize()
{
	if (WidgetTree != nullptr)
	{
		return false;
	}
	WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
	NativeOnInitialized();
	return true;
}

void UUserWidget::NativePaint(FPaintContext& Ctx)
{
	// The grid is rebuilt by every paint (it keeps its capacity): input goes to what is on the screen now.
	HittestGrid.Clear();
	FHittestGrid* Previous = Ctx.GetHittestGrid();
	Ctx.SetHittestGrid(&HittestGrid);
	Paint(
		Ctx, FVector2D::ZeroVector, FVector2D(static_cast<float>(Ctx.GetWidth()), static_cast<float>(Ctx.GetHeight())));
	Ctx.SetHittestGrid(Previous);
}

FVector2D UUserWidget::ComputeDesiredSize() const
{
	return WidgetTree != nullptr && WidgetTree->RootWidget != nullptr ? WidgetTree->RootWidget->GetDesiredSize()
																	  : FVector2D::ZeroVector;
}

void UUserWidget::OnPaint(FPaintContext& Ctx, const FVector2D& Position, const FVector2D& Size) const
{
	if (WidgetTree != nullptr && WidgetTree->RootWidget != nullptr)
	{
		WidgetTree->RootWidget->Paint(Ctx, Position, Size);
	}
}

void UUserWidget::SetFocusedWidget(UWidget* InWidget)
{
	if (FocusedWidget == InWidget)
	{
		return;
	}
	UWidget* Previous = FocusedWidget;
	FocusedWidget = InWidget;
	if (Previous != nullptr)
	{
		Previous->OnFocusLost();
	}
	if (InWidget != nullptr)
	{
		InWidget->OnFocusReceived();
	}
}

UWidget* UUserWidget::GetKeyTarget() const
{
	// A focused widget the last paint did not show (a hidden page of a switcher) does not take keys.
	if (FocusedWidget != nullptr && FocusedWidget != this && HittestGrid.FindGeometry(FocusedWidget) != nullptr)
	{
		return FocusedWidget;
	}
	return FocusedWidget == this || bIsFocusable ? const_cast<UUserWidget*>(this) : nullptr;
}

FReply UUserWidget::ProcessKeyDownEvent(const FKeyEvent& InKeyEvent)
{
	UWidget* Target = GetKeyTarget();
	if (Target == nullptr)
	{
		return FReply::Unhandled();
	}
	// The focused widget, then its parents, then the user widget (UE: the focus path's bubbling).
	if (Target != this)
	{
		for (UWidget* Widget = Target; Widget != nullptr; Widget = Widget->GetParent())
		{
			if (Widget->OnKeyDown(Widget->GetCachedGeometry(), InKeyEvent).IsEventHandled())
			{
				return FReply::Handled();
			}
		}
	}
	if (NativeOnKeyDown(GetCachedGeometry(), InKeyEvent).IsEventHandled())
	{
		return FReply::Handled();
	}
	// Unhandled: a navigation key moves the focus (from nothing: to the first focusable widget), and a menu keeps it.
	const EUINavigation Direction = FNavigationConfig::GetNavigationDirectionFromKey(InKeyEvent.GetKey());
	if (Direction != EUINavigation::Invalid)
	{
		UWidget* Next = Target == this ? HittestGrid.FindFirstFocusableWidget()
									   : HittestGrid.FindNextFocusableWidget(Target, Direction);
		if (Next != nullptr)
		{
			SetFocusedWidget(Next);
		}
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply UUserWidget::ProcessKeyUpEvent(const FKeyEvent& InKeyEvent)
{
	UWidget* Target = GetKeyTarget();
	if (Target == nullptr)
	{
		return FReply::Unhandled();
	}
	if (Target != this)
	{
		for (UWidget* Widget = Target; Widget != nullptr; Widget = Widget->GetParent())
		{
			if (Widget->OnKeyUp(Widget->GetCachedGeometry(), InKeyEvent).IsEventHandled())
			{
				return FReply::Handled();
			}
		}
	}
	if (NativeOnKeyUp(GetCachedGeometry(), InKeyEvent).IsEventHandled())
	{
		return FReply::Handled();
	}
	return FNavigationConfig::GetNavigationDirectionFromKey(InKeyEvent.GetKey()) != EUINavigation::Invalid
		? FReply::Handled()
		: FReply::Unhandled();
}

void UUserWidget::SetHoveredWidget(UWidget* InWidget, const FPointerEvent& MouseEvent)
{
	if (HoveredWidget == InWidget)
	{
		return;
	}
	UWidget* Previous = HoveredWidget;
	HoveredWidget = InWidget;
	if (Previous != nullptr)
	{
		Previous->OnMouseLeave(MouseEvent);
	}
	if (InWidget != nullptr)
	{
		InWidget->OnMouseEnter(InWidget->GetCachedGeometry(), MouseEvent);
	}
}

FReply UUserWidget::ProcessMouseMoveEvent(const FPointerEvent& MouseEvent)
{
	UWidget* Hovered = HittestGrid.FindTopmostWidgetAt(MouseEvent.GetScreenSpacePosition());
	SetHoveredWidget(Hovered, MouseEvent);
	return Hovered != nullptr ? FReply::Handled() : FReply::Unhandled();
}

FReply UUserWidget::ProcessMouseButtonDownEvent(const FPointerEvent& MouseEvent)
{
	UWidget* Target = HittestGrid.FindTopmostWidgetAt(MouseEvent.GetScreenSpacePosition());
	SetHoveredWidget(Target, MouseEvent);
	if (Target == nullptr)
	{
		return FReply::Unhandled();
	}
	// A click focuses what takes focus (Slate), and the widget that took the press gets the release.
	if (Target->SupportsKeyboardFocus())
	{
		SetFocusedWidget(Target);
	}
	if (Target->OnMouseButtonDown(Target->GetCachedGeometry(), MouseEvent).IsEventHandled())
	{
		PressedWidget = Target;
	}
	return FReply::Handled();
}

FReply UUserWidget::ProcessMouseButtonUpEvent(const FPointerEvent& MouseEvent)
{
	UWidget* Target = PressedWidget;
	PressedWidget = nullptr;
	if (Target == nullptr)
	{
		return HittestGrid.FindTopmostWidgetAt(MouseEvent.GetScreenSpacePosition()) != nullptr ? FReply::Handled()
																							   : FReply::Unhandled();
	}
	(void)Target->OnMouseButtonUp(Target->GetCachedGeometry(), MouseEvent);
	return FReply::Handled();
}
