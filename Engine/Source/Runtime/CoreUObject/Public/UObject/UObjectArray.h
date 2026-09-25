#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

class UObjectBase;

/**
 * One slot of GUObjectArray: the object, its internal flags and the slot's serial number (UE: FUObjectItem). 12 bytes
 * on the PS2, 16 on 64-bit desktops (UE also keeps a cluster index, which Leon has no use for).
 */
struct FUObjectItem
{
	UObjectBase* Object;
	/** EInternalObjectFlags. */
	int32 Flags;
	/** Set on first use by a weak pointer; 0 when none was allocated (UE). */
	int32 SerialNumber;

	FORCEINLINE EInternalObjectFlags GetFlags() const
	{
		return (EInternalObjectFlags)Flags;
	}

	FORCEINLINE bool HasAnyFlags(EInternalObjectFlags InFlags) const
	{
		return !!(Flags & int32(InFlags));
	}

	FORCEINLINE void SetFlags(EInternalObjectFlags FlagsToSet)
	{
		Flags |= int32(FlagsToSet);
	}

	FORCEINLINE void ClearFlags(EInternalObjectFlags FlagsToClear)
	{
		Flags &= ~int32(FlagsToClear);
	}

	FORCEINLINE bool IsPendingKill() const
	{
		return HasAnyFlags(EInternalObjectFlags::PendingKill);
	}

	FORCEINLINE bool IsRootSet() const
	{
		return HasAnyFlags(EInternalObjectFlags::RootSet);
	}
};

/**
 * Every live UObject, by index (UE: FUObjectArray). A fixed array of FPlatformProperties::MaxObjectsInGame slots,
 * allocated once when the object system starts; running out of slots is a fatal error that logs the capacity. Freed
 * slots (P10 garbage collection) are reused.
 */
class COREUOBJECT_API FUObjectArray
{
public:
	FUObjectArray();
	~FUObjectArray();

	FUObjectArray(const FUObjectArray&) = delete;
	FUObjectArray& operator=(const FUObjectArray&) = delete;

	/** Allocates InMaxUObjects slots (UE: AllocateObjectPool). */
	void AllocateObjectPool(int32 InMaxUObjects);

	/** Gives Object a slot and sets its InternalIndex; fatal when the array is full. */
	void AllocateUObjectIndex(UObjectBase* Object);

	/** Frees Object's slot for reuse and bumps no serial number (the next weak pointer to the slot gets a new one). */
	void FreeUObjectIndex(UObjectBase* Object);

	/** The slot of Index, or nullptr for an index out of range (UE). */
	FORCEINLINE FUObjectItem* IndexToObject(int32 Index)
	{
		return (Index >= 0 && Index < ObjLastNonGCIndex) ? &Objects[Index] : nullptr;
	}

	FORCEINLINE const FUObjectItem* IndexToObject(int32 Index) const
	{
		return (Index >= 0 && Index < ObjLastNonGCIndex) ? &Objects[Index] : nullptr;
	}

	/** The slot of a registered object (UE). */
	FUObjectItem* ObjectToObjectItem(const UObjectBase* Object);

	/** The index of a registered object, or INDEX_NONE (UE). */
	int32 ObjectToIndex(const UObjectBase* Object) const;

	/** True when Object is registered in its slot (UE). */
	bool IsValid(const UObjectBase* Object) const;

	/** The serial number of Index, allocating one when it has none (UE: AllocateSerialNumber). */
	int32 AllocateSerialNumber(int32 Index);

	/** The serial number of Index, 0 when none was allocated (UE). */
	FORCEINLINE int32 GetSerialNumber(int32 Index) const
	{
		const FUObjectItem* Item = IndexToObject(Index);
		return Item ? Item->SerialNumber : 0;
	}

	/** One past the highest slot ever used; iterate [0, GetObjectArrayNum()) skipping empty slots (UE). */
	FORCEINLINE int32 GetObjectArrayNum() const
	{
		return ObjLastNonGCIndex;
	}

	/** Slots holding an object (UE: GetObjectArrayNumMinusAvailable). */
	FORCEINLINE int32 GetObjectArrayNumMinusAvailable() const
	{
		return ObjLastNonGCIndex - ObjAvailableList.Num();
	}

	/** The capacity (UE: GetObjectArrayCapacity). */
	FORCEINLINE int32 GetObjectArrayCapacity() const
	{
		return MaxObjects;
	}

	/** Bytes of the slot array (for the platform budgets). */
	FORCEINLINE SIZE_T GetAllocatedSize() const
	{
		return SIZE_T(MaxObjects) * sizeof(FUObjectItem);
	}

	FORCEINLINE bool IsInitialized() const
	{
		return Objects != nullptr;
	}

private:
	FUObjectItem* Objects;
	int32 MaxObjects;
	/** One past the highest slot used so far. */
	int32 ObjLastNonGCIndex;
	/** Last serial number handed out. */
	int32 MasterSerialNumber;
	/** Freed slots, reused first. */
	TArray<int32> ObjAvailableList;
};

/** The global object array (UE: GUObjectArray). */
extern COREUOBJECT_API FUObjectArray GUObjectArray;
