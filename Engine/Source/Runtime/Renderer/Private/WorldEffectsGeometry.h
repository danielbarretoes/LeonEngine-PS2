#pragma once

#include "CoreMinimal.h"

class FEffectSpritePool;
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
 * The geometry and the mask of the world's impact marks, tracers and effect sprites (UWorld::ImpactMarks,
 * UWorld::Tracers, UWorld::EffectSprites), which the GS scene renderer draws: two triangles a mark, lifted off its
 * surface and turned by its serial, two a tracer, facing the camera, and two a sprite, square to the view. No GPU
 * calls, so the tests check it directly.
 */
struct FWorldEffectsGeometry
{
	/** The side of the mask texture, texels. */
	static constexpr int32 MaskSize = 32;

	/** The mask: MaskSize x MaskSize RGBA8 texels of a soft round spot (1 inside a quarter of the side, 0 at the rim).
	 */
	static void BuildMaskTexels(TArray<uint8>& OutTexels);
	/**
	 * The two triangles of each active mark, in slot order (six vertices a mark), into an array of the heap's or the
	 * frame's stack's allocator (TMemStackAllocator).
	 */
	template <typename AllocatorType>
	static void BuildImpactMarkVertices(
		const FImpactMarkPool& Marks, TArray<FWorldEffectVertex, AllocatorType>& OutVertices);
	/** The two triangles of each tracer, facing CameraLocation. */
	template <typename AllocatorType>
	static void BuildTracerVertices(const FTracerBatch& Tracers, const FVector& CameraLocation,
		TArray<FWorldEffectVertex, AllocatorType>& OutVertices);
	/**
	 * The two triangles of each effect sprite in use, square to the view (ViewMatrix: world to view space, its x right
	 * and its y up), the farthest from CameraLocation first (the alpha blending's order).
	 */
	template <typename AllocatorType>
	static void BuildEffectSpriteVertices(const FEffectSpritePool& Sprites, const FMatrix& ViewMatrix,
		const FVector& CameraLocation, TArray<FWorldEffectVertex, AllocatorType>& OutVertices);

	/** A blob shadow's opacity at the floor, its size over the bounds' larger half width, and the height it fades by.
	 */
	static constexpr float BlobShadowOpacity = 0.6f;
	static constexpr float BlobShadowSizeScale = 1.2f;
	static constexpr float BlobShadowFadeHeight = 150.0f;

	/** A blob shadow on the floor (Docs/PLANS/ps2-shipping.md N15, UPrimitiveComponent::bCastBlobShadow). */
	struct FBlobShadow
	{
		/** The point of the floor's plane under the bounds' centre, and the floor's unit normal. */
		FVector Center = FVector::ZeroVector;
		FVector Normal = FVector::UpVector;
		/** Half the square's side, cm: the mask's dark core is its inner half. */
		float HalfSize = 0.0f;
		float Opacity = 0.0f;
	};

	/**
	 * Where a primitive of world Bounds casts its blob shadow on a floor through FloorPoint with FloorNormal: under the
	 * bounds' centre along the vertical, BlobShadowSizeScale times the bounds' larger half width across,
	 * BlobShadowOpacity at the floor fading to nothing BlobShadowFadeHeight above it (the bounds' bottom over the
	 * floor). False when there is nothing to draw (too high, a floor steeper than 60 degrees, empty bounds).
	 */
	static bool PlaceBlobShadow(
		const FBox& Bounds, const FVector& FloorPoint, const FVector& FloorNormal, FBlobShadow& OutShadow);
	/** The two triangles of a blob shadow lying on its floor (lifted off it, black with its opacity, the mask's UV). */
	template <typename AllocatorType>
	static void AddBlobShadowVertices(
		const FBlobShadow& Shadow, TArray<FWorldEffectVertex, AllocatorType>& OutVertices);
};
