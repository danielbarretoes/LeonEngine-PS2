#include "HAL/PlatformAtomics.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProperties.h"
#include "HAL/UnrealMemory.h"
#include "Misc/Crc.h"
#include "Templates/AlignmentTemplates.h"
#include "UObject/NameTypes.h"

#include <cstddef>
#include <cstdlib>
#include <new>

namespace
{
	constexpr uint32 BlockSize = FPlatformProperties::NamePoolBlockSize;
	constexpr uint32 MaxBlocks = FPlatformProperties::NamePoolMaxBlocks;
	constexpr uint32 NumBuckets = FPlatformProperties::NamePoolHashBuckets;

	/** Entries are 4-byte aligned; an id is (block << 16) | (offset / 4). */
	constexpr uint32 EntryStride = 4;
	constexpr uint32 OffsetBits = 16;
	constexpr uint32 EndOfChain = 0xFFFFFFFFu;

	static_assert(BlockSize / EntryStride <= (1u << OffsetBits), "Name pool block too large for the id encoding");
	static_assert((NumBuckets & (NumBuckets - 1)) == 0, "Name pool bucket count must be a power of two");

	/** Global name pool (UE: FNamePool), created on first use and never destroyed. */
	class FNamePool
	{
	public:
		FNamePool()
		{
			Buckets = static_cast<uint32*>(FMemory::Malloc(NumBuckets * sizeof(uint32)));
			for (uint32 Index = 0; Index < NumBuckets; ++Index)
			{
				Buckets[Index] = EndOfChain;
			}

			// Hard-coded names first: NAME_None becomes entry 0.
#define REGISTER_NAME(Num, Name) HardcodedIds[Num] = FindOrAdd(#Name, int32(sizeof(#Name) - 1), FNAME_Add);
#include "UObject/UnrealNames.inl"
#undef REGISTER_NAME
			check(HardcodedIds[NAME_None].IsNone());
		}

		FNameEntryId FindOrAdd(const TCHAR* Name, int32 Len, EFindName FindType)
		{
			if (Len >= NAME_SIZE)
			{
				FPlatformMisc::LowLevelOutputDebugStringf(
					"FName: name longer than %d characters, truncated\n", int32(NAME_SIZE) - 1);
				Len = NAME_SIZE - 1;
			}

			const uint32 Hash = FCrc::Strihash_DEPRECATED(Len, Name);
			FScopeLock Lock(LockValue);

			uint32* BucketHead = &Buckets[Hash & (NumBuckets - 1)];
			for (uint32 Id = *BucketHead; Id != EndOfChain;)
			{
				const FNameEntry& Entry = Resolve(Id);
				if (Entry.Hash == Hash && Entry.Length == Len && FCString::Strnicmp(Entry.Chars, Name, Len) == 0)
				{
					return FNameEntryId::FromUnstableInt(Id);
				}
				Id = Entry.NextInBucket;
			}

			if (FindType == FNAME_Find)
			{
				return FNameEntryId();
			}

			const uint32 Id = Allocate(Len);
			FNameEntry& Entry = Resolve(Id);
			Entry.NextInBucket = *BucketHead;
			Entry.Hash = Hash;
			Entry.Length = uint16(Len);
			FMemory::Memcpy(Entry.Chars, Name, SIZE_T(Len));
			Entry.Chars[Len] = 0;
			*BucketHead = Id;
			++NumEntries;
			return FNameEntryId::FromUnstableInt(Id);
		}

		FORCEINLINE const FNameEntry& Resolve(FNameEntryId Id) const
		{
			return const_cast<FNamePool*>(this)->Resolve(Id.ToUnstableInt());
		}

		FORCEINLINE FNameEntry& Resolve(uint32 Id)
		{
			const uint32 Block = Id >> OffsetBits;
			const uint32 Offset = (Id & ((1u << OffsetBits) - 1)) * EntryStride;
			return *reinterpret_cast<FNameEntry*>(Blocks[Block] + Offset);
		}

		bool IsValidId(FNameEntryId Id) const
		{
			const uint32 Value = Id.ToUnstableInt();
			const uint32 Block = Value >> OffsetBits;
			const uint32 Offset = (Value & ((1u << OffsetBits) - 1)) * EntryStride;
			return Block < NumBlocks && (Block < NumBlocks - 1 || Offset < CurrentOffset);
		}

		FNameEntryId GetHardcodedId(EName Name) const
		{
			return HardcodedIds[Name];
		}

		int32 GetUsedBytes() const
		{
			return NumBlocks ? int32((NumBlocks - 1) * BlockSize + CurrentOffset) : 0;
		}

		int32 GetAllocatedBytes() const
		{
			return int32(NumBlocks * BlockSize + NumBuckets * sizeof(uint32));
		}

		int32 GetNumEntries() const
		{
			return NumEntries;
		}

	private:
		/** Spin lock (names may be created from worker threads on desktop). */
		struct FScopeLock
		{
			explicit FScopeLock(volatile int32& InLock)
				: Lock(InLock)
			{
				while (FPlatformAtomics::InterlockedCompareExchange(&Lock, 1, 0) != 0)
				{
				}
			}
			~FScopeLock()
			{
				FPlatformAtomics::InterlockedExchange(&Lock, 0);
			}
			volatile int32& Lock;
		};

		uint32 Allocate(int32 Len)
		{
			const uint32 Bytes = Align(uint32(offsetof(FNameEntry, Chars)) + uint32(Len) + 1, EntryStride);
			if (NumBlocks == 0 || CurrentOffset + Bytes > BlockSize)
			{
				if (NumBlocks == MaxBlocks)
				{
					FPlatformMisc::LowLevelOutputDebugStringf("FName: name pool exhausted (%u blocks of %u bytes); "
															  "raise NamePoolMaxBlocks for this platform\n",
						MaxBlocks, BlockSize);
					FPlatformMisc::RequestExit(true);
					std::abort();
				}
				Blocks[NumBlocks++] = static_cast<uint8*>(FMemory::Malloc(BlockSize, EntryStride));
				CurrentOffset = 0;
			}
			const uint32 Id = ((NumBlocks - 1) << OffsetBits) | (CurrentOffset / EntryStride);
			CurrentOffset += Bytes;
			return Id;
		}

		uint8* Blocks[MaxBlocks] = {};
		uint32 NumBlocks = 0;
		uint32 CurrentOffset = 0;
		uint32* Buckets = nullptr;
		int32 NumEntries = 0;
		volatile int32 LockValue = 0;
		FNameEntryId HardcodedIds[NAME_MaxHardcodedNameIndex];
	};

	FNamePool& GetNamePool()
	{
		// Placement new into static storage: the pool outlives every static FName.
		alignas(FNamePool) static uint8 Storage[sizeof(FNamePool)];
		static FNamePool* Pool = new (Storage) FNamePool();
		return *Pool;
	}

	/** Splits "Base_123" into Len("Base") and internal number 124; returns false when there is no valid suffix (UE). */
	bool ParseNumberSuffix(const TCHAR* Name, int32 Len, int32& OutBaseLen, int32& OutInternalNumber)
	{
		int32 Digits = 0;
		for (const TCHAR* Char = Name + Len - 1; Char >= Name && FChar::IsDigit(*Char); --Char)
		{
			++Digits;
		}

		const TCHAR* FirstDigit = Name + Len - Digits;
		constexpr int32 MaxDigits = 10;
		if (Digits > 0 && Digits < Len && *(FirstDigit - 1) == TEXT('_') && Digits <= MaxDigits)
		{
			// Leading zeros are part of the base name ("Name_01" stays a single string), except for "_0".
			if (Digits == 1 || *FirstDigit != TEXT('0'))
			{
				int64 Number = 0;
				for (int32 Index = 0; Index < Digits; ++Index)
				{
					Number = Number * 10 + (FirstDigit[Index] - TEXT('0'));
				}
				if (Number < 0x7ffffffe)
				{
					OutBaseLen = Len - Digits - 1;
					OutInternalNumber = int32(NAME_EXTERNAL_TO_INTERNAL(Number));
					return OutBaseLen > 0;
				}
			}
		}
		return false;
	}
} // namespace

FName::FName(const TCHAR* Name, EFindName FindType)
{
	Init(Name, Name ? FCString::Strlen(Name) : 0, NAME_NO_NUMBER_INTERNAL, FindType, true);
}

FName::FName(int32 Len, const TCHAR* Name, EFindName FindType)
{
	Init(Name, Name ? FCString::Strnlen(Name, SIZE_T(Len)) : 0, NAME_NO_NUMBER_INTERNAL, FindType, true);
}

FName::FName(const TCHAR* Name, int32 InNumber, EFindName FindType)
{
	Init(Name, Name ? FCString::Strlen(Name) : 0, InNumber, FindType, false);
}

void FName::Init(const TCHAR* Name, int32 Len, int32 InNumber, EFindName FindType, bool bParseNumber)
{
	if (Len == 0)
	{
		ComparisonIndex = FNameEntryId();
		Number = NAME_NO_NUMBER_INTERNAL;
		return;
	}

	int32 BaseLen = Len;
	int32 InternalNumber = InNumber;
	if (bParseNumber)
	{
		int32 ParsedBaseLen = 0;
		int32 ParsedNumber = 0;
		if (ParseNumberSuffix(Name, Len, ParsedBaseLen, ParsedNumber))
		{
			BaseLen = ParsedBaseLen;
			InternalNumber = ParsedNumber;
		}
	}

	ComparisonIndex = GetNamePool().FindOrAdd(Name, BaseLen, FindType);
	Number = uint32(InternalNumber);
	if (ComparisonIndex.IsNone() && FindType == FNAME_Find)
	{
		// Not found: the name is None.
		Number = NAME_NO_NUMBER_INTERNAL;
	}
}

const FNameEntry* FName::GetDisplayNameEntry() const
{
	return &GetNamePool().Resolve(ComparisonIndex);
}

FString FName::GetPlainNameString() const
{
	return GetDisplayNameEntry()->GetPlainNameString();
}

void FName::GetPlainANSIString(ANSICHAR (&AnsiName)[NAME_SIZE]) const
{
	const FNameEntry* Entry = GetDisplayNameEntry();
	FMemory::Memcpy(AnsiName, Entry->Chars, SIZE_T(Entry->Length) + 1);
}

FString FName::ToString() const
{
	FString Out;
	AppendString(Out);
	return Out;
}

void FName::ToString(FString& Out) const
{
	Out.Reset();
	AppendString(Out);
}

void FName::AppendString(FString& Out) const
{
	GetDisplayNameEntry()->AppendNameToString(Out);
	if (Number != NAME_NO_NUMBER_INTERNAL)
	{
		Out.AppendChar(TEXT('_'));
		Out.AppendInt(NAME_INTERNAL_TO_EXTERNAL(int32(Number)));
	}
}

bool FName::operator==(const TCHAR* Other) const
{
	if (!Other || !*Other)
	{
		return IsNone();
	}
	return FCString::Stricmp(*ToString(), Other) == 0;
}

bool FName::IsEqual(const FName& Other, const ENameCase CompareMethod, const bool bCompareNumber) const
{
	if (bCompareNumber && Number != Other.Number)
	{
		return false;
	}
	if (CompareMethod == ENameCase::IgnoreCase)
	{
		return ComparisonIndex == Other.ComparisonIndex;
	}
	// The pool keeps one casing per entry, so the display strings are equal exactly when the entries are.
	return ComparisonIndex == Other.ComparisonIndex;
}

int32 FName::Compare(const FName& Other) const
{
	if (ComparisonIndex != Other.ComparisonIndex)
	{
		const int32 Result = FCString::Stricmp(GetDisplayNameEntry()->Chars, Other.GetDisplayNameEntry()->Chars);
		if (Result != 0)
		{
			return Result;
		}
	}
	return int32(Number) - int32(Other.Number);
}

bool FName::IsValid() const
{
	return GetNamePool().IsValidId(ComparisonIndex);
}

int32 FName::GetNameEntryMemorySize()
{
	return GetNamePool().GetUsedBytes();
}

int32 FName::GetNameTableMemorySize()
{
	return GetNamePool().GetAllocatedBytes();
}

int32 FName::GetNumNames()
{
	return GetNamePool().GetNumEntries();
}

FNameEntryId FName::GetHardcodedEntryId(EName Name)
{
	return GetNamePool().GetHardcodedId(Name);
}
