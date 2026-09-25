#pragma once

#include "CoreMinimal.h"

/**
 * Baked world-space triangle mesh for static ComplexAsSimple lite (Arcade traces / QuerySupportZ; Jolt MeshShape on
 * rebuild).
 */
struct PHYSICSCORE_API FTriangleMeshCollision
{
	TArray<FVector> Positions;
	TArray<uint32> Indices;

	[[nodiscard]] bool IsValid() const
	{
		return Positions.Num() > 0 && Indices.Num() >= 3 && (Indices.Num() % 3) == 0;
	}

	void Clear()
	{
		Positions.Reset();
		Indices.Reset();
	}
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

/** Nearest segment hit against a triangle mesh (optional inflate for sphere / capsule). */
[[nodiscard]] bool SegmentTriangleMesh(const FVector& Start, const FVector& End, const FTriangleMeshCollision& Mesh,
	float Inflate, float& OutT, FVector& OutNormal);
