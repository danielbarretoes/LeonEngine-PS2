#pragma once

// Asynchronous file reads (UE: Runtime/Core/Public/Async/AsyncFileHandle.h; Docs/PLANS/ps2-shipping.md N24).

#include "CoreTypes.h"
#include "Templates/Function.h"
#include "Templates/SharedPointer.h"

class IAsyncReadRequest;
class IFileHandle;

/** A read's priority (UE: EAsyncIOPriorityAndFlags): the queue serves the highest first, in order within one. */
enum EAsyncIOPriorityAndFlags : uint32
{
	AIOP_MIN = 0,
	AIOP_Low = AIOP_MIN,
	AIOP_BelowNormal,
	AIOP_Normal,
	AIOP_High,
	AIOP_CriticalPath,
	AIOP_MAX = AIOP_CriticalPath,
	AIOP_NUM,
	AIOP_PRIORITY_MASK = 0x000000ff,
};

/**
 * Called once a request is over (UE: FAsyncFileCallBack): bWasCancelled, and the request, whose results are ready
 * unless it was cancelled or failed. Leon calls it on the game thread (FAsyncIOSystem::Tick, or the request's
 * PollCompletion / WaitCompletion), never on the IO thread: the EE's allocator and log are not thread safe.
 */
using FAsyncFileCallBack = TFunction<void(bool bWasCancelled, IAsyncReadRequest* Request)>;

/**
 * One asynchronous size or read request (UE: IAsyncReadRequest). The caller owns it: it deletes it once it is complete
 * (PollCompletion true, or after WaitCompletion), cancelled or not. The file handle that made it must outlive it.
 */
class CORE_API IAsyncReadRequest
{
public:
	virtual ~IAsyncReadRequest();

	IAsyncReadRequest(const IAsyncReadRequest&) = delete;
	IAsyncReadRequest& operator=(const IAsyncReadRequest&) = delete;

	/** True once the request is over and its callback ran (it runs here when it had not yet) (UE). */
	bool PollCompletion();

	/**
	 * Waits for the request to be over and runs its callback; 0 waits as long as it takes, else at most that many
	 * seconds. True when it is over (UE).
	 */
	bool WaitCompletion(float TimeLimitSeconds = 0.0f);

	/**
	 * Cancels the request (UE): a queued one never reads; one being read finishes and its bytes are dropped. The
	 * callback still runs, with bWasCancelled.
	 */
	void Cancel();

	/** A size request's result: the file's size, or -1 when it cannot be read (UE: GetSizeResults). */
	[[nodiscard]] int64 GetSizeResults() const;

	/**
	 * A read request's bytes, or nullptr when it failed or was cancelled (UE: GetReadResults). Memory the request
	 * allocated passes to the caller, who frees it with FMemory::Free; user supplied memory stays the caller's.
	 */
	[[nodiscard]] uint8* GetReadResults();

	/** Whether it was cancelled. */
	[[nodiscard]] bool WasCancelled() const
	{
		return bCanceled;
	}

	/** Whether it failed: the file could not be read (Leon). */
	[[nodiscard]] bool HasFailed() const
	{
		return Status == EStatus::Failed;
	}

	/** The request's priority (Leon: the queue's order). */
	[[nodiscard]] EAsyncIOPriorityAndFlags GetPriority() const
	{
		return Priority;
	}

protected:
	/** Where a request is. The IO thread moves it from Queued or Reading to Done / Failed; nothing else touches it. */
	enum class EStatus : int32
	{
		Queued,
		Reading,
		Done,
		Failed,
	};

	IAsyncReadRequest(FAsyncFileCallBack* InCallback, bool bInSizeRequest, uint8* InUserSuppliedMemory);

	/** The request's file (a read's), its offset and size in the file, and where the bytes go. */
	TSharedPtr<IFileHandle> File;
	int64 Offset = 0;
	int64 BytesToRead = 0;
	uint8* Memory = nullptr;
	bool bUserSuppliedMemory = false;
	bool bSizeRequest = false;
	int64 Size = -1;
	EAsyncIOPriorityAndFlags Priority = AIOP_Normal;

	/** Written by the IO thread, read by the game thread (a word: the EE writes it at once). */
	volatile EStatus Status = EStatus::Queued;
	volatile bool bCanceled = false;
	bool bCallbackCalled = false;
	/** Order of issue (the queue's order within a priority). */
	uint64 Serial = 0;
	FAsyncFileCallBack Callback;

	friend class FAsyncIOSystem;
	friend class FGenericAsyncReadFileHandle;
};

/**
 * A file opened for asynchronous reads (UE: IAsyncReadFileHandle; IPlatformFile::OpenAsyncRead). Delete it after its
 * requests.
 */
class CORE_API IAsyncReadFileHandle
{
public:
	virtual ~IAsyncReadFileHandle() = default;

	/** Asks for the file's size (UE); never null. */
	virtual IAsyncReadRequest* SizeRequest(FAsyncFileCallBack* CompleteCallback = nullptr) = 0;

	/**
	 * Asks for BytesToRead bytes at Offset (UE): into UserSuppliedMemory when given, else into memory the request
	 * allocates. Never null; a read past the end fails.
	 */
	virtual IAsyncReadRequest* ReadRequest(int64 Offset, int64 BytesToRead,
		EAsyncIOPriorityAndFlags PriorityAndFlags = AIOP_Normal, FAsyncFileCallBack* CompleteCallback = nullptr,
		uint8* UserSuppliedMemory = nullptr) = 0;
};

/**
 * The asynchronous handle over a synchronous one (UE: FGenericBaseRequest's handle): its reads are FAsyncIOSystem's,
 * made with File's Seek and Read at BaseOffset + Offset (a pak entry's offset in the pak) on the IO thread, the only
 * one that uses File once it is here. A null File fails every request (a file that does not exist).
 */
class CORE_API FGenericAsyncReadFileHandle final : public IAsyncReadFileHandle
{
public:
	FGenericAsyncReadFileHandle(TSharedPtr<IFileHandle> InFile, int64 InBaseOffset, int64 InSize);

	virtual IAsyncReadRequest* SizeRequest(FAsyncFileCallBack* CompleteCallback = nullptr) override;
	virtual IAsyncReadRequest* ReadRequest(int64 Offset, int64 BytesToRead,
		EAsyncIOPriorityAndFlags PriorityAndFlags = AIOP_Normal, FAsyncFileCallBack* CompleteCallback = nullptr,
		uint8* UserSuppliedMemory = nullptr) override;

private:
	TSharedPtr<IFileHandle> File;
	int64 BaseOffset = 0;
	int64 FileSize = -1;
};
