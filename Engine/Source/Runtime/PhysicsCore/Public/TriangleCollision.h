#pragma once

#include "AabbTree.h"
#include "CoreMinimal.h"

/**
 * Baked world-space triangle mesh for static ComplexAsSimple lite (FPhysScene's traces and QuerySupportZ). Tree, a
 * cache of the triangles, culls them for the segment queries: the first query builds it, and whoever changes Positions
 * or Indices of a mesh already queried rebuilds it (BuildTree), or clears the mesh first.
 */
struct PHYSICSCORE_API FTriangleMeshCollision
{
	TArray<FVector> Positions;
	TArray<uint32> Indices;
	/** The triangles (by index: Indices[3 * I ...]) a segment can hit; degenerate ones and bad indices are left out. */
	mutable FAabbTree Tree;

	[[nodiscard]] bool IsValid() const
	{
		return Positions.Num() > 0 && Indices.Num() >= 3 && (Indices.Num() % 3) == 0;
	}

	void Clear()
	{
		Positions.Reset();
		Indices.Reset();
		Tree.Reset();
	}

	/**
	 * Builds Tree from the triangles. A triangle's box holds every point SegmentTriangle accepts on it (its edge
	 * tolerance grows it about its centre); an inflated query grows the boxes by its inflate.
	 */
	void BuildTree() const;
};

/** Segment [Start, End] vs triangle. OutT in [0, 1] along the segment; the normal faces the segment's start. */
[[nodiscard]] bool SegmentTriangle(const FVector& Start, const FVector& End, const FVector& V0, const FVector& V1,
	const FVector& V2, float& OutT, FVector& OutNormal);

/**
 * Sphere / capsule approximation: intersects the plane offset by Inflate along the triangle normal (the slope
 * inflate idea). A hit counts only if the impact projects inside the triangle.
 */
[[nodiscard]] bool SegmentTriangleInflated(const FVector& Start, const FVector& End, const FVector& V0,
	const FVector& V1, const FVector& V2, float Inflate, float& OutT, FVector& OutNormal);

/**
 * Nearest segment hit against a triangle mesh (optional inflate for sphere / capsule), at most MaxT along the segment;
 * of triangles hit at the same time, the last one, whose index (Indices[3 * I ...]) goes to OutFaceIndex when given.
 * Builds the mesh's tree if it is not built.
 */
[[nodiscard]] bool SegmentTriangleMesh(const FVector& Start, const FVector& End, const FTriangleMeshCollision& Mesh,
	float Inflate, float& OutT, FVector& OutNormal, float MaxT = 1.0f, int32* OutFaceIndex = nullptr);
