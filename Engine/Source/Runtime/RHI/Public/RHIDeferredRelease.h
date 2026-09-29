#pragma once

#include "CoreMinimal.h"

/**
 * Memory the RHI may still read after the renderer is done with it, freed once the frames that read it have completed
 * (UE: the RHI's pending deletes, FRHIResource::FlushPendingDeletes). The PS2's frame chain reads a static mesh's LPS2
 * blob in place (its vertex batches by DMA REF: Docs/PLANS/ps2-shipping.md N14) a frame after it is recorded, so a
 * mesh destroyed or rebuilt meanwhile hands its blob here instead of freeing it (FLPS2Mesh).
 *
 * The platform RHI numbers its frames: SetFrameCounters says which frame is being recorded (what it may read of
 * memory released now) and the last one whose reads are over. With the defaults (0, 0), or whenever the recording
 * frame is not past the completed one, nothing is in flight and Release frees at once: the desktop, the tools, the
 * tests.
 */
class RHI_API FRHIDeferredRelease
{
public:
	/**
	 * The platform RHI's frame counters: RecordingFrame, the frame whose lists are being recorded now, and
	 * CompletedFrame, the last one the hardware has finished reading. Frees what the completed frames no longer read.
	 */
	static void SetFrameCounters(uint64 RecordingFrame, uint64 CompletedFrame);

	/** Frees Memory now when no frame in flight can read it, else once the frame being recorded has completed. */
	static void Release(TArray<uint8>&& Memory);

	/** The releases waiting for their frames. */
	[[nodiscard]] static int32 GetNumPending();
};
