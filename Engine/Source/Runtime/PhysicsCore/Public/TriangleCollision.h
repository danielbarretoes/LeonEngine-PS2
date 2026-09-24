#pragma once

#include <glm/vec3.hpp>

#include <cstdint>
#include <vector>

/// Baked world-space triangle mesh for static ComplexAsSimple lite (Arcade traces /
/// QuerySupportY; Jolt MeshShape on rebuild).
struct PHYSICSCORE_API FTriangleMeshCollision
{
	std::vector<glm::vec3> Positions;
	std::vector<std::uint32_t> Indices;

	[[nodiscard]] bool IsValid() const
	{
		return !Positions.empty() && Indices.size() >= 3 && (Indices.size() % 3) == 0;
	}

	void Clear()
	{
		Positions.clear();
		Indices.clear();
	}
};

/// Segment [start,end] vs triangle. `t` in [0,1] along the segment. Outward/front normal.
[[nodiscard]] bool SegmentTriangle(const glm::vec3& Start, const glm::vec3& End, const glm::vec3& V0,
	const glm::vec3& V1, const glm::vec3& V2, float& OutT, glm::vec3& OutNormal);

/// Sphere/capsule approximation: intersect against the plane offset by `inflate` along the
/// triangle normal (same idea as slope inflate). Hit only if the impact projects inside the tri.
[[nodiscard]] bool SegmentTriangleInflated(const glm::vec3& Start, const glm::vec3& End, const glm::vec3& V0,
	const glm::vec3& V1, const glm::vec3& V2, float Inflate, float& OutT, glm::vec3& OutNormal);

/// Nearest segment hit against a triangle mesh (optional inflate for sphere/capsule).
[[nodiscard]] bool SegmentTriangleMesh(const glm::vec3& Start, const glm::vec3& End, const FTriangleMeshCollision& Mesh,
	float Inflate, float& OutT, glm::vec3& OutNormal);
