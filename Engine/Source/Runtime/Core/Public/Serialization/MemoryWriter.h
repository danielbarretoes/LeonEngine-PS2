#pragma once

#include "Containers/Array.h"
#include "CoreTypes.h"
#include "HAL/UnrealMemory.h"
#include "Math/NumericLimits.h"
#include "Serialization/MemoryArchive.h"

/** Writes into a TArray<uint8>, growing it as needed (UE: FMemoryWriter). */
class CORE_API FMemoryWriter : public FMemoryArchive
{
public:
	/** bSetOffset starts writing at the end of the existing bytes. */
	explicit FMemoryWriter(TArray<uint8>& InBytes, bool bIsPersistent = false, bool bSetOffset = false)
		: Bytes(InBytes)
	{
		SetIsSaving(true);
		SetIsPersistent(bIsPersistent);
		if (bSetOffset)
		{
			Offset = InBytes.Num();
		}
	}

	virtual void Serialize(void* Data, int64 Num) override
	{
		const int64 NumBytesToAdd = Offset + Num - Bytes.Num();
		if (NumBytesToAdd > 0)
		{
			const int64 NewArrayCount = Bytes.Num() + NumBytesToAdd;
			if (NewArrayCount >= MAX_int32)
			{
				SetCriticalError();
				return;
			}
			Bytes.AddUninitialized(int32(NumBytesToAdd));
		}

		check((Offset + Num) <= Bytes.Num());

		if (Num)
		{
			FMemory::Memcpy(&Bytes[int32(Offset)], Data, SIZE_T(Num));
			Offset += Num;
		}
	}

	virtual FString GetArchiveName() const override
	{
		return FString("FMemoryWriter");
	}

	virtual int64 TotalSize() override
	{
		return Bytes.Num();
	}

protected:
	TArray<uint8>& Bytes;
};
