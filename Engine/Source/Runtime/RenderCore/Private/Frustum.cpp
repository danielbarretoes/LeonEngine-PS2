#include "Frustum.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace
{

	struct FRawPlane
	{
		float A = 0.0f;
		float B = 0.0f;
		float C = 0.0f;
		float D = 0.0f;
	};

	FRawPlane NormalizePlane(float InA, float InB, float InC, float InD)
	{
		const float Len = std::sqrt((InA * InA) + (InB * InB) + (InC * InC));
		if (Len > 1e-8f)
		{
			const float Inv = 1.0f / Len;
			return {.A = InA * Inv, .B = InB * Inv, .C = InC * Inv, .D = InD * Inv};
		}
		return {.A = 0.0f, .B = 1.0f, .C = 0.0f, .D = 0.0f};
	}

} // namespace

FBox FBox::FromLocalTransformed(const glm::vec3& LocalMin, const glm::vec3& LocalMax, const glm::mat4& Model)
{
	const std::array<glm::vec3, 8> Corners = {{
		{LocalMin.x, LocalMin.y, LocalMin.z},
		{LocalMax.x, LocalMin.y, LocalMin.z},
		{LocalMin.x, LocalMax.y, LocalMin.z},
		{LocalMax.x, LocalMax.y, LocalMin.z},
		{LocalMin.x, LocalMin.y, LocalMax.z},
		{LocalMax.x, LocalMin.y, LocalMax.z},
		{LocalMin.x, LocalMax.y, LocalMax.z},
		{LocalMax.x, LocalMax.y, LocalMax.z},
	}};

	FBox Box;
	Box.Min = glm::vec3(std::numeric_limits<float>::max());
	Box.Max = glm::vec3(std::numeric_limits<float>::lowest());
	for (const glm::vec3& Local : Corners)
	{
		const glm::vec3 World = glm::vec3(Model * glm::vec4(Local, 1.0f));
		Box.Min = glm::min(Box.Min, World);
		Box.Max = glm::max(Box.Max, World);
	}
	return Box;
}

bool FBox::IntersectRay(const glm::vec3& Origin, const glm::vec3& Dir, float& OutT) const
{
	constexpr float Eps = 1.0e-8f;
	float TMin = 0.0f;
	float TMax = std::numeric_limits<float>::max();

	for (int Axis = 0; Axis < 3; ++Axis)
	{
		const float O = Origin[Axis];
		const float LocalD = Dir[Axis];
		const float bMin = Min[Axis];
		const float bMax = Max[Axis];
		if (std::abs(LocalD) < Eps)
		{
			if (O < bMin || O > bMax)
			{
				return false;
			}
			continue;
		}
		float T0 = (bMin - O) / LocalD;
		float T1 = (bMax - O) / LocalD;
		if (T0 > T1)
		{
			std::swap(T0, T1);
		}
		TMin = std::max(TMin, T0);
		TMax = std::min(TMax, T1);
		if (TMin > TMax)
		{
			return false;
		}
	}

	if (TMax < 0.0f)
	{
		return false;
	}
	OutT = TMin >= 0.0f ? TMin : TMax;
	return OutT >= 0.0f;
}

void FFrustum::ExtractFromViewProjection(const glm::mat4& ViewProjection)
{
	// Gribb/Hartmann: combine clip-matrix columns into frustum planes.
	const glm::mat4& M = ViewProjection;
	const std::array<FRawPlane, 6> Raw = {{
		NormalizePlane(M[0][3] + M[0][0], M[1][3] + M[1][0], M[2][3] + M[2][0],
			M[3][3] + M[3][0]), // left
		NormalizePlane(M[0][3] - M[0][0], M[1][3] - M[1][0], M[2][3] - M[2][0],
			M[3][3] - M[3][0]), // right
		NormalizePlane(M[0][3] + M[0][1], M[1][3] + M[1][1], M[2][3] + M[2][1],
			M[3][3] + M[3][1]), // bottom
		NormalizePlane(M[0][3] - M[0][1], M[1][3] - M[1][1], M[2][3] - M[2][1],
			M[3][3] - M[3][1]), // top
		NormalizePlane(M[0][3] + M[0][2], M[1][3] + M[1][2], M[2][3] + M[2][2],
			M[3][3] + M[3][2]), // near
		NormalizePlane(M[0][3] - M[0][2], M[1][3] - M[1][2], M[2][3] - M[2][2],
			M[3][3] - M[3][2]), // far
	}};

	auto Assign = [](FPlane& Dst, const FRawPlane& Src)
	{
		Dst.Normal = {Src.A, Src.B, Src.C};
		Dst.Distance = Src.D;
	};
	Assign(Planes[0], Raw[0]);
	Assign(Planes[1], Raw[1]);
	Assign(Planes[2], Raw[2]);
	Assign(Planes[3], Raw[3]);
	Assign(Planes[4], Raw[4]);
	Assign(Planes[5], Raw[5]);
}

bool FFrustum::IntersectsAabb(const FBox& Box) const
{
	for (const FPlane& Plane : Planes)
	{
		const glm::vec3 Positive{
			Plane.Normal.x >= 0.0f ? Box.Max.x : Box.Min.x,
			Plane.Normal.y >= 0.0f ? Box.Max.y : Box.Min.y,
			Plane.Normal.z >= 0.0f ? Box.Max.z : Box.Min.z,
		};
		if ((glm::dot(Plane.Normal, Positive) + Plane.Distance) < 0.0f)
		{
			return false;
		}
	}
	return true;
}
