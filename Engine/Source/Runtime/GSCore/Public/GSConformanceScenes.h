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
	/** PSMT8 and PSMT4 through CSM1 CLUTs, and PSMCT16 with TEXA. */
	GSCORE_API void BuildClutAndFormats(FGSCommandList& List);
	/** MODULATE, HIGHLIGHT, fog and a fixed MIPMAP level. */
	GSCORE_API void BuildFunctionsFogAndMipmap(FGSCommandList& List);
	/** The alpha test with AFAIL, and the depth tests. */
	GSCORE_API void BuildPixelTests(FGSCommandList& List);
	/** Blending, COLCLAMP, PABE, FBA and FBMSK. */
	GSCORE_API void BuildBlendAndWrite(FGSCommandList& List);
	/** Dithering into a PSMCT16S frame buffer. */
	GSCORE_API void BuildDither16(FGSCommandList& List);

	/** Every scene, in the order above. */
	[[nodiscard]] GSCORE_API TArrayView<const FGSConformanceScene> GetScenes();
} // namespace GSConformance
