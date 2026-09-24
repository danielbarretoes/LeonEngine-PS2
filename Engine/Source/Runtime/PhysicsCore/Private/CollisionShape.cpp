#include "CollisionShape.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>

void HalfExtentsFromScale(const glm::vec3& Scale, float& HalfX, float& HalfY, float& HalfZ)
{
	HalfX = 0.5f * std::abs(Scale.x);
	HalfY = 0.5f * std::abs(Scale.y);
	HalfZ = 0.5f * std::abs(Scale.z);
}

float MassFromHalfExtents(float HalfX, float HalfY, float HalfZ)
{
	return std::max(0.08f, 8.0f * HalfX * HalfY * HalfZ);
}

void ClampPositionXZ(glm::vec3& Pos, float Bounds)
{
	Pos.x = std::clamp(Pos.x, -Bounds, Bounds);
	Pos.z = std::clamp(Pos.z, -Bounds, Bounds);
}

bool XzDiscOverlapsAabb(float X, float Z, float InRadius, float Cx, float Cz, float Hx, float Hz, float Inflate)
{
	Hx += Inflate;
	Hz += Inflate;
	const float NearestX = std::clamp(X, Cx - Hx, Cx + Hx);
	const float NearestZ = std::clamp(Z, Cz - Hz, Cz + Hz);
	const float Dx = X - NearestX;
	const float Dz = Z - NearestZ;
	return ((Dx * Dx) + (Dz * Dz)) <= (InRadius * InRadius);
}

bool CapsuleAabbMtv(float Px, float Pz, float InRadius, float Cx, float Cz, float Hx, float Hz, glm::vec2& OutNormal,
	float& OutPenetration)
{
	const float Dx = Px - Cx;
	const float Dz = Pz - Cz;
	const float ClosestX = std::clamp(Px, Cx - Hx, Cx + Hx);
	const float ClosestZ = std::clamp(Pz, Cz - Hz, Cz + Hz);
	const float Ox = Px - ClosestX;
	const float Oz = Pz - ClosestZ;
	const float DistSq = (Ox * Ox) + (Oz * Oz);

	if (DistSq > 1.0e-8f)
	{
		const float Dist = std::sqrt(DistSq);
		if (Dist >= InRadius)
		{
			return false;
		}
		OutNormal = {Ox / Dist, Oz / Dist};
		OutPenetration = InRadius - Dist;
		return OutPenetration > 0.0f;
	}

	const float OverlapX = Hx + InRadius - std::abs(Dx);
	const float OverlapZ = Hz + InRadius - std::abs(Dz);
	if (OverlapX <= 0.0f || OverlapZ <= 0.0f)
	{
		return false;
	}
	if (OverlapX < OverlapZ)
	{
		OutNormal = {Dx >= 0.0f ? 1.0f : -1.0f, 0.0f};
		OutPenetration = OverlapX;
	}
	else
	{
		OutNormal = {0.0f, Dz >= 0.0f ? 1.0f : -1.0f};
		OutPenetration = OverlapZ;
	}
	return true;
}

bool AabbOverlapY(float Ay, float Ahy, float By, float Bhy)
{
	return std::abs(Ay - By) < (Ahy + Bhy);
}

bool SeparateAabbXZ(glm::vec3& A, float Ahx, float Ahz, glm::vec3& B, float Bhx, float Bhz, float MoveA, float MoveB)
{
	return SeparateAabb(A, {Ahx, 1.0e6f, Ahz}, B, {Bhx, 1.0e6f, Bhz}, MoveA, MoveB, nullptr);
}

bool SeparateAabb(glm::vec3& A, const glm::vec3& AHalfExtents, glm::vec3& B, const glm::vec3& bHalfExtents, float MoveA,
	float MoveB, glm::vec3* OutNormal)
{
	const float OverlapX = (AHalfExtents.x + bHalfExtents.x) - std::abs(A.x - B.x);
	const float OverlapY = (AHalfExtents.y + bHalfExtents.y) - std::abs(A.y - B.y);
	const float OverlapZ = (AHalfExtents.z + bHalfExtents.z) - std::abs(A.z - B.z);
	if (OverlapX <= 0.0f || OverlapY <= 0.0f || OverlapZ <= 0.0f)
	{
		return false;
	}

	const float Share = MoveA + MoveB;
	if (Share <= 1.0e-6f)
	{
		return false;
	}

	glm::vec3 Mtv{0.0f};
	if (OverlapX <= OverlapY && OverlapX <= OverlapZ)
	{
		Mtv.x = (A.x >= B.x ? 1.0f : -1.0f) * OverlapX;
	}
	else if (OverlapY <= OverlapX && OverlapY <= OverlapZ)
	{
		Mtv.y = (A.y >= B.y ? 1.0f : -1.0f) * OverlapY;
	}
	else
	{
		Mtv.z = (A.z >= B.z ? 1.0f : -1.0f) * OverlapZ;
	}

	const float Inv = 1.0f / Share;
	A += Mtv * (MoveA * Inv);
	B -= Mtv * (MoveB * Inv);

	if (OutNormal != nullptr)
	{
		const float Len = glm::length(Mtv);
		*OutNormal = Len > 1.0e-8f ? (Mtv / Len) : glm::vec3{0.0f, 1.0f, 0.0f};
	}
	return true;
}
