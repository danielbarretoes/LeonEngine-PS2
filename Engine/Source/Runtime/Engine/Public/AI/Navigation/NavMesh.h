#pragma once

#include "CoreMinimal.h"

/** Baked walkable grid (Unreal NavMesh lite — no Recast). XY cells + floor height. */
struct ENGINE_API FNavMesh
{
	float OriginX = 0.0f;
	float OriginY = 0.0f;
	/** cm */
	float CellSize = 50.0f;
	/** Height of the grid (cm). */
	float FloorZ = 0.0f;
	int Width = 0;
	int Depth = 0;
	/** Row-major: index = iy * width + ix. true = walkable. */
	TArray<uint8> Walkable;

	[[nodiscard]] bool IsValid() const
	{
		return Width > 0 && Depth > 0 && Walkable.Num() > 0;
	}

	[[nodiscard]] bool InBounds(int Ix, int Iy) const
	{
		return Ix >= 0 && Iy >= 0 && Ix < Width && Iy < Depth;
	}

	[[nodiscard]] bool IsWalkable(int Ix, int Iy) const
	{
		return InBounds(Ix, Iy) && Walkable[Iy * Width + Ix] != 0;
	}

	[[nodiscard]] FVector CellCenter(int Ix, int Iy) const
	{
		return {OriginX + (static_cast<float>(Ix) + 0.5f) * CellSize,
			OriginY + (static_cast<float>(Iy) + 0.5f) * CellSize, FloorZ};
	}

	/** Nearest cell indices for a world XY point (clamped). Returns false if mesh empty. */
	[[nodiscard]] bool WorldToCell(float X, float Y, int& OutIx, int& OutIy) const;
};
