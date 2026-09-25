#include "AI/Navigation/NavigationSystem.h"

#include "BodyInstance.h"
#include "Debug/DebugDraw.h"
#include "Engine/Level.h"
#include "Physics/PhysScene.h"
#include "TriangleCollision.h"

namespace
{

	[[nodiscard]] bool IsFloorLikeBody(const FBodyInstance& InBody, float InCellSize)
	{
		const float Hz = FMath::Max(InBody.HalfExtents.Z, 0.1f);
		const float Horiz = FMath::Max(InBody.HalfExtents.X, InBody.HalfExtents.Y);
		// Unit plane scaled ~40x40x1 → hz=0.5 still floor-like by aspect (was wrongly a full-arena
		// blocker).
		if (Horiz / Hz >= 6.0f)
		{
			return true;
		}
		/** Half heights up to this (cm) are floor-like: a character steps over them. */
		constexpr float MaxFloorHalfHeight = 35.0f;
		if (InBody.HalfExtents.Z <= FMath::Max(MaxFloorHalfHeight, InCellSize * 0.75f))
		{
			return true;
		}
		return false;
	}

	[[nodiscard]] bool IsForcedNavBlockerTag(const ULevel& Level, SIZE_T MeshIndex)
	{
		if (MeshIndex >= Level.GetStaticMeshes().Num())
		{
			return false;
		}
		// Thin pads / volumes: keep as obstacle so paths go around (not climbable floor).
		return Level.GetStaticMeshes()[static_cast<int32>(MeshIndex)].Tag.Equals(
			NavTags::Blocker, ESearchCase::CaseSensitive);
	}

	/** Walkable for CMC (slopes) — must not carve a hole in the flat grid NavMesh. */
	[[nodiscard]] bool IsWalkableNavSurfaceTag(const ULevel& Level, SIZE_T MeshIndex)
	{
		if (MeshIndex >= Level.GetStaticMeshes().Num())
		{
			return false;
		}
		return Level.GetStaticMeshes()[static_cast<int32>(MeshIndex)].Tag.Equals(
			NavTags::Walkable, ESearchCase::CaseSensitive);
	}

	[[nodiscard]] bool ShouldSkipLevelMesh(const ULevel& Level, SIZE_T MeshIndex)
	{
		if (MeshIndex >= Level.GetStaticMeshes().Num())
		{
			return false;
		}
		// Arena floor plane only.
		return Level.GetStaticMeshes()[static_cast<int32>(MeshIndex)].EditorClass.Equals(
			"Plane", ESearchCase::CaseSensitive);
	}

	[[nodiscard]] bool BodyBlocksNavigation(
		const FBodyInstance& InBody, float FloorZ, float InCellSize, const ULevel* Level)
	{
		if (InBody.Type != EBodyType::Static)
		{
			return false;
		}
		if (Level != nullptr && ShouldSkipLevelMesh(*Level, InBody.LevelMeshIndex))
		{
			return false;
		}
		// NavWalkable (ramps): path across footprint; UCharacterMovementComponent climbs the mesh.
		if (Level != nullptr && IsWalkableNavSurfaceTag(*Level, InBody.LevelMeshIndex))
		{
			return false;
		}
		const float Bottom = InBody.Position.Z - InBody.HalfExtents.Z;
		const float Top = InBody.Position.Z + InBody.HalfExtents.Z;
		// Bodies overlapping the band a walking agent occupies above the floor (cm).
		constexpr float BandBottom = 5.0f;
		constexpr float BandTop = 220.0f;
		const bool bInHeightBand = Top > FloorZ + BandBottom && Bottom < FloorZ + BandTop;
		if (!bInHeightBand)
		{
			return false;
		}
		// FNavBlocker: thin slab may look floor-like by aspect but must block paths.
		if (Level != nullptr && IsForcedNavBlockerTag(*Level, InBody.LevelMeshIndex))
		{
			return true;
		}
		if (IsFloorLikeBody(InBody, InCellSize))
		{
			return false;
		}
		return true;
	}

	[[nodiscard]] bool AabbXYOverlapsPoint(
		float Cx, float Cy, float Inflate, float MinX, float MaxX, float MinY, float MaxY)
	{
		return Cx >= (MinX - Inflate) && Cx <= (MaxX + Inflate) && Cy >= (MinY - Inflate) && Cy <= (MaxY + Inflate);
	}

	[[nodiscard]] bool CellBlockedByBody(
		float Cx, float Cy, float CellHalf, float InAgentRadius, const FBodyInstance& InBody)
	{
		const float Inflate = InAgentRadius + CellHalf;
		return AabbXYOverlapsPoint(Cx, Cy, Inflate, InBody.Position.X - InBody.HalfExtents.X,
			InBody.Position.X + InBody.HalfExtents.X, InBody.Position.Y - InBody.HalfExtents.Y,
			InBody.Position.Y + InBody.HalfExtents.Y);
	}

	/** Tighter XY footprint from baked tris (rotated ramp) vs fat world AABB. */
	[[nodiscard]] bool CellBlockedByTriangleMesh(
		float Cx, float Cy, float CellHalf, float InAgentRadius, const FTriangleMeshCollision& InMesh)
	{
		const float Inflate = InAgentRadius + CellHalf;
		for (int32 I = 0; I + 2 < InMesh.Indices.Num(); I += 3)
		{
			const FVector& V0 = InMesh.Positions[static_cast<int32>(InMesh.Indices[I])];
			const FVector& V1 = InMesh.Positions[static_cast<int32>(InMesh.Indices[I + 1])];
			const FVector& V2 = InMesh.Positions[static_cast<int32>(InMesh.Indices[I + 2])];
			const float MinX = FMath::Min3(V0.X, V1.X, V2.X);
			const float MaxX = FMath::Max3(V0.X, V1.X, V2.X);
			const float MinY = FMath::Min3(V0.Y, V1.Y, V2.Y);
			const float MaxY = FMath::Max3(V0.Y, V1.Y, V2.Y);
			if (AabbXYOverlapsPoint(Cx, Cy, Inflate, MinX, MaxX, MinY, MaxY))
			{
				return true;
			}
		}
		return false;
	}

	struct AStarNode
	{
		int Ix = 0;
		int Iy = 0;
		float F = 0.0f;
	};

	/** Heap order for the open set: the lowest F on top. */
	struct AStarNodeLess
	{
		bool operator()(const AStarNode& A, const AStarNode& B) const
		{
			return A.F < B.F;
		}
	};

	[[nodiscard]] float Heuristic(int Ax, int Ay, int Bx, int By)
	{
		const float Dx = static_cast<float>(Ax - Bx);
		const float Dy = static_cast<float>(Ay - By);
		return FMath::Sqrt(Dx * Dx + Dy * Dy);
	}

	[[nodiscard]] int CellIndex(int InIx, int InIy, int Width)
	{
		return InIy * Width + InIx;
	}

} // namespace

void UNavigationSystem::Clear()
{
	Mesh = {};
	BlockerCount = 0;
	WalkableCellCount = 0;
}

void UNavigationSystem::BakeGrid(const FPhysScene& Physics, float FloorZ, float WalkBounds, const ULevel* Level)
{
	Clear();
	/** At least 1 m (cm). */
	const float Bounds = WalkBounds > 100.0f ? WalkBounds : 100.0f;
	const float Cell = CellSize;
	const int Dim = FMath::Max(4, static_cast<int>(FMath::CeilToFloat((Bounds * 2.0f) / Cell)));

	Mesh.OriginX = -Bounds;
	Mesh.OriginY = -Bounds;
	Mesh.CellSize = Cell;
	Mesh.FloorZ = FloorZ;
	Mesh.Width = Dim;
	Mesh.Depth = Dim;
	Mesh.Walkable.Init(1, Dim * Dim);

	const float CellHalf = Cell * 0.5f;
	struct FNavBlocker
	{
		const FBodyInstance* Body = nullptr;
		const FTriangleMeshCollision* TriMesh = nullptr;
	};
	TArray<FNavBlocker> Blockers;
	Blockers.Reserve(static_cast<SIZE_T>(Physics.GetBodies().Num()));
	const auto& TriMeshes = Physics.GetTriangleMeshes();
	for (int32 Bi = 0; Bi < Physics.GetBodies().Num(); ++Bi)
	{
		const FBodyInstance& LocalBody = Physics.GetBodies()[Bi];
		if (!BodyBlocksNavigation(LocalBody, FloorZ, Cell, Level))
		{
			continue;
		}
		FNavBlocker Blocker{};
		Blocker.Body = &LocalBody;
		if (LocalBody.CollisionShape == EBodyCollisionShape::TriangleMesh && Bi < TriMeshes.Num() &&
			TriMeshes[Bi].IsValid())
		{
			Blocker.TriMesh = &TriMeshes[Bi];
		}
		Blockers.Add(Blocker);
	}
	BlockerCount = static_cast<int>(Blockers.Num());

	int Walkable = 0;
	for (int LocalIy = 0; LocalIy < Dim; ++LocalIy)
	{
		for (int LocalIx = 0; LocalIx < Dim; ++LocalIx)
		{
			const FVector Center = Mesh.CellCenter(LocalIx, LocalIy);
			bool bBlocked = false;
			for (const FNavBlocker& Blocker : Blockers)
			{
				if (Blocker.TriMesh != nullptr)
				{
					if (CellBlockedByTriangleMesh(Center.X, Center.Y, CellHalf, AgentRadius, *Blocker.TriMesh))
					{
						bBlocked = true;
						break;
					}
				}
				else if (CellBlockedByBody(Center.X, Center.Y, CellHalf, AgentRadius, *Blocker.Body))
				{
					bBlocked = true;
					break;
				}
			}
			if (bBlocked)
			{
				Mesh.Walkable[CellIndex(LocalIx, LocalIy, Dim)] = 0;
			}
			else
			{
				++Walkable;
			}
		}
	}

	// Extra clearance dilation beyond per-sample inflate (agents larger than one cell).
	const int DilateRings = FMath::Max(0, static_cast<int>(FMath::CeilToFloat(AgentRadius / Cell)) - 1);
	if (DilateRings > 0)
	{
		TArray<uint8> Dilated = Mesh.Walkable;
		for (int LocalIy = 0; LocalIy < Dim; ++LocalIy)
		{
			for (int LocalIx = 0; LocalIx < Dim; ++LocalIx)
			{
				if (Mesh.Walkable[CellIndex(LocalIx, LocalIy, Dim)] == 0)
				{
					continue;
				}
				bool bNearBlocked = false;
				for (int Dy = -DilateRings; Dy <= DilateRings && !bNearBlocked; ++Dy)
				{
					for (int Dx = -DilateRings; Dx <= DilateRings; ++Dx)
					{
						const int Nx = LocalIx + Dx;
						const int Ny = LocalIy + Dy;
						if (Nx < 0 || Ny < 0 || Nx >= Dim || Ny >= Dim)
						{
							continue;
						}
						if (Mesh.Walkable[CellIndex(Nx, Ny, Dim)] == 0)
						{
							bNearBlocked = true;
							break;
						}
					}
				}
				if (bNearBlocked)
				{
					Dilated[CellIndex(LocalIx, LocalIy, Dim)] = 0;
				}
			}
		}
		Mesh.Walkable = MoveTemp(Dilated);
		Walkable = 0;
		for (uint8 W : Mesh.Walkable)
		{
			Walkable += W != 0 ? 1 : 0;
		}
	}
	WalkableCellCount = Walkable;
}

void UNavigationSystem::BuildFromPhysScene(const FPhysScene& Physics, float FloorZ, float WalkBounds)
{
	BakeGrid(Physics, FloorZ, WalkBounds, nullptr);
}

void UNavigationSystem::BuildFromLevel(const ULevel& Level, const FPhysScene& Physics, float FloorZ, float WalkBounds)
{
	BakeGrid(Physics, FloorZ, WalkBounds, &Level);
}

bool UNavigationSystem::ProjectPointToNavigation(const FVector& World, FVector& OutProjected) const
{
	if (!Mesh.IsValid())
	{
		return false;
	}
	int LocalIx = 0;
	int LocalIy = 0;
	if (!Mesh.WorldToCell(World.X, World.Y, LocalIx, LocalIy))
	{
		return false;
	}
	if (Mesh.IsWalkable(LocalIx, LocalIy))
	{
		OutProjected = Mesh.CellCenter(LocalIx, LocalIy);
		return true;
	}
	// Spiral search for nearest walkable cell.
	const int MaxR = FMath::Max(Mesh.Width, Mesh.Depth);
	for (int R = 1; R <= MaxR; ++R)
	{
		for (int Dy = -R; Dy <= R; ++Dy)
		{
			for (int Dx = -R; Dx <= R; ++Dx)
			{
				if (FMath::Abs(Dx) != R && FMath::Abs(Dy) != R)
				{
					continue;
				}
				const int Nx = LocalIx + Dx;
				const int Ny = LocalIy + Dy;
				if (Mesh.IsWalkable(Nx, Ny))
				{
					OutProjected = Mesh.CellCenter(Nx, Ny);
					return true;
				}
			}
		}
	}
	return false;
}

bool UNavigationSystem::FindPath(const FVector& Start, const FVector& End, TArray<FVector>& OutPath) const
{
	OutPath.Reset();
	if (!Mesh.IsValid() || WalkableCellCount <= 0)
	{
		return false;
	}

	FVector StartNav = FVector::ZeroVector;
	FVector EndNav = FVector::ZeroVector;
	if (!ProjectPointToNavigation(Start, StartNav) || !ProjectPointToNavigation(End, EndNav))
	{
		return false;
	}

	int Sx = 0;
	int Sy = 0;
	int Ex = 0;
	int Ey = 0;
	if (!Mesh.WorldToCell(StartNav.X, StartNav.Y, Sx, Sy) || !Mesh.WorldToCell(EndNav.X, EndNav.Y, Ex, Ey))
	{
		return false;
	}
	if (!Mesh.IsWalkable(Sx, Sy) || !Mesh.IsWalkable(Ex, Ey))
	{
		return false;
	}
	if (Sx == Ex && Sy == Ey)
	{
		OutPath.Add(EndNav);
		return true;
	}

	const int Width = Mesh.Width;
	const int Depth = Mesh.Depth;
	const int CellCount = Width * Depth;
	TArray<float> GScore;
	GScore.Init(TNumericLimits<float>::Max(), CellCount);
	TArray<int32> CameFrom;
	CameFrom.Init(-1, CellCount);
	TArray<uint8> Closed;
	Closed.Init(0, CellCount);

	TArray<AStarNode> Open;
	const int StartIdx = CellIndex(Sx, Sy, Width);
	GScore[StartIdx] = 0.0f;
	Open.HeapPush(AStarNode{Sx, Sy, Heuristic(Sx, Sy, Ex, Ey)}, AStarNodeLess());

	static constexpr int Dx[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
	static constexpr int Dy[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
	static constexpr float Cost[8] = {1.4142f, 1.0f, 1.4142f, 1.0f, 1.0f, 1.4142f, 1.0f, 1.4142f};

	bool bFound = false;
	while (Open.Num() > 0)
	{
		AStarNode Cur;
		Open.HeapPop(Cur, AStarNodeLess(), false);
		const int CurIdx = CellIndex(Cur.Ix, Cur.Iy, Width);
		if (Closed[CurIdx] != 0)
		{
			continue;
		}
		Closed[CurIdx] = 1;
		if (Cur.Ix == Ex && Cur.Iy == Ey)
		{
			bFound = true;
			break;
		}

		for (int I = 0; I < 8; ++I)
		{
			const int Nx = Cur.Ix + Dx[I];
			const int Ny = Cur.Iy + Dy[I];
			if (!Mesh.IsWalkable(Nx, Ny))
			{
				continue;
			}
			// No corner-cutting through blocked diagonals.
			if (Dx[I] != 0 && Dy[I] != 0)
			{
				if (!Mesh.IsWalkable(Cur.Ix + Dx[I], Cur.Iy) || !Mesh.IsWalkable(Cur.Ix, Cur.Iy + Dy[I]))
				{
					continue;
				}
			}
			const int NIdx = CellIndex(Nx, Ny, Width);
			if (Closed[NIdx] != 0)
			{
				continue;
			}
			const float Tentative = GScore[CurIdx] + Cost[I];
			if (Tentative >= GScore[NIdx])
			{
				continue;
			}
			CameFrom[NIdx] = CurIdx;
			GScore[NIdx] = Tentative;
			Open.HeapPush(AStarNode{Nx, Ny, Tentative + Heuristic(Nx, Ny, Ex, Ey)}, AStarNodeLess());
		}
	}

	if (!bFound)
	{
		return false;
	}

	TArray<FVector> Reverse;
	int Idx = CellIndex(Ex, Ey, Width);
	while (Idx >= 0)
	{
		const int LocalIx = Idx % Width;
		const int LocalIy = Idx / Width;
		Reverse.Add(Mesh.CellCenter(LocalIx, LocalIy));
		Idx = CameFrom[Idx];
	}
	OutPath.Reset(Reverse.Num());
	for (int32 I = Reverse.Num() - 1; I >= 0; --I)
	{
		OutPath.Add(Reverse[I]);
	}
	if (OutPath.Num() > 0)
	{
		OutPath.Last() = EndNav;
	}
	return OutPath.Num() > 0;
}

void UNavigationSystem::AppendDebugDraw(FDebugDraw& Draw) const
{
	if (!Mesh.IsValid())
	{
		return;
	}

	/** cm above the floor, against z-fighting. */
	const float Z = Mesh.FloorZ + 4.0f;
	const float Half = Mesh.CellSize * 0.5f;
	constexpr FLinearColor Walkable(0.15f, 0.85f, 0.35f);
	constexpr FLinearColor Blocked(0.95f, 0.2f, 0.15f);

	for (int LocalIy = 0; LocalIy < Mesh.Depth; ++LocalIy)
	{
		for (int LocalIx = 0; LocalIx < Mesh.Width; ++LocalIx)
		{
			const FVector Center = Mesh.CellCenter(LocalIx, LocalIy);
			const float X0 = Center.X - Half;
			const float X1 = Center.X + Half;
			const float Y0 = Center.Y - Half;
			const float Y1 = Center.Y + Half;
			const FLinearColor& Color = Mesh.IsWalkable(LocalIx, LocalIy) ? Walkable : Blocked;
			Draw.AddLine(FVector(X0, Y0, Z), FVector(X1, Y0, Z), Color);
			Draw.AddLine(FVector(X1, Y0, Z), FVector(X1, Y1, Z), Color);
			Draw.AddLine(FVector(X1, Y1, Z), FVector(X0, Y1, Z), Color);
			Draw.AddLine(FVector(X0, Y1, Z), FVector(X0, Y0, Z), Color);
			if (!Mesh.IsWalkable(LocalIx, LocalIy))
			{
				Draw.AddLine(FVector(X0, Y0, Z), FVector(X1, Y1, Z), Color);
				Draw.AddLine(FVector(X1, Y0, Z), FVector(X0, Y1, Z), Color);
			}
		}
	}
}
