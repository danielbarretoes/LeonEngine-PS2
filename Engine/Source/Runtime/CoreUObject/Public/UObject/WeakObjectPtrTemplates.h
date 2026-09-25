#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"

/** A typed FWeakObjectPtr (UE: TWeakObjectPtr). Same layout, so an FWeakObjectProperty reflects it. */
template <class T = UObject>
struct TWeakObjectPtr
{
public:
	TWeakObjectPtr() = default;

	FORCEINLINE TWeakObjectPtr(const T* Object)
		: WeakPtr((const UObject*)Object)
	{
	}

	template <class OtherT, typename = std::enable_if_t<std::is_convertible_v<OtherT*, T*>>>
	FORCEINLINE TWeakObjectPtr(const TWeakObjectPtr<OtherT>& Other)
		: WeakPtr(Other.WeakPtr)
	{
	}

	FORCEINLINE TWeakObjectPtr& operator=(const T* Object)
	{
		WeakPtr = (const UObject*)Object;
		return *this;
	}

	FORCEINLINE void Reset()
	{
		WeakPtr.Reset();
	}

	/** The object, or nullptr once it no longer exists (UE). */
	FORCEINLINE T* Get() const
	{
		return (T*)WeakPtr.Get();
	}

	FORCEINLINE T* operator->() const
	{
		return Get();
	}

	FORCEINLINE T& operator*() const
	{
		return *Get();
	}

	FORCEINLINE bool IsValid() const
	{
		return WeakPtr.IsValid();
	}

	FORCEINLINE bool IsStale() const
	{
		return WeakPtr.IsStale();
	}

	FORCEINLINE explicit operator bool() const
	{
		return WeakPtr.IsValid();
	}

	FORCEINLINE bool operator==(const TWeakObjectPtr& Other) const
	{
		return WeakPtr == Other.WeakPtr;
	}

	FORCEINLINE bool operator!=(const TWeakObjectPtr& Other) const
	{
		return WeakPtr != Other.WeakPtr;
	}

	FORCEINLINE friend uint32 GetTypeHash(const TWeakObjectPtr& WeakObjectPtr)
	{
		return GetTypeHash(WeakObjectPtr.WeakPtr);
	}

private:
	template <class>
	friend struct TWeakObjectPtr;

	FWeakObjectPtr WeakPtr;
};

static_assert(sizeof(TWeakObjectPtr<>) == sizeof(FWeakObjectPtr), "TWeakObjectPtr must be an FWeakObjectPtr");
