#pragma once

#include "Containers/ArrayView.h"
#include "CoreMinimal.h"
#include "GSCommandList.h"
#include "GSTypes.h"

/** A conformance scene: its name, its frame buffer's format and the function that records it. */
struct FGSConformanceScene
{
	const TCHAR* Name;
	EGSPixelFormat FrameFormat;
	void (*Build)(FGSCommandList& List);
};

/**
 * The GS conformance scenes (Docs/PLANS/ps2-gs-parity.md, P2 and P3): each exercises one area of the GS User's Manual
 * in a 64 x 32 frame buffer at FBP 0 (FBW 1) with its Z buffer at ZBP 32, and uses local memory below 512 KB only.
 * The reference rasterizer's tests check them pixel by pixel; the GSConformance program draws them on the PS2, and the
 * Win64 preview must match them too. Each scene sets up and clears its frame and Z buffer, so they run in any order.
 */
namespace GSConformance
{
	constexpr uint32 FrameWidth = 64;
	constexpr uint32 FrameHeight = 32;
	/** The scenes use the local memory below this address only. */
	constexpr uint32 LocalMemoryBytes = 512 * 1024;

	[[nodiscard]] GSCORE_API FGSFrame MakeFrame(EGSPixelFormat Format = EGSPixelFormat::PSMCT32);
	/** The Z buffer (PSMZ32 at ZBP 32); bMask leaves it unwritten. */
	[[nodiscard]] GSCORE_API FGSZBuf MakeZBuf(bool bMask = true);

	/** Top-left fill rule for triangles and sprites. */
	GSCORE_API void BuildDrawingRules(FGSCommandList& List);
	/** Strips, fans, XYZ3, points, lines and flat shading. */
	GSCORE_API void BuildPrimitives(FGSCommandList& List);
	GSCORE_API void BuildGouraudAndScissor(FGSCommandList& List);
	/** Point and bilinear sampling, and the wrap modes. */
	GSCORE_API void BuildTextureSampling(FGSCommandList& List);
	/**
	 * PSMT8 and PSMT4 through CSM1 CLUTs, PSMCT16 with TEXA, and paletted MIPMAP levels through one CLUT (trilinear,
	 * the LOD from Q) with the CLUT loaded by CLD 2 to 5 as the scene renderer's texture cache loads it.
	 */
	GSCORE_API void BuildClutAndFormats(FGSCommandList& List);
	/** Two PSMT8 textures in adjacent blocks with their CLUTs in adjacent 4-block runs (the GS's local memory layout).
	 */
	GSCORE_API void BuildTwoPalettes(FGSCommandList& List);
	/** MODULATE, HIGHLIGHT, fog and a fixed MIPMAP level. */
	GSCORE_API void BuildFunctionsFogAndMipmap(FGSCommandList& List);
	/** The alpha test with AFAIL, and the depth tests. */
	GSCORE_API void BuildPixelTests(FGSCommandList& List);
	/** Blending, COLCLAMP, PABE, FBA and FBMSK. */
	GSCORE_API void BuildBlendAndWrite(FGSCommandList& List);
	/** Dithering into a PSMCT16S frame buffer. */
	GSCORE_API void BuildDither16(FGSCommandList& List);
	/**
	 * MIPMAP: the LOD from Q (LCM 0) with NEAREST_MIPMAP_NEAREST, NEAREST_MIPMAP_LINEAR and LINEAR_MIPMAP_LINEAR, L,
	 * K and MXL; fixed LODs (LCM 1) with MMAG below 0; bilinear levels, trilinear and a floor in perspective.
	 */
	GSCORE_API void BuildMipmapLod(FGSCommandList& List);
	/** The alpha test's eight methods, its four AFAIL, and FB_ONLY on a Gouraud alpha ramp. */
	GSCORE_API void BuildAlphaTest(FGSCommandList& List);
	/** Fog: XYZF2's F interpolated, textured, a sprite's second vertex, the FOG register with XYZ2, XYZF3. */
	GSCORE_API void BuildFog(FGSCommandList& List);
	/** TEXA (TA0, TA1, AEM) on 16-bit and 24-bit texels, and the four texture functions with TCC RGB and RGBA. */
	GSCORE_API void BuildTexAAndFunctions(FGSCommandList& List);
	/** REPEAT, CLAMP, REGION_CLAMP and REGION_REPEAT: point, bilinear, on MIPMAP level 1 and from negative STQ. */
	GSCORE_API void BuildClampModes(FGSCommandList& List);
	/** A strip and a fan skipping a triangle with XYZ3, mirrored and STQ sprites, lines, a line strip, points. */
	GSCORE_API void BuildStripsAndSprites(FGSCommandList& List);
	/** The whole (A - B) * C >> 7 + D: Cd factors, Ad, FIX and As above 0x80, negative results, COLCLAMP wrapping. */
	GSCORE_API void BuildBlendEquation(FGSCommandList& List);
	/** PABE, FBA, the destination alpha test and FBMSK bit by bit. */
	GSCORE_API void BuildPabeFbaDate(FGSCommandList& List);
	/** PSMCT16S: blended pixels dithered against the 16-bit destination, COLCLAMP off, Ad 0 or 0x80, FBMSK. */
	GSCORE_API void BuildDither16Blend(FGSCommandList& List);
	/** CLUT loads by CLD 0 to 5 with CBP0 / CBP1 (a stale CLUT kept on purpose), and two PSMT4 CLUTs by CSA. */
	GSCORE_API void BuildClutLoads(FGSCommandList& List);

	/** Every scene, in the order above. */
	[[nodiscard]] GSCORE_API TArrayView<const FGSConformanceScene> GetScenes();
} // namespace GSConformance
