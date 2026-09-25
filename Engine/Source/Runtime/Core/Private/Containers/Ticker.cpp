#include "Containers/Ticker.h"

#include "HAL/PlatformAtomics.h"

FTicker& FTicker::GetCoreTicker()
{
	static FTicker Ticker;
	return Ticker;
}

FDelegateHandle FTicker::AddTicker(const FTickerDelegate& InDelegate, float InDelay)
{
	FElement& Element = Elements.AddDefaulted_GetRef();
	Element.FireTime = CurrentTime + InDelay;
	Element.DelayTime = InDelay;
	Element.Delegate = InDelegate;
	return InDelegate.GetHandle();
}

FDelegateHandle FTicker::AddTicker(const TCHAR* /*InName*/, float InDelay, TFunction<bool(float)> Function)
{
	return AddTicker(FTickerDelegate::CreateLambda(MoveTemp(Function)), InDelay);
}

void FTicker::RemoveTicker(FDelegateHandle Handle)
{
	for (int32 Index = 0; Index < Elements.Num(); ++Index)
	{
		if (Elements[Index].Delegate.GetHandle() == Handle)
		{
			if (bInTick)
			{
				// Keep indices stable while ticking; dropped after the loop.
				Elements[Index].bRemoved = true;
			}
			else
			{
				Elements.RemoveAt(Index);
			}
			return;
		}
	}
}

void FTicker::Tick(float DeltaTime)
{
	CurrentTime += DeltaTime;
	bInTick = true;

	// Delegates added during this Tick fire next frame.
	const int32 NumAtStart = Elements.Num();
	for (int32 Index = 0; Index < NumAtStart; ++Index)
	{
		if (Elements[Index].bRemoved || Elements[Index].FireTime > CurrentTime)
		{
			continue;
		}

		// Copy: the delegate may add tickers (reallocating Elements) while it runs.
		const FTickerDelegate Delegate = Elements[Index].Delegate;
		const bool bKeep = Delegate.IsBound() &&
			Delegate.Execute(Elements[Index].DelayTime > 0.0f ? Elements[Index].DelayTime : DeltaTime);
		if (!bKeep)
		{
			Elements[Index].bRemoved = true;
		}
		else
		{
			Elements[Index].FireTime = CurrentTime + Elements[Index].DelayTime;
		}
	}

	bInTick = false;
	Elements.RemoveAll([](const FElement& Element) { return Element.bRemoved; });
}

uint64 FDelegateHandle::GenerateNewID()
{
	// Starts at 1: 0 is the invalid handle.
	static volatile int64 NextID = 1;
	return uint64(FPlatformAtomics::InterlockedIncrement(&NextID));
}
