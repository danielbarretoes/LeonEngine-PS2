#include "Primitives.h"

FMeshData MakeCube()
{
	// 6 faces x 4 vertices (unique normals / UVs per face corner).
	FMeshData Data;
	Data.Vertices = {
		// +Z
		FVertex(FVector(-0.5f, -0.5f, 0.5f), FVector(0, 0, 1), FVector2D(0, 0)),
		FVertex(FVector(0.5f, -0.5f, 0.5f), FVector(0, 0, 1), FVector2D(1, 0)),
		FVertex(FVector(0.5f, 0.5f, 0.5f), FVector(0, 0, 1), FVector2D(1, 1)),
		FVertex(FVector(-0.5f, 0.5f, 0.5f), FVector(0, 0, 1), FVector2D(0, 1)),
		// -Z
		FVertex(FVector(0.5f, -0.5f, -0.5f), FVector(0, 0, -1), FVector2D(0, 0)),
		FVertex(FVector(-0.5f, -0.5f, -0.5f), FVector(0, 0, -1), FVector2D(1, 0)),
		FVertex(FVector(-0.5f, 0.5f, -0.5f), FVector(0, 0, -1), FVector2D(1, 1)),
		FVertex(FVector(0.5f, 0.5f, -0.5f), FVector(0, 0, -1), FVector2D(0, 1)),
		// +Y
		FVertex(FVector(-0.5f, 0.5f, 0.5f), FVector(0, 1, 0), FVector2D(0, 0)),
		FVertex(FVector(0.5f, 0.5f, 0.5f), FVector(0, 1, 0), FVector2D(1, 0)),
		FVertex(FVector(0.5f, 0.5f, -0.5f), FVector(0, 1, 0), FVector2D(1, 1)),
		FVertex(FVector(-0.5f, 0.5f, -0.5f), FVector(0, 1, 0), FVector2D(0, 1)),
		// -Y
		FVertex(FVector(-0.5f, -0.5f, -0.5f), FVector(0, -1, 0), FVector2D(0, 0)),
		FVertex(FVector(0.5f, -0.5f, -0.5f), FVector(0, -1, 0), FVector2D(1, 0)),
		FVertex(FVector(0.5f, -0.5f, 0.5f), FVector(0, -1, 0), FVector2D(1, 1)),
		FVertex(FVector(-0.5f, -0.5f, 0.5f), FVector(0, -1, 0), FVector2D(0, 1)),
		// +X
		FVertex(FVector(0.5f, -0.5f, 0.5f), FVector(1, 0, 0), FVector2D(0, 0)),
		FVertex(FVector(0.5f, -0.5f, -0.5f), FVector(1, 0, 0), FVector2D(1, 0)),
		FVertex(FVector(0.5f, 0.5f, -0.5f), FVector(1, 0, 0), FVector2D(1, 1)),
		FVertex(FVector(0.5f, 0.5f, 0.5f), FVector(1, 0, 0), FVector2D(0, 1)),
		// -X
		FVertex(FVector(-0.5f, -0.5f, -0.5f), FVector(-1, 0, 0), FVector2D(0, 0)),
		FVertex(FVector(-0.5f, -0.5f, 0.5f), FVector(-1, 0, 0), FVector2D(1, 0)),
		FVertex(FVector(-0.5f, 0.5f, 0.5f), FVector(-1, 0, 0), FVector2D(1, 1)),
		FVertex(FVector(-0.5f, 0.5f, -0.5f), FVector(-1, 0, 0), FVector2D(0, 1)),
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
		FVertex(FVector(-H, 0.0f, -H), FVector(0.0f, 1.0f, 0.0f), FVector2D(0.0f, 0.0f)),
		FVertex(FVector(H, 0.0f, -H), FVector(0.0f, 1.0f, 0.0f), FVector2D(UvScale, 0.0f)),
		FVertex(FVector(H, 0.0f, H), FVector(0.0f, 1.0f, 0.0f), FVector2D(UvScale, UvScale)),
		FVertex(FVector(-H, 0.0f, H), FVector(0.0f, 1.0f, 0.0f), FVector2D(0.0f, UvScale)),
	};
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

	constexpr float Radius = 0.5f;
	for (int32 Y = 0; Y <= Rings; ++Y)
	{
		const float V = static_cast<float>(Y) / static_cast<float>(Rings);
		const float Phi = V * PI;
		const float SinPhi = FMath::Sin(Phi);
		const float CosPhi = FMath::Cos(Phi);
		for (int32 X = 0; X <= Segments; ++X)
		{
			const float U = static_cast<float>(X) / static_cast<float>(Segments);
			const float Theta = U * 2.0f * PI;
			const FVector Normal(FMath::Cos(Theta) * SinPhi, CosPhi, FMath::Sin(Theta) * SinPhi);
			Data.Vertices.Add(FVertex(Normal * Radius, Normal, FVector2D(U, 1.0f - V)));
		}
	}

	for (int32 Y = 0; Y < Rings; ++Y)
	{
		for (int32 X = 0; X < Segments; ++X)
		{
			const uint32 I0 = static_cast<uint32>(Y * (Segments + 1) + X);
			const uint32 I1 = I0 + static_cast<uint32>(Segments + 1);
			// CCW when viewed from outside (matches outward normals + back-face cull).
			Data.Indices.Append({I0, I0 + 1, I1, I0 + 1, I1 + 1, I1});
		}
	}
	return Data;
}
