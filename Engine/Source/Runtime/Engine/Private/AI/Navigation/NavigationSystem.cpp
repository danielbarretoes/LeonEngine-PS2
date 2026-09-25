#include "AI/Navigation/NavigationSystem.h"

#include "BodyInstance.h"
#include "Debug/DebugDraw.h"
#include "Engine/Level.h"
#include "Physics/PhysScene.h"
#include "TriangleCollision.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <queue>
#include <vector>

namespace
{

	[[nodiscard]] bool IsFloorLikeBody(const FBodyInstance& InBody, float InCellSize)
	{
		const float Hy = std::max(InBody.HalfExtents.Y, 0.001f);
		const float Horiz = std::max(InBody.HalfExtents.X, InBody.HalfExtents.Z);
		// Unit plane scaled ~40x1x40 → hy=0.5 still floor-like by aspect (was wrongly a full-arena
		// blocker).
		if (Horiz / Hy >= 6.0f)
		{
			return true;
		}
		if (InBody.HalfExtents.Y <= std::max(0.35f, InCellSize * 0.75f))
		{
			return true;
		}
		return false;
	}

	[[nodiscard]] bool IsForcedNavBlockerTag(const ULevel& Level, std::size_t MeshIndex)
	{
		if (MeshIndex >= Level.GetStaticMeshes().size())
		{
			return false;
		}
		// Thin pads / volumes: keep as obstacle so paths go around (not climbable floor).
		return Level.GetStaticMeshes()[MeshIndex].Tag == NavTags::Blocker;
	}

	/// Walkable for CMC (slopes) — must not carve a hole in the flat grid NavMesh.
	[[nodiscard]] bool IsWalkableNavSurfaceTag(const ULevel& Level, std::size_t MeshIndex)
	{
		if (MeshIndex >= Level.GetStaticMeshes().size())
		{
			return false;
		}
		return Level.GetStaticMeshes()[MeshIndex].Tag == NavTags::Walkable;
	}

	[[nodiscard]] bool ShouldSkipLevelMesh(const ULevel& Level, std::size_t MeshIndex)
	{
		if (MeshIndex >= Level.GetStaticMeshes().size())
		{
			return false;
		}
		// Arena floor plane only.
		return Level.GetStaticMeshes()[MeshIndex].EditorClass == "Plane";
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

	/// Tighter XZ footprint from baked tris (rotated ramp) vs fat world AABB.
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

	struct AStarNodeGreater
	{
		bool operator()(const AStarNode& A, const AStarNode& B) const
		{
			return A.F > B.F;
		}
	};

	[[nodiscard]] float Heuristic(int Ax, int Az, int Bx, int Bz)
	{
		const float Dx = static_cast<float>(Ax - Bx);
		const float Dz = static_cast<float>(Az - Bz);
		return std::sqrt(Dx * Dx + Dz * Dz);
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
	const int Dim = std::max(4, static_cast<int>(std::ceil((Bounds * 2.0f) / Cell)));

	Mesh.OriginX = -Bounds;
	Mesh.OriginZ = -Bounds;
	Mesh.CellSize = Cell;
	Mesh.FloorY = FloorY;
	Mesh.Width = Dim;
	Mesh.Depth = Dim;
	Mesh.Walkable.assign(static_cast<std::size_t>(Dim * Dim), 1);

	const float CellHalf = Cell * 0.5f;
	struct FNavBlocker
	{
		const FBodyInstance* Body = nullptr;
		const FTriangleMeshCollision* TriMesh = nullptr;
	};
	std::vector<FNavBlocker> Blockers;
	Blockers.reserve(static_cast<std::size_t>(Physics.GetBodies().Num()));
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
		Blockers.push_back(Blocker);
	}
	BlockerCount = static_cast<int>(Blockers.size());

	int Walkable = 0;
	for (int LocalIz = 0; LocalIz < Dim; ++LocalIz)
	{
		for (int LocalIx = 0; LocalIx < Dim; ++LocalIx)
		{
			const glm::vec3 Center = Mesh.CellCenter(LocalIx, LocalIz);
			bool bBlocked = false;
			for (const FNavBlocker& Blocker : Blockers)
			{
				if (Blocker.TriMesh != nullptr)
				{
					if (CellBlockedByTriangleMesh(Center.x, Center.z, CellHalf, AgentRadius, *Blocker.TriMesh))
					{
						bBlocked = true;
						break;
					}
				}
				else if (CellBlockedByBody(Center.x, Center.z, CellHalf, AgentRadius, *Blocker.Body))
				{
					bBlocked = true;
					break;
				}
			}
			if (bBlocked)
			{
				Mesh.Walkable[static_cast<std::size_t>(CellIndex(LocalIx, LocalIz, Dim))] = 0;
			}
			else
			{
				++Walkable;
			}
		}
	}

	// Extra clearance dilation beyond per-sample inflate (agents larger than one cell).
	const int DilateRings = std::max(0, static_cast<int>(std::ceil(AgentRadius / Cell)) - 1);
	if (DilateRings > 0)
	{
		std::vector<std::uint8_t> Dilated = Mesh.Walkable;
		for (int LocalIz = 0; LocalIz < Dim; ++LocalIz)
		{
			for (int LocalIx = 0; LocalIx < Dim; ++LocalIx)
			{
				if (Mesh.Walkable[static_cast<std::size_t>(CellIndex(LocalIx, LocalIz, Dim))] == 0)
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
						if (Mesh.Walkable[static_cast<std::size_t>(CellIndex(Nx, Nz, Dim))] == 0)
						{
							bNearBlocked = true;
							break;
						}
					}
				}
				if (bNearBlocked)
				{
					Dilated[static_cast<std::size_t>(CellIndex(LocalIx, LocalIz, Dim))] = 0;
				}
			}
		}
		Mesh.Walkable.swap(Dilated);
		Walkable = 0;
		for (std::uint8_t W : Mesh.Walkable)
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

bool UNavigationSystem::ProjectPointToNavigation(const glm::vec3& World, glm::vec3& OutProjected) const
{
	if (!Mesh.IsValid())
	{
		return false;
	}
	int LocalIx = 0;
	int LocalIz = 0;
	if (!Mesh.WorldToCell(World.x, World.z, LocalIx, LocalIz))
	{
		return false;
	}
	if (Mesh.IsWalkable(LocalIx, LocalIz))
	{
		OutProjected = Mesh.CellCenter(LocalIx, LocalIz);
		return true;
	}
	// Spiral search for nearest walkable cell.
	const int MaxR = std::max(Mesh.Width, Mesh.Depth);
	for (int R = 1; R <= MaxR; ++R)
	{
		for (int Dz = -R; Dz <= R; ++Dz)
		{
			for (int Dx = -R; Dx <= R; ++Dx)
			{
				if (std::abs(Dx) != R && std::abs(Dz) != R)
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

bool UNavigationSystem::FindPath(const glm::vec3& Start, const glm::vec3& End, std::vector<glm::vec3>& OutPath) const
{
	OutPath.clear();
	if (!Mesh.IsValid() || WalkableCellCount <= 0)
	{
		return false;
	}

	glm::vec3 StartNav{};
	glm::vec3 EndNav{};
	if (!ProjectPointToNavigation(Start, StartNav) || !ProjectPointToNavigation(End, EndNav))
	{
		return false;
	}

	int Sx = 0;
	int Sz = 0;
	int Ex = 0;
	int Ez = 0;
	if (!Mesh.WorldToCell(StartNav.x, StartNav.z, Sx, Sz) || !Mesh.WorldToCell(EndNav.x, EndNav.z, Ex, Ez))
	{
		return false;
	}
	if (!Mesh.IsWalkable(Sx, Sz) || !Mesh.IsWalkable(Ex, Ez))
	{
		return false;
	}
	if (Sx == Ex && Sz == Ez)
	{
		OutPath.push_back(EndNav);
		return true;
	}

	const int Width = Mesh.Width;
	const int Depth = Mesh.Depth;
	const int CellCount = Width * Depth;
	std::vector<float> GScore(static_cast<std::size_t>(CellCount), std::numeric_limits<float>::infinity());
	std::vector<int> CameFrom(static_cast<std::size_t>(CellCount), -1);
	std::vector<std::uint8_t> Closed(static_cast<std::size_t>(CellCount), 0);

	std::priority_queue<AStarNode, std::vector<AStarNode>, AStarNodeGreater> Open;
	const int StartIdx = CellIndex(Sx, Sz, Width);
	GScore[static_cast<std::size_t>(StartIdx)] = 0.0f;
	Open.push(AStarNode{Sx, Sz, Heuristic(Sx, Sz, Ex, Ez)});

	static constexpr int Dx[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
	static constexpr int Dz[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
	static constexpr float Cost[8] = {1.4142f, 1.0f, 1.4142f, 1.0f, 1.0f, 1.4142f, 1.0f, 1.4142f};

	bool bFound = false;
	while (!Open.empty())
	{
		const AStarNode Cur = Open.top();
		Open.pop();
		const int CurIdx = CellIndex(Cur.Ix, Cur.Iz, Width);
		if (Closed[static_cast<std::size_t>(CurIdx)] != 0)
		{
			continue;
		}
		Closed[static_cast<std::size_t>(CurIdx)] = 1;
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
			if (Closed[static_cast<std::size_t>(NIdx)] != 0)
			{
				continue;
			}
			const float Tentative = GScore[static_cast<std::size_t>(CurIdx)] + Cost[I];
			if (Tentative >= GScore[static_cast<std::size_t>(NIdx)])
			{
				continue;
			}
			CameFrom[static_cast<std::size_t>(NIdx)] = CurIdx;
			GScore[static_cast<std::size_t>(NIdx)] = Tentative;
			Open.push(AStarNode{Nx, Nz, Tentative + Heuristic(Nx, Nz, Ex, Ez)});
		}
	}

	if (!bFound)
	{
		return false;
	}

	std::vector<glm::vec3> Reverse;
	int Idx = CellIndex(Ex, Ez, Width);
	while (Idx >= 0)
	{
		const int LocalIx = Idx % Width;
		const int LocalIz = Idx / Width;
		Reverse.push_back(Mesh.CellCenter(LocalIx, LocalIz));
		Idx = CameFrom[static_cast<std::size_t>(Idx)];
	}
	std::reverse(Reverse.begin(), Reverse.end());
	if (!Reverse.empty())
	{
		Reverse.back() = EndNav;
	}
	OutPath = std::move(Reverse);
	return !OutPath.empty();
}

void UNavigationSystem::AppendDebugDraw(FDebugDraw& Draw) const
{
	if (!Mesh.IsValid())
	{
		return;
	}

	const float Y = Mesh.FloorY + 0.04f;
	const float Half = Mesh.CellSize * 0.5f;
	constexpr glm::vec3 Walkable{0.15f, 0.85f, 0.35f};
	constexpr glm::vec3 Blocked{0.95f, 0.2f, 0.15f};

	for (int LocalIz = 0; LocalIz < Mesh.Depth; ++LocalIz)
	{
		for (int LocalIx = 0; LocalIx < Mesh.Width; ++LocalIx)
		{
			const glm::vec3 Center = Mesh.CellCenter(LocalIx, LocalIz);
			const float X0 = Center.x - Half;
			const float X1 = Center.x + Half;
			const float Z0 = Center.z - Half;
			const float Z1 = Center.z + Half;
			const glm::vec3& Color = Mesh.IsWalkable(LocalIx, LocalIz) ? Walkable : Blocked;
			Draw.AddLine({X0, Y, Z0}, {X1, Y, Z0}, Color);
			Draw.AddLine({X1, Y, Z0}, {X1, Y, Z1}, Color);
			Draw.AddLine({X1, Y, Z1}, {X0, Y, Z1}, Color);
			Draw.AddLine({X0, Y, Z1}, {X0, Y, Z0}, Color);
			if (!Mesh.IsWalkable(LocalIx, LocalIz))
			{
				Draw.AddLine({X0, Y, Z0}, {X1, Y, Z1}, Color);
				Draw.AddLine({X1, Y, Z0}, {X0, Y, Z1}, Color);
			}
		}
	}
}
