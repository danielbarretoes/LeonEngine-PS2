#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/WeakObjectPtr.h"

class UClass;
class UObject;

/**
 * A soft reference: an FSoftObjectPath plus a cached weak pointer to the object once resolved (UE: FSoftObjectPtr,
 * a TPersistentObjectPtr<FSoftObjectPath>). Minimal until P10 / P11: Get() resolves objects already in memory.
 */
struct COREUOBJECT_API FSoftObjectPtr
{
	FSoftObjectPtr() = default;

	explicit FSoftObjectPtr(const FSoftObjectPath& InObjectID)
		: ObjectID(InObjectID)
	{
	}

	explicit FSoftObjectPtr(const UObject* Object)
	{
		(*this) = Object;
	}

	/** Points at Object (its path and the object itself), or at nothing. */
	void operator=(const UObject* Object);

	FORCEINLINE void operator=(const FSoftObjectPath& InObjectID)
	{
		WeakPtr.Reset();
		ObjectID = InObjectID;
	}

	FORCEINLINE void Reset()
	{
		WeakPtr.Reset();
		ObjectID.Reset();
	}

	/** The object when it is in memory, else nullptr (UE). */
	UObject* Get() const;

	FORCEINLINE const FSoftObjectPath& GetUniqueID() const
	{
		return ObjectID;
	}

	FORCEINLINE const FSoftObjectPath& ToSoftObjectPath() const
	{
		return ObjectID;
	}

	FORCEINLINE bool IsNull() const
	{
		return ObjectID.IsNull();
	}

	FORCEINLINE bool IsValid() const
	{
		return Get() != nullptr;
	}

	/** True when the path names an object that is not in memory (UE). */
	FORCEINLINE bool IsPending() const
	{
		return !IsNull() && !IsValid();
	}

	FORCEINLINE FString ToString() const
	{
		return ObjectID.ToString();
	}

	FORCEINLINE bool operator==(const FSoftObjectPtr& Other) const
	{
		return ObjectID == Other.ObjectID;
	}

	FORCEINLINE bool operator!=(const FSoftObjectPtr& Other) const
	{
		return ObjectID != Other.ObjectID;
	}

	FORCEINLINE friend uint32 GetTypeHash(const FSoftObjectPtr& Ptr)
	{
		return GetTypeHash(Ptr.ObjectID);
	}

private:
	mutable FWeakObjectPtr WeakPtr;
	FSoftObjectPath ObjectID;
};

/** A typed FSoftObjectPtr (UE: TSoftObjectPtr). Same layout, so an FSoftObjectProperty reflects it. */
template <class T = UObject>
struct TSoftObjectPtr
{
public:
	TSoftObjectPtr() = default;

	FORCEINLINE TSoftObjectPtr(const T* Object)
		: SoftObjectPtr((const UObject*)Object)
	{
	}

	explicit FORCEINLINE TSoftObjectPtr(const FSoftObjectPath& ObjectPath)
		: SoftObjectPtr(ObjectPath)
	{
	}

	FORCEINLINE TSoftObjectPtr& operator=(const T* Object)
	{
		SoftObjectPtr = (const UObject*)Object;
		return *this;
	}

	FORCEINLINE void Reset()
	{
		SoftObjectPtr.Reset();
	}

	/** The object when it is in memory, else nullptr (UE). */
	FORCEINLINE T* Get() const
	{
		return (T*)SoftObjectPtr.Get();
	}

	FORCEINLINE T* operator->() const
	{
		return Get();
	}

	FORCEINLINE bool IsNull() const
	{
		return SoftObjectPtr.IsNull();
	}

	FORCEINLINE bool IsValid() const
	{
		return SoftObjectPtr.IsValid();
	}

	FORCEINLINE bool IsPending() const
	{
		return SoftObjectPtr.IsPending();
	}

	FORCEINLINE const FSoftObjectPath& ToSoftObjectPath() const
	{
		return SoftObjectPtr.ToSoftObjectPath();
	}

	FORCEINLINE FString ToString() const
	{
		return SoftObjectPtr.ToString();
	}

	FORCEINLINE bool operator==(const TSoftObjectPtr& Other) const
	{
		return SoftObjectPtr == Other.SoftObjectPtr;
	}

	FORCEINLINE bool operator!=(const TSoftObjectPtr& Other) const
	{
		return SoftObjectPtr != Other.SoftObjectPtr;
	}

	FORCEINLINE friend uint32 GetTypeHash(const TSoftObjectPtr& Ptr)
	{
		return GetTypeHash(Ptr.SoftObjectPtr);
	}

private:
	FSoftObjectPtr SoftObjectPtr;
};

/** A soft reference to a class that is T or derives from it (UE: TSoftClassPtr). Same layout as FSoftObjectPtr. */
template <class TClass = UObject>
struct TSoftClassPtr
{
public:
	TSoftClassPtr() = default;

	FORCEINLINE TSoftClassPtr(const UClass* From)
		: SoftObjectPtr((const UObject*)From)
	{
	}

	explicit FORCEINLINE TSoftClassPtr(const FSoftObjectPath& ObjectPath)
		: SoftObjectPtr(ObjectPath)
	{
	}

	FORCEINLINE void Reset()
	{
		SoftObjectPtr.Reset();
	}

	/** The class when it is in memory, else nullptr (UE). */
	FORCEINLINE UClass* Get() const
	{
		return (UClass*)SoftObjectPtr.Get();
	}

	FORCEINLINE bool IsNull() const
	{
		return SoftObjectPtr.IsNull();
	}

	FORCEINLINE bool IsValid() const
	{
		return SoftObjectPtr.IsValid();
	}

	FORCEINLINE const FSoftObjectPath& ToSoftObjectPath() const
	{
		return SoftObjectPtr.ToSoftObjectPath();
	}

	FORCEINLINE FString ToString() const
	{
		return SoftObjectPtr.ToString();
	}

	FORCEINLINE bool operator==(const TSoftClassPtr& Other) const
	{
		return SoftObjectPtr == Other.SoftObjectPtr;
	}

	FORCEINLINE bool operator!=(const TSoftClassPtr& Other) const
	{
		return SoftObjectPtr != Other.SoftObjectPtr;
	}

	FORCEINLINE friend uint32 GetTypeHash(const TSoftClassPtr& Ptr)
	{
		return GetTypeHash(Ptr.SoftObjectPtr);
	}

private:
	FSoftObjectPtr SoftObjectPtr;
};

static_assert(sizeof(TSoftObjectPtr<>) == sizeof(FSoftObjectPtr), "TSoftObjectPtr must be an FSoftObjectPtr");
static_assert(sizeof(TSoftClassPtr<>) == sizeof(FSoftObjectPtr), "TSoftClassPtr must be an FSoftObjectPtr");
