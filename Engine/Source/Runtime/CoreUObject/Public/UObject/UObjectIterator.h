#pragma once

// Iteration over every live object of a class (UE: UObject/UObjectIterator.h).

#include "CoreMinimal.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
#include "UObject/UObjectArray.h"

/**
 * Iterates every live object in GUObjectArray, optionally of one class (UE: FObjectIterator / FUObjectArray
 * iteration). Class default objects are skipped by default (RF_ClassDefaultObject in AdditionalExclusionFlags), as
 * in UE.
 */
class FObjectIterator
{
public:
	explicit FObjectIterator(UClass* InClass = UObject::StaticClass(), bool bOnlyGCedObjects = false,
		EObjectFlags AdditionalExclusionFlags = RF_ClassDefaultObject,
		EInternalObjectFlags InInternalExclusionFlags = EInternalObjectFlags::None)
		: Class(InClass)
		, ExclusionFlags(AdditionalExclusionFlags)
		, InternalExclusionFlags(InInternalExclusionFlags | EInternalObjectFlags::Unreachable)
		, Index(-1)
		, CurrentObject(nullptr)
	{
		(void)bOnlyGCedObjects;
		Advance();
	}

	FORCEINLINE void operator++()
	{
		Advance();
	}

	FORCEINLINE explicit operator bool() const
	{
		return CurrentObject != nullptr;
	}

	FORCEINLINE bool operator!() const
	{
		return CurrentObject == nullptr;
	}

	FORCEINLINE UObject* operator*() const
	{
		return CurrentObject;
	}

	FORCEINLINE UObject* operator->() const
	{
		return CurrentObject;
	}

protected:
	void Advance()
	{
		CurrentObject = nullptr;
		const int32 Num = GUObjectArray.GetObjectArrayNum();
		while (++Index < Num)
		{
			FUObjectItem* Item = GUObjectArray.IndexToObject(Index);
			UObject* Object = Item ? (UObject*)Item->Object : nullptr;
			if (Object && !Item->HasAnyFlags(InternalExclusionFlags) && !Object->HasAnyFlags(ExclusionFlags) &&
				(Class == UObject::StaticClass() || Object->IsA(Class)))
			{
				CurrentObject = Object;
				return;
			}
		}
	}

	UClass* Class;
	EObjectFlags ExclusionFlags;
	EInternalObjectFlags InternalExclusionFlags;
	int32 Index;
	UObject* CurrentObject;
};

/** Iterates every live T (UE: TObjectIterator). */
template <class T>
class TObjectIterator : public FObjectIterator
{
public:
	explicit TObjectIterator(EObjectFlags AdditionalExclusionFlags = RF_ClassDefaultObject,
		bool bIncludeDerivedClasses = true, EInternalObjectFlags InInternalExclusionFlags = EInternalObjectFlags::None)
		: FObjectIterator(T::StaticClass(), false, AdditionalExclusionFlags, InInternalExclusionFlags)
		, bExactClass(!bIncludeDerivedClasses)
	{
		SkipInexact();
	}

	FORCEINLINE void operator++()
	{
		FObjectIterator::operator++();
		SkipInexact();
	}

	FORCEINLINE T* operator*() const
	{
		return (T*)CurrentObject;
	}

	FORCEINLINE T* operator->() const
	{
		return (T*)CurrentObject;
	}

private:
	void SkipInexact()
	{
		while (bExactClass && CurrentObject && CurrentObject->GetClass() != T::StaticClass())
		{
			FObjectIterator::operator++();
		}
	}

	bool bExactClass;
};

/** Range-for over every live T (UE: TObjectRange). */
template <class T>
class TObjectRange
{
public:
	explicit TObjectRange(EObjectFlags AdditionalExclusionFlags = RF_ClassDefaultObject,
		bool bIncludeDerivedClasses = true, EInternalObjectFlags InInternalExclusionFlags = EInternalObjectFlags::None)
		: It(AdditionalExclusionFlags, bIncludeDerivedClasses, InInternalExclusionFlags)
	{
	}

	class FIterator
	{
	public:
		explicit FIterator(const TObjectIterator<T>& InIt)
			: It(InIt)
		{
		}

		FORCEINLINE T* operator*() const
		{
			return *It;
		}

		FORCEINLINE void operator++()
		{
			++It;
		}

		/** Only compares with the end iterator. */
		FORCEINLINE bool operator!=(const FIterator&) const
		{
			return (bool)It;
		}

	private:
		TObjectIterator<T> It;
	};

	FIterator begin() const
	{
		return FIterator(It);
	}

	FIterator end() const
	{
		return FIterator(It);
	}

private:
	TObjectIterator<T> It;
};
