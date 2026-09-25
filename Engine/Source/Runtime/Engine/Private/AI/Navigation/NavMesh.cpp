#include "AI/Navigation/NavMesh.h"

bool FNavMesh::WorldToCell(float X, float Y, int& OutIx, int& OutIy) const
{
	if (!IsValid() || CellSize <= 0.0f)
	{
		return false;
	}
	OutIx = static_cast<int>(FMath::FloorToFloat((X - OriginX) / CellSize));
	OutIy = static_cast<int>(FMath::FloorToFloat((Y - OriginY) / CellSize));
	if (OutIx < 0)
	{
		OutIx = 0;
	}
	if (OutIy < 0)
	{
		OutIy = 0;
	}
	if (OutIx >= Width)
	{
		OutIx = Width - 1;
	}
	if (OutIy >= Depth)
	{
		OutIy = Depth - 1;
	}
	return true;
}
