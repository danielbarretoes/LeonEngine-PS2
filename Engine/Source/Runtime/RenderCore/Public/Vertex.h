#pragma once

#include "CoreMinimal.h"

/** Interleaved GPU vertex attributes (matches the mesh VAO layout and the .lmesh vertex records). */
struct RENDERCORE_API FVertex
{
	FVector Position = FVector::ZeroVector;
	FVector Normal = FVector::ZeroVector;
	FVector2D TexCoord = FVector2D::ZeroVector;
	/** XYZ = tangent; W = bitangent handedness (+-1) for mirrored UVs. */
	FVector4 Tangent = FVector4(0.0f, 0.0f, 0.0f, 1.0f);

	FVertex() = default;

	FVertex(const FVector& InPosition, const FVector& InNormal, const FVector2D& InTexCoord,
		const FVector4& InTangent = FVector4(0.0f, 0.0f, 0.0f, 1.0f))
		: Position(InPosition)
		, Normal(InNormal)
		, TexCoord(InTexCoord)
		, Tangent(InTangent)
	{
	}
};

// The .lmesh format stores vertices as raw 48-byte records.
static_assert(sizeof(FVertex) == 48, "FVertex must stay 48 bytes (.lmesh vertex records)");
static_assert(offsetof(FVertex, Normal) == 12 && offsetof(FVertex, TexCoord) == 24 && offsetof(FVertex, Tangent) == 32,
	"FVertex attribute offsets are part of the .lmesh format and the VAO layout");
