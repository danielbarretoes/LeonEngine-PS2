#pragma once

#include "CoreMinimal.h"
#include "Layout/Margin.h"
#include "Styling/SlateBrush.h"
#include "SlateTypes.generated.h"

/**
 * How a button looks in each of its states (UE: FButtonStyle, SlateCore there). A focused button (the pad's or the
 * keyboard's) looks hovered.
 */
USTRUCT()
struct UMG_API FButtonStyle
{
	GENERATED_BODY()

	FButtonStyle();

	/** UE: Normal, Hovered, Pressed, Disabled. */
	UPROPERTY()
	FSlateBrush Normal;
	UPROPERTY()
	FSlateBrush Hovered;
	UPROPERTY()
	FSlateBrush Pressed;
	UPROPERTY()
	FSlateBrush Disabled;

	/** The space around the content, released and pressed (UE: NormalPadding, PressedPadding). */
	FMargin NormalPadding = FMargin(12.0f, 1.0f, 12.0f, 1.0f);
	FMargin PressedPadding = FMargin(12.0f, 2.0f, 12.0f, 0.0f);

	/** UE: SetNormal ... SetDisabled. */
	FButtonStyle& SetNormal(const FSlateBrush& InNormal)
	{
		Normal = InNormal;
		return *this;
	}
	FButtonStyle& SetHovered(const FSlateBrush& InHovered)
	{
		Hovered = InHovered;
		return *this;
	}
	FButtonStyle& SetPressed(const FSlateBrush& InPressed)
	{
		Pressed = InPressed;
		return *this;
	}
	FButtonStyle& SetDisabled(const FSlateBrush& InDisabled)
	{
		Disabled = InDisabled;
		return *this;
	}
};
