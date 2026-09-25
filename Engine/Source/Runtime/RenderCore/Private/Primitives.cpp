#include "Primitives.h"

// The shapes are the legacy (Y-up) primitives in the engine basis: every position and normal has Y and Z swapped, UVs
// and index order are kept (FLegacyCoordinateConversion), so they look the same on screen.

FMeshData MakeCube()
{
	// 6 faces x 4 vertices (unique normals / UVs per face corner).
	constexpr float H = 0.5f * PrimitiveEdgeLength;
	FMeshData Data;
	Data.Vertices = {
		// +Y
		FVertex(FVector(-H, H, -H), FVector(0, 1, 0), FVector2D(0, 0)),
		FVertex(FVector(H, H, -H), FVector(0, 1, 0), FVector2D(1, 0)),
		FVertex(FVector(H, H, H), FVector(0, 1, 0), FVector2D(1, 1)),
		FVertex(FVector(-H, H, H), FVector(0, 1, 0), FVector2D(0, 1)),
		// -Y
		FVertex(FVector(H, -H, -H), FVector(0, -1, 0), FVector2D(0, 0)),
		FVertex(FVector(-H, -H, -H), FVector(0, -1, 0), FVector2D(1, 0)),
		FVertex(FVector(-H, -H, H), FVector(0, -1, 0), FVector2D(1, 1)),
		FVertex(FVector(H, -H, H), FVector(0, -1, 0), FVector2D(0, 1)),
		// +Z (top)
		FVertex(FVector(-H, H, H), FVector(0, 0, 1), FVector2D(0, 0)),
		FVertex(FVector(H, H, H), FVector(0, 0, 1), FVector2D(1, 0)),
		FVertex(FVector(H, -H, H), FVector(0, 0, 1), FVector2D(1, 1)),
		FVertex(FVector(-H, -H, H), FVector(0, 0, 1), FVector2D(0, 1)),
		// -Z (bottom)
		FVertex(FVector(-H, -H, -H), FVector(0, 0, -1), FVector2D(0, 0)),
		FVertex(FVector(H, -H, -H), FVector(0, 0, -1), FVector2D(1, 0)),
		FVertex(FVector(H, H, -H), FVector(0, 0, -1), FVector2D(1, 1)),
		FVertex(FVector(-H, H, -H), FVector(0, 0, -1), FVector2D(0, 1)),
		// +X
		FVertex(FVector(H, H, -H), FVector(1, 0, 0), FVector2D(0, 0)),
		FVertex(FVector(H, -H, -H), FVector(1, 0, 0), FVector2D(1, 0)),
		FVertex(FVector(H, -H, H), FVector(1, 0, 0), FVector2D(1, 1)),
		FVertex(FVector(H, H, H), FVector(1, 0, 0), FVector2D(0, 1)),
		// -X
		FVertex(FVector(-H, -H, -H), FVector(-1, 0, 0), FVector2D(0, 0)),
		FVertex(FVector(-H, H, -H), FVector(-1, 0, 0), FVector2D(1, 0)),
		FVertex(FVector(-H, H, H), FVector(-1, 0, 0), FVector2D(1, 1)),
		FVertex(FVector(-H, -H, H), FVector(-1, 0, 0), FVector2D(0, 1)),
	};

	Data.Indices.Reserve(36);
	for (uint32 Face = 0; Face < 6; ++Face)
	{
		const uint32 B = Face * 4;
		Data.Indices.Append({B + 0, B + 1, B + 2, B + 0, B + 2, B + 3});
	}
	return Data;
}

FMeshData MakePlane(float Size, float UvScale)
{
	const float H = Size * 0.5f;
	FMeshData Data;
	Data.Vertices = {
		FVertex(FVector(-H, -H, 0.0f), FVector(0.0f, 0.0f, 1.0f), FVector2D(0.0f, 0.0f)),
		FVertex(FVector(H, -H, 0.0f), FVector(0.0f, 0.0f, 1.0f), FVector2D(UvScale, 0.0f)),
		FVertex(FVector(H, H, 0.0f), FVector(0.0f, 0.0f, 1.0f), FVector2D(UvScale, UvScale)),
		FVertex(FVector(-H, H, 0.0f), FVector(0.0f, 0.0f, 1.0f), FVector2D(0.0f, UvScale)),
	};
	// Front face seen from above (+Z).
	Data.Indices = {0, 2, 1, 0, 3, 2};
	return Data;
}

FMeshData MakeSphere(int32 Segments, int32 Rings)
{
	Segments = FMath::Max(Segments, 3);
	Rings = FMath::Max(Rings, 2);

	FMeshData Data;
	Data.Vertices.Reserve((Rings + 1) * (Segments + 1));
	Data.Indices.Reserve(Rings * Segments * 6);

	constexpr float Radius = 0.5f * PrimitiveEdgeLength;
	for (int32 Ring = 0; Ring <= Rings; ++Ring)
	{
		const float V = static_cast<float>(Ring) / static_cast<float>(Rings);
		const float Phi = V * PI;
		const float SinPhi = FMath::Sin(Phi);
		const float CosPhi = FMath::Cos(Phi);
		for (int32 Segment = 0; Segment <= Segments; ++Segment)
		{
			const float U = static_cast<float>(Segment) / static_cast<float>(Segments);
			const float Theta = U * 2.0f * PI;
			// Poles on Z: V = 0 is the top (+Z).
			const FVector Normal(FMath::Cos(Theta) * SinPhi, FMath::Sin(Theta) * SinPhi, CosPhi);
			Data.Vertices.Add(FVertex(Normal * Radius, Normal, FVector2D(U, 1.0f - V)));
		}
	}

	for (int32 Ring = 0; Ring < Rings; ++Ring)
	{
		for (int32 Segment = 0; Segment < Segments; ++Segment)
		{
			const uint32 I0 = static_cast<uint32>(Ring * (Segments + 1) + Segment);
			const uint32 I1 = I0 + static_cast<uint32>(Segments + 1);
			// Front faces outward (matches outward normals + back-face cull).
			Data.Indices.Append({I0, I0 + 1, I1, I0 + 1, I1 + 1, I1});
		}
	}
	return Data;
}
