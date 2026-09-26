#pragma once

#include "Blueprint/PaintContext.h"
#include "Components/SlateWrapperTypes.h"
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Widget.generated.h"

class UPanelSlot;
class UPanelWidget;

/**
 * The base of every UMG widget (UE: UWidget, a UVisual there). A widget measures itself (GetDesiredSize) and paints
 * into the rectangle its parent's slot gives it (Paint, Slate's OnPaint in UE: Leon has no Slate widgets behind UMG).
 * A widget inside a panel knows its slot (Slot) and so its parent (GetParent). Leon's widgets take no input.
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

	/** The panel holding the widget, or null (UE: GetParent). */
	[[nodiscard]] UPanelWidget* GetParent() const;
	/** Leaves its panel (UE: RemoveFromParent). */
	virtual void RemoveFromParent();

	/** The size the widget asks for, pixels: zero when Collapsed (UE: GetDesiredSize). */
	[[nodiscard]] FVector2D GetDesiredSize() const;

	/** Paints the widget in the rectangle at Position of Size, pixels, unless it is not visible. */
	void Paint(FPaintContext& Ctx, const FVector2D& Position, const FVector2D& Size) const;

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
};
