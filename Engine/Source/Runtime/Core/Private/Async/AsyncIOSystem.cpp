#include "Async/AsyncIOSystem.h"

#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/LowLevelMemTracker.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "HAL/UnrealMemory.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/AssertionMacros.h"

// Asynchronous reads (Docs/PLANS/ps2-shipping.md N24): the requests of IAsyncReadFileHandle, the queue the platform's
// IO thread (or the game thread) serves, and the callbacks the game thread runs.

// IAsyncReadRequest

IAsyncReadRequest::IAsyncReadRequest(FAsyncFileCallBack* InCallback, bool bInSizeRequest, uint8* InUserSuppliedMemory)
	: Memory(InUserSuppliedMemory)
	, bUserSuppliedMemory(InUserSuppliedMemory != nullptr)
	, bSizeRequest(bInSizeRequest)
{
	if (InCallback != nullptr)
	{
		Callback = *InCallback;
	}
}

IAsyncReadRequest::~IAsyncReadRequest()
{
	// Never waited for: out of the queue before it goes, or read to its end if the IO thread has it (UE requires a wait
	// first; Leon keeps a forgotten one safe).
	FAsyncIOSystem& System = FAsyncIOSystem::Get();
	if (Status == EStatus::Queued && !System.Unqueue(this))
	{
		(void)System.WaitFor(this);
	}
	else if (Status == EStatus::Reading)
	{
		(void)System.WaitFor(this);
	}
	System.Forget(this);
	if (!bUserSuppliedMemory && Memory != nullptr)
	{
		FMemory::Free(Memory);
	}
}

bool IAsyncReadRequest::PollCompletion()
{
	if (Status != EStatus::Done && Status != EStatus::Failed)
	{
		return false;
	}
	if (!bCallbackCalled)
	{
		bCallbackCalled = true;
		if (Callback)
		{
			Callback(bCanceled, this);
		}
	}
	return true;
}

bool IAsyncReadRequest::WaitCompletion(float TimeLimitSeconds)
{
	return FAsyncIOSystem::Get().WaitFor(this, TimeLimitSeconds) && PollCompletion();
}

void IAsyncReadRequest::Cancel()
{
	if (bCanceled || Status == EStatus::Done || Status == EStatus::Failed)
	{
		return;
	}
	bCanceled = true;
	// A queued one ends now; the IO thread stops one it reads at its next chunk.
	if (FAsyncIOSystem::Get().Unqueue(this))
	{
		Status = EStatus::Done;
	}
}

int64 IAsyncReadRequest::GetSizeResults() const
{
	return bSizeRequest && Status == EStatus::Done && !bCanceled ? Size : -1;
}

uint8* IAsyncReadRequest::GetReadResults()
{
	if (bSizeRequest || Status != EStatus::Done || bCanceled)
	{
		return nullptr;
	}
	uint8* Result = Memory;
	if (!bUserSuppliedMemory)
	{
		// The caller's now (UE).
		Memory = nullptr;
	}
	return Result;
}

// FGenericAsyncReadFileHandle

namespace
{
	/** A request of FGenericAsyncReadFileHandle (UE: FGenericReadRequest). */
	class FGenericAsyncReadRequest final : public IAsyncReadRequest
	{
	public:
		FGenericAsyncReadRequest(FAsyncFileCallBack* InCallback, bool bInSizeRequest, uint8* InUserSuppliedMemory)
			: IAsyncReadRequest(InCallback, bInSizeRequest, InUserSuppliedMemory)
		{
		}

		void SetSize(int64 InSize)
		{
			Size = InSize;
			Status = InSize >= 0 ? EStatus::Done : EStatus::Failed;
		}

		void SetRead(const TSharedPtr<IFileHandle>& InFile, int64 InOffset, int64 InBytesToRead,
			EAsyncIOPriorityAndFlags InPriority, bool bInValid)
		{
			File = InFile;
			Offset = InOffset;
			BytesToRead = InBytesToRead;
			Priority = EAsyncIOPriorityAndFlags(InPriority & AIOP_PRIORITY_MASK);
			if (!bInValid)
			{
				Status = EStatus::Failed;
				return;
			}
			if (Memory == nullptr && BytesToRead > 0)
			{
				// Allocated here, on the game thread: the IO thread never allocates.
				LLM_SCOPE(ELLMTag::LoadMapMisc);
				Memory = static_cast<uint8*>(FMemory::Malloc(SIZE_T(BytesToRead), 16));
			}
			if (BytesToRead == 0)
			{
				Status = EStatus::Done;
			}
		}
	};
} // namespace

FGenericAsyncReadFileHandle::FGenericAsyncReadFileHandle(
	TSharedPtr<IFileHandle> InFile, int64 InBaseOffset, int64 InSize)
	: File(MoveTemp(InFile))
	, BaseOffset(InBaseOffset)
	, FileSize(File ? InSize : -1)
{
}

IAsyncReadRequest* FGenericAsyncReadFileHandle::SizeRequest(FAsyncFileCallBack* CompleteCallback)
{
	FGenericAsyncReadRequest* Request = new FGenericAsyncReadRequest(CompleteCallback, true, nullptr);
	// Known since the handle opened: done at once, its callback at the next tick.
	Request->SetSize(FileSize);
	FAsyncIOSystem::Get().Queue(Request);
	return Request;
}

IAsyncReadRequest* FGenericAsyncReadFileHandle::ReadRequest(int64 Offset, int64 BytesToRead,
	EAsyncIOPriorityAndFlags PriorityAndFlags, FAsyncFileCallBack* CompleteCallback, uint8* UserSuppliedMemory)
{
	FGenericAsyncReadRequest* Request = new FGenericAsyncReadRequest(CompleteCallback, false, UserSuppliedMemory);
	const bool bValid = File && Offset >= 0 && BytesToRead >= 0 && Offset + BytesToRead <= FileSize;
	Request->SetRead(File, BaseOffset + Offset, BytesToRead, PriorityAndFlags, bValid);
	FAsyncIOSystem::Get().Queue(Request);
	return Request;
}

// FAsyncIOSystem

FAsyncIOSystem& FAsyncIOSystem::Get()
{
	static FAsyncIOSystem System;
	return System;
}

FAsyncIOSystem::FAsyncIOSystem()
{
	Pending.Reserve(64);
	Live.Reserve(64);
	{
		// The coalesced reads' buffer, made here: the IO thread never allocates.
		LLM_SCOPE(ELLMTag::EngineMisc);
		CoalesceBuffer = static_cast<uint8*>(FMemory::Malloc(SIZE_T(CoalesceBytes), 64));
	}
	PlatformWorker = CreatePlatformAsyncIOWorker(*this);
	Worker = PlatformWorker;
}

FAsyncIOSystem::~FAsyncIOSystem()
{
	// The EE's thread stays asleep in its semaphore until the program ends; nothing to join.
	delete PlatformWorker;
	FMemory::Free(CoalesceBuffer);
}

void FAsyncIOSystem::Lock()
{
	if (Worker != nullptr)
	{
		Worker->Lock();
	}
}

void FAsyncIOSystem::Unlock()
{
	if (Worker != nullptr)
	{
		Worker->Unlock();
	}
}

void FAsyncIOSystem::SetWorkerForTests(IAsyncIOWorker* InWorker)
{
	checkf(GetNumPendingRequests() == 0, "The IO worker changed with requests pending");
	Worker = InWorker;
	// The sweep starts again (a test's order does not depend on the reads before it).
	LastFile = nullptr;
	LastEnd = 0;
}

void FAsyncIOSystem::Queue(IAsyncReadRequest* Request)
{
	Request->Serial = NextSerial++;
	Live.Add(Request);
	if (Request->Status != IAsyncReadRequest::EStatus::Queued)
	{
		return;
	}
	Lock();
	Pending.Add(Request);
	Unlock();
	if (Worker != nullptr)
	{
		Worker->WakeWorker();
	}
}

bool FAsyncIOSystem::Unqueue(IAsyncReadRequest* Request)
{
	Lock();
	const int32 Index = Pending.Find(Request);
	if (Index != INDEX_NONE)
	{
		Pending.RemoveAt(Index, 1, false);
	}
	Unlock();
	return Index != INDEX_NONE;
}

void FAsyncIOSystem::RaisePriority(IAsyncReadRequest* Request, EAsyncIOPriorityAndFlags Priority)
{
	Lock();
	if (Request->Status == IAsyncReadRequest::EStatus::Queued && Request->Priority < Priority)
	{
		Request->Priority = Priority;
	}
	Unlock();
}

void FAsyncIOSystem::Forget(IAsyncReadRequest* Request)
{
	const int32 Index = Live.Find(Request);
	if (Index != INDEX_NONE)
	{
		Live.RemoveAt(Index, 1, false);
	}
}

IAsyncReadRequest* FAsyncIOSystem::TakeNext()
{
	// Within the highest priority, the read nearest ahead of the last one in the same file, else the first of a file
	// (UE: FAsyncIOSystemBase takes the request closest to the last offset): the disc sweeps forward through the
	// requests instead of seeking back and forth in issue order.
	const auto SeekCost = [this](const IAsyncReadRequest* Request)
	{
		const bool bAhead = Request->File.Get() == LastFile && Request->Offset >= LastEnd;
		return bAhead ? uint64(Request->Offset - LastEnd) : (uint64(1) << 62) + uint64(Request->Offset);
	};
	int32 Best = INDEX_NONE;
	uint64 BestCost = 0;
	for (int32 Index = 0; Index < Pending.Num(); ++Index)
	{
		const IAsyncReadRequest* Candidate = Pending[Index];
		const uint64 Cost = SeekCost(Candidate);
		if (Best == INDEX_NONE || Candidate->Priority > Pending[Best]->Priority ||
			(Candidate->Priority == Pending[Best]->Priority &&
				(Cost < BestCost || (Cost == BestCost && Candidate->Serial < Pending[Best]->Serial))))
		{
			Best = Index;
			BestCost = Cost;
		}
	}
	if (Best == INDEX_NONE)
	{
		return nullptr;
	}
	IAsyncReadRequest* Request = Pending[Best];
	// No shrinking: the IO thread must not allocate or free.
	Pending.RemoveAt(Best, 1, false);
	Request->Status = IAsyncReadRequest::EStatus::Reading;
	LastFile = Request->File.Get();
	LastEnd = Request->Offset + Request->BytesToRead;
	return Request;
}

int32 FAsyncIOSystem::TakeBatch(IAsyncReadRequest** OutBatch)
{
	IAsyncReadRequest* First = TakeNext();
	if (First == nullptr)
	{
		return 0;
	}
	OutBatch[0] = First;
	int32 Count = 1;
	// The queued reads just after it in the same file join it, while the whole span fits the buffer and each gap is
	// short: one read of the disc instead of several (UE: the pak precacher's merged requests).
	const int64 Start = First->Offset;
	int64 End = First->Offset + First->BytesToRead;
	while (Count < MaxBatch && End - Start <= CoalesceBytes)
	{
		int32 Next = INDEX_NONE;
		for (int32 Index = 0; Index < Pending.Num(); ++Index)
		{
			const IAsyncReadRequest* Candidate = Pending[Index];
			if (Candidate->File != First->File || Candidate->bCanceled || Candidate->BytesToRead <= 0 ||
				Candidate->Offset < End || Candidate->Offset - End > MaxCoalesceGap ||
				Candidate->Offset + Candidate->BytesToRead - Start > CoalesceBytes)
			{
				continue;
			}
			if (Next == INDEX_NONE || Candidate->Offset < Pending[Next]->Offset)
			{
				Next = Index;
			}
		}
		if (Next == INDEX_NONE)
		{
			break;
		}
		IAsyncReadRequest* Request = Pending[Next];
		Pending.RemoveAt(Next, 1, false);
		Request->Status = IAsyncReadRequest::EStatus::Reading;
		End = Request->Offset + Request->BytesToRead;
		OutBatch[Count++] = Request;
	}
	LastEnd = End;
	return Count;
}

void FAsyncIOSystem::Execute(IAsyncReadRequest** Batch, int32 Count)
{
	const uint64 StartCycles = FPlatformTime::Cycles64();
	if (Count > 1)
	{
		// One read of the span into the buffer, then each request's part.
		IAsyncReadRequest& First = *Batch[0];
		const IAsyncReadRequest& Last = *Batch[Count - 1];
		const int64 Span = Last.Offset + Last.BytesToRead - First.Offset;
		const bool bOk = First.File->Seek(First.Offset) && First.File->Read(CoalesceBuffer, Span);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			IAsyncReadRequest& Request = *Batch[Index];
			if (bOk && !Request.bCanceled)
			{
				FMemory::Memcpy(
					Request.Memory, CoalesceBuffer + (Request.Offset - First.Offset), SIZE_T(Request.BytesToRead));
			}
			Request.Status =
				bOk || Request.bCanceled ? IAsyncReadRequest::EStatus::Done : IAsyncReadRequest::EStatus::Failed;
		}
		BytesRead = BytesRead + uint64(bOk ? Span : 0);
		NumReads = NumReads + uint32(Count);
		NumChunks = NumChunks + 1;
		ReadCycles = ReadCycles + (FPlatformTime::Cycles64() - StartCycles);
		return;
	}
	IAsyncReadRequest& Request = *Batch[0];
	int64 Done = 0;
	uint32 Chunks = 0;
	bool bOk = Request.File.IsValid();
	while (bOk && Done < Request.BytesToRead && !Request.bCanceled)
	{
		const int64 Chunk = FMath::Min<int64>(ChunkBytes, Request.BytesToRead - Done);
		bOk = Request.File->Seek(Request.Offset + Done) && Request.File->Read(Request.Memory + Done, Chunk);
		Done += bOk ? Chunk : 0;
		++Chunks;
	}
	BytesRead = BytesRead + uint64(Done);
	NumReads = NumReads + 1;
	NumChunks = NumChunks + Chunks;
	ReadCycles = ReadCycles + (FPlatformTime::Cycles64() - StartCycles);
	Request.Status = bOk || Request.bCanceled ? IAsyncReadRequest::EStatus::Done : IAsyncReadRequest::EStatus::Failed;
}

void FAsyncIOSystem::ServiceRequests()
{
	IAsyncReadRequest* Batch[MaxBatch];
	for (;;)
	{
		Lock();
		const int32 Count = TakeBatch(Batch);
		Unlock();
		if (Count == 0)
		{
			return;
		}
		Execute(Batch, Count);
		if (Worker != nullptr)
		{
			for (int32 Index = 0; Index < Count; ++Index)
			{
				Worker->NotifyCompletion();
			}
		}
	}
}

void FAsyncIOSystem::WaitForAnyCompletion()
{
	if (Worker != nullptr)
	{
		Worker->WaitForAnyCompletion();
		return;
	}
	IAsyncReadRequest* Batch[MaxBatch];
	if (const int32 Count = TakeBatch(Batch))
	{
		Execute(Batch, Count);
	}
}

bool FAsyncIOSystem::WaitFor(IAsyncReadRequest* Request, float TimeLimitSeconds)
{
	const uint64 StartCycles = FPlatformTime::Cycles64();
	const uint64 LimitMicroseconds = TimeLimitSeconds > 0.0f ? uint64(TimeLimitSeconds * 1000000.0f) : 0;
	while (
		Request->Status == IAsyncReadRequest::EStatus::Queued || Request->Status == IAsyncReadRequest::EStatus::Reading)
	{
		if (LimitMicroseconds > 0)
		{
			if (FPlatformTime::CyclesToMicroseconds(FPlatformTime::Cycles64() - StartCycles) >= LimitMicroseconds)
			{
				return false;
			}
			if (Worker != nullptr)
			{
				// No timed wait on a kernel semaphore: look again soon.
				FPlatformProcess::Sleep(0.001f);
				continue;
			}
		}
		WaitForAnyCompletion();
	}
	return true;
}

void FAsyncIOSystem::DispatchCallbacks()
{
	// A callback may issue requests or delete others: the ended ones first, in issue order, then each that is still
	// live (the scratch array keeps its capacity: no allocation a frame).
	DispatchScratch.Reset();
	for (IAsyncReadRequest* Request : Live)
	{
		if (!Request->bCallbackCalled &&
			(Request->Status == IAsyncReadRequest::EStatus::Done ||
				Request->Status == IAsyncReadRequest::EStatus::Failed))
		{
			DispatchScratch.Add(Request);
		}
	}
	for (IAsyncReadRequest* Request : DispatchScratch)
	{
		if (Live.Contains(Request))
		{
			(void)Request->PollCompletion();
		}
	}
}

void FAsyncIOSystem::Tick()
{
	if (Worker == nullptr)
	{
		IAsyncReadRequest* Batch[MaxBatch];
		while (const int32 Count = TakeBatch(Batch))
		{
			Execute(Batch, Count);
		}
	}
	DispatchCallbacks();
}

int32 FAsyncIOSystem::GetNumPendingRequests() const
{
	int32 Count = 0;
	for (const IAsyncReadRequest* Request : Live)
	{
		Count += Request->Status == IAsyncReadRequest::EStatus::Queued ||
				Request->Status == IAsyncReadRequest::EStatus::Reading
			? 1
			: 0;
	}
	return Count;
}
