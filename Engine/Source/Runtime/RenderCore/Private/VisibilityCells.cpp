#include "VisibilityCells.h"

namespace
{

	/** A portal is walked through at most this deep (a map's cells are a few dozen; the walk stays bounded). */
	constexpr int32 MaxDepth = 16;
	/** The clip w below which a point is behind the eye (cm: UE's projection's w is the view's depth). */
	constexpr float MinW = 1.0f;

	[[nodiscard]] uint64 CellBit(int32 Cell)
	{
		return uint64(1) << uint32(Cell);
	}

	[[nodiscard]] FBox2D FullScreen()
	{
		return FBox2D(FVector2D(-1.0f, -1.0f), FVector2D(1.0f, 1.0f));
	}

	[[nodiscard]] bool Overlap(const FBox2D& A, const FBox2D& B, FBox2D& Out)
	{
		Out.Min = FVector2D(FMath::Max(A.Min.X, B.Min.X), FMath::Max(A.Min.Y, B.Min.Y));
		Out.Max = FVector2D(FMath::Min(A.Max.X, B.Max.X), FMath::Min(A.Max.Y, B.Max.Y));
		Out.bIsValid = Out.Min.X < Out.Max.X && Out.Min.Y < Out.Max.Y;
		return Out.bIsValid;
	}

} // namespace

void FVisibilityCellGraph::Reset()
{
	Cells.Reset();
	Portals.Reset();
}

int32 FVisibilityCellGraph::AddCell(FName Name, const FBox& Bounds)
{
	if (Cells.Num() >= MaxCells || FindCell(Name) != INDEX_NONE || !Bounds.IsValid)
	{
		return INDEX_NONE;
	}
	return Cells.Add({Name, Bounds});
}

int32 FVisibilityCellGraph::FindCell(FName Name) const
{
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (Cells[Index].Name == Name)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

bool FVisibilityCellGraph::AddPortal(int32 CellA, int32 CellB, const FVector (&Corners)[4])
{
	if (!Cells.IsValidIndex(CellA) || !Cells.IsValidIndex(CellB) || CellA == CellB)
	{
		return false;
	}
	FPortal& Portal = Portals.AddDefaulted_GetRef();
	Portal.CellA = CellA;
	Portal.CellB = CellB;
	for (int32 Corner = 0; Corner < 4; ++Corner)
	{
		Portal.Corners[Corner] = Corners[Corner];
		Portal.NearBounds += Corners[Corner];
	}
	Portal.NearBounds = Portal.NearBounds.ExpandBy(PortalNearDistance);
	return true;
}

int32 FVisibilityCellGraph::FindCellAt(const FVector& Point) const
{
	int32 Best = INDEX_NONE;
	float BestVolume = 0.0f;
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		const FBox& Bounds = Cells[Index].Bounds;
		if (!Bounds.IsInsideOrOn(Point))
		{
			continue;
		}
		const float Volume = Bounds.GetVolume();
		if (Best == INDEX_NONE || Volume < BestVolume)
		{
			Best = Index;
			BestVolume = Volume;
		}
	}
	return Best;
}

uint64 FVisibilityCellGraph::GetCellMask(const FBox& Bounds) const
{
	uint64 Mask = 0;
	if (!Bounds.IsValid)
	{
		return Mask;
	}
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (Cells[Index].Bounds.Intersect(Bounds))
		{
			Mask |= CellBit(Index);
		}
	}
	return Mask;
}

bool FVisibilityCellGraph::ProjectPortal(const FPortal& Portal, const FMatrix& ViewProjection, FBox2D& OutRect)
{
	// The quad in clip space, clipped to w >= MinW (Sutherland-Hodgman against one plane), then its points' box on the
	// screen.
	FVector4 Clip[4];
	for (int32 Corner = 0; Corner < 4; ++Corner)
	{
		Clip[Corner] = ViewProjection.TransformPosition(Portal.Corners[Corner]);
	}
	FVector4 Polygon[8];
	int32 Count = 0;
	for (int32 Corner = 0; Corner < 4; ++Corner)
	{
		const FVector4& A = Clip[Corner];
		const FVector4& B = Clip[(Corner + 1) % 4];
		const bool bInA = A.W >= MinW;
		const bool bInB = B.W >= MinW;
		if (bInA)
		{
			Polygon[Count++] = A;
		}
		if (bInA != bInB)
		{
			const float T = (MinW - A.W) / (B.W - A.W);
			Polygon[Count++] = A + ((B - A) * T);
		}
	}
	if (Count < 3)
	{
		return false;
	}
	OutRect = FBox2D(ForceInit);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const float InvW = 1.0f / Polygon[Index].W;
		OutRect += FVector2D(Polygon[Index].X * InvW, Polygon[Index].Y * InvW);
	}
	return true;
}

void FVisibilityCellGraph::Visit(int32 Cell, const FBox2D& Rect, uint64 PathMask, int32 Depth,
	const FMatrix& ViewProjection, const FVector& EyeLocation, FVisibleCells& OutVisible) const
{
	const uint64 Bit = CellBit(Cell);
	if ((OutVisible.Mask & Bit) == 0)
	{
		OutVisible.Mask |= Bit;
		OutVisible.Rects[Cell] = Rect;
	}
	else
	{
		OutVisible.Rects[Cell] += Rect;
	}
	if (Depth >= MaxDepth)
	{
		return;
	}
	for (const FPortal& Portal : Portals)
	{
		const int32 Other = Portal.CellA == Cell ? Portal.CellB : Portal.CellB == Cell ? Portal.CellA : INDEX_NONE;
		if (Other == INDEX_NONE || (PathMask & CellBit(Other)) != 0)
		{
			continue;
		}
		++OutVisible.NumPortalsTested;
		FBox2D Narrowed;
		if (Portal.NearBounds.IsInsideOrOn(EyeLocation))
		{
			// Standing in the doorway: the near plane may cut the quad away, but the next cell is in front.
			Narrowed = Rect;
		}
		else
		{
			FBox2D PortalRect;
			if (!ProjectPortal(Portal, ViewProjection, PortalRect) || !Overlap(Rect, PortalRect, Narrowed))
			{
				continue;
			}
		}
		Visit(Other, Narrowed, PathMask | CellBit(Other), Depth + 1, ViewProjection, EyeLocation, OutVisible);
	}
}

void FVisibilityCellGraph::FindVisibleCells(
	const FMatrix& ViewProjection, const FVector& EyeLocation, FVisibleCells& OutVisible) const
{
	OutVisible.Mask = ~uint64(0);
	OutVisible.EyeCell = FindCellAt(EyeLocation);
	OutVisible.NumPortalsTested = 0;
	if (OutVisible.EyeCell == INDEX_NONE)
	{
		// No cells, or the eye outside them all (above the map, a free camera): everything.
		for (int32 Index = 0; Index < Cells.Num(); ++Index)
		{
			OutVisible.Rects[Index] = FullScreen();
		}
		return;
	}
	OutVisible.Mask = 0;
	Visit(OutVisible.EyeCell, FullScreen(), CellBit(OutVisible.EyeCell), 0, ViewProjection, EyeLocation, OutVisible);
}
