#pragma once

#include "Containers/Array.h"
#include "CoreTypes.h"
#include "HAL/UnrealMemory.h"
#include "Math/UnrealMathUtility.h"
#include "Serialization/MemoryArchive.h"

/** Reads from a TArray<uint8>; reading past the end is an error and leaves the rest untouched (UE: FMemoryReader). */
class CORE_API FMemoryReader : public FMemoryArchive
{
public:
	explicit FMemoryReader(const TArray<uint8>& InBytes, bool bIsPersistent = false)
		: Bytes(InBytes)
		, LimitSize(INDEX_NONE)
	{
		SetIsLoading(true);
		SetIsPersistent(bIsPersistent);
	}

	virtual FString GetArchiveName() const override
	{
		return FString("FMemoryReader");
	}

	virtual int64 TotalSize() override
	{
		return LimitSize != INDEX_NONE ? FMath::Min<int64>(Bytes.Num(), LimitSize) : Bytes.Num();
	}

	virtual void Serialize(void* Data, int64 Num) override
	{
		if (Num && !IsError())
		{
			// Only serialize if we have the requested amount of data.
			if (Offset + Num <= TotalSize())
			{
				FMemory::Memcpy(Data, &Bytes[int32(Offset)], SIZE_T(Num));
				Offset += Num;
			}
			else
			{
				SetError();
			}
		}
	}

	/** Treats only the first NewSize bytes as the archive (UE: SetLimitSize). */
	void SetLimitSize(int64 NewSize)
	{
		LimitSize = NewSize;
	}

private:
	const TArray<uint8>& Bytes;
	int64 LimitSize;
};
