#pragma once

#include "CoreMinimal.h"
#include "GSCommandList.h"
#include "GSTypes.h"

/**
 * The drawing environment every GS list is recorded against (Leon): a Width x Height frame buffer and its Z buffer,
 * the window offset that puts pixel (0, 0) of the frame at primitive coordinate 2048 - Width / 2, 2048 - Height / 2
 * (so the frame's center is at 2048, 2048, with room on every side for the guard band), the whole frame as scissor,
 * the depth test GEQUAL (a larger Z is nearer), standard blending (FGSAlpha::Translucent), and dithering on a 16-bit
 * frame buffer. A list recorded against it sets the per-draw registers it needs (PRIM, TEST, ALPHA, TEX0, ...) and
 * leaves the environment's own registers as it found them.
 *
 * The PS2 RHI appends it at the start of each frame and after each recorded list; the scene renderer and the tests use
 * its coordinates.
 */
struct GSCORE_API FGSDrawEnvironment
{
	/** The primitive coordinate of the frame's center. */
	static constexpr float PrimitiveCenter = 2048.0f;
	/** The largest Z of a PSMZ24 buffer, the nearest depth. */
	static constexpr uint32 MaxDepth24 = 0xffffffu;

	FGSFrame Frame;
	FGSZBuf ZBuf;
	uint16 Width = 640;
	uint16 Height = 448;

	/** Appends the environment's registers (context 0). */
	void Append(FGSCommandList& List) const;

	/** The depth test of 3D draws (GEQUAL) or of overlays (ALWAYS). */
	[[nodiscard]] static FGSTest DepthTest(bool bDepthTest);

	/** A vertex at pixel (X, Y) of the frame (top-left origin; pixel centers at integers) with depth Z. */
	[[nodiscard]] FGSXYZ PixelVertex(float X, float Y, uint32 Z = 0) const;
	/** The primitive coordinate of pixel column X and row Y. */
	[[nodiscard]] float PrimitiveX(float X) const
	{
		return PrimitiveCenter - (float(Width) * 0.5f) + X;
	}
	[[nodiscard]] float PrimitiveY(float Y) const
	{
		return PrimitiveCenter - (float(Height) * 0.5f) + Y;
	}
};
