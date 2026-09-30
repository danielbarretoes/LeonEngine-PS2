#pragma once

#include "CoreMinimal.h"
#include "Layout/Geometry.h"
#include "Types/SlateEnums.h"
#include "UObject/WeakObjectPtr.h"

class UWidget;

/**
 * The interactable widgets a user widget painted last, in paint order, with where they went (UE: FHittestGrid, a
 * list here: Leon's menus hold tens of widgets): what is under the mouse, and where focus goes in a direction. Rebuilt
 * by each paint, it keeps its capacity (no allocation once the menu has been painted).
 */
class UMG_API FHittestGrid
{
public:
	/** Forgets the widgets (the next paint adds them again). */
	void Clear()
	{
		Entries.Reset();
	}

	/** A widget painted at Geometry (UWidget::Paint adds the interactable ones). */
	void AddWidget(UWidget* Widget, const FGeometry& Geometry);

	/** The last painted widget under Location (the topmost), or null (UE: GetBubblePath's first widget). */
	[[nodiscard]] UWidget* FindTopmostWidgetAt(const FVector2D& Location) const;

	/** The first painted widget that takes keyboard focus, or null. */
	[[nodiscard]] UWidget* FindFirstFocusableWidget() const;

	/**
	 * The widget focus moves to from From in Direction (UE: FindNextFocusableWidget): From's explicit rule for it, else
	 * the nearest focusable widget whose centre lies that way (the distance along the direction plus twice the
	 * distance across it); Next and Previous go through the widgets in paint order. Null at the edge.
	 */
	[[nodiscard]] UWidget* FindNextFocusableWidget(const UWidget* From, EUINavigation Direction) const;

	/** Where Widget was painted, or null when this paint did not paint it. */
	[[nodiscard]] const FGeometry* FindGeometry(const UWidget* Widget) const;

	[[nodiscard]] int32 Num() const
	{
		return Entries.Num();
	}

private:
	struct FWidgetEntry
	{
		TWeakObjectPtr<UWidget> Widget;
		FGeometry Geometry;
	};

	/** Whether the entry's widget is alive and takes keyboard focus now. */
	[[nodiscard]] static bool IsFocusable(const FWidgetEntry& Entry);

	TArray<FWidgetEntry> Entries;
};
