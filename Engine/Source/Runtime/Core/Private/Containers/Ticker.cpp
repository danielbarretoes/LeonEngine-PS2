#include "Containers/Ticker.h"

#include <cstddef>
#include <utility>

FTicker& FTicker::GetCoreTicker()
{
	static FTicker Ticker;
	return Ticker;
}

FTicker::FDelegateHandle FTicker::AddTicker(FTickerDelegate Delegate)
{
	FElement Element;
	Element.Id = NextId++;
	Element.Delegate = std::move(Delegate);
	Elements.push_back(std::move(Element));
	FDelegateHandle Handle;
	Handle.Id = Elements.back().Id;
	return Handle;
}

void FTicker::RemoveTicker(FDelegateHandle Handle)
{
	for (size_t Index = 0; Index < Elements.size(); ++Index)
	{
		if (Elements[Index].Id == Handle.Id)
		{
			Elements.erase(Elements.begin() + static_cast<std::ptrdiff_t>(Index));
			return;
		}
	}
}

void FTicker::Tick(float DeltaTime)
{
	for (size_t Index = 0; Index < Elements.size();)
	{
		if (Elements[Index].Delegate(DeltaTime))
		{
			++Index;
		}
		else
		{
			Elements.erase(Elements.begin() + static_cast<std::ptrdiff_t>(Index));
		}
	}
}
