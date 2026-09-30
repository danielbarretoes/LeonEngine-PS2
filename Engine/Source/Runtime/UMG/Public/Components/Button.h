#pragma once

#include "Components/ContentWidget.h"
#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "Styling/SlateTypes.h"
#include "Button.generated.h"

DECLARE_MULTICAST_DELEGATE(FOnButtonClickedEvent);
DECLARE_MULTICAST_DELEGATE(FOnButtonPressedEvent);
DECLARE_MULTICAST_DELEGATE(FOnButtonReleasedEvent);
DECLARE_MULTICAST_DELEGATE(FOnButtonHoverEvent);

/**
 * A clickable panel around one child (UE: UButton): its style's brush for its state (normal, hovered or focused,
 * pressed, disabled) behind the child, inside the style's padding.
 *
 * - The mouse: pressing over it presses it, releasing over it clicks (UE: the DownAndUp click method).
 * - The keyboard and the pad: it takes the focus (UE: IsFocusable); Accept (Enter, Space, the pad's Cross) presses it
 *   and its release clicks (FNavigationConfig).
 *
 * OnClicked, OnPressed, OnReleased, OnHovered and OnUnhovered are UE's events (native multicast delegates in Leon).
 */
UCLASS()
class UMG_API UButton : public UContentWidget
{
	GENERATED_BODY()

public:
	UButton(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** UE: SetStyle / WidgetStyle. */
	void SetStyle(const FButtonStyle& InStyle)
	{
		WidgetStyle = InStyle;
	}
	[[nodiscard]] const FButtonStyle& GetStyle() const
	{
		return WidgetStyle;
	}
	/** Multiplies the style's brushes (UE: SetBackgroundColor). */
	void SetBackgroundColor(const FLinearColor& InBackgroundColor)
	{
		BackgroundColor = InBackgroundColor;
	}
	/** Takes the keyboard and pad focus (UE: IsFocusable). */
	void SetIsFocusable(bool bInIsFocusable)
	{
		bIsFocusable = bInIsFocusable;
	}
	/** Held down by the mouse or by Accept (UE: IsPressed). */
	[[nodiscard]] bool IsPressed() const
	{
		return bIsPressed;
	}

	FOnButtonClickedEvent OnClicked;
	FOnButtonPressedEvent OnPressed;
	FOnButtonReleasedEvent OnReleased;
	FOnButtonHoverEvent OnHovered;
	FOnButtonHoverEvent OnUnhovered;

	bool IsInteractable() const override
	{
		return true;
	}
	bool SupportsKeyboardFocus() const override
	{
		return bIsFocusable;
	}

	FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	FReply OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	void OnMouseLeave(const FPointerEvent& MouseEvent) override;
	void OnFocusLost() override;

protected:
	FVector2D ComputeDesiredSize() const override;
	void OnPaint(FPaintContext& Ctx, const FVector2D& Position, const FVector2D& Size) const override;

private:
	void Press();
	/** Releases a press; bClick: it ends over the button (or by Accept), so it clicks. */
	void Release(bool bClick);

	UPROPERTY()
	FButtonStyle WidgetStyle;
	FLinearColor BackgroundColor = FLinearColor::White;
	bool bIsFocusable = true;
	bool bIsPressed = false;
};
