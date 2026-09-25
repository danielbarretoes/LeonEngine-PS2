#pragma once

// Bulk data: raw payloads of an object stored outside its tagged properties (UE: Serialization/BulkData.h).

#include "CoreMinimal.h"
#include "Serialization/Archive.h"

class UObject;

/** How a bulk data payload is stored (UE: EBulkDataFlags; the subset Leon uses, UE values). */
enum EBulkDataFlags : uint32
{
	BULKDATA_None = 0,
	/** The payload is at the end of the package file, after the exports (the default in a package, D13). */
	BULKDATA_PayloadAtEndOfFile = 1 << 0,
	/** Empty: no payload (UE). */
	BULKDATA_Unused = 1 << 5,
	/** Keep the payload inline, right after its header, even in a package (UE). */
	BULKDATA_ForceInlinePayload = 1 << 6,
	/** The element count and size are 64-bit; Leon always sets it (UE). */
	BULKDATA_Size64Bit = 1 << 13,
};

/** Access modes of FByteBulkData::Lock (UE: LOCK_* ). */
enum EBulkDataLockFlags : uint32
{
	LOCK_NONE = 0,
	LOCK_READ_ONLY = 1,
	LOCK_READ_WRITE = 2,
};

/**
 * A byte payload an object serializes in its native Serialize (UE: FByteBulkData), for data too big for tagged
 * properties (mesh buffers, texture mips, sound samples). In a package the payload goes after all the exports, at
 * FPackageFileSummary::BulkDataStartOffset (plan decision D13), unless BULKDATA_ForceInlinePayload; any other archive
 * gets it inline. Leon loads it eagerly with its owner (no lazy loading, streaming or compression).
 *
 * Access: `void* Data = BulkData.Lock(LOCK_READ_WRITE); BulkData.Realloc(Count); ...; BulkData.Unlock();` and
 * `const void* Data = BulkData.LockReadOnly(); ...; BulkData.Unlock();`.
 */
class COREUOBJECT_API FByteBulkData
{
public:
	FByteBulkData() = default;
	FByteBulkData(const FByteBulkData& Other);
	FByteBulkData& operator=(const FByteBulkData& Other);
	~FByteBulkData();

	/** Number of bytes (UE: GetElementCount). */
	FORCEINLINE int64 GetElementCount() const
	{
		return ElementCount;
	}

	/** Size of one element: 1 (UE). */
	FORCEINLINE int32 GetElementSize() const
	{
		return 1;
	}

	/** Size of the payload in bytes (UE). */
	FORCEINLINE int64 GetBulkDataSize() const
	{
		return ElementCount;
	}

	FORCEINLINE uint32 GetBulkDataFlags() const
	{
		return BulkDataFlags;
	}

	FORCEINLINE void SetBulkDataFlags(uint32 FlagsToSet)
	{
		BulkDataFlags |= FlagsToSet;
	}

	FORCEINLINE void ClearBulkDataFlags(uint32 FlagsToClear)
	{
		BulkDataFlags &= ~FlagsToClear;
	}

	/** Where the last load read the payload from, relative to BulkDataStartOffset (end-of-file payloads), or -1. */
	FORCEINLINE int64 GetBulkDataOffsetInFile() const
	{
		return BulkDataOffsetInFile;
	}

	/** True: Leon keeps payloads in memory (UE: IsBulkDataLoaded). */
	FORCEINLINE bool IsBulkDataLoaded() const
	{
		return true;
	}

	FORCEINLINE bool IsLocked() const
	{
		return LockStatus != LOCK_NONE;
	}

	/** Locks the payload for LockFlags access and returns it (nullptr when empty); must not be locked (UE). */
	void* Lock(uint32 LockFlags);

	/** Locks the payload for reading (UE). */
	const void* LockReadOnly() const;

	/** Ends a Lock / LockReadOnly (UE). */
	void Unlock() const;

	/** Resizes the payload to InElementCount bytes, keeping the start; locked for writing (UE). */
	void* Realloc(int64 InElementCount);

	/**
	 * Copies the payload into *Dest (allocated with FMemory::Malloc when *Dest is null); with bDiscardInternalCopy
	 * the payload is released afterwards (UE: GetCopy).
	 */
	void GetCopy(void** Dest, bool bDiscardInternalCopy = true);

	/** Frees the payload (UE). */
	void RemoveBulkData();

	/**
	 * Loads or saves the payload with Owner's native data (UE). Header: flags (uint32), element count, size on disk
	 * and offset (int64 each), then the payload itself when inline; an end-of-file payload is written by the package
	 * saver after the exports and its offset patched in.
	 */
	void Serialize(FArchive& Ar, UObject* Owner, int32 Index = INDEX_NONE);

private:
	void* Data = nullptr;
	int64 ElementCount = 0;
	uint32 BulkDataFlags = BULKDATA_None;
	mutable uint32 LockStatus = LOCK_NONE;
	int64 BulkDataOffsetInFile = INDEX_NONE;
};
