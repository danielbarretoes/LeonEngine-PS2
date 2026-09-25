#include "MeshData.h"

namespace
{
	/** glm::normalize: V / |V| with no tolerance (the tangents keep the values glm produced). */
	FVector Normalize(const FVector& V)
	{
		return V * (1.0f / FMath::Sqrt(V | V));
	}
} // namespace

void ComputeTangents(FMeshData& Data)
{
	if (Data.IsEmpty())
	{
		return;
	}

	TArray<FVector> TanAcc;
	TArray<FVector> BitAcc;
	TanAcc.Init(FVector::ZeroVector, Data.Vertices.Num());
	BitAcc.Init(FVector::ZeroVector, Data.Vertices.Num());

	for (int32 I = 0; I + 2 < Data.Indices.Num(); I += 3)
	{
		const int32 I0 = static_cast<int32>(Data.Indices[I + 0]);
		const int32 I1 = static_cast<int32>(Data.Indices[I + 1]);
		const int32 I2 = static_cast<int32>(Data.Indices[I + 2]);

		const FVertex& V0 = Data.Vertices[I0];
		const FVertex& V1 = Data.Vertices[I1];
		const FVertex& V2 = Data.Vertices[I2];

		const FVector E1 = V1.Position - V0.Position;
		const FVector E2 = V2.Position - V0.Position;
		const FVector2D D1 = V1.TexCoord - V0.TexCoord;
		const FVector2D D2 = V2.TexCoord - V0.TexCoord;

		const float Det = (D1.X * D2.Y) - (D2.X * D1.Y);
		if (FMath::Abs(Det) < 1e-8f)
		{
			continue;
		}
		const float Inv = 1.0f / Det;
		const FVector Tangent = ((E1 * D2.Y) - (E2 * D1.Y)) * Inv;
		const FVector Bitangent = ((E2 * D1.X) - (E1 * D2.X)) * Inv;
		TanAcc[I0] += Tangent;
		TanAcc[I1] += Tangent;
		TanAcc[I2] += Tangent;
		BitAcc[I0] += Bitangent;
		BitAcc[I1] += Bitangent;
		BitAcc[I2] += Bitangent;
	}

	for (int32 I = 0; I < Data.Vertices.Num(); ++I)
	{
		FVertex& Vertex = Data.Vertices[I];
		const FVector N = Vertex.Normal;
		FVector T = TanAcc[I];
		if ((T | T) < 1e-8f)
		{
			T = FMath::Abs(N.Y) < 0.9f ? Normalize(N ^ FVector(0, 1, 0)) : Normalize(N ^ FVector(1, 0, 0));
			Vertex.Tangent = FVector4(T, 1.0f);
			continue;
		}
		T = Normalize(T - (N * (N | T)));
		const float Handedness = (((N ^ T) | BitAcc[I]) < 0.0f) ? -1.0f : 1.0f;
		Vertex.Tangent = FVector4(T, Handedness);
	}
}
