#pragma once

#include "Math/Box.h"
#include "Math/Plane.h"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

/// World AABB of a local box moved by a glm model matrix (bridge for the glm mesh bounds until P6).
[[nodiscard]] RENDERCORE_API FBox TransformLocalBox(
	const glm::vec3& LocalMin, const glm::vec3& LocalMax, const glm::mat4& Model);

/// View-projection frustum as 6 planes; a point is inside when PlaneDot >= 0 for all of them.
class RENDERCORE_API FFrustum
{
public:
	void ExtractFromViewProjection(const glm::mat4& ViewProjection);

	/// True if the AABB is at least partially inside the frustum.
	[[nodiscard]] bool IntersectsAabb(const FBox& Box) const;

private:
	FPlane Planes[6]{};
};
