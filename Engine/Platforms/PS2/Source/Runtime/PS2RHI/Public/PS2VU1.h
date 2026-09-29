#pragma once

#include "CoreMinimal.h"
#include "GSDrawEnvironment.h"
#include "GSVertexBatch.h"

/**
 * VU1 and its microprograms (Docs/PLANS/ps2-shipping.md N14; Private/VU1/VU1Programs.vsm): what draws the scene's
 * vertex batches (FGSCommandList::DrawVertexBatch) on the PS2. FPS2RHI uploads the programs with the display and hands
 * each frame's batches to them in its VIF1 chain; -novu1 turns VU1 off, and then the renderer sends every batch
 * through the C++ emitter instead (the reference, FGSPrimitiveEmitter::AddVertexBatch).
 */
class PS2RHI_API FPS2VU1
{
public:
	/** The GIF packet a batch leaves in VU1's memory: its GIFtag, then ST, RGBAQ and XYZF2 a vertex (N15's fog). */
	static constexpr uint32 GetPacketQuadwords(uint32 NumVertices)
	{
		return 1 + (3 * NumVertices);
	}

	/** Whether VU1 draws the vertex batches: not with -novu1 on the command line. */
	[[nodiscard]] static bool IsEnabled();

	/** Uploads the microprograms to VU1's micro memory (VIF1 MPG) and waits; once, before the first batch. */
	static void UploadPrograms();

	/**
	 * Runs one batch of Draw on VU1 through the renderer's own VIF1 encoding, without XGKICK, waits for it and copies
	 * the GIF packet it built from VU1's data memory into OutQuadwords (GetPacketQuadwords, as 64-bit pairs): what
	 * VU1Conformance compares with the C++ emitter. Environment gives the screen's mapping. False if VU1 did not end.
	 */
	static bool RunBatchForTest(const FGSVertexDraw& Draw, const FGSVertexBatch& Batch,
		const FGSDrawEnvironment& Environment, TArray<uint64>& OutQuadwords);
};
