#include "TriangleCollision.h"

namespace
{

	/** Edge-side tolerance of PointInTriangle (cm^2: unit normal, edge vector and offset). */
	constexpr float PointInTriangleTolerance = 1.0e-1f;
	/** Degenerate triangle: |Edge1 ^ Edge2| below this (cm^2). */
	constexpr float MinTriangleNormalLength = 1.0e-4f;
	/** Segment parallel to the plane: |Normal | Dir| below this (cm). */
	constexpr float MinSegmentAlongNormal = 1.0e-6f;
	/** Growth of a triangle's box in the tree for rounding (cm, and a share of its largest coordinate). */
	constexpr float TriangleBoxMargin = 0.25f;
	constexpr float TriangleBoxRelativeMargin = 1.0e-5f;

	[[nodiscard]] bool PointInTriangle(
		const FVector& P, const FVector& A, const FVector& B, const FVector& C, const FVector& Normal)
	{
		const FVector Edge0 = B - A;
		const FVector Edge1 = C - B;
		const FVector Edge2 = A - C;
		if ((Normal | (Edge0 ^ (P - A))) < -PointInTriangleTolerance)
		{
			return false;
		}
		if ((Normal | (Edge1 ^ (P - B))) < -PointInTriangleTolerance)
		{
			return false;
		}
		if ((Normal | (Edge2 ^ (P - C))) < -PointInTriangleTolerance)
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
	if (NLen < MinTriangleNormalLength)
	{
		return false;
	}
	Normal /= NLen;

	const FVector Dir = End - Start;
	const float Denom = Normal | Dir;
	if (FMath::Abs(Denom) < MinSegmentAlongNormal)
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
	if (NLen < MinTriangleNormalLength)
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
	if (FMath::Abs(Denom) < MinSegmentAlongNormal)
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

void FTriangleMeshCollision::BuildTree() const
{
	TArray<FBox> Bounds;
	TArray<int32> Triangles;
	const int32 TriCount = IsValid() ? Indices.Num() / 3 : 0;
	const uint32 VertexCount = static_cast<uint32>(Positions.Num());
	Bounds.Reserve(TriCount);
	Triangles.Reserve(TriCount);
	for (int32 Tri = 0; Tri < TriCount; ++Tri)
	{
		const uint32 I0 = Indices[Tri * 3 + 0];
		const uint32 I1 = Indices[Tri * 3 + 1];
		const uint32 I2 = Indices[Tri * 3 + 2];
		if (I0 >= VertexCount || I1 >= VertexCount || I2 >= VertexCount)
		{
			continue;
		}
		const FVector& V0 = Positions[I0];
		const FVector& V1 = Positions[I1];
		const FVector& V2 = Positions[I2];
		// The same test as SegmentTriangle / SegmentTriangleInflated: they never hit a degenerate triangle.
		const float NLen = ((V1 - V0) ^ (V2 - V0)).Size();
		if (NLen < MinTriangleNormalLength)
		{
			continue;
		}
		// PointInTriangle accepts barycentric coordinates down to -Tolerance / NLen: the triangle scaled about its
		// centroid by 1 + 3 * Tolerance / NLen. The margin covers the rounding of the tests.
		const FVector Centroid = (V0 + V1 + V2) / 3.0f;
		const float Scale = 1.0f + (3.0f * PointInTriangleTolerance / NLen);
		FBox Box(ForceInit);
		Box += Centroid + ((V0 - Centroid) * Scale);
		Box += Centroid + ((V1 - Centroid) * Scale);
		Box += Centroid + ((V2 - Centroid) * Scale);
		const float Largest = FMath::Max(Box.Min.GetAbsMax(), Box.Max.GetAbsMax());
		const FVector Margin(TriangleBoxMargin + (Largest * TriangleBoxRelativeMargin));
		Bounds.Add(FBox(Box.Min - Margin, Box.Max + Margin));
		Triangles.Add(Tri);
	}
	Tree.Build(Bounds.GetData(), Triangles.GetData(), Triangles.Num());
}

bool SegmentTriangleMesh(const FVector& Start, const FVector& End, const FTriangleMeshCollision& Mesh, float Inflate,
	float& OutT, FVector& OutNormal, float MaxT, int32* OutFaceIndex)
{
	if (!Mesh.IsValid())
	{
		return false;
	}
	if (!Mesh.Tree.IsBuilt())
	{
		Mesh.BuildTree();
	}
	// The nearest hit; of hits at the same time, the last triangle (the order the triangles were once tested in).
	int32 BestTri = INDEX_NONE;
	float BestT = MaxT;
	FVector BestN(0.0f, 0.0f, 1.0f);
	const bool bInflated = Inflate > 1.0e-4f;
	const FAabbTreeSegment Segment(Start, End);
	Mesh.Tree.ForEachSegmentHit(Segment, FVector(bInflated ? Inflate : 0.0f), MaxT,
		[&](int32 Tri, float CurrentMaxT)
		{
			const FVector& V0 = Mesh.Positions[static_cast<int32>(Mesh.Indices[Tri * 3 + 0])];
			const FVector& V1 = Mesh.Positions[static_cast<int32>(Mesh.Indices[Tri * 3 + 1])];
			const FVector& V2 = Mesh.Positions[static_cast<int32>(Mesh.Indices[Tri * 3 + 2])];
			float HitT = 1.0f;
			FVector HitN = FVector::ZeroVector;
			const bool bOk = bInflated ? SegmentTriangleInflated(Start, End, V0, V1, V2, Inflate, HitT, HitN)
									   : SegmentTriangle(Start, End, V0, V1, V2, HitT, HitN);
			if (bOk && (HitT < BestT || (HitT == BestT && Tri > BestTri)))
			{
				BestT = HitT;
				BestN = HitN;
				BestTri = Tri;
				return HitT;
			}
			return CurrentMaxT;
		});
	if (BestTri == INDEX_NONE)
	{
		return false;
	}
	OutT = BestT;
	OutNormal = BestN;
	if (OutFaceIndex != nullptr)
	{
		*OutFaceIndex = BestTri;
	}
	return true;
}
