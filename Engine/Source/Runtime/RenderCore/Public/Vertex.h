#pragma once

#include "CoreMinimal.h"

/**
 * A static mesh vertex: what the GS scene renderer transforms, lights and textures (UE: the position, tangent and UV
 * vertex buffers of FStaticMeshVertexBuffers, which Leon keeps as one array). Position in the mesh's space
 * (centimetres), unit normal, UV (the albedo map's, scaled by the material's UvScale).
 */
struct RENDERCORE_API FVertex
{
	FVector Position = FVector::ZeroVector;
	FVector Normal = FVector::ZeroVector;
	FVector2D TexCoord = FVector2D::ZeroVector;

	FVertex() = default;

	FVertex(const FVector& InPosition, const FVector& InNormal, const FVector2D& InTexCoord)
		: Position(InPosition)
		, Normal(InNormal)
		, TexCoord(InTexCoord)
	{
	}
};

// A mesh's vertex memory on the EE is 32 bytes per vertex (the PS2 budgets count it).
static_assert(sizeof(FVertex) == 32, "FVertex is 32 bytes: position, normal and UV");
