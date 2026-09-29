#pragma once

#include "CoreMinimal.h"
#include "GSCommandList.h"
#include "GSLocalMemory.h"
#include "GSTexelDecoder.h"
#include "GSTypes.h"

/**
 * A software Graphics Synthesizer: it executes an FGSCommandList into its local memory by the GS User's Manual, and
 * is the oracle the Win64 preview's GS emulator and the PS2 backend are compared with (Docs/PLANS/ps2-gs-parity.md).
 *
 * What it follows from the manual:
 * - Primitives (3.2): the vertex queue of each primitive type, XYZ2 / XYZF2 kicking and XYZ3 / XYZF3 not; flat shading
 *   takes the color set before the kick; a sprite takes Z and fog from its second vertex.
 * - Rasterization (2.4.4, 3.2.9): pixel centers on integer window coordinates, the window coordinates being the
 *   primitive's minus XYOFFSET; a triangle's or a sprite's left and top sides are drawn, the right and bottom ones not;
 *   a point draws its nearest pixel.
 * - Texture mapping (3.4): STQ (s = S / Q, perspective correct) or UV (affine); texel centers at .5; the wrap modes
 *   (the region fields shifted by the level); point and bilinear sampling; the LOD formula per pixel from Q and the
 *   MIPMAP filters with MIPTBP1 / MIPTBP2's levels; the formats (5-bit colors shifted left 3, TEXA's alpha, CLUTs in
 *   CSM1 through the temporary buffer at CSA, loaded by CLD with CBP0 / CBP1); the texture functions with
 *   A * B = (A x B) >> 7 clamped.
 * - Fog (3.5), the scissor (inclusive), alpha, destination alpha and depth tests with AFAIL (3.7), blending
 *   (A - B) * C >> 7 + D with PABE (3.8), dithering, color clamp, FBA, the 16-bit packing and FBMSK (3.9).
 * - Transfers (4.3): host to local, packed as the manual's transfer format.
 *
 * Where the manual gives no precision, it uses exact arithmetic: attributes are interpolated as the vertices' sum
 * weighted by the edge functions over the area, one rounding in double precision (whole where the GS's would be
 * whole; the DDA's rounding is not modeled), then rounded to nearest; bilinear weights are exact, the LOD's log2 is in
 * double precision, and a blend's negative (A - B) * C shifts toward minus infinity. Lines are stepped by GSStepLine
 * (GSCore, shared with the emulator): along their major axis without their end point (the manual only sketches it).
 * The memory is FGSLocalMemory, laid out as the GS's (pages, blocks and columns, manual chapter 8), shared with the
 * desktop's emulator. A CLUT is read in CSM1 from a buffer 64 pixels wide (upload it with DBW = 1). A Z beyond the Z
 * buffer format's range is clamped to its maximum (the manual does not say; the renderer keeps Z in range).
 */
class GSREFERENCE_API FGSReferenceRasterizer
{
public:
	FGSReferenceRasterizer();

	/** Executes the list's register writes in order, drawing and transferring into the local memory. */
	void Execute(const FGSCommandList& List);

	/**
	 * Width x Height pixels of a frame buffer as RGBA: 16-bit colors shifted left 3 with the alpha bit as 0x80, 24-bit
	 * pixels with alpha 0x80.
	 */
	[[nodiscard]] TArray<FColor> ReadFrame(const FGSFrame& Frame, uint32 Width, uint32 Height) const;
	/** A Z buffer's value at (X, Y) in a buffer WidthPixels wide (a multiple of 64, as FBW gives it). */
	[[nodiscard]] uint32 ReadZ(const FGSZBuf& ZBuf, uint32 WidthPixels, uint32 X, uint32 Y) const;

	[[nodiscard]] const FGSLocalMemory& GetMemory() const
	{
		return Memory;
	}

private:
	/** A vertex as the GS latches it at an XYZ write: the registers' current values. */
	struct FVertex
	{
		/** Window coordinates in sixteenths of a pixel (the primitive's minus XYOFFSET). */
		int32 X = 0;
		int32 Y = 0;
		uint32 Z = 0;
		uint8 F = 0;
		FGSRGBAQ Color;
		FGSST ST;
		FGSUV UV;
	};

	/** A pixel's inputs from the rasterizer. */
	struct FFragment
	{
		int32 X = 0;
		int32 Y = 0;
		double Z = 0.0;
		double F = 0.0;
		double R = 0.0;
		double G = 0.0;
		double B = 0.0;
		double A = 0.0;
		double S = 0.0;
		double T = 0.0;
		double Q = 1.0;
		double U = 0.0;
		double V = 0.0;
	};

	/** The registers of one drawing context. */
	struct FContext
	{
		FGSTex0 Tex0;
		FGSTex1 Tex1;
		FGSClamp Clamp;
		FGSMipTbp MipTbp1;
		FGSMipTbp MipTbp2;
		FGSXYOffset XYOffset;
		FGSScissor Scissor;
		FGSAlpha Alpha;
		FGSTest Test;
		bool bFba = false;
		FGSFrame Frame;
		FGSZBuf ZBuf;
	};

	void WriteRegister(const FGSRegisterWrite& Write, const FGSCommandList& List);
	void AddVertex(uint16 X, uint16 Y, uint32 Z, uint8 F, bool bKick);

	void DrawPoint(const FVertex& Vertex);
	void DrawLine(const FVertex& From, const FVertex& To);
	void DrawTriangle(const FVertex& V0, const FVertex& V1, const FVertex& V2);
	void DrawSprite(const FVertex& V0, const FVertex& V1);
	/** The pixel pipeline for one fragment: texture, fog, tests, blending, the frame buffer and Z writes. */
	void ShadePixel(const FFragment& Fragment);

	/** The texture's color at a fragment (LOD, MIPMAP level and filter chosen as TEX1 says). */
	[[nodiscard]] FColor SampleTexture(const FContext& Context, const FFragment& Fragment) const;
	/** One level's texel at integer texel coordinates, wrapped, converted to RGBA. */
	[[nodiscard]] FColor FetchTexel(const FContext& Context, uint32 Level, int32 U, int32 V) const;
	[[nodiscard]] FColor FilterLevel(const FContext& Context, uint32 Level, double U, double V, bool bBilinear) const;

	[[nodiscard]] const FContext& GetContext() const
	{
		return Contexts[Prim.Context];
	}

	FGSLocalMemory Memory;
	FContext Contexts[2];
	FGSPrim Prim;
	FGSRGBAQ RGBAQ;
	FGSST ST;
	FGSUV UV;
	FGSFog Fog;
	FGSFogCol FogCol;
	FGSDimx Dimx;
	bool bDither = false;
	bool bColorClamp = true;
	bool bPixelAlphaBlend = false;
	FGSTexA TexA;

	/** The pending transfer's registers. */
	FGSBitBltBuf BitBltBuf;
	FGSTrxPos TrxPos;
	FGSTrxReg TrxReg;

	/** The CLUT temporary buffer (FGSTexelDecoder decodes through it). */
	FGSClutBuffer Clut;

	/** The vertex queue of the current primitive, and the fan's first vertex. */
	TArray<FVertex> Queue;
	FVertex FanFirst;
	int32 NumVertices = 0;
};
