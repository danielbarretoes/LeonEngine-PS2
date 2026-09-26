#pragma once

#include "CoreMinimal.h"

class FImpactMarkPool;
class FTracerBatch;

/** The vertex the world's effects are made of: position, the mask's UV, the colour with the strength in A. */
struct FWorldEffectVertex
{
	FVector Position;
	FVector2D TexCoord;
	FLinearColor Color;
};

/**
 * The geometry and the mask of the world's impact marks and tracers (UWorld::ImpactMarks, UWorld::Tracers), which the
 * GS scene renderer draws: two triangles a mark, lifted off its surface and turned by its serial, and two a tracer,
 * facing the camera. No GPU calls, so the tests check it directly.
 */
struct FWorldEffectsGeometry
{
	/** The side of the mask texture, texels. */
	static constexpr int32 MaskSize = 32;

	/** The mask: MaskSize x MaskSize RGBA8 texels of a soft round spot (1 inside a quarter of the side, 0 at the rim).
	 */
	static void BuildMaskTexels(TArray<uint8>& OutTexels);
	/** The two triangles of each active mark, in slot order (six vertices a mark). */
	static void BuildImpactMarkVertices(const FImpactMarkPool& Marks, TArray<FWorldEffectVertex>& OutVertices);
	/** The two triangles of each tracer, facing CameraLocation. */
	static void BuildTracerVertices(
		const FTracerBatch& Tracers, const FVector& CameraLocation, TArray<FWorldEffectVertex>& OutVertices);
};
