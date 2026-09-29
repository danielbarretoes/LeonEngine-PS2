#pragma once

// The queue that serves the asynchronous reads (Leon; UE 4's FAsyncIOSystemBase and its IO thread,
// Docs/PLANS/ps2-shipping.md N24).

#include "Async/AsyncFileHandle.h"
#include "Containers/Array.h"
#include "CoreTypes.h"

class FAsyncIOSystem;

/**
 * The platform's side of the IO system (Leon): a thread that reads while the game runs, the lock around the queue and
 * the waits. The PS2's is an EE thread above the game's priority with kernel semaphores (it sleeps in the IOP's reads,
 * so the game thread runs meanwhile); where the platform makes none (Win64), the game thread reads the queue itself, in
 * its order, at FAsyncIOSystem::Tick and in a wait: a synchronous completion, the same on every run.
 */
class CORE_API IAsyncIOWorker
{
public:
	virtual ~IAsyncIOWorker() = default;

	/** The queue's lock (the game thread and the IO thread both change it). */
	virtual void Lock() = 0;
	virtual void Unlock() = 0;

	/** A request was queued: the thread wakes up. */
	virtual void WakeWorker() = 0;

	/** Sleeps until the thread ends some request (the caller checks whether it is the one it waits for). */
	virtual void WaitForAnyCompletion() = 0;

	/** The thread ended a request (called on the IO thread): wakes WaitForAnyCompletion. */
	virtual void NotifyCompletion() = 0;
};

/**
 * The platform's worker running System's queue (FAsyncIOSystem::ServiceRequests on its thread), or nullptr where the
 * game thread reads (Win64). Defined by each platform.
 */
IAsyncIOWorker* CreatePlatformAsyncIOWorker(FAsyncIOSystem& System);

/**
 * The asynchronous reads' queue (Leon). IAsyncReadFileHandle's requests come here; the IO thread takes the highest
 * priority first, within a priority the read nearest ahead of the last (issue order breaks ties), coalesces the queued
 * reads close to it (CoalesceBytes), and reads a large one in chunks of ChunkBytes (a cancelled request stops
 * between chunks, and a synchronous read on the game thread never waits long behind one). The game thread runs the
 * callbacks of the requests that ended (Tick, or the request's PollCompletion / WaitCompletion).
 *
 * The IO thread allocates nothing and logs nothing (GMalloc and the log are the game thread's): it only reads into
 * memory the request already has.
 */
class CORE_API FAsyncIOSystem
{
public:
	/** A read's chunk. */
	static constexpr int64 ChunkBytes = 64 * 1024;
	/**
	 * Queued reads close together in a file are read at once (ps2-shipping N24b): at most CoalesceBytes from the first
	 * one's offset, gaps of at most MaxCoalesceGap between them, MaxBatch requests. On the PS2 a read of the disc costs
	 * about 20 ms before its bytes (the IOP's calls and the emulated drive), so a gap of 16 KB is cheaper to read than
	 * to skip.
	 */
	static constexpr int64 CoalesceBytes = 128 * 1024;
	static constexpr int64 MaxCoalesceGap = 16 * 1024;
	static constexpr int32 MaxBatch = 16;

	/** The engine's system (created, with its thread, at first use). */
	static FAsyncIOSystem& Get();

	FAsyncIOSystem();
	~FAsyncIOSystem();

	FAsyncIOSystem(const FAsyncIOSystem&) = delete;
	FAsyncIOSystem& operator=(const FAsyncIOSystem&) = delete;

	/** Queues a request (the game thread; a size request is done already and only waits for its callback). */
	void Queue(IAsyncReadRequest* Request);

	/** Takes a queued request out of the queue; false when the IO thread already has it. */
	bool Unqueue(IAsyncReadRequest* Request);

	/**
	 * Raises a queued request to Priority (UE: a flush of a request on its way): the game thread waits for it, so it
	 * goes before the others. Nothing for one being read or over.
	 */
	void RaisePriority(IAsyncReadRequest* Request, EAsyncIOPriorityAndFlags Priority);

	/** Forgets a request that is being deleted (it must be over). */
	void Forget(IAsyncReadRequest* Request);

	/**
	 * Once a frame on the game thread (the engine's ProcessAsyncLoading): without an IO thread reads everything queued;
	 * then runs the callbacks of the requests that ended, in issue order.
	 */
	void Tick();

	/**
	 * Waits until Request is over (without an IO thread, reads the queue up to it); a TimeLimitSeconds above 0 gives up
	 * after that long. True when it is over.
	 */
	bool WaitFor(IAsyncReadRequest* Request, float TimeLimitSeconds = 0.0f);

	/** Waits for the IO thread to end some request, or reads the next one without a thread. */
	void WaitForAnyCompletion();

	/** The IO thread's loop body: reads queued requests until none is left (the platform's worker calls it). */
	void ServiceRequests();

	/** Whether an IO thread reads (the PS2) or the game thread does (Win64). */
	[[nodiscard]] bool HasWorkerThread() const
	{
		return Worker != nullptr;
	}

	/** Requests not over yet (queued or being read). */
	[[nodiscard]] int32 GetNumPendingRequests() const;

	/** Requests issued and not deleted (for the tests: none leaks). */
	[[nodiscard]] int32 GetNumLiveRequests() const
	{
		return Live.Num();
	}

	/** Bytes read since the start (for the logs). */
	[[nodiscard]] uint64 GetBytesRead() const
	{
		return BytesRead;
	}

	/** The reads' cost since the start (for the logs): requests read, their chunks, and the cycles spent reading. */
	[[nodiscard]] uint32 GetNumReads() const
	{
		return NumReads;
	}
	[[nodiscard]] uint32 GetNumChunks() const
	{
		return NumChunks;
	}
	[[nodiscard]] uint64 GetReadCycles() const
	{
		return ReadCycles;
	}

	/**
	 * Replaces the platform's worker (the tests read the queue on the game thread to check its order); null: none. Only
	 * with no request pending.
	 */
	void SetWorkerForTests(IAsyncIOWorker* InWorker);

	/** Gives the queue back to the platform's worker (after SetWorkerForTests). */
	void RestorePlatformWorker()
	{
		SetWorkerForTests(PlatformWorker);
	}

private:
	/** Takes the next request to read (the lock held); null when none is queued. */
	IAsyncReadRequest* TakeNext();

	/**
	 * Takes the next request and the queued ones that coalesce with it, in offset order, into OutBatch (MaxBatch
	 * entries; the lock held). The number taken, 0 when none is queued.
	 */
	int32 TakeBatch(IAsyncReadRequest** OutBatch);

	/** Reads a batch (any thread; no allocation): one request in chunks, several through CoalesceBuffer. */
	void Execute(IAsyncReadRequest** Batch, int32 Count);

	/** Runs the callbacks of the requests that ended (the game thread). */
	void DispatchCallbacks();

	void Lock();
	void Unlock();

	IAsyncIOWorker* Worker = nullptr;
	/** The worker CreatePlatformAsyncIOWorker made (Worker may be the tests' instead). */
	IAsyncIOWorker* PlatformWorker = nullptr;
	/** Queued requests; the IO thread only removes from it (no allocation), the game thread adds. */
	TArray<IAsyncReadRequest*> Pending;
	/** Every request issued and not deleted, in issue order (the game thread's). */
	TArray<IAsyncReadRequest*> Live;
	/** The requests DispatchCallbacks runs. */
	TArray<IAsyncReadRequest*> DispatchScratch;
	/** A coalesced read's bytes (CoalesceBytes), before each request's part is copied out. */
	uint8* CoalesceBuffer = nullptr;
	uint64 NextSerial = 1;
	/** Where the last read ended (TakeNext's sweep); the IO thread's, under the lock. */
	const IFileHandle* LastFile = nullptr;
	int64 LastEnd = 0;
	volatile uint64 BytesRead = 0;
	volatile uint64 ReadCycles = 0;
	volatile uint32 NumReads = 0;
	volatile uint32 NumChunks = 0;
};
