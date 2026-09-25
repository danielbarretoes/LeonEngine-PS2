#include "CollisionShape.h"

void HalfExtentsFromScale(const FVector& Scale, float& HalfX, float& HalfY, float& HalfZ)
{
	HalfX = 0.5f * FMath::Abs(Scale.X);
	HalfY = 0.5f * FMath::Abs(Scale.Y);
	HalfZ = 0.5f * FMath::Abs(Scale.Z);
}

float MassFromHalfExtents(float HalfX, float HalfY, float HalfZ)
{
	return FMath::Max(0.08f, 8.0f * HalfX * HalfY * HalfZ);
}

void ClampPositionXZ(FVector& Pos, float Bounds)
{
	Pos.X = FMath::Clamp(Pos.X, -Bounds, Bounds);
	Pos.Z = FMath::Clamp(Pos.Z, -Bounds, Bounds);
}

bool XzDiscOverlapsAabb(float X, float Z, float InRadius, float Cx, float Cz, float Hx, float Hz, float Inflate)
{
	Hx += Inflate;
	Hz += Inflate;
	const float NearestX = FMath::Clamp(X, Cx - Hx, Cx + Hx);
	const float NearestZ = FMath::Clamp(Z, Cz - Hz, Cz + Hz);
	const float Dx = X - NearestX;
	const float Dz = Z - NearestZ;
	return ((Dx * Dx) + (Dz * Dz)) <= (InRadius * InRadius);
}

bool CapsuleAabbMtv(float Px, float Pz, float InRadius, float Cx, float Cz, float Hx, float Hz, FVector2D& OutNormal,
	float& OutPenetration)
{
	const float Dx = Px - Cx;
	const float Dz = Pz - Cz;
	const float ClosestX = FMath::Clamp(Px, Cx - Hx, Cx + Hx);
	const float ClosestZ = FMath::Clamp(Pz, Cz - Hz, Cz + Hz);
	const float Ox = Px - ClosestX;
	const float Oz = Pz - ClosestZ;
	const float DistSq = (Ox * Ox) + (Oz * Oz);

	if (DistSq > 1.0e-8f)
	{
		const float Dist = FMath::Sqrt(DistSq);
		if (Dist >= InRadius)
		{
			return false;
		}
		OutNormal = FVector2D(Ox / Dist, Oz / Dist);
		OutPenetration = InRadius - Dist;
		return OutPenetration > 0.0f;
	}

	const float OverlapX = Hx + InRadius - FMath::Abs(Dx);
	const float OverlapZ = Hz + InRadius - FMath::Abs(Dz);
	if (OverlapX <= 0.0f || OverlapZ <= 0.0f)
	{
		return false;
	}
	if (OverlapX < OverlapZ)
	{
		OutNormal = FVector2D(Dx >= 0.0f ? 1.0f : -1.0f, 0.0f);
		OutPenetration = OverlapX;
	}
	else
	{
		OutNormal = FVector2D(0.0f, Dz >= 0.0f ? 1.0f : -1.0f);
		OutPenetration = OverlapZ;
	}
	return true;
}

bool AabbOverlapY(float Ay, float Ahy, float By, float Bhy)
{
	return FMath::Abs(Ay - By) < (Ahy + Bhy);
}

bool SeparateAabbXZ(FVector& A, float Ahx, float Ahz, FVector& B, float Bhx, float Bhz, float MoveA, float MoveB)
{
	return SeparateAabb(A, FVector(Ahx, 1.0e6f, Ahz), B, FVector(Bhx, 1.0e6f, Bhz), MoveA, MoveB, nullptr);
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

	FVector Mtv = FVector::ZeroVector;
	if (OverlapX <= OverlapY && OverlapX <= OverlapZ)
	{
		Mtv.X = (A.X >= B.X ? 1.0f : -1.0f) * OverlapX;
	}
	else if (OverlapY <= OverlapX && OverlapY <= OverlapZ)
	{
		Mtv.Y = (A.Y >= B.Y ? 1.0f : -1.0f) * OverlapY;
	}
	else
	{
		Mtv.Z = (A.Z >= B.Z ? 1.0f : -1.0f) * OverlapZ;
	}

	const float Inv = 1.0f / Share;
	A += Mtv * (MoveA * Inv);
	B -= Mtv * (MoveB * Inv);

	if (OutNormal != nullptr)
	{
		const float Len = Mtv.Size();
		*OutNormal = Len > 1.0e-8f ? (Mtv / Len) : FVector(0.0f, 1.0f, 0.0f);
	}
	return true;
}
