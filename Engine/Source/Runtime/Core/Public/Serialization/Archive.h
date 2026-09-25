#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "UObject/NameTypes.h"

#include <type_traits>

class FText;

/**
 * Base of every serializer: one operator<< per type both reads and writes, depending on IsLoading / IsSaving
 * (UE: FArchive, with FArchiveState folded in). Integers are little-endian on disk.
 *
 * Leon: FName and FText serialize as their strings in this base class (UE's base does nothing for FName; the
 * package linker overrides it in P11). FString is written as UTF-8 bytes with a positive length (UE writes UTF-16
 * with a negative length for non-ASCII text).
 */
class CORE_API FArchive
{
public:
	FArchive() = default;
	FArchive(const FArchive&) = default;
	FArchive& operator=(const FArchive&) = default;
	virtual ~FArchive() = default;

	/** Reads or writes Length raw bytes. */
	virtual void Serialize(void* /*V*/, int64 /*Length*/)
	{
	}

	/** Reads or writes LengthBits bits, rounded up to bytes (UE: SerializeBits). */
	virtual void SerializeBits(void* V, int64 LengthBits);

	/** Writes a uint32 as 1-5 bytes, 7 bits per byte (UE: SerializeIntPacked). */
	virtual void SerializeIntPacked(uint32& Value);

	virtual FArchive& operator<<(FName& Value);
	virtual FArchive& operator<<(FText& Value);

	/** Position, or INDEX_NONE when the archive cannot tell. */
	virtual int64 Tell()
	{
		return INDEX_NONE;
	}

	virtual int64 TotalSize()
	{
		return INDEX_NONE;
	}

	virtual bool AtEnd()
	{
		const int64 Pos = Tell();
		return ((Pos != INDEX_NONE) && (Pos >= TotalSize()));
	}

	virtual void Seek(int64 /*InPos*/)
	{
	}

	virtual void Flush()
	{
	}

	/** Flushes and releases the underlying resource; false when an error happened (UE: Close). */
	virtual bool Close()
	{
		return !ArIsError;
	}

	virtual FString GetArchiveName() const
	{
		return FString("FArchive");
	}

	/** Size of an object for memory counting; nothing by default. */
	virtual void CountBytes(SIZE_T /*InNum*/, SIZE_T /*InMax*/)
	{
	}

	/** Reads or writes Length bytes in the archive's byte order (UE: ByteOrderSerialize). */
	FORCEINLINE FArchive& ByteOrderSerialize(void* V, int32 Length)
	{
		if (!IsByteSwapping())
		{
			Serialize(V, Length);
			return *this;
		}
		return SerializeByteOrderSwapped(V, Length);
	}

	FORCEINLINE bool IsLoading() const
	{
		return ArIsLoading;
	}
	FORCEINLINE bool IsSaving() const
	{
		return ArIsSaving;
	}
	FORCEINLINE bool IsPersistent() const
	{
		return ArIsPersistent;
	}
	FORCEINLINE bool IsError() const
	{
		return ArIsError;
	}
	FORCEINLINE bool GetError() const
	{
		return ArIsError;
	}
	FORCEINLINE bool IsCriticalError() const
	{
		return ArIsCriticalError;
	}
	FORCEINLINE bool IsFilterEditorOnly() const
	{
		return ArIsFilterEditorOnly;
	}
	FORCEINLINE bool IsByteSwapping() const
	{
		// Every supported platform is little-endian; swapping only happens when forced.
		return ArForceByteSwapping;
	}
	FORCEINLINE bool IsCooking() const
	{
		return ArIsCooking;
	}

	void SetIsLoading(bool bIsLoading)
	{
		ArIsLoading = bIsLoading;
	}
	void SetIsSaving(bool bIsSaving)
	{
		ArIsSaving = bIsSaving;
	}
	void SetIsPersistent(bool bIsPersistent)
	{
		ArIsPersistent = bIsPersistent;
	}
	void SetFilterEditorOnly(bool bInFilterEditorOnly)
	{
		ArIsFilterEditorOnly = bInFilterEditorOnly;
	}
	void SetByteSwapping(bool bEnabled)
	{
		ArForceByteSwapping = bEnabled;
	}
	void SetIsCooking(bool bCooking)
	{
		ArIsCooking = bCooking;
	}

	void SetError()
	{
		ArIsError = true;
	}

	/** An error that makes the rest of the data unusable (UE: SetCriticalError). */
	void SetCriticalError()
	{
		ArIsError = true;
		ArIsCriticalError = true;
	}

	void ClearError()
	{
		ArIsError = false;
		ArIsCriticalError = false;
	}

	// Primitive types. Single bytes are copied as they are, wider types go through ByteOrderSerialize.

	FORCEINLINE friend FArchive& operator<<(FArchive& Ar, ANSICHAR& Value)
	{
		Ar.Serialize(&Value, 1);
		return Ar;
	}
	FORCEINLINE friend FArchive& operator<<(FArchive& Ar, uint8& Value)
	{
		Ar.Serialize(&Value, 1);
		return Ar;
	}
	FORCEINLINE friend FArchive& operator<<(FArchive& Ar, int8& Value)
	{
		Ar.Serialize(&Value, 1);
		return Ar;
	}
	FORCEINLINE friend FArchive& operator<<(FArchive& Ar, uint16& Value)
	{
		Ar.ByteOrderSerialize(&Value, sizeof(Value));
		return Ar;
	}
	FORCEINLINE friend FArchive& operator<<(FArchive& Ar, int16& Value)
	{
		Ar.ByteOrderSerialize(&Value, sizeof(Value));
		return Ar;
	}
	FORCEINLINE friend FArchive& operator<<(FArchive& Ar, uint32& Value)
	{
		Ar.ByteOrderSerialize(&Value, sizeof(Value));
		return Ar;
	}
	FORCEINLINE friend FArchive& operator<<(FArchive& Ar, int32& Value)
	{
		Ar.ByteOrderSerialize(&Value, sizeof(Value));
		return Ar;
	}
	FORCEINLINE friend FArchive& operator<<(FArchive& Ar, uint64& Value)
	{
		Ar.ByteOrderSerialize(&Value, sizeof(Value));
		return Ar;
	}
	FORCEINLINE friend FArchive& operator<<(FArchive& Ar, int64& Value)
	{
		Ar.ByteOrderSerialize(&Value, sizeof(Value));
		return Ar;
	}
	FORCEINLINE friend FArchive& operator<<(FArchive& Ar, float& Value)
	{
		Ar.ByteOrderSerialize(&Value, sizeof(Value));
		return Ar;
	}
	FORCEINLINE friend FArchive& operator<<(FArchive& Ar, double& Value)
	{
		Ar.ByteOrderSerialize(&Value, sizeof(Value));
		return Ar;
	}

	/** Booleans are 32-bit on disk; a stored value above 1 is an error (UE). */
	friend FArchive& operator<<(FArchive& Ar, bool& D)
	{
		uint32 OldUBoolValue = D ? 1 : 0;
		Ar.Serialize(&OldUBoolValue, sizeof(OldUBoolValue));
		if (OldUBoolValue > 1)
		{
			Ar.SetError();
		}
		D = !!OldUBoolValue;
		return Ar;
	}

	friend CORE_API FArchive& operator<<(FArchive& Ar, FString& Value);

	/** Serializes an enum class through its underlying integer. */
	template <typename EnumType, std::enable_if_t<std::is_enum<EnumType>::value, int> = 0>
	friend FArchive& operator<<(FArchive& Ar, EnumType& Value)
	{
		return Ar << reinterpret_cast<std::underlying_type_t<EnumType>&>(Value);
	}

protected:
	FArchive& SerializeByteOrderSwapped(void* V, int32 Length);

	bool ArIsLoading = false;
	bool ArIsSaving = false;
	bool ArIsPersistent = false;
	bool ArIsError = false;
	bool ArIsCriticalError = false;
	bool ArIsFilterEditorOnly = false;
	bool ArForceByteSwapping = false;
	bool ArIsCooking = false;
};

/**
 * TArray: element count as int32, then the elements; single-byte elements go in bulk. A negative or impossible
 * count on load is a critical error (UE: TArray's operator<<).
 */
template <typename ElementType, typename AllocatorType>
FArchive& operator<<(FArchive& Ar, TArray<ElementType, AllocatorType>& A)
{
	int32 SerializeNum = Ar.IsLoading() ? 0 : A.Num();
	Ar << SerializeNum;

	if (Ar.IsLoading())
	{
		const int64 Remaining = Ar.TotalSize() != INDEX_NONE ? Ar.TotalSize() - Ar.Tell() : SerializeNum;
		if (SerializeNum < 0 || (sizeof(ElementType) == 1 && SerializeNum > Remaining))
		{
			Ar.SetCriticalError();
			A.Empty();
			return Ar;
		}
	}

	if (SerializeNum == 0)
	{
		if (Ar.IsLoading())
		{
			A.Empty();
		}
		return Ar;
	}

	if constexpr (sizeof(ElementType) == 1)
	{
		if (Ar.IsLoading())
		{
			A.Empty(SerializeNum);
			A.AddUninitialized(SerializeNum);
		}
		Ar.Serialize(A.GetData(), SerializeNum);
	}
	else if (Ar.IsLoading())
	{
		A.Empty(SerializeNum);
		for (int32 Index = 0; Index < SerializeNum && !Ar.IsError(); ++Index)
		{
			Ar << A.AddDefaulted_GetRef();
		}
	}
	else
	{
		for (int32 Index = 0; Index < SerializeNum; ++Index)
		{
			Ar << A[Index];
		}
	}
	return Ar;
}

/** TSet: count, then the elements (UE: TSet's operator<<). */
template <typename ElementType, typename KeyFuncs, typename Allocator>
FArchive& operator<<(FArchive& Ar, TSet<ElementType, KeyFuncs, Allocator>& Set)
{
	int32 NewNumElements = Set.Num();
	Ar << NewNumElements;

	if (Ar.IsLoading())
	{
		if (NewNumElements < 0)
		{
			Ar.SetCriticalError();
			return Ar;
		}
		Set.Empty(NewNumElements);
		for (int32 ElementIndex = 0; ElementIndex < NewNumElements && !Ar.IsError(); ElementIndex++)
		{
			ElementType Element;
			Ar << Element;
			Set.Add(MoveTemp(Element));
		}
	}
	else
	{
		for (ElementType& Element : Set)
		{
			Ar << Element;
		}
	}
	return Ar;
}

/** TPair: key, then value. */
template <typename KeyType, typename ValueType>
FArchive& operator<<(FArchive& Ar, TPair<KeyType, ValueType>& Pair)
{
	Ar << Pair.Key;
	Ar << Pair.Value;
	return Ar;
}

/** TMap: count, then key / value pairs (UE: TMap's operator<<). */
template <typename KeyType, typename ValueType, typename SetAllocator, typename KeyFuncs>
FArchive& operator<<(FArchive& Ar, TMap<KeyType, ValueType, SetAllocator, KeyFuncs>& Map)
{
	int32 NewNumElements = Map.Num();
	Ar << NewNumElements;

	if (Ar.IsLoading())
	{
		if (NewNumElements < 0)
		{
			Ar.SetCriticalError();
			return Ar;
		}
		Map.Empty(NewNumElements);
		for (int32 ElementIndex = 0; ElementIndex < NewNumElements && !Ar.IsError(); ElementIndex++)
		{
			KeyType Key;
			ValueType Value;
			Ar << Key;
			Ar << Value;
			Map.Add(MoveTemp(Key), MoveTemp(Value));
		}
	}
	else
	{
		for (auto& Pair : Map)
		{
			Ar << Pair.Key;
			Ar << Pair.Value;
		}
	}
	return Ar;
}
