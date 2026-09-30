#pragma once

#include "Blueprint/PaintContext.h"
#include "Components/SlateWrapperTypes.h"
#include "CoreMinimal.h"
#include "Input/Events.h"
#include "Input/Reply.h"
#include "Layout/Geometry.h"
#include "Types/SlateEnums.h"
#include "UObject/Object.h"
#include "Widget.generated.h"

class UPanelSlot;
class UPanelWidget;
class UUserWidget;

/**
 * The base of every UMG widget (UE: UWidget, a UVisual there). A widget measures itself (GetDesiredSize) and paints
 * into the rectangle its parent's slot gives it (Paint, Slate's OnPaint in UE: Leon has no Slate widgets behind UMG).
 * A widget inside a panel knows its slot (Slot) and so its parent (GetParent).
 *
 * Input (Leon's small Slate): the user widget a widget belongs to (GetOwningUserWidget) holds the focus and routes
 * the HUD's key and mouse events (UUserWidget::ProcessKeyDownEvent ...): keys go to the focused widget and up through
 * its parents (OnKeyDown: Slate's handlers, which Leon's widgets implement themselves), the arrows and the d-pad move
 * the focus (FNavigationConfig), and the mouse hovers and clicks the interactable widget under it. Each paint records
 * where a widget went (GetCachedGeometry) and puts the interactable ones in the user widget's hit-test grid.
 */
UCLASS(Abstract)
class UMG_API UWidget : public UObject
{
	GENERATED_BODY()

public:
	UWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The slot of the panel holding the widget, null at a tree's root (UE: Slot). */
	UPROPERTY()
	UPanelSlot* Slot = nullptr;

	[[nodiscard]] ESlateVisibility GetVisibility() const
	{
		return Visibility;
	}
	void SetVisibility(ESlateVisibility InVisibility)
	{
		Visibility = InVisibility;
	}
	/** Drawn: neither Hidden nor Collapsed (UE: IsVisible). */
	[[nodiscard]] bool IsVisible() const
	{
		return Visibility != ESlateVisibility::Hidden && Visibility != ESlateVisibility::Collapsed;
	}

	/** Takes input and draws as enabled (UE: GetIsEnabled / SetIsEnabled); a disabled parent disables its children. */
	[[nodiscard]] bool GetIsEnabled() const;
	void SetIsEnabled(bool bInIsEnabled)
	{
		bIsEnabled = bInIsEnabled;
	}

	/** The panel holding the widget, or null (UE: GetParent). */
	[[nodiscard]] UPanelWidget* GetParent() const;
	/** Leaves its panel (UE: RemoveFromParent). */
	virtual void RemoveFromParent();

	/** The size the widget asks for, pixels: zero when Collapsed (UE: GetDesiredSize). */
	[[nodiscard]] FVector2D GetDesiredSize() const;

	/** Paints the widget in the rectangle at Position of Size, pixels, unless it is not visible. */
	void Paint(FPaintContext& Ctx, const FVector2D& Position, const FVector2D& Size) const;

	/** Where the last paint put the widget (UE: GetCachedGeometry). */
	[[nodiscard]] const FGeometry& GetCachedGeometry() const
	{
		return CachedGeometry;
	}

	// Focus and navigation

	/**
	 * The outermost user widget the widget belongs to (its tree's, or a user widget's own): the one that holds its
	 * focus (Leon; UE's focus belongs to the Slate user).
	 */
	[[nodiscard]] UUserWidget* GetOwningUserWidget() const;

	/** Gives the widget the keyboard and gamepad focus (UE: SetKeyboardFocus / SetFocus). */
	void SetKeyboardFocus();
	void SetFocus()
	{
		SetKeyboardFocus();
	}
	/** UE: HasKeyboardFocus, HasAnyUserFocus (Leon has one user). */
	[[nodiscard]] bool HasKeyboardFocus() const;
	[[nodiscard]] bool HasAnyUserFocus() const
	{
		return HasKeyboardFocus();
	}
	/** The mouse is over it (UE: IsHovered). */
	[[nodiscard]] bool IsHovered() const;

	/** Focus goes from this widget to InWidget in Direction (UE: SetNavigationRuleExplicit); null restores the search.
	 */
	void SetNavigationRuleExplicit(EUINavigation Direction, UWidget* InWidget);
	[[nodiscard]] UWidget* GetExplicitNavigation(EUINavigation Direction) const;

	/** Takes clicks and hover: goes into the hit-test grid (UE: IsInteractable). */
	[[nodiscard]] virtual bool IsInteractable() const
	{
		return false;
	}
	/** Can hold the keyboard focus (Slate: SupportsKeyboardFocus). */
	[[nodiscard]] virtual bool SupportsKeyboardFocus() const
	{
		return false;
	}

	// Input (Slate's SWidget handlers; unhandled by default)

	virtual FReply OnKeyDown(const FGeometry& /*MyGeometry*/, const FKeyEvent& /*InKeyEvent*/)
	{
		return FReply::Unhandled();
	}
	virtual FReply OnKeyUp(const FGeometry& /*MyGeometry*/, const FKeyEvent& /*InKeyEvent*/)
	{
		return FReply::Unhandled();
	}
	virtual FReply OnMouseButtonDown(const FGeometry& /*MyGeometry*/, const FPointerEvent& /*MouseEvent*/)
	{
		return FReply::Unhandled();
	}
	virtual FReply OnMouseButtonUp(const FGeometry& /*MyGeometry*/, const FPointerEvent& /*MouseEvent*/)
	{
		return FReply::Unhandled();
	}
	virtual void OnMouseEnter(const FGeometry& /*MyGeometry*/, const FPointerEvent& /*MouseEvent*/)
	{
	}
	virtual void OnMouseLeave(const FPointerEvent& /*MouseEvent*/)
	{
	}
	virtual void OnFocusReceived()
	{
	}
	virtual void OnFocusLost()
	{
	}

protected:
	/** The size the widget needs to show its content (Slate: ComputeDesiredSize). */
	[[nodiscard]] virtual FVector2D ComputeDesiredSize() const
	{
		return FVector2D::ZeroVector;
	}
	/** Draws the widget in its rectangle (Slate: OnPaint). */
	virtual void OnPaint(FPaintContext& /*Ctx*/, const FVector2D& /*Position*/, const FVector2D& /*Size*/) const
	{
	}

private:
	/** UE: Visibility. */
	ESlateVisibility Visibility = ESlateVisibility::Visible;
	/** UE: bIsEnabled. */
	bool bIsEnabled = true;
	/** Where the last paint put it. */
	mutable FGeometry CachedGeometry;
	/** The explicit navigation rules, by EUINavigation (UE: Navigation's rules); empty: none. */
	UPROPERTY()
	TArray<UWidget*> ExplicitNavigation;
};
