#pragma once

#include "CoreMinimal.h"

/**
 * The VU1 microprograms that draw a vertex batch on the PS2 (Docs/PLANS/ps2-shipping.md N14). Each reads the batch's
 * streams as a LPS2 v2 batch holds them and builds the GIF packet of its triangle strip in VU1's memory; the skinned
 * ones pose each vertex first with its two palette bones (N14b).
 */
enum class EGSVertexProgram : uint8
{
	/** The baked colour times the material's colour. */
	StaticUnlit,
	/** The baked colour times the material's colour times the ambient share plus one directional light (Lambert). */
	StaticLit,
	/** StaticUnlit of a skinned batch, posed by its palette. */
	SkinnedUnlit,
	/** StaticLit of a skinned batch, posed by its palette (the normal too). */
	SkinnedLit,
};

/**
 * A bone's skin matrix as VU1 reads it (3 quadwords, whose alignment the palette's memory gives): UE's row vector
 * matrix M (InverseBindPose * the pose, the mesh's space) without its fourth column, Rows[R] = (M[R][0], M[R][1],
 * M[R][2], M[3][R]): a position is posed as X Rows[0] + Y Rows[1] + Z Rows[2] + (Rows[0].W, Rows[1].W, Rows[2].W), a
 * normal without the last term.
 */
struct GSCORE_API FGSSkinMatrix
{
	float Rows[3][4] = {{1.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 0.0f}};

	/** M's skin matrix (M an affine transform: its fourth column is (0, 0, 0, 1)). */
	[[nodiscard]] static FGSSkinMatrix FromMatrix(const FMatrix& M);
	/** (P, 1) * M in FMatrix::TransformPosition's order of operations. */
	[[nodiscard]] FVector TransformPosition(const FVector& P) const
	{
		return FVector(P.X * Rows[0][0] + P.Y * Rows[1][0] + P.Z * Rows[2][0] + Rows[0][3],
			P.X * Rows[0][1] + P.Y * Rows[1][1] + P.Z * Rows[2][1] + Rows[1][3],
			P.X * Rows[0][2] + P.Y * Rows[1][2] + P.Z * Rows[2][2] + Rows[2][3]);
	}
	/** (V, 0) * M. */
	[[nodiscard]] FVector TransformVector(const FVector& V) const
	{
		return FVector(V.X * Rows[0][0] + V.Y * Rows[1][0] + V.Z * Rows[2][0],
			V.X * Rows[0][1] + V.Y * Rows[1][1] + V.Z * Rows[2][1],
			V.X * Rows[0][2] + V.Y * Rows[1][2] + V.Z * Rows[2][2]);
	}
};
static_assert(sizeof(FGSSkinMatrix) == 48, "A skin matrix is three quadwords");

/**
 * The lights of a frame in world space, and the per vertex Lambert the renderer lights what moves with (the C++
 * reference of the lit microprogram): the ambient light (the map's environment), then each directional light by N.L,
 * then each point light by N.L and its range attenuation squared. No specular: the GS has no pixel stage.
 */
struct GSCORE_API FGSVertexLights
{
	static constexpr int32 MaxDirectional = 2;
	static constexpr int32 MaxPoint = 4;

	struct FDirectional
	{
		/** Where the light travels (unit). */
		FVector Direction = FVector::ForwardVector;
		FVector Color = FVector::OneVector;
	};

	struct FPoint
	{
		FVector Position = FVector::ZeroVector;
		FVector Color = FVector::OneVector;
		/** The range in centimetres: nothing is lit farther. */
		float Radius = 100.0f;
	};

	/** The ambient light (RGB). */
	FVector Ambient = FVector::ZeroVector;
	int32 NumDirectional = 0;
	int32 NumPoint = 0;
	FDirectional Directional[MaxDirectional];
	FPoint Point[MaxPoint];

	/** The light reaching a world point with a unit normal (RGB). */
	[[nodiscard]] FVector Irradiance(const FVector& Position, const FVector& Normal) const;
};

/**
 * The GS's fog of a draw (manual 3.5; Docs/PLANS/ps2-shipping.md N15): each vertex's fog coefficient F (XYZF2), 255
 * the vertex's colour and 0 the fog colour (FOGCOL), which the GS blends between per pixel with PRIM's FGE. The
 * renderer's linear fog makes F a line in the vertex's depth: F = Offset + Scale x w (w the clip space w: the view's
 * depth, cm), clamped to 0..255 and rounded. VU1's microprograms compute the same (the header's quadword 7, z and w).
 */
struct GSCORE_API FGSVertexFog
{
	/** FGE on the draw's primitives and F on its vertices; off, F is 255 and the GS ignores it. */
	bool bEnabled = false;
	float Scale = 0.0f;
	float Offset = 255.0f;

	/**
	 * The linear fog from StartDistance (F 255: no fog) to EndDistance (F 0: the fog's colour), in the view's depth;
	 * an end not beyond the start fogs everything past the start.
	 */
	[[nodiscard]] static FGSVertexFog MakeLinear(float StartDistance, float EndDistance);

	/** F at a vertex of clip w: Offset + Scale x w, clamped to 0..255 and rounded (VU1 adds 0.5 and truncates). */
	[[nodiscard]] uint8 GetF(float W) const
	{
		const float F = FMath::Clamp(Offset + (Scale * W), 0.0f, 255.0f);
		return uint8(FMath::FloorToInt(F + 0.5f));
	}
};

/**
 * How a mesh section is drawn: the state every vertex batch of it shares (a mesh's LPS2 v2 quantization, its
 * transforms, its material and the frame's lights). FGSCommandList keeps the draws its batches name.
 */
struct GSCORE_API FGSVertexDraw
{
	/** A position is Quantized * PositionScale + PositionBias, per axis, in the mesh's space (LPS2 v2's header). */
	float PositionScale[3] = {1.0f, 1.0f, 1.0f};
	float PositionBias[3] = {0.0f, 0.0f, 0.0f};
	/** UE's row vector transforms: the mesh's space to clip space (x, y in [-w, w], z in [0, w]) and to the world. */
	FMatrix LocalToClip = FMatrix::Identity;
	FMatrix LocalToWorld = FMatrix::Identity;
	/**
	 * A normal of the mesh's space to the world: the inverse transpose of LocalToWorld (its transpose adjoint, negated
	 * when LocalToWorld mirrors), normalized after the transform.
	 */
	FMatrix NormalToWorld = FMatrix::Identity;
	/** The material's colour (RGB) and alpha, which the baked colour scales; lit, the light scales the RGB too. */
	FLinearColor Color = FLinearColor::White;
	/** The material's repeats per texture coordinate unit. */
	FVector2D UvScale = FVector2D(1.0f, 1.0f);
	bool bLit = false;
	/** A skinned mesh's section: its batches have skin streams and palettes (FGSVertexBatch::Skin). */
	bool bSkinned = false;
	/** The primitive's TME (the colour then is MODULATE's: 0x80 is 1.0) and ABE. */
	bool bTextured = false;
	bool bBlend = false;
	/** The frame's lights (lit draws: what is lit per frame, not a Static component's baked light). */
	FGSVertexLights Lights;
	/** The fog (the world pass's; none in the view model pass). */
	FGSVertexFog Fog;

	/** The point lights VU1's lit programs take (N29). */
	static constexpr int32 MaxVU1PointLights = 2;

	/**
	 * The microprogram that draws the batches of this draw on VU1, false when none does: VU1 lights with the ambient
	 * share, one directional light and up to MaxVU1PointLights point lights, so a lit draw with more is the EE's.
	 * A skinned draw's are the Skinned programs.
	 */
	[[nodiscard]] bool GetProgram(EGSVertexProgram& OutProgram) const;
};

/**
 * A vertex batch to draw (FGSCommandList::DrawVertexBatch): LPS2 v2's batch of at most MaxVertices vertices of
 * triangle strips, read where the mesh keeps it (each stream quadword aligned: the PS2 hands them to the VIF by
 * reference, uncopied), drawn with the list's draw Draw. Its vertices must need no clipping: the batch's sphere is
 * inside the guard band and between the near and far planes (plan D8). A skinned batch (at most MaxSkinnedVertices)
 * poses each vertex with its skin's two palette bones first: Weight0 (V M0) + Weight1 (V M1), the normal the same way.
 *
 * What it draws is what FGSPrimitiveEmitter::AddVertexBatch sends (the reference): a TRISTRIP of the batch's vertices,
 * Gouraud, textured and blended as the draw says, where vertex i closes the triangle (i - 2, i - 1, i) unless its
 * strip flags say no kick, wound (i - 1, i - 2, i) when they say reversed; a triangle outside a side of the view or
 * back facing is not drawn.
 */
struct GSCORE_API FGSVertexBatch
{
	/** LPS2 v2's batch size, what VU1's double buffered data memory takes. */
	static constexpr uint32 MaxVertices = 64;
	/** A skinned batch's size and palette (LPS2 v2's skinned batch). */
	static constexpr uint32 MaxSkinnedVertices = 48;
	static constexpr uint32 MaxBones = 24;
	/** The strip flags in a normal's W (LPS2 v2). */
	static constexpr uint8 FlagNoKick = 0x80;
	static constexpr uint8 FlagReversed = 0x01;

	/** The draw it belongs to: an index of the list's GetVertexDraws. */
	int32 Draw = 0;
	/** 3 to MaxVertices. */
	uint32 NumVertices = 0;
	/** 3 int16 a vertex, quantized (the draw's scale and bias). */
	const int16* Positions = nullptr;
	/** 4 int8 a vertex: the normal times 127, then the strip flags. */
	const int8* Normals = nullptr;
	/** 4 uint8 a vertex: the baked RGBA. */
	const uint8* Colors = nullptr;
	/** 2 int16 a vertex: 4.12 fixed point, plus TexCoordOffset. */
	const int16* TexCoords = nullptr;
	/** The whole repeats added to the texture coordinates. */
	float TexCoordOffset[2] = {0.0f, 0.0f};
	/**
	 * A skinned batch's skin, 4 uint8 a vertex: two palette indices, then their weights in 1/255 steps (adding up to
	 * 255); null for a static batch.
	 */
	const uint8* Skin = nullptr;
	/**
	 * A skinned batch's palette: NumBones skin matrices of the pose, which its skin's indices name. Recorded in a list
	 * it is the list's (FGSCommandList::AllocateSkinPalette: the PS2 sends it to VU1 by reference).
	 */
	const FGSSkinMatrix* Palette = nullptr;
	uint32 NumBones = 0;

	[[nodiscard]] bool IsSkinned() const
	{
		return Skin != nullptr;
	}
};
