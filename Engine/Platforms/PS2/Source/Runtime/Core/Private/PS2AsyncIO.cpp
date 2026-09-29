#include "Async/AsyncIOSystem.h"
#include "HAL/PlatformMisc.h"

#include <kernel.h>

// The EE's IO thread (Docs/PLANS/ps2-shipping.md N24): FAsyncIOSystem's reads run here, one priority above the game
// thread. A read is a SIF RPC to the IOP (fio through libcglue, each call under their own locks), and the thread sleeps
// in its semaphore while the IOP and the drive work, so the game thread runs meanwhile; when the RPC ends the thread
// wakes and takes the next chunk at once. The IO thread only reads through its own file descriptors into memory the
// request already has: no GMalloc, no log.

namespace
{
	/** The IO thread's stack: Seek and Read through newlib and fio, no deep calls. */
	constexpr int32 IOThreadStackSize = 16 * 1024;
	u8 GIOThreadStack[IOThreadStackSize] __attribute__((aligned(16)));

	class FPS2AsyncIOWorker final : public IAsyncIOWorker
	{
	public:
		explicit FPS2AsyncIOWorker(FAsyncIOSystem& InSystem)
			: System(InSystem)
		{
			ee_sema_t Mutex{};
			Mutex.init_count = 1;
			Mutex.max_count = 1;
			LockSema = CreateSema(&Mutex);

			ee_sema_t Work{};
			Work.init_count = 0;
			Work.max_count = 0x7fff;
			WorkSema = CreateSema(&Work);

			ee_sema_t Done{};
			Done.init_count = 0;
			Done.max_count = 0x7fff;
			CompletionSema = CreateSema(&Done);

			// One priority above the game thread (a lower number), so a finished IOP call resumes the reads at once.
			ee_thread_status_t Self{};
			(void)ReferThreadStatus(GetThreadId(), &Self);
			int32 GamePriority = Self.current_priority;
			if (GamePriority < 2)
			{
				GamePriority = 2;
				(void)ChangeThreadPriority(GetThreadId(), GamePriority);
			}

			ee_thread_t Thread{};
			Thread.func = reinterpret_cast<void*>(&ThreadEntry);
			Thread.stack = GIOThreadStack;
			Thread.stack_size = IOThreadStackSize;
			Thread.gp_reg = &_gp;
			Thread.initial_priority = GamePriority - 1;
			ThreadId = CreateThread(&Thread);
			if (LockSema < 0 || WorkSema < 0 || CompletionSema < 0 || ThreadId < 0 || StartThread(ThreadId, this) < 0)
			{
				FPlatformMisc::LowLevelOutputDebugString("PS2AsyncIO: the IO thread could not start\n");
				bStarted = false;
				return;
			}
			bStarted = true;
		}

		[[nodiscard]] bool IsStarted() const
		{
			return bStarted;
		}

		virtual void Lock() override
		{
			WaitSema(LockSema);
		}

		virtual void Unlock() override
		{
			SignalSema(LockSema);
		}

		virtual void WakeWorker() override
		{
			SignalSema(WorkSema);
		}

		virtual void WaitForAnyCompletion() override
		{
			WaitSema(CompletionSema);
		}

		virtual void NotifyCompletion() override
		{
			SignalSema(CompletionSema);
		}

	private:
		static void ThreadEntry(void* Argument)
		{
			FPS2AsyncIOWorker& Worker = *static_cast<FPS2AsyncIOWorker*>(Argument);
			for (;;)
			{
				WaitSema(Worker.WorkSema);
				Worker.System.ServiceRequests();
			}
		}

		FAsyncIOSystem& System;
		int32 LockSema = -1;
		int32 WorkSema = -1;
		int32 CompletionSema = -1;
		int32 ThreadId = -1;
		bool bStarted = false;
	};
} // namespace

IAsyncIOWorker* CreatePlatformAsyncIOWorker(FAsyncIOSystem& System)
{
	FPS2AsyncIOWorker* Worker = new FPS2AsyncIOWorker(System);
	if (!Worker->IsStarted())
	{
		// The game thread reads, as on Win64.
		delete Worker;
		return nullptr;
	}
	FPlatformMisc::LowLevelOutputDebugString("PS2AsyncIO: the IO thread reads the disc\n");
	return Worker;
}
