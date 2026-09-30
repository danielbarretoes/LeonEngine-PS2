#pragma once

#include "CoreMinimal.h"

/**
 * Where a widget was painted: its top-left corner on the canvas and its size, pixels (UE: FGeometry, without the
 * render transforms: Leon's widgets are never scaled or rotated).
 */
struct FGeometry
{
	FGeometry() = default;
	FGeometry(const FVector2D& InAbsolutePosition, const FVector2D& InLocalSize)
		: AbsolutePosition(InAbsolutePosition)
		, LocalSize(InLocalSize)
	{
	}

	/** UE: GetAbsolutePosition. */
	[[nodiscard]] const FVector2D& GetAbsolutePosition() const
	{
		return AbsolutePosition;
	}
	/** UE: GetLocalSize. */
	[[nodiscard]] const FVector2D& GetLocalSize() const
	{
		return LocalSize;
	}
	/** The centre, on the canvas. */
	[[nodiscard]] FVector2D GetAbsoluteCenter() const
	{
		return AbsolutePosition + (LocalSize * 0.5f);
	}
	/** Whether a canvas point is inside (UE: IsUnderLocation). */
	[[nodiscard]] bool IsUnderLocation(const FVector2D& AbsoluteCoordinate) const
	{
		return AbsoluteCoordinate.X >= AbsolutePosition.X && AbsoluteCoordinate.Y >= AbsolutePosition.Y &&
			AbsoluteCoordinate.X < AbsolutePosition.X + LocalSize.X &&
			AbsoluteCoordinate.Y < AbsolutePosition.Y + LocalSize.Y;
	}
	/** A canvas point in the widget's own space (UE: AbsoluteToLocal). */
	[[nodiscard]] FVector2D AbsoluteToLocal(const FVector2D& AbsoluteCoordinate) const
	{
		return AbsoluteCoordinate - AbsolutePosition;
	}

private:
	FVector2D AbsolutePosition = FVector2D::ZeroVector;
	FVector2D LocalSize = FVector2D::ZeroVector;
};
