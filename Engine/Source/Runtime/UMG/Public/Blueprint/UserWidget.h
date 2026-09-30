#pragma once

#include "Blueprint/PaintContext.h"
#include "Components/Widget.h"
#include "CoreMinimal.h"
#include "Input/HittestGrid.h"
#include "UserWidget.generated.h"

class AHUD;
class UWidgetTree;

/**
 * A widget made of a tree of widgets (UE: UUserWidget). AHUD::AddWidget makes it with the HUD as its outer and calls
 * Initialize, which makes its WidgetTree and calls NativeOnInitialized (build the tree there), then NativeConstruct
 * (UE: CreateWidget + AddToViewport). The HUD ticks the visible ones (NativeTick: refresh the tree from the game) and
 * paints them (NativePaint: the tree over the whole viewport).
 *
 * Input (Leon's small Slate, UE's FSlateApplication routing): the user widget holds its tree's focus, hover and mouse
 * capture, and the HUD hands it the player's keys and mouse (ProcessKeyDownEvent ...). A key goes to the focused
 * widget, then up through its parents to NativeOnKeyDown; unhandled, a navigation key moves the focus (FHittestGrid,
 * the widgets painted last) and the others go back to the game. A user widget with nothing focused takes keys only
 * when bIsFocusable (a menu): its first arrow or d-pad press focuses its first focusable widget.
 */
UCLASS()
class UMG_API UUserWidget : public UWidget
{
	GENERATED_BODY()

public:
	UUserWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The widget's widgets (UE: WidgetTree). */
	UPROPERTY(Transient)
	UWidgetTree* WidgetTree = nullptr;

	/** Takes the keys while nothing inside is focused (UE: bIsFocusable): a menu's, not a HUD's. */
	bool bIsFocusable = false;

	/** Makes the tree once, then NativeOnInitialized; false when already initialized (UE: Initialize). */
	bool Initialize();

	virtual void NativeOnInitialized()
	{
	}
	virtual void NativeConstruct()
	{
	}
	virtual void NativeTick(float /*DeltaTime*/)
	{
	}
	/** Paints the tree's root over the viewport, collecting its interactable widgets into the hit-test grid. */
	virtual void NativePaint(FPaintContext& Ctx);
	virtual void NativeDestruct()
	{
	}

	/** The keys that reach the user widget itself: the focused widget and its parents did not take them (UE). */
	virtual FReply NativeOnKeyDown(const FGeometry& /*InGeometry*/, const FKeyEvent& /*InKeyEvent*/)
	{
		return FReply::Unhandled();
	}
	virtual FReply NativeOnKeyUp(const FGeometry& /*InGeometry*/, const FKeyEvent& /*InKeyEvent*/)
	{
		return FReply::Unhandled();
	}

	/** The HUD that added the widget; null if not added (Leon; UE's user widgets know their player instead). */
	[[nodiscard]] AHUD* GetOwningHUD() const
	{
		return OwningHud;
	}

	// The input routing (Leon; UE: FSlateApplication's ProcessKeyDownEvent and the rest)

	/** A key pressed: the focused widget's, then navigation. Handled when something took it. */
	FReply ProcessKeyDownEvent(const FKeyEvent& InKeyEvent);
	/** A key released: the focused widget's (a button clicks on Accept's release). */
	FReply ProcessKeyUpEvent(const FKeyEvent& InKeyEvent);
	/** The mouse moved to a canvas position: hover moves; handled when over an interactable widget. */
	FReply ProcessMouseMoveEvent(const FPointerEvent& MouseEvent);
	/** A mouse button pressed: the widget under the mouse takes it (and the focus, when it takes focus). */
	FReply ProcessMouseButtonDownEvent(const FPointerEvent& MouseEvent);
	/** A mouse button released: the widget that took the press gets it. */
	FReply ProcessMouseButtonUpEvent(const FPointerEvent& MouseEvent);

	/** The focused widget of the tree (itself, when the user widget has the focus); null for none. */
	[[nodiscard]] UWidget* GetFocusedWidget() const
	{
		return FocusedWidget;
	}
	/** Moves the focus (UWidget::SetKeyboardFocus calls it): OnFocusLost, then OnFocusReceived. */
	void SetFocusedWidget(UWidget* InWidget);
	/** The widget under the mouse, or null. */
	[[nodiscard]] UWidget* GetHoveredWidget() const
	{
		return HoveredWidget;
	}
	/** The interactable widgets painted last (Leon: UE's hit-test grid). */
	[[nodiscard]] const FHittestGrid& GetHittestGrid() const
	{
		return HittestGrid;
	}

protected:
	/** The root's. */
	FVector2D ComputeDesiredSize() const override;
	void OnPaint(FPaintContext& Ctx, const FVector2D& Position, const FVector2D& Size) const override;
	/** A user widget with bIsFocusable holds the focus itself. */
	bool SupportsKeyboardFocus() const override
	{
		return bIsFocusable;
	}

private:
	friend class AHUD;

	/** Where the key event goes first: the focused widget painted last, else the user widget itself. */
	[[nodiscard]] UWidget* GetKeyTarget() const;
	/** Sets the hovered widget, with OnMouseLeave / OnMouseEnter. */
	void SetHoveredWidget(UWidget* InWidget, const FPointerEvent& MouseEvent);

	/** The HUD that added the widget (not a UPROPERTY: UMG cannot reflect Engine's AHUD; the HUD outlives it). */
	AHUD* OwningHud = nullptr;
	/** The focused, hovered and mouse-pressed widgets. */
	UPROPERTY(Transient)
	UWidget* FocusedWidget = nullptr;
	UPROPERTY(Transient)
	UWidget* HoveredWidget = nullptr;
	UPROPERTY(Transient)
	UWidget* PressedWidget = nullptr;
	/** The interactable widgets of the last paint. */
	FHittestGrid HittestGrid;
};
