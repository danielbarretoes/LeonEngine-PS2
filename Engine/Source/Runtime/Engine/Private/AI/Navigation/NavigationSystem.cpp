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
		const float Hy = FMath::Max(InBody.HalfExtents.Y, 0.001f);
		const float Horiz = FMath::Max(InBody.HalfExtents.X, InBody.HalfExtents.Z);
		// Unit plane scaled ~40x1x40 → hy=0.5 still floor-like by aspect (was wrongly a full-arena
		// blocker).
		if (Horiz / Hy >= 6.0f)
		{
			return true;
		}
		if (InBody.HalfExtents.Y <= FMath::Max(0.35f, InCellSize * 0.75f))
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
		const FBodyInstance& InBody, float FloorY, float InCellSize, const ULevel* Level)
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
		const float Bottom = InBody.Position.Y - InBody.HalfExtents.Y;
		const float Top = InBody.Position.Y + InBody.HalfExtents.Y;
		const bool bInHeightBand = Top > FloorY + 0.05f && Bottom < FloorY + 2.2f;
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

	[[nodiscard]] bool AabbXZOverlapsPoint(
		float Cx, float Cz, float Inflate, float MinX, float MaxX, float MinZ, float MaxZ)
	{
		return Cx >= (MinX - Inflate) && Cx <= (MaxX + Inflate) && Cz >= (MinZ - Inflate) && Cz <= (MaxZ + Inflate);
	}

	[[nodiscard]] bool CellBlockedByBody(
		float Cx, float Cz, float CellHalf, float InAgentRadius, const FBodyInstance& InBody)
	{
		const float Inflate = InAgentRadius + CellHalf;
		return AabbXZOverlapsPoint(Cx, Cz, Inflate, InBody.Position.X - InBody.HalfExtents.X,
			InBody.Position.X + InBody.HalfExtents.X, InBody.Position.Z - InBody.HalfExtents.Z,
			InBody.Position.Z + InBody.HalfExtents.Z);
	}

	/** Tighter XZ footprint from baked tris (rotated ramp) vs fat world AABB. */
	[[nodiscard]] bool CellBlockedByTriangleMesh(
		float Cx, float Cz, float CellHalf, float InAgentRadius, const FTriangleMeshCollision& InMesh)
	{
		const float Inflate = InAgentRadius + CellHalf;
		for (int32 I = 0; I + 2 < InMesh.Indices.Num(); I += 3)
		{
			const FVector& V0 = InMesh.Positions[static_cast<int32>(InMesh.Indices[I])];
			const FVector& V1 = InMesh.Positions[static_cast<int32>(InMesh.Indices[I + 1])];
			const FVector& V2 = InMesh.Positions[static_cast<int32>(InMesh.Indices[I + 2])];
			const float MinX = FMath::Min3(V0.X, V1.X, V2.X);
			const float MaxX = FMath::Max3(V0.X, V1.X, V2.X);
			const float MinZ = FMath::Min3(V0.Z, V1.Z, V2.Z);
			const float MaxZ = FMath::Max3(V0.Z, V1.Z, V2.Z);
			if (AabbXZOverlapsPoint(Cx, Cz, Inflate, MinX, MaxX, MinZ, MaxZ))
			{
				return true;
			}
		}
		return false;
	}

	struct AStarNode
	{
		int Ix = 0;
		int Iz = 0;
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

	[[nodiscard]] float Heuristic(int Ax, int Az, int Bx, int Bz)
	{
		const float Dx = static_cast<float>(Ax - Bx);
		const float Dz = static_cast<float>(Az - Bz);
		return FMath::Sqrt(Dx * Dx + Dz * Dz);
	}

	[[nodiscard]] int CellIndex(int InIx, int InIz, int Width)
	{
		return InIz * Width + InIx;
	}

} // namespace

void UNavigationSystem::Clear()
{
	Mesh = {};
	BlockerCount = 0;
	WalkableCellCount = 0;
}

void UNavigationSystem::BakeGrid(const FPhysScene& Physics, float FloorY, float WalkBounds, const ULevel* Level)
{
	Clear();
	const float Bounds = WalkBounds > 1.0f ? WalkBounds : 1.0f;
	const float Cell = CellSize;
	const int Dim = FMath::Max(4, static_cast<int>(FMath::CeilToFloat((Bounds * 2.0f) / Cell)));

	Mesh.OriginX = -Bounds;
	Mesh.OriginZ = -Bounds;
	Mesh.CellSize = Cell;
	Mesh.FloorY = FloorY;
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
		if (!BodyBlocksNavigation(LocalBody, FloorY, Cell, Level))
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
	for (int LocalIz = 0; LocalIz < Dim; ++LocalIz)
	{
		for (int LocalIx = 0; LocalIx < Dim; ++LocalIx)
		{
			const FVector Center = Mesh.CellCenter(LocalIx, LocalIz);
			bool bBlocked = false;
			for (const FNavBlocker& Blocker : Blockers)
			{
				if (Blocker.TriMesh != nullptr)
				{
					if (CellBlockedByTriangleMesh(Center.X, Center.Z, CellHalf, AgentRadius, *Blocker.TriMesh))
					{
						bBlocked = true;
						break;
					}
				}
				else if (CellBlockedByBody(Center.X, Center.Z, CellHalf, AgentRadius, *Blocker.Body))
				{
					bBlocked = true;
					break;
				}
			}
			if (bBlocked)
			{
				Mesh.Walkable[CellIndex(LocalIx, LocalIz, Dim)] = 0;
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
		for (int LocalIz = 0; LocalIz < Dim; ++LocalIz)
		{
			for (int LocalIx = 0; LocalIx < Dim; ++LocalIx)
			{
				if (Mesh.Walkable[CellIndex(LocalIx, LocalIz, Dim)] == 0)
				{
					continue;
				}
				bool bNearBlocked = false;
				for (int Dz = -DilateRings; Dz <= DilateRings && !bNearBlocked; ++Dz)
				{
					for (int Dx = -DilateRings; Dx <= DilateRings; ++Dx)
					{
						const int Nx = LocalIx + Dx;
						const int Nz = LocalIz + Dz;
						if (Nx < 0 || Nz < 0 || Nx >= Dim || Nz >= Dim)
						{
							continue;
						}
						if (Mesh.Walkable[CellIndex(Nx, Nz, Dim)] == 0)
						{
							bNearBlocked = true;
							break;
						}
					}
				}
				if (bNearBlocked)
				{
					Dilated[CellIndex(LocalIx, LocalIz, Dim)] = 0;
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

void UNavigationSystem::BuildFromPhysScene(const FPhysScene& Physics, float FloorY, float WalkBounds)
{
	BakeGrid(Physics, FloorY, WalkBounds, nullptr);
}

void UNavigationSystem::BuildFromLevel(const ULevel& Level, const FPhysScene& Physics, float FloorY, float WalkBounds)
{
	BakeGrid(Physics, FloorY, WalkBounds, &Level);
}

bool UNavigationSystem::ProjectPointToNavigation(const FVector& World, FVector& OutProjected) const
{
	if (!Mesh.IsValid())
	{
		return false;
	}
	int LocalIx = 0;
	int LocalIz = 0;
	if (!Mesh.WorldToCell(World.X, World.Z, LocalIx, LocalIz))
	{
		return false;
	}
	if (Mesh.IsWalkable(LocalIx, LocalIz))
	{
		OutProjected = Mesh.CellCenter(LocalIx, LocalIz);
		return true;
	}
	// Spiral search for nearest walkable cell.
	const int MaxR = FMath::Max(Mesh.Width, Mesh.Depth);
	for (int R = 1; R <= MaxR; ++R)
	{
		for (int Dz = -R; Dz <= R; ++Dz)
		{
			for (int Dx = -R; Dx <= R; ++Dx)
			{
				if (FMath::Abs(Dx) != R && FMath::Abs(Dz) != R)
				{
					continue;
				}
				const int Nx = LocalIx + Dx;
				const int Nz = LocalIz + Dz;
				if (Mesh.IsWalkable(Nx, Nz))
				{
					OutProjected = Mesh.CellCenter(Nx, Nz);
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
	int Sz = 0;
	int Ex = 0;
	int Ez = 0;
	if (!Mesh.WorldToCell(StartNav.X, StartNav.Z, Sx, Sz) || !Mesh.WorldToCell(EndNav.X, EndNav.Z, Ex, Ez))
	{
		return false;
	}
	if (!Mesh.IsWalkable(Sx, Sz) || !Mesh.IsWalkable(Ex, Ez))
	{
		return false;
	}
	if (Sx == Ex && Sz == Ez)
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
	const int StartIdx = CellIndex(Sx, Sz, Width);
	GScore[StartIdx] = 0.0f;
	Open.HeapPush(AStarNode{Sx, Sz, Heuristic(Sx, Sz, Ex, Ez)}, AStarNodeLess());

	static constexpr int Dx[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
	static constexpr int Dz[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
	static constexpr float Cost[8] = {1.4142f, 1.0f, 1.4142f, 1.0f, 1.0f, 1.4142f, 1.0f, 1.4142f};

	bool bFound = false;
	while (Open.Num() > 0)
	{
		AStarNode Cur;
		Open.HeapPop(Cur, AStarNodeLess(), false);
		const int CurIdx = CellIndex(Cur.Ix, Cur.Iz, Width);
		if (Closed[CurIdx] != 0)
		{
			continue;
		}
		Closed[CurIdx] = 1;
		if (Cur.Ix == Ex && Cur.Iz == Ez)
		{
			bFound = true;
			break;
		}

		for (int I = 0; I < 8; ++I)
		{
			const int Nx = Cur.Ix + Dx[I];
			const int Nz = Cur.Iz + Dz[I];
			if (!Mesh.IsWalkable(Nx, Nz))
			{
				continue;
			}
			// No corner-cutting through blocked diagonals.
			if (Dx[I] != 0 && Dz[I] != 0)
			{
				if (!Mesh.IsWalkable(Cur.Ix + Dx[I], Cur.Iz) || !Mesh.IsWalkable(Cur.Ix, Cur.Iz + Dz[I]))
				{
					continue;
				}
			}
			const int NIdx = CellIndex(Nx, Nz, Width);
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
			Open.HeapPush(AStarNode{Nx, Nz, Tentative + Heuristic(Nx, Nz, Ex, Ez)}, AStarNodeLess());
		}
	}

	if (!bFound)
	{
		return false;
	}

	TArray<FVector> Reverse;
	int Idx = CellIndex(Ex, Ez, Width);
	while (Idx >= 0)
	{
		const int LocalIx = Idx % Width;
		const int LocalIz = Idx / Width;
		Reverse.Add(Mesh.CellCenter(LocalIx, LocalIz));
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

	const float Y = Mesh.FloorY + 0.04f;
	const float Half = Mesh.CellSize * 0.5f;
	constexpr FLinearColor Walkable(0.15f, 0.85f, 0.35f);
	constexpr FLinearColor Blocked(0.95f, 0.2f, 0.15f);

	for (int LocalIz = 0; LocalIz < Mesh.Depth; ++LocalIz)
	{
		for (int LocalIx = 0; LocalIx < Mesh.Width; ++LocalIx)
		{
			const FVector Center = Mesh.CellCenter(LocalIx, LocalIz);
			const float X0 = Center.X - Half;
			const float X1 = Center.X + Half;
			const float Z0 = Center.Z - Half;
			const float Z1 = Center.Z + Half;
			const FLinearColor& Color = Mesh.IsWalkable(LocalIx, LocalIz) ? Walkable : Blocked;
			Draw.AddLine(FVector(X0, Y, Z0), FVector(X1, Y, Z0), Color);
			Draw.AddLine(FVector(X1, Y, Z0), FVector(X1, Y, Z1), Color);
			Draw.AddLine(FVector(X1, Y, Z1), FVector(X0, Y, Z1), Color);
			Draw.AddLine(FVector(X0, Y, Z1), FVector(X0, Y, Z0), Color);
			if (!Mesh.IsWalkable(LocalIx, LocalIz))
			{
				Draw.AddLine(FVector(X0, Y, Z0), FVector(X1, Y, Z1), Color);
				Draw.AddLine(FVector(X1, Y, Z0), FVector(X0, Y, Z1), Color);
			}
		}
	}
}
