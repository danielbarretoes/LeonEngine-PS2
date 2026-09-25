#include "TriangleCollision.h"

namespace
{

	[[nodiscard]] bool PointInTriangle(
		const FVector& P, const FVector& A, const FVector& B, const FVector& C, const FVector& Normal)
	{
		const FVector Edge0 = B - A;
		const FVector Edge1 = C - B;
		const FVector Edge2 = A - C;
		if ((Normal | (Edge0 ^ (P - A))) < -1.0e-5f)
		{
			return false;
		}
		if ((Normal | (Edge1 ^ (P - B))) < -1.0e-5f)
		{
			return false;
		}
		if ((Normal | (Edge2 ^ (P - C))) < -1.0e-5f)
		{
			return false;
		}
		return true;
	}

} // namespace

bool SegmentTriangle(const FVector& Start, const FVector& End, const FVector& V0, const FVector& V1, const FVector& V2,
	float& OutT, FVector& OutNormal)
{
	const FVector Edge1 = V1 - V0;
	const FVector Edge2 = V2 - V0;
	FVector Normal = Edge1 ^ Edge2;
	const float NLen = Normal.Size();
	if (NLen < 1.0e-8f)
	{
		return false;
	}
	Normal /= NLen;

	const FVector Dir = End - Start;
	const float Denom = Normal | Dir;
	if (FMath::Abs(Denom) < 1.0e-8f)
	{
		return false;
	}
	const float T = (Normal | (V0 - Start)) / Denom;
	if (T < 0.0f || T > 1.0f)
	{
		return false;
	}
	const FVector Hit = Start + (Dir * T);
	if (!PointInTriangle(Hit, V0, V1, V2, Normal))
	{
		return false;
	}
	OutT = T;
	// Face the incoming ray (the UE blocking normal points toward the tracer).
	OutNormal = (Denom < 0.0f) ? Normal : -Normal;
	return true;
}

bool SegmentTriangleInflated(const FVector& Start, const FVector& End, const FVector& V0, const FVector& V1,
	const FVector& V2, float Inflate, float& OutT, FVector& OutNormal)
{
	const FVector Edge1 = V1 - V0;
	const FVector Edge2 = V2 - V0;
	FVector Normal = Edge1 ^ Edge2;
	const float NLen = Normal.Size();
	if (NLen < 1.0e-8f)
	{
		return false;
	}
	Normal /= NLen;

	const float Pad = FMath::Max(Inflate, 0.0f);
	// Offset the plane toward the start of the segment (sphere center approach).
	const float DStart = (Start - V0) | Normal;
	const FVector PlaneN = (DStart >= 0.0f) ? Normal : -Normal;
	const FVector PlanePoint = V0 + (PlaneN * Pad);

	const FVector Dir = End - Start;
	const float Denom = PlaneN | Dir;
	if (FMath::Abs(Denom) < 1.0e-8f)
	{
		return false;
	}
	const float T = (PlaneN | (PlanePoint - Start)) / Denom;
	if (T < 0.0f || T > 1.0f)
	{
		return false;
	}
	const FVector Hit = Start + (Dir * T);
	const FVector OnTri = Hit - (PlaneN * Pad);
	if (!PointInTriangle(OnTri, V0, V1, V2, Normal))
	{
		return false;
	}
	OutT = T;
	OutNormal = PlaneN;
	return true;
}

bool SegmentTriangleMesh(const FVector& Start, const FVector& End, const FTriangleMeshCollision& Mesh, float Inflate,
	float& OutT, FVector& OutNormal)
{
	if (!Mesh.IsValid())
	{
		return false;
	}
	bool bAny = false;
	float BestT = 1.0f;
	FVector BestN(0.0f, 1.0f, 0.0f);
	const int32 TriCount = Mesh.Indices.Num() / 3;
	const uint32 VertexCount = static_cast<uint32>(Mesh.Positions.Num());
	for (int32 Tri = 0; Tri < TriCount; ++Tri)
	{
		const uint32 I0 = Mesh.Indices[Tri * 3 + 0];
		const uint32 I1 = Mesh.Indices[Tri * 3 + 1];
		const uint32 I2 = Mesh.Indices[Tri * 3 + 2];
		if (I0 >= VertexCount || I1 >= VertexCount || I2 >= VertexCount)
		{
			continue;
		}
		float HitT = 1.0f;
		FVector HitN = FVector::ZeroVector;
		const bool bOk = (Inflate > 1.0e-6f)
			? SegmentTriangleInflated(
				  Start, End, Mesh.Positions[I0], Mesh.Positions[I1], Mesh.Positions[I2], Inflate, HitT, HitN)
			: SegmentTriangle(Start, End, Mesh.Positions[I0], Mesh.Positions[I1], Mesh.Positions[I2], HitT, HitN);
		if (!bOk || HitT > BestT)
		{
			continue;
		}
		BestT = HitT;
		BestN = HitN;
		bAny = true;
	}
	if (!bAny)
	{
		return false;
	}
	OutT = BestT;
	OutNormal = BestN;
	return true;
}
