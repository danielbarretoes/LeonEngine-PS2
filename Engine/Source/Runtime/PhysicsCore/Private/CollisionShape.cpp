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

bool SegmentAabb(
	const FVector& Start, const FVector& End, const FVector& Mn, const FVector& Mx, float& OutT, FVector& OutNormal)
{
	const FVector Dir = End - Start;
	float TEnter = 0.0f;
	float TExit = 1.0f;
	FVector EnterNormal(0.0f, 0.0f, 1.0f);
	bool bHitFace = false;

	// X, then the vertical Z, then Y: on equal entry times the earlier axis gives the normal (the order of the
	// Y-up world: X, vertical, second horizontal).
	constexpr int32 AxisOrder[3] = {0, 2, 1};
	for (const int32 Axis : AxisOrder)
	{
		if (FMath::Abs(Dir[Axis]) < 1.0e-6f)
		{
			if (Start[Axis] < Mn[Axis] || Start[Axis] > Mx[Axis])
			{
				return false;
			}
			continue;
		}

		const float Inv = 1.0f / Dir[Axis];
		float T0 = (Mn[Axis] - Start[Axis]) * Inv;
		float T1 = (Mx[Axis] - Start[Axis]) * Inv;
		float NormalSign = -1.0f;
		if (Inv < 0.0f)
		{
			Swap(T0, T1);
			NormalSign = 1.0f;
		}

		if (T0 > TEnter)
		{
			TEnter = T0;
			EnterNormal = FVector::ZeroVector;
			EnterNormal[Axis] = NormalSign;
			bHitFace = true;
		}
		TExit = FMath::Min(TExit, T1);
		if (TEnter > TExit)
		{
			return false;
		}
	}

	if (TEnter < 0.0f || TEnter > 1.0f)
	{
		return false;
	}

	if (!bHitFace && TEnter <= 0.0f)
	{
		OutT = 0.0f;
		OutNormal = FVector(0.0f, 0.0f, 1.0f);
		return true;
	}

	OutT = TEnter;
	OutNormal = EnterNormal;
	const float Len = OutNormal.Size();
	if (Len > 1.0e-6f)
	{
		OutNormal /= Len;
	}
	return true;
}

bool SegmentUprightCapsule(const FVector& Start, const FVector& End, const FVector& Center, float Radius,
	float CylinderHalfHeight, float& OutT, FVector& OutNormal)
{
	const float R = FMath::Max(Radius, 0.0f);
	const float Hc = FMath::Max(CylinderHalfHeight, 0.0f);
	const FVector Bottom = Center - FVector(0.0f, 0.0f, Hc);
	const FVector Top = Center + FVector(0.0f, 0.0f, Hc);

	auto ClosestOnAxis = [&](const FVector& P)
	{ return FVector(Center.X, Center.Y, FMath::Clamp(P.Z, Bottom.Z, Top.Z)); };
	auto NormalFrom = [&](const FVector& P)
	{
		const FVector Away = P - ClosestOnAxis(P);
		const float Len = Away.Size();
		return Len > 1.0e-6f ? Away / Len : FVector(0.0f, 0.0f, 1.0f);
	};

	// Starting inside: an immediate hit, as the boxes do.
	if ((Start - ClosestOnAxis(Start)).SizeSquared() <= R * R)
	{
		OutT = 0.0f;
		OutNormal = NormalFrom(Start);
		return true;
	}

	const FVector D = End - Start;
	float BestT = 2.0f;

	// The side of the infinite cylinder, kept where it is between the caps.
	const float A = (D.X * D.X) + (D.Y * D.Y);
	if (A > 1.0e-8f)
	{
		const float Sx = Start.X - Center.X;
		const float Sy = Start.Y - Center.Y;
		const float B = 2.0f * ((Sx * D.X) + (Sy * D.Y));
		const float C = (Sx * Sx) + (Sy * Sy) - (R * R);
		const float Disc = (B * B) - (4.0f * A * C);
		if (Disc >= 0.0f)
		{
			const float T = (-B - FMath::Sqrt(Disc)) / (2.0f * A);
			const float Z = Start.Z + (D.Z * T);
			if (T >= 0.0f && T <= 1.0f && Z >= Bottom.Z && Z <= Top.Z)
			{
				BestT = T;
			}
		}
	}

	// The two hemispheres (whole spheres: their parts inside the cylinder are entered through its side first).
	for (const FVector& SphereCenter : {Bottom, Top})
	{
		const FVector S = Start - SphereCenter;
		const float Qa = D.SizeSquared();
		if (Qa < 1.0e-8f)
		{
			continue;
		}
		const float Qb = 2.0f * (S | D);
		const float Qc = S.SizeSquared() - (R * R);
		const float Disc = (Qb * Qb) - (4.0f * Qa * Qc);
		if (Disc < 0.0f)
		{
			continue;
		}
		const float T = (-Qb - FMath::Sqrt(Disc)) / (2.0f * Qa);
		if (T >= 0.0f && T <= 1.0f && T < BestT)
		{
			BestT = T;
		}
	}

	if (BestT > 1.0f)
	{
		return false;
	}
	OutT = BestT;
	OutNormal = NormalFrom(Start + (D * BestT));
	return true;
}

bool SegmentFloorZ(const FVector& Start, const FVector& End, float FloorZ, float& OutT, FVector& OutNormal)
{
	const float Dz = End.Z - Start.Z;
	if (FMath::Abs(Dz) < 1.0e-6f)
	{
		return false;
	}
	const float T = (FloorZ - Start.Z) / Dz;
	if (T < 0.0f || T > 1.0f)
	{
		return false;
	}
	OutT = T;
	OutNormal = FVector(0.0f, 0.0f, Dz < 0.0f ? 1.0f : -1.0f);
	return true;
}
