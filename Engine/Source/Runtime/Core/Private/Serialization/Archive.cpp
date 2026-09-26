#include "Serialization/Archive.h"

#include "Internationalization/Text.h"

void FArchive::SerializeBits(void* V, int64 LengthBits)
{
	Serialize(V, (LengthBits + 7) / 8);

	if (IsLoading() && (LengthBits % 8) != 0)
	{
		static_cast<uint8*>(V)[LengthBits / 8] &= uint8((1 << (LengthBits & 7)) - 1);
	}
}

void FArchive::SerializeIntPacked(uint32& Value)
{
	if (IsLoading())
	{
		Value = 0;
		uint8 Count = 0;
		uint8 More = 1;
		while (More)
		{
			uint8 NextByte = 0;
			Serialize(&NextByte, 1); // Read next byte
			// Five bytes hold 32 bits: a sixth is corrupt data (checked before its shift, 35 bits, would overflow).
			if (IsError() || Count >= 5)
			{
				SetError();
				return;
			}
			More = NextByte & 1; // Check 1 bit to see if there's more after this
			NextByte = uint8(NextByte >> 1); // Shift to get actual 7 bit value
			Value += uint32(NextByte) << (7 * Count++); // Add to total value
		}
	}
	else
	{
		uint8 PackedBytes[5];
		int32 Length = 0;
		uint32 Remaining = Value;
		for (;;)
		{
			uint8 NextByte = uint8(Remaining & 0x7f); // Get next 7 bits to write
			Remaining = Remaining >> 7; // Update Value to the remaining bits
			NextByte = uint8(NextByte << 1); // Make room for 'more' bit
			if (Remaining > 0)
			{
				NextByte |= 1; // set more bit
				PackedBytes[Length++] = NextByte;
			}
			else
			{
				PackedBytes[Length++] = NextByte;
				break;
			}
		}
		Serialize(PackedBytes, Length);
	}
}

FArchive& FArchive::operator<<(FName& Value)
{
	FString String = IsSaving() ? Value.ToString() : FString();
	*this << String;
	if (IsLoading())
	{
		Value = FName(*String);
	}
	return *this;
}

FArchive& FArchive::operator<<(FText& Value)
{
	FString String = IsSaving() ? Value.ToString() : FString();
	*this << String;
	if (IsLoading())
	{
		Value = FText::FromString(String);
	}
	return *this;
}

FArchive& FArchive::SerializeByteOrderSwapped(void* V, int32 Length)
{
	if (IsLoading())
	{
		// Read and swap.
		Serialize(V, Length);
		uint8* Bytes = static_cast<uint8*>(V);
		for (int32 Index = 0; Index < Length / 2; Index++)
		{
			const uint8 Temp = Bytes[Index];
			Bytes[Index] = Bytes[Length - Index - 1];
			Bytes[Length - Index - 1] = Temp;
		}
	}
	else
	{
		// Swap a copy and write it.
		uint8 Swapped[16];
		check(Length <= int32(sizeof(Swapped)));
		const uint8* Bytes = static_cast<const uint8*>(V);
		for (int32 Index = 0; Index < Length; Index++)
		{
			Swapped[Index] = Bytes[Length - Index - 1];
		}
		Serialize(Swapped, Length);
	}
	return *this;
}

FArchive& operator<<(FArchive& Ar, FString& Value)
{
	if (Ar.IsLoading())
	{
		int32 SaveNum = 0;
		Ar << SaveNum;

		// A negative count is UE's UTF-16 form, which Leon does not write; anything longer than what is left is
		// corrupt data.
		const int64 Remaining = Ar.TotalSize() != INDEX_NONE ? Ar.TotalSize() - Ar.Tell() : SaveNum;
		if (SaveNum < 0 || SaveNum > Remaining)
		{
			Ar.SetCriticalError();
			Value.Empty();
			return Ar;
		}

		TArray<TCHAR>& Data = Value.GetCharArray();
		Data.Empty(SaveNum);
		if (SaveNum > 0)
		{
			Data.AddUninitialized(SaveNum);
			Ar.Serialize(Data.GetData(), SaveNum);
			// The terminator must be there; a string of just the terminator is the empty string.
			if (Data[SaveNum - 1] != 0)
			{
				Ar.SetCriticalError();
				Value.Empty();
				return Ar;
			}
			if (SaveNum == 1)
			{
				Data.Empty();
			}
		}
	}
	else
	{
		// Length including the terminator, 0 for the empty string (UE's ANSI form).
		int32 Num = Value.IsEmpty() ? 0 : Value.Len() + 1;
		Ar << Num;
		if (Num > 0)
		{
			Ar.Serialize(const_cast<TCHAR*>(*Value), Num);
		}
	}
	return Ar;
}
