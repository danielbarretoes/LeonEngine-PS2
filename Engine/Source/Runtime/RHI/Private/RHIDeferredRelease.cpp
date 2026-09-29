#include "RHIDeferredRelease.h"

namespace
{

	/** Memory released while a frame was being recorded: freed once that frame has completed. */
	struct FPendingRelease
	{
		uint64 Frame = 0;
		TArray<uint8> Memory;
	};

	uint64 GRecordingFrame = 0;
	uint64 GCompletedFrame = 0;

	TArray<FPendingRelease>& GetPending()
	{
		static TArray<FPendingRelease> Pending;
		return Pending;
	}

} // namespace

void FRHIDeferredRelease::SetFrameCounters(uint64 RecordingFrame, uint64 CompletedFrame)
{
	GRecordingFrame = RecordingFrame;
	GCompletedFrame = CompletedFrame;
	// Nothing in flight (the recording frame not past the completed one): everything goes.
	const bool bIdle = RecordingFrame <= CompletedFrame;
	TArray<FPendingRelease>& Pending = GetPending();
	for (int32 Index = Pending.Num() - 1; Index >= 0; --Index)
	{
		if (bIdle || Pending[Index].Frame <= CompletedFrame)
		{
			Pending.RemoveAt(Index);
		}
	}
}

void FRHIDeferredRelease::Release(TArray<uint8>&& Memory)
{
	if (Memory.Num() == 0 && Memory.Max() == 0)
	{
		return;
	}
	if (GRecordingFrame <= GCompletedFrame)
	{
		Memory.Empty();
		return;
	}
	FPendingRelease& Entry = GetPending().AddDefaulted_GetRef();
	Entry.Frame = GRecordingFrame;
	Entry.Memory = MoveTemp(Memory);
}

int32 FRHIDeferredRelease::GetNumPending()
{
	return GetPending().Num();
}
