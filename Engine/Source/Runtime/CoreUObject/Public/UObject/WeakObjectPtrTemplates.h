#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplatesFwd.h"

#include <cstddef>
#include <type_traits>

/**
 * A typed FWeakObjectPtr (UE: TWeakObjectPtr). Same layout, so an FWeakObjectProperty reflects it. It does not keep
 * the object alive: Get() returns nullptr once the object is garbage collected or pending kill. The template
 * parameters' defaults are in Core's UObject/WeakObjectPtrTemplatesFwd.h, so Core code (UObject delegates) can name it.
 */
template <class T, class TWeakObjectPtrBase>
struct TWeakObjectPtr
{
public:
	TWeakObjectPtr() = default;

	FORCEINLINE TWeakObjectPtr(std::nullptr_t)
	{
	}

	FORCEINLINE TWeakObjectPtr(const T* Object)
		: WeakPtr((const UObject*)Object)
	{
	}

	template <class OtherT, typename = std::enable_if_t<std::is_convertible_v<OtherT*, T*>>>
	FORCEINLINE TWeakObjectPtr(const TWeakObjectPtr<OtherT, TWeakObjectPtrBase>& Other)
		: WeakPtr(Other.WeakPtr)
	{
	}

	FORCEINLINE TWeakObjectPtr& operator=(const T* Object)
	{
		WeakPtr = (const UObject*)Object;
		return *this;
	}

	template <class OtherT, typename = std::enable_if_t<std::is_convertible_v<OtherT*, T*>>>
	FORCEINLINE TWeakObjectPtr& operator=(const TWeakObjectPtr<OtherT, TWeakObjectPtrBase>& Other)
	{
		WeakPtr = Other.WeakPtr;
		return *this;
	}

	FORCEINLINE void Reset()
	{
		WeakPtr.Reset();
	}

	/** The object, or nullptr once it no longer exists or is pending kill (UE). */
	FORCEINLINE T* Get() const
	{
		return (T*)WeakPtr.Get();
	}

	/** The object, also when it is pending kill with bEvenIfPendingKill (UE). */
	FORCEINLINE T* Get(bool bEvenIfPendingKill) const
	{
		return (T*)WeakPtr.Get(bEvenIfPendingKill);
	}

	FORCEINLINE T* operator->() const
	{
		return Get();
	}

	FORCEINLINE T& operator*() const
	{
		return *Get();
	}

	/** True while the object exists and (unless bEvenIfPendingKill) is not pending kill (UE). */
	FORCEINLINE bool IsValid(bool bEvenIfPendingKill = false, bool bThreadsafeTest = false) const
	{
		return WeakPtr.IsValid(bEvenIfPendingKill, bThreadsafeTest);
	}

	/** True when the pointer was set to an object that no longer exists (UE). */
	FORCEINLINE bool IsStale(bool bIncludingIfPendingKill = true, bool bThreadsafeTest = false) const
	{
		return WeakPtr.IsStale(bIncludingIfPendingKill, bThreadsafeTest);
	}

	/** True when the pointer was never set, or was reset (UE). */
	FORCEINLINE bool IsExplicitlyNull() const
	{
		return WeakPtr.IsExplicitlyNull();
	}

	FORCEINLINE bool HasSameIndexAndSerialNumber(const TWeakObjectPtr& Other) const
	{
		return WeakPtr.HasSameIndexAndSerialNumber(Other.WeakPtr);
	}

	FORCEINLINE explicit operator bool() const
	{
		return WeakPtr.IsValid();
	}

	/** Equal when both point at the same object, or both are invalid (UE). */
	template <class OtherT>
	FORCEINLINE bool operator==(const TWeakObjectPtr<OtherT, TWeakObjectPtrBase>& Other) const
	{
		return WeakPtr == Other.WeakPtr;
	}

	template <class OtherT>
	FORCEINLINE bool operator!=(const TWeakObjectPtr<OtherT, TWeakObjectPtrBase>& Other) const
	{
		return WeakPtr != Other.WeakPtr;
	}

	/** Equal to the raw pointer it resolves to (UE). */
	FORCEINLINE bool operator==(const T* Other) const
	{
		return Get() == Other;
	}

	FORCEINLINE bool operator!=(const T* Other) const
	{
		return Get() != Other;
	}

	FORCEINLINE bool operator==(std::nullptr_t) const
	{
		return !IsValid();
	}

	FORCEINLINE bool operator!=(std::nullptr_t) const
	{
		return IsValid();
	}

	FORCEINLINE friend uint32 GetTypeHash(const TWeakObjectPtr& WeakObjectPtr)
	{
		return GetTypeHash(WeakObjectPtr.WeakPtr);
	}

private:
	template <class, class>
	friend struct TWeakObjectPtr;

	TWeakObjectPtrBase WeakPtr;
};

static_assert(sizeof(TWeakObjectPtr<>) == sizeof(FWeakObjectPtr), "TWeakObjectPtr must be an FWeakObjectPtr");
