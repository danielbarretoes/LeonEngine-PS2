#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

class UObject;
struct FUObjectItem;

/**
 * A reference that does not keep its object alive and reads as null once the object is gone (UE: FWeakObjectPtr).
 * It stores the object's GUObjectArray index and the slot's serial number. Garbage collection frees the slot and its
 * serial number; the next object in the slot gets a new one on first weak use, so old weak pointers stay stale. A
 * pending-kill or unreachable object reads as null too, unless asked for explicitly.
 */
struct COREUOBJECT_API FWeakObjectPtr
{
	FORCEINLINE FWeakObjectPtr()
		: ObjectIndex(INDEX_NONE)
		, ObjectSerialNumber(0)
	{
	}

	FWeakObjectPtr(const UObject* Object)
	{
		(*this) = Object;
	}

	FWeakObjectPtr(const FWeakObjectPtr& Other) = default;
	FWeakObjectPtr& operator=(const FWeakObjectPtr& Other) = default;

	/** Points at Object (allocating its serial number on first use) or at nothing. */
	void operator=(const UObject* Object);

	FORCEINLINE void Reset()
	{
		ObjectIndex = INDEX_NONE;
		ObjectSerialNumber = 0;
	}

	/** The object, or nullptr when it no longer exists or is pending kill (UE). */
	UObject* Get() const;

	/** The object, also when it is pending kill with bEvenIfPendingKill (UE). */
	UObject* Get(bool bEvenIfPendingKill) const;

	/** True while the object exists and (unless bEvenIfPendingKill) is not pending kill (UE). */
	bool IsValid(bool bEvenIfPendingKill = false, bool bThreadsafeTest = false) const;

	/**
	 * True when the pointer was set to an object that no longer exists; with bIncludingIfPendingKill (the default) a
	 * pending-kill object counts as gone (UE).
	 */
	bool IsStale(bool bIncludingIfPendingKill = true, bool bThreadsafeTest = false) const;

	/** True when the pointer was never set, or was reset (UE). */
	FORCEINLINE bool IsExplicitlyNull() const
	{
		return ObjectIndex == INDEX_NONE;
	}

	/** True when both point at the same slot and serial number, alive or not (UE). */
	FORCEINLINE bool HasSameIndexAndSerialNumber(const FWeakObjectPtr& Other) const
	{
		return ObjectIndex == Other.ObjectIndex && ObjectSerialNumber == Other.ObjectSerialNumber;
	}

	/** Equal when both point at the same object, or both are invalid (UE). */
	FORCEINLINE bool operator==(const FWeakObjectPtr& Other) const
	{
		return HasSameIndexAndSerialNumber(Other) || (!IsValid() && !Other.IsValid());
	}

	FORCEINLINE bool operator!=(const FWeakObjectPtr& Other) const
	{
		return !(*this == Other);
	}

	FORCEINLINE friend uint32 GetTypeHash(const FWeakObjectPtr& WeakObjectPtr)
	{
		return uint32(WeakObjectPtr.ObjectIndex) ^ uint32(WeakObjectPtr.ObjectSerialNumber);
	}

private:
	/** The live item of the slot when its serial number still matches, else nullptr. */
	const FUObjectItem* Internal_GetObjectItem() const;

	int32 ObjectIndex;
	int32 ObjectSerialNumber;
};

// TWeakObjectPtr (UObject/WeakObjectPtrTemplates.h) builds on FWeakObjectPtr; include it for the typed pointer.
#include "UObject/WeakObjectPtrTemplates.h"
