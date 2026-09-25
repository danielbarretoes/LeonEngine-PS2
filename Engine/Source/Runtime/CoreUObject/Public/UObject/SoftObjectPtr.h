#pragma once

#include "CoreMinimal.h"
#include "Templates/Casts.h"
#include "UObject/PersistentObjectPtr.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/WeakObjectPtr.h"

#include <cstddef>
#include <type_traits>

class UClass;
class UObject;

/**
 * A soft reference: an FSoftObjectPath plus a cached weak pointer to the object once found (UE: FSoftObjectPtr, a
 * TPersistentObjectPtr<FSoftObjectPath>). It never keeps the object alive; Get() finds it when it is in memory, and
 * LoadSynchronous loads its package when it is not.
 */
struct COREUOBJECT_API FSoftObjectPtr : public TPersistentObjectPtr<FSoftObjectPath>
{
	FSoftObjectPtr() = default;

	explicit FSoftObjectPtr(const FSoftObjectPath& InObjectID)
		: TPersistentObjectPtr<FSoftObjectPath>(InObjectID)
	{
	}

	explicit FSoftObjectPtr(const UObject* Object)
	{
		(*this) = Object;
	}

	using TPersistentObjectPtr<FSoftObjectPath>::operator=;

	FORCEINLINE const FSoftObjectPath& ToSoftObjectPath() const
	{
		return GetUniqueID();
	}

	FORCEINLINE FString ToString() const
	{
		return ToSoftObjectPath().ToString();
	}

	FORCEINLINE FString GetLongPackageName() const
	{
		return ToSoftObjectPath().GetLongPackageName();
	}

	FORCEINLINE FString GetAssetName() const
	{
		return ToSoftObjectPath().GetAssetName();
	}

	/** The object, loading its package when needed (UE). */
	UObject* LoadSynchronous() const;
};

/** A typed FSoftObjectPtr (UE: TSoftObjectPtr). Same layout, so an FSoftObjectProperty reflects it. */
template <class T = UObject>
struct TSoftObjectPtr
{
public:
	TSoftObjectPtr() = default;

	FORCEINLINE TSoftObjectPtr(std::nullptr_t)
	{
	}

	FORCEINLINE TSoftObjectPtr(const T* Object)
		: SoftObjectPtr((const UObject*)Object)
	{
	}

	explicit FORCEINLINE TSoftObjectPtr(const FSoftObjectPath& ObjectPath)
		: SoftObjectPtr(ObjectPath)
	{
	}

	template <class OtherT, typename = std::enable_if_t<std::is_convertible_v<OtherT*, T*>>>
	FORCEINLINE TSoftObjectPtr(const TSoftObjectPtr<OtherT>& Other)
		: SoftObjectPtr(Other.SoftObjectPtr)
	{
	}

	FORCEINLINE TSoftObjectPtr& operator=(const T* Object)
	{
		SoftObjectPtr = (const UObject*)Object;
		return *this;
	}

	FORCEINLINE TSoftObjectPtr& operator=(const FSoftObjectPath& ObjectPath)
	{
		SoftObjectPtr = ObjectPath;
		return *this;
	}

	FORCEINLINE void Reset()
	{
		SoftObjectPtr.Reset();
	}

	/** The object when it is in memory and a T, else nullptr (UE). */
	FORCEINLINE T* Get() const
	{
		return Cast<T>(SoftObjectPtr.Get());
	}

	FORCEINLINE T* operator->() const
	{
		return Get();
	}

	FORCEINLINE T& operator*() const
	{
		return *Get();
	}

	/** The object, loading its package when needed (UE). */
	FORCEINLINE T* LoadSynchronous() const
	{
		return Cast<T>(SoftObjectPtr.LoadSynchronous());
	}

	/** True when no path is set (UE). */
	FORCEINLINE bool IsNull() const
	{
		return SoftObjectPtr.IsNull();
	}

	/** True when the object is in memory (UE). */
	FORCEINLINE bool IsValid() const
	{
		return Get() != nullptr;
	}

	/** True when the path names an object that is not in memory (UE). */
	FORCEINLINE bool IsPending() const
	{
		return SoftObjectPtr.IsPending();
	}

	/** True when the object was found once and has gone since (UE). */
	FORCEINLINE bool IsStale() const
	{
		return SoftObjectPtr.IsStale();
	}

	FORCEINLINE explicit operator bool() const
	{
		return IsValid();
	}

	FORCEINLINE const FSoftObjectPath& ToSoftObjectPath() const
	{
		return SoftObjectPtr.ToSoftObjectPath();
	}

	FORCEINLINE FString ToString() const
	{
		return SoftObjectPtr.ToString();
	}

	FORCEINLINE FString GetLongPackageName() const
	{
		return SoftObjectPtr.GetLongPackageName();
	}

	FORCEINLINE FString GetAssetName() const
	{
		return SoftObjectPtr.GetAssetName();
	}

	FORCEINLINE bool operator==(const TSoftObjectPtr& Other) const
	{
		return SoftObjectPtr == Other.SoftObjectPtr;
	}

	FORCEINLINE bool operator!=(const TSoftObjectPtr& Other) const
	{
		return SoftObjectPtr != Other.SoftObjectPtr;
	}

	FORCEINLINE bool operator==(std::nullptr_t) const
	{
		return IsNull();
	}

	FORCEINLINE bool operator!=(std::nullptr_t) const
	{
		return !IsNull();
	}

	FORCEINLINE friend uint32 GetTypeHash(const TSoftObjectPtr& Ptr)
	{
		return GetTypeHash(Ptr.SoftObjectPtr);
	}

private:
	template <class>
	friend struct TSoftObjectPtr;

	FSoftObjectPtr SoftObjectPtr;
};

/** A soft reference to a class that is TClass or derives from it (UE: TSoftClassPtr). Same layout as FSoftObjectPtr. */
template <class TClass = UObject>
struct TSoftClassPtr
{
public:
	TSoftClassPtr() = default;

	FORCEINLINE TSoftClassPtr(std::nullptr_t)
	{
	}

	FORCEINLINE TSoftClassPtr(const UClass* From)
		: SoftObjectPtr((const UObject*)From)
	{
	}

	explicit FORCEINLINE TSoftClassPtr(const FSoftObjectPath& ObjectPath)
		: SoftObjectPtr(ObjectPath)
	{
	}

	template <class OtherT, typename = std::enable_if_t<std::is_base_of_v<TClass, OtherT>>>
	FORCEINLINE TSoftClassPtr(const TSoftClassPtr<OtherT>& Other)
		: SoftObjectPtr(Other.SoftObjectPtr)
	{
	}

	FORCEINLINE TSoftClassPtr& operator=(const UClass* From)
	{
		SoftObjectPtr = (const UObject*)From;
		return *this;
	}

	FORCEINLINE TSoftClassPtr& operator=(const FSoftObjectPath& ObjectPath)
	{
		SoftObjectPtr = ObjectPath;
		return *this;
	}

	FORCEINLINE void Reset()
	{
		SoftObjectPtr.Reset();
	}

	/** The class when it is in memory and derives from TClass, else nullptr (UE). */
	FORCEINLINE UClass* Get() const
	{
		UClass* Class = Cast<UClass>(SoftObjectPtr.Get());
		return Class && Class->IsChildOf(TClass::StaticClass()) ? Class : nullptr;
	}

	FORCEINLINE UClass* operator*() const
	{
		return Get();
	}

	FORCEINLINE UClass* operator->() const
	{
		return Get();
	}

	/** The class, loading its package when needed (UE). */
	FORCEINLINE UClass* LoadSynchronous() const
	{
		UClass* Class = Cast<UClass>(SoftObjectPtr.LoadSynchronous());
		return Class && Class->IsChildOf(TClass::StaticClass()) ? Class : nullptr;
	}

	FORCEINLINE bool IsNull() const
	{
		return SoftObjectPtr.IsNull();
	}

	FORCEINLINE bool IsValid() const
	{
		return Get() != nullptr;
	}

	FORCEINLINE bool IsPending() const
	{
		return SoftObjectPtr.IsPending();
	}

	FORCEINLINE bool IsStale() const
	{
		return SoftObjectPtr.IsStale();
	}

	FORCEINLINE explicit operator bool() const
	{
		return IsValid();
	}

	FORCEINLINE const FSoftObjectPath& ToSoftObjectPath() const
	{
		return SoftObjectPtr.ToSoftObjectPath();
	}

	FORCEINLINE FString ToString() const
	{
		return SoftObjectPtr.ToString();
	}

	FORCEINLINE FString GetLongPackageName() const
	{
		return SoftObjectPtr.GetLongPackageName();
	}

	FORCEINLINE FString GetAssetName() const
	{
		return SoftObjectPtr.GetAssetName();
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
	template <class>
	friend struct TSoftClassPtr;

	FSoftObjectPtr SoftObjectPtr;
};

static_assert(sizeof(TSoftObjectPtr<>) == sizeof(FSoftObjectPtr), "TSoftObjectPtr must be an FSoftObjectPtr");
static_assert(sizeof(TSoftClassPtr<>) == sizeof(FSoftObjectPtr), "TSoftClassPtr must be an FSoftObjectPtr");
