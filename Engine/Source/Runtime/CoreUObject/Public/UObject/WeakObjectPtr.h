#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

class UObject;

/**
 * A reference that does not keep its object alive and reads as null once the object is gone (UE: FWeakObjectPtr).
 * It stores the object's GUObjectArray index and the slot's serial number; a slot reused by another object gets a
 * new serial number. Objects are only destroyed from P10 on (garbage collection), so until then a weak pointer to a
 * live object always resolves.
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

	/** The object, or nullptr when it no longer exists (UE). */
	UObject* Get() const;

	/** True while the object exists (UE). */
	bool IsValid() const;

	/** True when the pointer was set to an object that no longer exists (UE). */
	bool IsStale() const;

	FORCEINLINE bool operator==(const FWeakObjectPtr& Other) const
	{
		return (ObjectIndex == Other.ObjectIndex && ObjectSerialNumber == Other.ObjectSerialNumber) ||
			(!IsValid() && !Other.IsValid());
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
	int32 ObjectIndex;
	int32 ObjectSerialNumber;
};
