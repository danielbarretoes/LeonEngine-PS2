#include "Components/Button.h"

#include "Framework/Application/NavigationConfig.h"

FButtonStyle::FButtonStyle()
{
	// Leon's default look: a dark panel, lighter when hovered or focused, lighter still when pressed.
	Normal.TintColor = FLinearColor(0.10f, 0.10f, 0.12f, 0.85f);
	Hovered.TintColor = FLinearColor(0.25f, 0.25f, 0.30f, 0.90f);
	Pressed.TintColor = FLinearColor(0.40f, 0.40f, 0.45f, 0.95f);
	Disabled.TintColor = FLinearColor(0.10f, 0.10f, 0.10f, 0.50f);
}

UButton::UButton(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FVector2D UButton::ComputeDesiredSize() const
{
	const UWidget* Content = GetContent();
	return WidgetStyle.NormalPadding.GetDesiredSize() +
		(Content != nullptr ? Content->GetDesiredSize() : FVector2D::ZeroVector);
}

void UButton::OnPaint(FPaintContext& Ctx, const FVector2D& Position, const FVector2D& Size) const
{
	// UE's order: disabled, pressed, hovered, normal; a focused button looks hovered.
	const FSlateBrush& Brush = !GetIsEnabled() ? WidgetStyle.Disabled
		: bIsPressed                           ? WidgetStyle.Pressed
		: IsHovered() || HasKeyboardFocus()    ? WidgetStyle.Hovered
											   : WidgetStyle.Normal;
	Ctx.DrawBrush(Brush, Position.X, Position.Y, Size.X, Size.Y, BackgroundColor);
	if (const UWidget* Content = GetContent())
	{
		const FMargin& Padding = bIsPressed ? WidgetStyle.PressedPadding : WidgetStyle.NormalPadding;
		Content->Paint(Ctx, Position + FVector2D(Padding.Left, Padding.Top), Size - Padding.GetDesiredSize());
	}
}

void UButton::Press()
{
	if (!bIsPressed)
	{
		bIsPressed = true;
		OnPressed.Broadcast();
	}
}

void UButton::Release(bool bClick)
{
	if (!bIsPressed)
	{
		return;
	}
	bIsPressed = false;
	OnReleased.Broadcast();
	if (bClick)
	{
		OnClicked.Broadcast();
	}
}

FReply UButton::OnKeyDown(const FGeometry& /*MyGeometry*/, const FKeyEvent& InKeyEvent)
{
	if (GetIsEnabled() &&
		FNavigationConfig::GetNavigationActionFromKey(InKeyEvent.GetKey()) == EUINavigationAction::Accept)
	{
		Press();
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply UButton::OnKeyUp(const FGeometry& /*MyGeometry*/, const FKeyEvent& InKeyEvent)
{
	if (FNavigationConfig::GetNavigationActionFromKey(InKeyEvent.GetKey()) == EUINavigationAction::Accept && bIsPressed)
	{
		Release(GetIsEnabled());
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply UButton::OnMouseButtonDown(const FGeometry& /*MyGeometry*/, const FPointerEvent& MouseEvent)
{
	if (GetIsEnabled() && MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		Press();
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply UButton::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton || !bIsPressed)
	{
		return FReply::Unhandled();
	}
	Release(GetIsEnabled() && MyGeometry.IsUnderLocation(MouseEvent.GetScreenSpacePosition()));
	return FReply::Handled();
}

void UButton::OnMouseEnter(const FGeometry& /*MyGeometry*/, const FPointerEvent& /*MouseEvent*/)
{
	OnHovered.Broadcast();
}

void UButton::OnMouseLeave(const FPointerEvent& /*MouseEvent*/)
{
	OnUnhovered.Broadcast();
}

void UButton::OnFocusLost()
{
	// A key press does not survive the focus moving away (the mouse's does: it holds its own capture).
	if (bIsPressed)
	{
		Release(false);
	}
}
