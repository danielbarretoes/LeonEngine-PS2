#include "Async/AsyncFileHandle.h"
#include "Async/AsyncIOSystem.h"
#include "CoreMinimal.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/**
	 * A file in memory that notes the offset of each read it serves, in a fixed table (the PS2's IO thread may not
	 * allocate).
	 */
	class FRecordingFile final : public IFileHandle
	{
	public:
		static constexpr int32 MaxReads = 64;

		explicit FRecordingFile(int32 Size)
		{
			Bytes.SetNumUninitialized(Size);
			for (int32 Index = 0; Index < Size; ++Index)
			{
				Bytes[Index] = uint8((Index * 7 + Index / 251) & 0xff);
			}
		}

		virtual int64 Tell() override
		{
			return Position;
		}
		virtual bool Seek(int64 NewPosition) override
		{
			Position = NewPosition;
			return NewPosition >= 0 && NewPosition <= Bytes.Num();
		}
		virtual bool SeekFromEnd(int64 NewPositionRelativeToEnd) override
		{
			return Seek(Bytes.Num() + NewPositionRelativeToEnd);
		}
		virtual bool Read(uint8* Destination, int64 BytesToRead) override
		{
			if (Position < 0 || Position + BytesToRead > Bytes.Num())
			{
				return false;
			}
			if (NumReads < MaxReads)
			{
				ReadOffsets[NumReads++] = Position;
			}
			FMemory::Memcpy(Destination, Bytes.GetData() + Position, SIZE_T(BytesToRead));
			Position += BytesToRead;
			return true;
		}
		virtual bool Write(const uint8*, int64) override
		{
			return false;
		}
		virtual bool Flush(const bool) override
		{
			return true;
		}
		virtual bool Truncate(int64) override
		{
			return false;
		}

		TArray<uint8> Bytes;
		int64 Position = 0;
		int64 ReadOffsets[MaxReads] = {};
		volatile int32 NumReads = 0;
	};

	/** Reads the queue on the game thread (the platform's worker put aside) while it lives: the tests set the order. */
	class FGameThreadReadsScope
	{
	public:
		FGameThreadReadsScope()
		{
			FAsyncIOSystem::Get().SetWorkerForTests(nullptr);
		}
		~FGameThreadReadsScope()
		{
			// Back to the platform's worker (the IO thread on the PS2).
			FAsyncIOSystem::Get().RestorePlatformWorker();
		}
	};
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAsyncIOOrderTest, "System.Core.AsyncIO.Order",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FAsyncIOOrderTest::RunTest(const FString& Parameters)
{
	// The queue serves the highest priority first; within one, the read nearest ahead of the last (the disc sweeps
	// forward, UE's closest offset), then the first of the file; and nothing before it is asked to.
	FGameThreadReadsScope Scope;
	// The reads are further apart than MaxCoalesceGap: each is a read of its own.
	constexpr int64 Apart = FAsyncIOSystem::MaxCoalesceGap * 2;
	TSharedPtr<FRecordingFile> File = MakeShared<FRecordingFile>(int32(Apart * 4));
	FGenericAsyncReadFileHandle Handle(File, 0, File->Bytes.Num());
	IAsyncReadRequest* Low = Handle.ReadRequest(0, 16, AIOP_Low);
	IAsyncReadRequest* Normal1 = Handle.ReadRequest(Apart, 16, AIOP_Normal);
	IAsyncReadRequest* High = Handle.ReadRequest(Apart * 2, 16, AIOP_High);
	IAsyncReadRequest* Normal2 = Handle.ReadRequest(Apart * 3, 16, AIOP_Normal);
	TestEqual("Nothing read before the tick", int32(File->NumReads), 0);
	TestEqual("Four pending", FAsyncIOSystem::Get().GetNumPendingRequests(), 4);
	FAsyncIOSystem::Get().Tick();
	TestEqual("Four reads", int32(File->NumReads), 4);
	TestEqual("High first", File->ReadOffsets[0], Apart * 2);
	TestEqual("Then the normal one ahead of it", File->ReadOffsets[1], Apart * 3);
	TestEqual("Then the one behind (the sweep starts again)", File->ReadOffsets[2], Apart);
	TestEqual("Low last", File->ReadOffsets[3], int64(0));
	TestEqual("None pending", FAsyncIOSystem::Get().GetNumPendingRequests(), 0);
	for (IAsyncReadRequest* Request : {Low, Normal1, High, Normal2})
	{
		TestTrue("Complete", Request->PollCompletion());
		FMemory::Free(Request->GetReadResults());
		delete Request;
	}
	TestEqual("None live", FAsyncIOSystem::Get().GetNumLiveRequests(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAsyncIOCoalesceTest, "System.Core.AsyncIO.Coalesce",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FAsyncIOCoalesceTest::RunTest(const FString& Parameters)
{
	// Queued reads close together in the file are one read (gaps up to MaxCoalesceGap, CoalesceBytes in all); a far
	// one, or one past the span, is a read of its own; each request gets its own bytes (ps2-shipping N24b).
	FGameThreadReadsScope Scope;
	constexpr int32 Size = int32(FAsyncIOSystem::CoalesceBytes) * 3;
	TSharedPtr<FRecordingFile> File = MakeShared<FRecordingFile>(Size);
	FGenericAsyncReadFileHandle Handle(File, 0, Size);
	const int64 Gap = FAsyncIOSystem::MaxCoalesceGap;
	struct FRead
	{
		int64 Offset;
		int64 Bytes;
	};
	const FRead Reads[] = {
		{1000, 500}, // the first
		{3000, 700}, // after a small gap: joins
		{3700, 100}, // right after: joins
		{3800 + Gap + 1, 50}, // a gap too long: a read of its own
		{int64(Size) - 4000, 400}, // far: its own
	};
	TArray<IAsyncReadRequest*> Requests;
	for (const FRead& Read : Reads)
	{
		Requests.Add(Handle.ReadRequest(Read.Offset, Read.Bytes, AIOP_Normal));
	}
	FAsyncIOSystem::Get().Tick();
	TestEqual("Three reads of the file", int32(File->NumReads), 3);
	TestEqual("The first spans the three close ones", File->ReadOffsets[0], int64(1000));
	TestEqual("Then the one past the gap", File->ReadOffsets[1], Reads[3].Offset);
	TestEqual("Then the far one", File->ReadOffsets[2], Reads[4].Offset);
	for (int32 Index = 0; Index < Requests.Num(); ++Index)
	{
		IAsyncReadRequest* Request = Requests[Index];
		TestTrue("Complete", Request->PollCompletion());
		uint8* Bytes = Request->GetReadResults();
		TestTrue("Its bytes",
			Bytes != nullptr &&
				FMemory::Memcmp(Bytes, File->Bytes.GetData() + Reads[Index].Offset, SIZE_T(Reads[Index].Bytes)) == 0);
		FMemory::Free(Bytes);
		delete Request;
	}
	TestEqual("None live", FAsyncIOSystem::Get().GetNumLiveRequests(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAsyncIOCompletionTest, "System.Core.AsyncIO.Completion",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FAsyncIOCompletionTest::RunTest(const FString& Parameters)
{
	// On the platform's IO thread (the PS2's EE thread; Win64 reads at the wait): a read of several chunks, into memory
	// the request allocates and into the caller's, a size request, and the callbacks on the game thread.
	constexpr int32 Size = int32(FAsyncIOSystem::ChunkBytes) * 3 + 123;
	TSharedPtr<FRecordingFile> File = MakeShared<FRecordingFile>(Size);
	FGenericAsyncReadFileHandle Handle(File, 0, Size);

	int32 Callbacks = 0;
	bool bCancelledSeen = false;
	FAsyncFileCallBack Callback = [&Callbacks, &bCancelledSeen](bool bWasCancelled, IAsyncReadRequest*)
	{
		++Callbacks;
		bCancelledSeen |= bWasCancelled;
	};
	IAsyncReadRequest* Whole = Handle.ReadRequest(0, Size, AIOP_Normal, &Callback);
	TArray<uint8> Mine;
	Mine.SetNumZeroed(100);
	IAsyncReadRequest* IntoMine = Handle.ReadRequest(1000, 100, AIOP_Normal, &Callback, Mine.GetData());
	IAsyncReadRequest* SizeRequest = Handle.SizeRequest(&Callback);
	TestTrue("The whole file", Whole->WaitCompletion());
	TestTrue("Into my memory", IntoMine->WaitCompletion());
	FAsyncIOSystem::Get().Tick();
	TestTrue("The size", SizeRequest->PollCompletion());
	TestEqual("Three callbacks", Callbacks, 3);
	TestFalse("None cancelled", bCancelledSeen);
	TestEqual("The size's result", SizeRequest->GetSizeResults(), int64(Size));

	uint8* Bytes = Whole->GetReadResults();
	TestTrue("Every byte", Bytes != nullptr && FMemory::Memcmp(Bytes, File->Bytes.GetData(), Size) == 0);
	TestNull("The memory is mine now", Whole->GetReadResults());
	FMemory::Free(Bytes);
	TestTrue("My memory holds the bytes",
		IntoMine->GetReadResults() == Mine.GetData() &&
			FMemory::Memcmp(Mine.GetData(), File->Bytes.GetData() + 1000, 100) == 0);

	// Past the end, or without a file: failed at once, the callback still runs.
	IAsyncReadRequest* PastEnd = Handle.ReadRequest(Size - 10, 20);
	FGenericAsyncReadFileHandle NoFile(nullptr, 0, -1);
	IAsyncReadRequest* Missing = NoFile.ReadRequest(0, 10);
	IAsyncReadRequest* MissingSize = NoFile.SizeRequest();
	TestTrue("Past the end ends", PastEnd->WaitCompletion());
	TestTrue("Failed", PastEnd->HasFailed() && PastEnd->GetReadResults() == nullptr);
	TestTrue("No file ends", Missing->WaitCompletion() && Missing->HasFailed());
	TestTrue("No size", MissingSize->WaitCompletion() && MissingSize->GetSizeResults() == -1);
	for (IAsyncReadRequest* Request : {Whole, IntoMine, SizeRequest, PastEnd, Missing, MissingSize})
	{
		delete Request;
	}
	TestEqual("None live", FAsyncIOSystem::Get().GetNumLiveRequests(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAsyncIOCancelTest, "System.Core.AsyncIO.Cancel",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FAsyncIOCancelTest::RunTest(const FString& Parameters)
{
	// A cancelled request never reads, its callback says so and it has no bytes; the others go on.
	FGameThreadReadsScope Scope;
	TSharedPtr<FRecordingFile> File = MakeShared<FRecordingFile>(1024);
	FGenericAsyncReadFileHandle Handle(File, 0, File->Bytes.Num());
	bool bCancelled = false;
	FAsyncFileCallBack Callback = [&bCancelled](bool bWasCancelled, IAsyncReadRequest*) { bCancelled = bWasCancelled; };
	IAsyncReadRequest* Kept = Handle.ReadRequest(0, 64);
	IAsyncReadRequest* Dropped = Handle.ReadRequest(512, 64, AIOP_High, &Callback);
	Dropped->Cancel();
	TestTrue("Over at once", Dropped->PollCompletion());
	TestTrue("The callback was told", bCancelled);
	TestTrue("Cancelled", Dropped->WasCancelled());
	TestNull("No bytes", Dropped->GetReadResults());
	FAsyncIOSystem::Get().Tick();
	TestEqual("Only the kept one read", int32(File->NumReads), 1);
	TestEqual("At its offset", File->ReadOffsets[0], int64(0));
	TestTrue("The kept one ended", Kept->PollCompletion() && !Kept->WasCancelled());
	FMemory::Free(Kept->GetReadResults());

	// A request deleted without a wait leaves the queue (Leon keeps a forgotten one safe).
	IAsyncReadRequest* Forgotten = Handle.ReadRequest(128, 64);
	delete Forgotten;
	FAsyncIOSystem::Get().Tick();
	TestEqual("The forgotten one never read", int32(File->NumReads), 1);
	delete Kept;
	delete Dropped;
	TestEqual("None live", FAsyncIOSystem::Get().GetNumLiveRequests(), 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
