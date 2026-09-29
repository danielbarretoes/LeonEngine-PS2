#include "TimerManager.h"

uint64 FTimerManager::SecondsToTimeUnits(float Seconds)
{
	if (!(Seconds > 0.0f))
	{
		return 0;
	}
	return static_cast<uint64>((Seconds * static_cast<float>(TimeUnitsPerSecond)) + 0.5f);
}

float FTimerManager::TimeUnitsToSeconds(uint64 Units)
{
	return static_cast<float>(Units) / static_cast<float>(TimeUnitsPerSecond);
}

void FTimerManager::SetTimer(
	FTimerHandle& InOutHandle, const FTimerDelegate& InDelegate, float InRate, bool InbLoop, float InFirstDelay)
{
	InternalSetTimer(InOutHandle, FTimerDelegate(InDelegate), InRate, InbLoop, InFirstDelay, true);
}

void FTimerManager::SetTimer(FTimerHandle& InOutHandle, float InRate, bool InbLoop, float InFirstDelay)
{
	InternalSetTimer(InOutHandle, FTimerDelegate(), InRate, InbLoop, InFirstDelay, false);
}

FTimerHandle FTimerManager::SetTimerForNextTick(const FTimerDelegate& InDelegate)
{
	// One unit: the next Tick moves the clock at least that far, and this one is past it.
	FTimerHandle Handle;
	InternalSetTimerUnits(Handle, FTimerDelegate(InDelegate), 1, 1, false, true);
	return Handle;
}

void FTimerManager::InternalSetTimer(FTimerHandle& InOutHandle, FTimerDelegate&& InDelegate, float InRate, bool InbLoop,
	float InFirstDelay, bool bRequiresDelegate)
{
	const uint64 Rate = SecondsToTimeUnits(InRate);
	const uint64 FirstDelay = InFirstDelay >= 0.0f ? SecondsToTimeUnits(InFirstDelay) : Rate;
	InternalSetTimerUnits(InOutHandle, MoveTemp(InDelegate), Rate, FirstDelay, InbLoop, bRequiresDelegate);
}

void FTimerManager::InternalSetTimerUnits(FTimerHandle& InOutHandle, FTimerDelegate&& InDelegate, uint64 Rate,
	uint64 FirstDelay, bool InbLoop, bool bRequiresDelegate)
{
	// The handle's old timer goes first (UE).
	ClearTimer(InOutHandle);
	if (Rate == 0)
	{
		return;
	}
	int32 Index = INDEX_NONE;
	if (FreeIndices.Num() > 0)
	{
		Index = FreeIndices.Pop(false);
	}
	else
	{
		Index = Timers.AddDefaulted();
		SerialNumbers.Add(0);
	}
	uint32& Serial = SerialNumbers[Index];
	Serial = Serial == MAX_uint32 ? 1u : Serial + 1u;
	const uint64 Handle = (static_cast<uint64>(Serial) << 32) | static_cast<uint64>(static_cast<uint32>(Index));

	FTimerData& Data = Timers[Index];
	Data.Delegate = MoveTemp(InDelegate);
	Data.Rate = Rate;
	Data.bLoop = InbLoop;
	Data.bRequiresDelegate = bRequiresDelegate;
	Data.SetOrder = ++NextSetOrder;
	Data.Handle = Handle;
	Data.ExpireTime = InternalTime + FirstDelay;
	Data.Status = ETimerStatus::Active;
	HeapPush(Index);
	InOutHandle.Handle = Handle;
}

void FTimerManager::ClearTimer(FTimerHandle& InHandle)
{
	if (FTimerData* Data = FindTimer(InHandle))
	{
		switch (Data->Status)
		{
			case ETimerStatus::Active:
				// Left in the heap until it comes up.
				Data->Status = ETimerStatus::ActivePendingRemoval;
				Data->Delegate.Unbind();
				break;
			case ETimerStatus::Executing:
				// Its callback is running: Tick frees it once the callback returns.
				Data->Status = ETimerStatus::ActivePendingRemoval;
				break;
			case ETimerStatus::Free:
			case ETimerStatus::ActivePendingRemoval:
				break;
		}
	}
	InHandle.Invalidate();
}

void FTimerManager::ClearAllTimersForObject(const void* Object)
{
	if (Object == nullptr)
	{
		return;
	}
	for (int32 Index = 0; Index < Timers.Num(); ++Index)
	{
		const FTimerData& Data = Timers[Index];
		if ((Data.Status == ETimerStatus::Active || Data.Status == ETimerStatus::Executing) &&
			Data.Delegate.IsBoundToObject(Object))
		{
			FTimerHandle Handle;
			Handle.Handle = Data.Handle;
			ClearTimer(Handle);
		}
	}
}

void FTimerManager::ClearAllTimers()
{
	for (int32 Index = 0; Index < Timers.Num(); ++Index)
	{
		if (Timers[Index].Status != ETimerStatus::Free)
		{
			FTimerHandle Handle;
			Handle.Handle = Timers[Index].Handle;
			ClearTimer(Handle);
		}
	}
}

const FTimerManager::FTimerData* FTimerManager::FindTimer(FTimerHandle InHandle) const
{
	if (!InHandle.IsValid())
	{
		return nullptr;
	}
	const int32 Index = static_cast<int32>(static_cast<uint32>(InHandle.Handle));
	if (!Timers.IsValidIndex(Index))
	{
		return nullptr;
	}
	const FTimerData& Data = Timers[Index];
	if (Data.Handle != InHandle.Handle || Data.Status == ETimerStatus::Free ||
		Data.Status == ETimerStatus::ActivePendingRemoval)
	{
		return nullptr;
	}
	return &Data;
}

FTimerManager::FTimerData* FTimerManager::FindTimer(FTimerHandle InHandle)
{
	return const_cast<FTimerData*>(static_cast<const FTimerManager*>(this)->FindTimer(InHandle));
}

void FTimerManager::FreeTimer(int32 Index)
{
	FTimerData& Data = Timers[Index];
	Data.Delegate.Unbind();
	Data.Status = ETimerStatus::Free;
	Data.Handle = 0;
	FreeIndices.Add(Index);
}

bool FTimerManager::IsTimerActive(FTimerHandle InHandle) const
{
	const FTimerData* Data = FindTimer(InHandle);
	if (Data == nullptr)
	{
		return false;
	}
	// A one-shot timer whose callback runs is done.
	return Data->Status != ETimerStatus::Executing || Data->bLoop;
}

float FTimerManager::GetTimerRemaining(FTimerHandle InHandle) const
{
	const FTimerData* Data = FindTimer(InHandle);
	if (Data == nullptr || !IsTimerActive(InHandle))
	{
		return -1.0f;
	}
	if (Data->Status == ETimerStatus::Executing)
	{
		// A looping timer in its callback: the next period.
		return TimeUnitsToSeconds(Data->Rate);
	}
	return TimeUnitsToSeconds(Data->ExpireTime > InternalTime ? Data->ExpireTime - InternalTime : 0);
}

float FTimerManager::GetTimerElapsed(FTimerHandle InHandle) const
{
	const FTimerData* Data = FindTimer(InHandle);
	if (Data == nullptr || !IsTimerActive(InHandle))
	{
		return -1.0f;
	}
	return FMath::Max(0.0f, TimeUnitsToSeconds(Data->Rate) - GetTimerRemaining(InHandle));
}

float FTimerManager::GetTimerRate(FTimerHandle InHandle) const
{
	const FTimerData* Data = FindTimer(InHandle);
	return Data != nullptr ? TimeUnitsToSeconds(Data->Rate) : -1.0f;
}

int32 FTimerManager::GetNumActiveTimers() const
{
	int32 Count = 0;
	for (const FTimerData& Data : Timers)
	{
		Count += Data.Status == ETimerStatus::Active || (Data.Status == ETimerStatus::Executing && Data.bLoop) ? 1 : 0;
	}
	return Count;
}

bool FTimerManager::HeapLess(int32 A, int32 B) const
{
	const FTimerData& DataA = Timers[A];
	const FTimerData& DataB = Timers[B];
	return DataA.ExpireTime < DataB.ExpireTime ||
		(DataA.ExpireTime == DataB.ExpireTime && DataA.SetOrder < DataB.SetOrder);
}

void FTimerManager::HeapPush(int32 Index)
{
	int32 Child = ActiveTimerHeap.Add(Index);
	while (Child > 0)
	{
		const int32 Parent = (Child - 1) / 2;
		if (!HeapLess(ActiveTimerHeap[Child], ActiveTimerHeap[Parent]))
		{
			break;
		}
		Swap(ActiveTimerHeap[Child], ActiveTimerHeap[Parent]);
		Child = Parent;
	}
}

int32 FTimerManager::HeapPop()
{
	const int32 Top = ActiveTimerHeap[0];
	const int32 Last = ActiveTimerHeap.Pop(false);
	if (ActiveTimerHeap.Num() > 0)
	{
		ActiveTimerHeap[0] = Last;
		int32 Parent = 0;
		for (;;)
		{
			const int32 Left = (Parent * 2) + 1;
			if (Left >= ActiveTimerHeap.Num())
			{
				break;
			}
			const int32 Right = Left + 1;
			int32 Smallest = Left;
			if (Right < ActiveTimerHeap.Num() && HeapLess(ActiveTimerHeap[Right], ActiveTimerHeap[Left]))
			{
				Smallest = Right;
			}
			if (!HeapLess(ActiveTimerHeap[Smallest], ActiveTimerHeap[Parent]))
			{
				break;
			}
			Swap(ActiveTimerHeap[Smallest], ActiveTimerHeap[Parent]);
			Parent = Smallest;
		}
	}
	return Top;
}

void FTimerManager::Tick(float DeltaTime)
{
	InternalTime += SecondsToTimeUnits(DeltaTime);
	while (ActiveTimerHeap.Num() > 0)
	{
		const int32 Top = ActiveTimerHeap[0];
		if (Timers[Top].Status == ETimerStatus::ActivePendingRemoval)
		{
			(void)HeapPop();
			FreeTimer(Top);
			continue;
		}
		if (InternalTime < Timers[Top].ExpireTime)
		{
			break;
		}
		(void)HeapPop();
		if (Timers[Top].bRequiresDelegate && !Timers[Top].Delegate.IsBound())
		{
			// Its object went away (UE drops it without a call).
			FreeTimer(Top);
			continue;
		}
		const uint64 Handle = Timers[Top].Handle;
		// A looping timer that fell behind fires once per period it missed (UE).
		const uint64 CallCount =
			Timers[Top].bLoop ? ((InternalTime - Timers[Top].ExpireTime) / Timers[Top].Rate) + 1 : 1;
		Timers[Top].Status = ETimerStatus::Executing;
		uint64 CallsMade = 0;
		for (uint64 Call = 0; Call < CallCount; ++Call)
		{
			// Called in place (no copy, no allocation): the callback may set timers, which may move the array, but the
			// binding lives on the heap and an executing timer's binding is never released before it returns.
			(void)Timers[Top].Delegate.ExecuteIfBound();
			++CallsMade;
			if (Timers[Top].Status != ETimerStatus::Executing)
			{
				break;
			}
		}
		// An executing timer's slot is never freed or reused inside its callback.
		FTimerData& Data = Timers[Top];
		check(Data.Handle == Handle);
		if (Data.Status == ETimerStatus::Executing && Data.bLoop &&
			(!Data.bRequiresDelegate || Data.Delegate.IsBound()))
		{
			Data.ExpireTime += CallsMade * Data.Rate;
			Data.Status = ETimerStatus::Active;
			HeapPush(Top);
		}
		else
		{
			FreeTimer(Top);
		}
	}
}
