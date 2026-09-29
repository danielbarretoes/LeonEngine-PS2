#pragma once

#include "CoreMinimal.h"

/**
 * The cells and portals of a map (Leon, Docs/PLANS/ps2-shipping.md N15; UE 4.27 has no portals, its precomputed
 * visibility volumes play the part): a cell is a box of the map (a glTF `VIS_<Cell>` node's), a portal a quad joining
 * two cells (`PORTAL_<CellA>_<CellB>`). What a view sees is found from the cell the eye is in, through the portals
 * whose quad the view sees, each narrowing the screen rectangle the cells behind it are seen through (the classic
 * portal rendering of Luebke and Georges, with rectangles). A primitive draws when one of the cells its bounds touch is
 * visible, or when it touches none (it is outside every cell).
 *
 * A map without cells, or an eye in no cell, sees everything.
 */
class RENDERCORE_API FVisibilityCellGraph
{
public:
	/** Cells a mask holds (a primitive's cells are a 64-bit mask). */
	static constexpr int32 MaxCells = 64;
	/** A portal closer to the eye than this (cm) is open whole: the view is passing through it. */
	static constexpr float PortalNearDistance = 60.0f;

	/** A cell: its name (the node's suffix) and its box in the world. */
	struct FCell
	{
		FName Name;
		FBox Bounds = FBox(ForceInit);
	};

	/** A portal: the two cells it joins and its quad's corners in the world (in order around it). */
	struct FPortal
	{
		int32 CellA = INDEX_NONE;
		int32 CellB = INDEX_NONE;
		FVector Corners[4];
		/** The quad's box, grown by PortalNearDistance: an eye inside it opens the portal whole. */
		FBox NearBounds = FBox(ForceInit);
	};

	/** What a view sees: the visible cells, and the screen rectangle (normalized device x, y) each is seen through. */
	struct FVisibleCells
	{
		/** A bit per visible cell; every bit when the eye is in no cell (or the map has none). */
		uint64 Mask = ~uint64(0);
		/** The eye's cell, INDEX_NONE for none. */
		int32 EyeCell = INDEX_NONE;
		/** Per visible cell, the union of the rectangles it is seen through ([-1, 1] x [-1, 1] for the eye's). */
		FBox2D Rects[MaxCells];
		/** Portals the walk projected (a cost measure). */
		int32 NumPortalsTested = 0;
	};

	void Reset();
	[[nodiscard]] bool IsEmpty() const
	{
		return Cells.Num() == 0;
	}
	[[nodiscard]] const TArray<FCell>& GetCells() const
	{
		return Cells;
	}
	[[nodiscard]] const TArray<FPortal>& GetPortals() const
	{
		return Portals;
	}

	/** Adds a cell; its index, or INDEX_NONE when MaxCells are there or the name is taken. */
	int32 AddCell(FName Name, const FBox& Bounds);
	/** The index of the cell named Name, or INDEX_NONE. */
	[[nodiscard]] int32 FindCell(FName Name) const;
	/** Adds a portal between two cells; false for a cell that is not there or a portal to itself. */
	bool AddPortal(int32 CellA, int32 CellB, const FVector (&Corners)[4]);

	/** The cell a point is in (the smallest box that holds it), or INDEX_NONE. */
	[[nodiscard]] int32 FindCellAt(const FVector& Point) const;
	/** The cells a box touches, a bit each (0: none, outside every cell). */
	[[nodiscard]] uint64 GetCellMask(const FBox& Bounds) const;

	/**
	 * The cells a view sees: from the cell of EyeLocation, through every portal whose quad (clipped to what is in
	 * front of the eye) projects into the rectangle the cell was reached through, the next cell with the intersection
	 * of the two; a cell is not entered twice on one path. ViewProjection takes the world to clip space with w the
	 * view's depth (UE's projection).
	 */
	void FindVisibleCells(const FMatrix& ViewProjection, const FVector& EyeLocation, FVisibleCells& OutVisible) const;

	/** Whether a primitive of CellMask (GetCellMask) is seen: it is outside every cell, or one of its cells is visible.
	 */
	[[nodiscard]] static bool IsVisible(uint64 CellMask, const FVisibleCells& Visible)
	{
		return CellMask == 0 || (CellMask & Visible.Mask) != 0;
	}

private:
	/** The screen rectangle of a portal's quad, false when none of it is in front of the eye. */
	[[nodiscard]] static bool ProjectPortal(const FPortal& Portal, const FMatrix& ViewProjection, FBox2D& OutRect);
	void Visit(int32 Cell, const FBox2D& Rect, uint64 PathMask, int32 Depth, const FMatrix& ViewProjection,
		const FVector& EyeLocation, FVisibleCells& OutVisible) const;

	TArray<FCell> Cells;
	TArray<FPortal> Portals;
};
