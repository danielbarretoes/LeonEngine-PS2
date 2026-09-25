#pragma once

#include "CoreMinimal.h"

/** Baked walkable grid (Unreal NavMesh lite — no Recast). XZ cells + floor height. */
struct ENGINE_API FNavMesh
{
	float OriginX = 0.0f;
	float OriginZ = 0.0f;
	/** cm */
	float CellSize = 50.0f;
	float FloorY = 0.0f;
	int Width = 0;
	int Depth = 0;
	/** Row-major: index = iz * width + ix. true = walkable. */
	TArray<uint8> Walkable;

	[[nodiscard]] bool IsValid() const
	{
		return Width > 0 && Depth > 0 && Walkable.Num() > 0;
	}

	[[nodiscard]] bool InBounds(int Ix, int Iz) const
	{
		return Ix >= 0 && Iz >= 0 && Ix < Width && Iz < Depth;
	}

	[[nodiscard]] bool IsWalkable(int Ix, int Iz) const
	{
		return InBounds(Ix, Iz) && Walkable[Iz * Width + Ix] != 0;
	}

	[[nodiscard]] FVector CellCenter(int Ix, int Iz) const
	{
		return {OriginX + (static_cast<float>(Ix) + 0.5f) * CellSize, FloorY,
			OriginZ + (static_cast<float>(Iz) + 0.5f) * CellSize};
	}

	/** Nearest cell indices for a world XZ point (clamped). Returns false if mesh empty. */
	[[nodiscard]] bool WorldToCell(float X, float Z, int& OutIx, int& OutIz) const;
};
