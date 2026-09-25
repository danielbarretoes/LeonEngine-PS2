#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Templates/MemoryOps.h"
#include "Templates/TypeCompatibleBytes.h"
#include "Templates/TypeHash.h"
#include "Templates/UnrealTemplate.h"

#include <new>

/** A value that may be unset (UE: TOptional). */
template <typename OptionalType>
struct TOptional
{
public:
	typedef OptionalType ElementType;

	TOptional()
		: bIsSet(false)
	{
	}

	TOptional(const OptionalType& InValue)
		: bIsSet(false)
	{
		Emplace(InValue);
	}

	TOptional(OptionalType&& InValue)
		: bIsSet(false)
	{
		Emplace(MoveTempIfPossible(InValue));
	}

	template <typename... ArgsType>
	explicit TOptional(EInPlace, ArgsType&&... Args)
		: bIsSet(false)
	{
		Emplace(Forward<ArgsType>(Args)...);
	}

	TOptional(const TOptional& Other)
		: bIsSet(false)
	{
		if (Other.bIsSet)
		{
			Emplace(Other.GetValue());
		}
	}

	TOptional(TOptional&& Other)
		: bIsSet(false)
	{
		if (Other.bIsSet)
		{
			Emplace(MoveTempIfPossible(Other.GetValue()));
		}
	}

	~TOptional()
	{
		Reset();
	}

	TOptional& operator=(const TOptional& Other)
	{
		if (&Other != this)
		{
			if (Other.bIsSet)
			{
				*this = Other.GetValue();
			}
			else
			{
				Reset();
			}
		}
		return *this;
	}

	TOptional& operator=(TOptional&& Other)
	{
		if (&Other != this)
		{
			if (Other.bIsSet)
			{
				*this = MoveTempIfPossible(Other.GetValue());
			}
			else
			{
				Reset();
			}
		}
		return *this;
	}

	TOptional& operator=(const OptionalType& InValue)
	{
		if (&InValue != (const OptionalType*)&Value)
		{
			if (bIsSet)
			{
				*(OptionalType*)&Value = InValue;
			}
			else
			{
				Emplace(InValue);
			}
		}
		return *this;
	}

	TOptional& operator=(OptionalType&& InValue)
	{
		if (&InValue != (const OptionalType*)&Value)
		{
			if (bIsSet)
			{
				*(OptionalType*)&Value = MoveTempIfPossible(InValue);
			}
			else
			{
				Emplace(MoveTempIfPossible(InValue));
			}
		}
		return *this;
	}

	void Reset()
	{
		if (bIsSet)
		{
			bIsSet = false;
			DestructItem((OptionalType*)&Value);
		}
	}

	/** Destroys any value and constructs one from Args; returns it. */
	template <typename... ArgsType>
	OptionalType& Emplace(ArgsType&&... Args)
	{
		Reset();
		OptionalType* Result = new (&Value) OptionalType(Forward<ArgsType>(Args)...);
		bIsSet = true;
		return *Result;
	}

	friend bool operator==(const TOptional& Lhs, const TOptional& Rhs)
	{
		if (Lhs.bIsSet != Rhs.bIsSet)
		{
			return false;
		}
		if (!Lhs.bIsSet) // both unset
		{
			return true;
		}
		return (*(const OptionalType*)&Lhs.Value) == (*(const OptionalType*)&Rhs.Value);
	}

	friend bool operator!=(const TOptional& Lhs, const TOptional& Rhs)
	{
		return !(Lhs == Rhs);
	}

	bool IsSet() const
	{
		return bIsSet;
	}

	FORCEINLINE explicit operator bool() const
	{
		return bIsSet;
	}

	/** The value (check-fails when unset). */
	const OptionalType& GetValue() const
	{
		checkf(IsSet(),
			"It is an error to call GetValue() on an unset TOptional. Please either check IsSet() or use "
			"Get(DefaultValue) instead.");
		return *(const OptionalType*)&Value;
	}
	OptionalType& GetValue()
	{
		checkf(IsSet(),
			"It is an error to call GetValue() on an unset TOptional. Please either check IsSet() or use "
			"Get(DefaultValue) instead.");
		return *(OptionalType*)&Value;
	}

	const OptionalType* operator->() const
	{
		return &GetValue();
	}
	OptionalType* operator->()
	{
		return &GetValue();
	}

	const OptionalType& operator*() const
	{
		return GetValue();
	}
	OptionalType& operator*()
	{
		return GetValue();
	}

	/** The value, or DefaultValue when unset. */
	const OptionalType& Get(const OptionalType& DefaultValue) const
	{
		return IsSet() ? *(const OptionalType*)&Value : DefaultValue;
	}

	/** Pointer to the value, or nullptr when unset. */
	OptionalType* GetPtrOrNull()
	{
		return IsSet() ? (OptionalType*)&Value : nullptr;
	}
	const OptionalType* GetPtrOrNull() const
	{
		return IsSet() ? (const OptionalType*)&Value : nullptr;
	}

private:
	TTypeCompatibleBytes<OptionalType> Value;
	bool bIsSet;
};

template <typename OptionalType>
FORCEINLINE uint32 GetTypeHash(const TOptional<OptionalType>& Optional)
{
	return Optional.IsSet() ? GetTypeHash(*Optional) : 0;
}
