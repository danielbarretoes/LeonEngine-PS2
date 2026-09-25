#pragma once

// A UClass* restricted to T and its subclasses (UE: Templates/SubclassOf.h).

#include "CoreMinimal.h"
#include "UObject/Class.h"

/**
 * A class that is T or derives from T (UE: TSubclassOf). Stored as a plain UClass*, so an FClassProperty can reflect
 * it; reading it (Get, *, ->) returns nullptr when the stored class is not a T.
 */
template <class TClass>
class TSubclassOf
{
	template <class TClassA>
	friend class TSubclassOf;

public:
	FORCEINLINE TSubclassOf()
		: Class(nullptr)
	{
	}

	FORCEINLINE TSubclassOf(UClass* From)
		: Class(From)
	{
	}

	/** From another TSubclassOf whose type derives from TClass. */
	template <class TClassA, typename = std::enable_if_t<std::is_base_of_v<TClass, TClassA>>>
	FORCEINLINE TSubclassOf(const TSubclassOf<TClassA>& From)
		: Class(*From)
	{
	}

	template <class TClassA, typename = std::enable_if_t<std::is_base_of_v<TClass, TClassA>>>
	FORCEINLINE TSubclassOf& operator=(const TSubclassOf<TClassA>& From)
	{
		Class = *From;
		return *this;
	}

	FORCEINLINE TSubclassOf& operator=(UClass* From)
	{
		Class = From;
		return *this;
	}

	/** The class, or nullptr when it is not a TClass (UE). */
	FORCEINLINE UClass* operator*() const
	{
		if (!Class || !Class->IsChildOf(TClass::StaticClass()))
		{
			return nullptr;
		}
		return Class;
	}

	FORCEINLINE UClass* Get() const
	{
		return **this;
	}

	FORCEINLINE UClass* operator->() const
	{
		return **this;
	}

	FORCEINLINE operator UClass*() const
	{
		return **this;
	}

	/** The class default object as a TClass, or nullptr (UE). */
	FORCEINLINE TClass* GetDefaultObject() const
	{
		UClass* Result = **this;
		return Result ? (TClass*)Result->GetDefaultObject() : nullptr;
	}

	FORCEINLINE friend uint32 GetTypeHash(const TSubclassOf& SubclassOf)
	{
		return GetTypeHash(SubclassOf.Class);
	}

private:
	UClass* Class;
};

static_assert(sizeof(TSubclassOf<UObject>) == sizeof(UClass*), "TSubclassOf must be a UClass* for FClassProperty");
