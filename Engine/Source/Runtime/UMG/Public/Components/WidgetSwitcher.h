#pragma once

#include "Components/PanelWidget.h"
#include "CoreMinimal.h"
#include "WidgetSwitcher.generated.h"

/**
 * Shows one of its children, the active one, over its whole rectangle (UE: UWidgetSwitcher): a menu's pages. It asks
 * for the largest of its children's sizes, so switching does not move what is around it. The children not shown are
 * not painted, so they take no clicks and no focus.
 */
UCLASS()
class UMG_API UWidgetSwitcher : public UPanelWidget
{
	GENERATED_BODY()

public:
	UWidgetSwitcher(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** UE: GetNumWidgets. */
	[[nodiscard]] int32 GetNumWidgets() const
	{
		return GetChildrenCount();
	}
	/** UE: GetActiveWidgetIndex / SetActiveWidgetIndex (clamped to the children). */
	[[nodiscard]] int32 GetActiveWidgetIndex() const;
	void SetActiveWidgetIndex(int32 Index);
	/** UE: SetActiveWidget: the index of a child; nothing for another widget. */
	void SetActiveWidget(UWidget* Widget);
	/** UE: GetActiveWidget / GetWidgetAtIndex. */
	[[nodiscard]] UWidget* GetActiveWidget() const;
	[[nodiscard]] UWidget* GetWidgetAtIndex(int32 Index) const
	{
		return GetChildAt(Index);
	}

protected:
	FVector2D ComputeDesiredSize() const override;
	void OnPaint(FPaintContext& Ctx, const FVector2D& Position, const FVector2D& Size) const override;

private:
	int32 ActiveWidgetIndex = 0;
};
