#pragma once

#include "Containers/Array.h"
#include "CoreTypes.h"
#include "Delegates/Delegate.h"
#include "Templates/Function.h"

/** Per-frame callback; return false to unregister (UE: FTickerDelegate). */
DECLARE_DELEGATE_RetVal_OneParam(bool, FTickerDelegate, float);

/**
 * Callbacks ticked by the engine loop (UE: FTicker). A delegate fires every Tick (or every InDelay seconds) until it
 * returns false or is removed; removing during Tick is safe.
 */
class CORE_API FTicker
{
public:
	/** The ticker FEngineLoop::Tick drives every frame. */
	static FTicker& GetCoreTicker();

	FDelegateHandle AddTicker(const FTickerDelegate& InDelegate, float InDelay = 0.0f);

	/** Wraps a function in a delegate (UE: AddTicker(Name, Delay, Function)). */
	FDelegateHandle AddTicker(const TCHAR* InName, float InDelay, TFunction<bool(float)> Function);

	void RemoveTicker(FDelegateHandle Handle);

	/** Fires the due delegates; DeltaTime is the frame time in seconds. */
	void Tick(float DeltaTime);

private:
	struct FElement
	{
		double FireTime = 0.0;
		float DelayTime = 0.0f;
		FTickerDelegate Delegate;
		bool bRemoved = false;
	};

	TArray<FElement> Elements;
	double CurrentTime = 0.0;
	bool bInTick = false;
};
