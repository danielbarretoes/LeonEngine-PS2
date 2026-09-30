#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

/** A key pressed or released (UE: FKeyEvent). */
class FKeyEvent
{
public:
	FKeyEvent() = default;
	explicit FKeyEvent(const FKey& InKey, bool bInIsRepeat = false)
		: Key(InKey)
		, bIsRepeat(bInIsRepeat)
	{
	}

	/** UE: GetKey. */
	[[nodiscard]] const FKey& GetKey() const
	{
		return Key;
	}
	/** UE: IsRepeat. */
	[[nodiscard]] bool IsRepeat() const
	{
		return bIsRepeat;
	}

private:
	FKey Key;
	bool bIsRepeat = false;
};

/** The mouse: where it is on the canvas and the button that changed, if any (UE: FPointerEvent). */
class FPointerEvent
{
public:
	FPointerEvent() = default;
	FPointerEvent(const FVector2D& InScreenSpacePosition, const FKey& InEffectingButton)
		: ScreenSpacePosition(InScreenSpacePosition)
		, EffectingButton(InEffectingButton)
	{
	}

	/** The position on the canvas, pixels (UE: GetScreenSpacePosition). */
	[[nodiscard]] const FVector2D& GetScreenSpacePosition() const
	{
		return ScreenSpacePosition;
	}
	/** The button pressed or released; none for a move (UE: GetEffectingButton). */
	[[nodiscard]] const FKey& GetEffectingButton() const
	{
		return EffectingButton;
	}

private:
	FVector2D ScreenSpacePosition = FVector2D::ZeroVector;
	FKey EffectingButton;
};
