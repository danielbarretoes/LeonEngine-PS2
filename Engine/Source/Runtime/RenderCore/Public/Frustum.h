#pragma once

#include "CoreMinimal.h"
#include "Math/VectorMath.h"

/** World AABB of a local box moved by a model matrix. */
[[nodiscard]] RENDERCORE_API FBox TransformLocalBox(
	const FVector& LocalMin, const FVector& LocalMax, const FMatrix& Model);

/**
 * View-projection frustum as 6 planes; a point is inside when PlaneDot >= 0 for all of them.
 * The matrix goes from world to GL clip space (-w <= z <= w; see GLClipSpace.h).
 *
 * The planes are kept four to a quadword (FVectorPlaneSet), normalized, and tested by FVectorMath: on the PS2 VU0
 * tests a box or a sphere against four planes at once (Docs/PLANS/ps2-shipping.md N15).
 */
class RENDERCORE_API FFrustum
{
public:
	void ExtractFromViewProjection(const FMatrix& ViewProjection);

	/** True if the AABB is at least partially inside the frustum. */
	[[nodiscard]] bool IntersectsAabb(const FBox& Box) const;

	/** True if the sphere is at least partially inside the frustum (centimetres: the planes are normalized). */
	[[nodiscard]] bool IntersectsSphere(const FVector& Center, float Radius) const;

	[[nodiscard]] const FVectorPlaneSet& GetPlanes() const
	{
		return Planes;
	}

private:
	FVectorPlaneSet Planes;
};
