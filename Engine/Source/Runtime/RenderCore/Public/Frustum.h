#pragma once

#include "CoreMinimal.h"

/** World AABB of a local box moved by a model matrix. */
[[nodiscard]] RENDERCORE_API FBox TransformLocalBox(
	const FVector& LocalMin, const FVector& LocalMax, const FMatrix& Model);

/**
 * View-projection frustum as 6 planes; a point is inside when PlaneDot >= 0 for all of them.
 * The matrix is the renderer's clip transform (the glm column-vector layout, copied as it is until the renderer
 * migrates; see GlmInterop.h).
 */
class RENDERCORE_API FFrustum
{
public:
	void ExtractFromViewProjection(const FMatrix& ViewProjection);

	/** True if the AABB is at least partially inside the frustum. */
	[[nodiscard]] bool IntersectsAabb(const FBox& Box) const;

private:
	FPlane Planes[6]{};
};
