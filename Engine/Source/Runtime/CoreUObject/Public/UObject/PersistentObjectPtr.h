#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"

class UObject;

/**
 * A reference that names its object by a persistent id and caches the object once found (UE: TPersistentObjectPtr).
 * TObjectID provides ResolveObject, IsValid, Reset, GetOrCreateIDForObject and the GetCurrentTag counter; the soft
 * pointers use FSoftObjectPath. The cached pointer is weak, so this never keeps the object alive.
 */
template <class TObjectID>
struct TPersistentObjectPtr
{
	FORCEINLINE TPersistentObjectPtr()
		: TagAtLastTest(0)
	{
	}

	explicit FORCEINLINE TPersistentObjectPtr(const TObjectID& InObjectID)
		: TagAtLastTest(0)
		, ObjectID(InObjectID)
	{
	}

	FORCEINLINE void Reset()
	{
		WeakPtr.Reset();
		ObjectID.Reset();
		TagAtLastTest = 0;
	}

	/** Forgets the cached object; the next Get resolves the id again (UE). */
	FORCEINLINE void ResetWeakPtr()
	{
		WeakPtr.Reset();
		TagAtLastTest = 0;
	}

	FORCEINLINE void operator=(const TObjectID& InObjectID)
	{
		WeakPtr.Reset();
		ObjectID = InObjectID;
		TagAtLastTest = 0;
	}

	/** Points at Object: its id and the object itself (UE). */
	FORCEINLINE void operator=(const UObject* Object)
	{
		if (Object)
		{
			ObjectID = TObjectID::GetOrCreateIDForObject(Object);
			WeakPtr = Object;
			TagAtLastTest = TObjectID::GetCurrentTag();
		}
		else
		{
			Reset();
		}
	}

	FORCEINLINE const TObjectID& GetUniqueID() const
	{
		return ObjectID;
	}

	FORCEINLINE TObjectID& GetUniqueID()
	{
		return ObjectID;
	}

	/**
	 * The object, or nullptr when it is not in memory (or pending kill). A failed lookup is only retried once objects
	 * appeared since (the id's tag changed) or the cached object went away (UE).
	 */
	UObject* Get() const
	{
		UObject* Object = WeakPtr.Get();
		if (!Object && ObjectID.IsValid() &&
			(TObjectID::GetCurrentTag() != TagAtLastTest || !WeakPtr.IsExplicitlyNull()))
		{
			Object = ObjectID.ResolveObject();
			WeakPtr = Object;
			TagAtLastTest = TObjectID::GetCurrentTag();
			// A pending-kill object reads as null, as through a weak pointer.
			Object = WeakPtr.Get();
		}
		return Object;
	}

	FORCEINLINE UObject* operator->() const
	{
		return Get();
	}

	/** True when the id names an object (loaded or not) (UE). */
	FORCEINLINE bool IsNull() const
	{
		return !ObjectID.IsValid();
	}

	/** True when the object is in memory (UE). */
	FORCEINLINE bool IsValid() const
	{
		return Get() != nullptr;
	}

	/** True when the cached object went away (UE). */
	FORCEINLINE bool IsStale() const
	{
		return WeakPtr.IsStale();
	}

	/** True when the id names an object that is not in memory (UE). */
	FORCEINLINE bool IsPending() const
	{
		return Get() == nullptr && ObjectID.IsValid();
	}

	FORCEINLINE explicit operator bool() const
	{
		return IsValid();
	}

	FORCEINLINE bool operator==(const TPersistentObjectPtr& Rhs) const
	{
		return ObjectID == Rhs.ObjectID;
	}

	FORCEINLINE bool operator!=(const TPersistentObjectPtr& Rhs) const
	{
		return ObjectID != Rhs.ObjectID;
	}

	FORCEINLINE friend uint32 GetTypeHash(const TPersistentObjectPtr& Ptr)
	{
		return GetTypeHash(Ptr.ObjectID);
	}

private:
	mutable FWeakObjectPtr WeakPtr;
	/** TObjectID::GetCurrentTag() when WeakPtr was last resolved. */
	mutable int32 TagAtLastTest;
	TObjectID ObjectID;
};
