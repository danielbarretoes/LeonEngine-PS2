#pragma once

#include "CoreMinimal.h"

/** Space around a widget, pixels (UE: FMargin, Layout/Margin.h). */
struct FMargin
{
	float Left = 0.0f;
	float Top = 0.0f;
	float Right = 0.0f;
	float Bottom = 0.0f;

	FMargin() = default;
	/** The same space on every side. */
	FMargin(float Uniform)
		: Left(Uniform)
		, Top(Uniform)
		, Right(Uniform)
		, Bottom(Uniform)
	{
	}
	/** Horizontal on the left and right, Vertical on the top and bottom. */
	FMargin(float Horizontal, float Vertical)
		: Left(Horizontal)
		, Top(Vertical)
		, Right(Horizontal)
		, Bottom(Vertical)
	{
	}
	FMargin(float InLeft, float InTop, float InRight, float InBottom)
		: Left(InLeft)
		, Top(InTop)
		, Right(InRight)
		, Bottom(InBottom)
	{
	}

	/** Left + Right and Top + Bottom (UE: GetDesiredSize). */
	[[nodiscard]] FVector2D GetDesiredSize() const
	{
		return FVector2D(Left + Right, Top + Bottom);
	}
};
