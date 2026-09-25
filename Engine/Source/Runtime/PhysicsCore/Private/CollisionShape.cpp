#include "CollisionShape.h"

void HalfExtentsFromScale(const FVector& Scale, float& HalfX, float& HalfY, float& HalfZ)
{
	constexpr float HalfSize = 0.5f * BasicShapeSize;
	HalfX = HalfSize * FMath::Abs(Scale.X);
	HalfY = HalfSize * FMath::Abs(Scale.Y);
	HalfZ = HalfSize * FMath::Abs(Scale.Z);
}

float MassFromHalfExtents(float HalfX, float HalfY, float HalfZ)
{
	const float HalfXMetres = HalfX / PhysicsCentimetresPerMetre;
	const float HalfYMetres = HalfY / PhysicsCentimetresPerMetre;
	const float HalfZMetres = HalfZ / PhysicsCentimetresPerMetre;
	return FMath::Max(0.08f, 8.0f * HalfXMetres * HalfYMetres * HalfZMetres);
}

void ClampPositionXY(FVector& Pos, float Bounds)
{
	Pos.X = FMath::Clamp(Pos.X, -Bounds, Bounds);
	Pos.Y = FMath::Clamp(Pos.Y, -Bounds, Bounds);
}

bool XYDiscOverlapsAabb(float X, float Y, float InRadius, float Cx, float Cy, float Hx, float Hy, float Inflate)
{
	Hx += Inflate;
	Hy += Inflate;
	const float NearestX = FMath::Clamp(X, Cx - Hx, Cx + Hx);
	const float NearestY = FMath::Clamp(Y, Cy - Hy, Cy + Hy);
	const float Dx = X - NearestX;
	const float Dy = Y - NearestY;
	return ((Dx * Dx) + (Dy * Dy)) <= (InRadius * InRadius);
}

bool CapsuleAabbMtv(float Px, float Py, float InRadius, float Cx, float Cy, float Hx, float Hy, FVector2D& OutNormal,
	float& OutPenetration)
{
	const float Dx = Px - Cx;
	const float Dy = Py - Cy;
	const float ClosestX = FMath::Clamp(Px, Cx - Hx, Cx + Hx);
	const float ClosestY = FMath::Clamp(Py, Cy - Hy, Cy + Hy);
	const float Ox = Px - ClosestX;
	const float Oy = Py - ClosestY;
	const float DistSq = (Ox * Ox) + (Oy * Oy);

	if (DistSq > 1.0e-4f)
	{
		const float Dist = FMath::Sqrt(DistSq);
		if (Dist >= InRadius)
		{
			return false;
		}
		OutNormal = FVector2D(Ox / Dist, Oy / Dist);
		OutPenetration = InRadius - Dist;
		return OutPenetration > 0.0f;
	}

	const float OverlapX = Hx + InRadius - FMath::Abs(Dx);
	const float OverlapY = Hy + InRadius - FMath::Abs(Dy);
	if (OverlapX <= 0.0f || OverlapY <= 0.0f)
	{
		return false;
	}
	if (OverlapX < OverlapY)
	{
		OutNormal = FVector2D(Dx >= 0.0f ? 1.0f : -1.0f, 0.0f);
		OutPenetration = OverlapX;
	}
	else
	{
		OutNormal = FVector2D(0.0f, Dy >= 0.0f ? 1.0f : -1.0f);
		OutPenetration = OverlapY;
	}
	return true;
}

bool AabbOverlapZ(float Az, float Ahz, float Bz, float Bhz)
{
	return FMath::Abs(Az - Bz) < (Ahz + Bhz);
}

bool SeparateAabbXY(FVector& A, float Ahx, float Ahy, FVector& B, float Bhx, float Bhy, float MoveA, float MoveB)
{
	/** Half height (cm) that makes the boxes overlap on Z whatever their heights. */
	constexpr float UnboundedHalfZ = 1.0e8f;
	return SeparateAabb(
		A, FVector(Ahx, Ahy, UnboundedHalfZ), B, FVector(Bhx, Bhy, UnboundedHalfZ), MoveA, MoveB, nullptr);
}

bool SeparateAabb(FVector& A, const FVector& AHalfExtents, FVector& B, const FVector& BHalfExtents, float MoveA,
	float MoveB, FVector* OutNormal)
{
	const float OverlapX = (AHalfExtents.X + BHalfExtents.X) - FMath::Abs(A.X - B.X);
	const float OverlapY = (AHalfExtents.Y + BHalfExtents.Y) - FMath::Abs(A.Y - B.Y);
	const float OverlapZ = (AHalfExtents.Z + BHalfExtents.Z) - FMath::Abs(A.Z - B.Z);
	if (OverlapX <= 0.0f || OverlapY <= 0.0f || OverlapZ <= 0.0f)
	{
		return false;
	}

	const float Share = MoveA + MoveB;
	if (Share <= 1.0e-6f)
	{
		return false;
	}

	// Ties go to X, then to the vertical Z, then to Y: the order of the Y-up world (X, vertical, second horizontal).
	FVector Mtv = FVector::ZeroVector;
	if (OverlapX <= OverlapY && OverlapX <= OverlapZ)
	{
		Mtv.X = (A.X >= B.X ? 1.0f : -1.0f) * OverlapX;
	}
	else if (OverlapZ <= OverlapX && OverlapZ <= OverlapY)
	{
		Mtv.Z = (A.Z >= B.Z ? 1.0f : -1.0f) * OverlapZ;
	}
	else
	{
		Mtv.Y = (A.Y >= B.Y ? 1.0f : -1.0f) * OverlapY;
	}

	const float Inv = 1.0f / Share;
	A += Mtv * (MoveA * Inv);
	B -= Mtv * (MoveB * Inv);

	if (OutNormal != nullptr)
	{
		const float Len = Mtv.Size();
		*OutNormal = Len > 1.0e-6f ? (Mtv / Len) : FVector(0.0f, 0.0f, 1.0f);
	}
	return true;
}
