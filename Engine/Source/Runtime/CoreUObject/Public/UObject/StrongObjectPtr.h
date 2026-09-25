#pragma once

// A UObject pointer that keeps its object alive from non-UObject code (UE: UObject/StrongObjectPtr.h).

#include "CoreMinimal.h"
#include "Templates/UniquePtr.h"
#include "UObject/GCObject.h"

#include <cstddef>
#include <type_traits>

/**
 * Owns a reference to a UObject that the garbage collector sees, like a UPROPERTY, from code that is not a UObject
 * (UE: TStrongObjectPtr). The reference lives in a small FGCObject, so the pointer can be copied and moved. As with
 * any FGCObject reference, a pending-kill object is cleared (Get() becomes null) at the next collection.
 */
template <typename ObjectType>
class TStrongObjectPtr
{
public:
	FORCEINLINE TStrongObjectPtr(std::nullptr_t = nullptr)
	{
	}

	FORCEINLINE explicit TStrongObjectPtr(ObjectType* InObject)
		: ReferenceCollector(
			  InObject ? MakeUnique<FInternalReferenceCollector>(InObject) : TUniquePtr<FInternalReferenceCollector>())
	{
	}

	FORCEINLINE TStrongObjectPtr(const TStrongObjectPtr& InOther)
		: ReferenceCollector(InOther.Get() ? MakeUnique<FInternalReferenceCollector>(InOther.Get())
										   : TUniquePtr<FInternalReferenceCollector>())
	{
	}

	FORCEINLINE TStrongObjectPtr(TStrongObjectPtr&& InOther) = default;

	template <typename OtherObjectType,
		typename = std::enable_if_t<std::is_convertible_v<OtherObjectType*, ObjectType*>>>
	FORCEINLINE TStrongObjectPtr(const TStrongObjectPtr<OtherObjectType>& InOther)
		: TStrongObjectPtr((ObjectType*)InOther.Get())
	{
	}

	FORCEINLINE TStrongObjectPtr& operator=(const TStrongObjectPtr& InOther)
	{
		if (this != &InOther)
		{
			Reset(InOther.Get());
		}
		return *this;
	}

	FORCEINLINE TStrongObjectPtr& operator=(TStrongObjectPtr&& InOther) = default;

	FORCEINLINE ObjectType& operator*() const
	{
		check(IsValid());
		return *Get();
	}

	FORCEINLINE ObjectType* operator->() const
	{
		check(IsValid());
		return Get();
	}

	FORCEINLINE bool IsValid() const
	{
		return Get() != nullptr;
	}

	FORCEINLINE explicit operator bool() const
	{
		return IsValid();
	}

	/** The object, or nullptr (UE). */
	FORCEINLINE ObjectType* Get() const
	{
		return ReferenceCollector ? ReferenceCollector->GetObject() : nullptr;
	}

	/** Points at InNewObject, or at nothing (UE). */
	FORCEINLINE void Reset(ObjectType* InNewObject = nullptr)
	{
		if (InNewObject)
		{
			if (ReferenceCollector)
			{
				ReferenceCollector->SetObject(InNewObject);
			}
			else
			{
				ReferenceCollector = MakeUnique<FInternalReferenceCollector>(InNewObject);
			}
		}
		else
		{
			ReferenceCollector.Reset();
		}
	}

	FORCEINLINE friend uint32 GetTypeHash(const TStrongObjectPtr& InStrongObjectPtr)
	{
		return GetTypeHash(InStrongObjectPtr.Get());
	}

	template <typename OtherObjectType>
	FORCEINLINE bool operator==(const TStrongObjectPtr<OtherObjectType>& InRHS) const
	{
		return Get() == InRHS.Get();
	}

	template <typename OtherObjectType>
	FORCEINLINE bool operator!=(const TStrongObjectPtr<OtherObjectType>& InRHS) const
	{
		return Get() != InRHS.Get();
	}

private:
	/** The FGCObject that reports the pointer to the collector (UE). */
	class FInternalReferenceCollector : public FGCObject
	{
	public:
		explicit FInternalReferenceCollector(ObjectType* InObject)
			: Object(InObject)
		{
		}

		FORCEINLINE ObjectType* GetObject() const
		{
			return Object;
		}

		FORCEINLINE void SetObject(ObjectType* InObject)
		{
			Object = InObject;
		}

		virtual void AddReferencedObjects(FReferenceCollector& Collector) override
		{
			Collector.AddReferencedObject(Object);
		}

		virtual FString GetReferencerName() const override
		{
			return TEXT("TStrongObjectPtr");
		}

	private:
		ObjectType* Object;
	};

	TUniquePtr<FInternalReferenceCollector> ReferenceCollector;
};
