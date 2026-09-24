#pragma once

#include "CoreTypes.h"

#include <functional>
#include <vector>

/**
 * Per-frame callbacks ticked by the engine loop (UE: FTicker / FTickerDelegate).
 * A callback returns false to unregister itself.
 */
class CORE_API FTicker
{
public:
	using FTickerDelegate = std::function<bool(float DeltaTime)>;

	struct FDelegateHandle
	{
		int32 Id = 0;

		bool IsValid() const
		{
			return Id != 0;
		}
	};

	/** The ticker FEngineLoop::Tick drives every frame. */
	static FTicker& GetCoreTicker();

	FDelegateHandle AddTicker(FTickerDelegate Delegate);
	void RemoveTicker(FDelegateHandle Handle);

	/** Runs every registered delegate; removes the ones that return false. */
	void Tick(float DeltaTime);

private:
	struct FElement
	{
		int32 Id = 0;
		FTickerDelegate Delegate;
	};

	std::vector<FElement> Elements;
	int32 NextId = 1;
};
