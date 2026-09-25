#pragma once

#include "CoreMinimal.h"

/** Interleaved GPU vertex attributes (matches the mesh VAO layout: the renderer uploads the array as it is). */
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

// The vertex buffer holds raw 48-byte records (the VAO strides and offsets).
static_assert(sizeof(FVertex) == 48, "FVertex must stay 48 bytes (the VAO stride)");
static_assert(offsetof(FVertex, Normal) == 12 && offsetof(FVertex, TexCoord) == 24 && offsetof(FVertex, Tangent) == 32,
	"FVertex attribute offsets are part of the VAO layout");
