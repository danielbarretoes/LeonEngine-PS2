#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Templates/TypeHash.h"
#include "Templates/UnrealTemplate.h"

#include <cstddef>
#include <type_traits>

/** Deletes with delete / delete[] (UE: TDefaultDelete). */
template <typename T>
struct TDefaultDelete
{
	TDefaultDelete() = default;
	TDefaultDelete(const TDefaultDelete&) = default;
	TDefaultDelete& operator=(const TDefaultDelete&) = default;
	~TDefaultDelete() = default;

	template <typename U, std::enable_if_t<std::is_convertible_v<U*, T*>, int> = 0>
	TDefaultDelete(const TDefaultDelete<U>&)
	{
	}

	void operator()(T* Ptr) const
	{
		static_assert(sizeof(T) > 0, "Cannot delete an incomplete type");
		delete Ptr;
	}
};

template <typename T>
struct TDefaultDelete<T[]>
{
	TDefaultDelete() = default;

	template <typename U, std::enable_if_t<std::is_convertible_v<U (*)[], T (*)[]>, int> = 0>
	TDefaultDelete(const TDefaultDelete<U[]>&)
	{
	}

	template <typename U>
	void operator()(U* Ptr) const
	{
		delete[] Ptr;
	}
};

/** Single-owner smart pointer (UE: TUniquePtr). */
template <typename T, typename Deleter = TDefaultDelete<T>>
class TUniquePtr : private Deleter
{
	template <typename OtherT, typename OtherDeleter>
	friend class TUniquePtr;

public:
	using ElementType = T;

	FORCEINLINE TUniquePtr()
		: Deleter()
		, Ptr(nullptr)
	{
	}

	explicit FORCEINLINE TUniquePtr(T* InPtr)
		: Deleter()
		, Ptr(InPtr)
	{
	}

	template <typename InDeleter>
	FORCEINLINE TUniquePtr(T* InPtr, InDeleter&& InDeleterValue)
		: Deleter(Forward<InDeleter>(InDeleterValue))
		, Ptr(InPtr)
	{
	}

	FORCEINLINE TUniquePtr(std::nullptr_t)
		: Deleter()
		, Ptr(nullptr)
	{
	}

	FORCEINLINE TUniquePtr(TUniquePtr&& Other)
		: Deleter(MoveTemp(Other.GetDeleter()))
		, Ptr(Other.Ptr)
	{
		Other.Ptr = nullptr;
	}

	template <typename OtherT, typename OtherDeleter,
		std::enable_if_t<!std::is_array_v<OtherT> && std::is_convertible_v<OtherT*, T*>, int> = 0>
	FORCEINLINE TUniquePtr(TUniquePtr<OtherT, OtherDeleter>&& Other)
		: Deleter(MoveTemp(Other.GetDeleter()))
		, Ptr(Other.Ptr)
	{
		Other.Ptr = nullptr;
	}

	TUniquePtr(const TUniquePtr&) = delete;
	TUniquePtr& operator=(const TUniquePtr&) = delete;

	FORCEINLINE TUniquePtr& operator=(TUniquePtr&& Other)
	{
		if (this != &Other)
		{
			// We delete last, because we don't want odd side effects if the destructor of T relies on the state of
			// this or Other.
			T* OldPtr = Ptr;
			Ptr = Other.Ptr;
			Other.Ptr = nullptr;
			GetDeleter() = MoveTemp(Other.GetDeleter());
			if (OldPtr)
			{
				GetDeleter()(OldPtr);
			}
		}
		return *this;
	}

	template <typename OtherT, typename OtherDeleter,
		std::enable_if_t<!std::is_array_v<OtherT> && std::is_convertible_v<OtherT*, T*>, int> = 0>
	FORCEINLINE TUniquePtr& operator=(TUniquePtr<OtherT, OtherDeleter>&& Other)
	{
		T* OldPtr = Ptr;
		Ptr = Other.Ptr;
		Other.Ptr = nullptr;
		GetDeleter() = MoveTemp(Other.GetDeleter());
		if (OldPtr)
		{
			GetDeleter()(OldPtr);
		}
		return *this;
	}

	FORCEINLINE TUniquePtr& operator=(std::nullptr_t)
	{
		Reset();
		return *this;
	}

	FORCEINLINE ~TUniquePtr()
	{
		if (Ptr)
		{
			GetDeleter()(Ptr);
		}
	}

	FORCEINLINE bool IsValid() const
	{
		return Ptr != nullptr;
	}

	FORCEINLINE explicit operator bool() const
	{
		return IsValid();
	}

	FORCEINLINE bool operator!() const
	{
		return !IsValid();
	}

	FORCEINLINE T* operator->() const
	{
		return Ptr;
	}

	FORCEINLINE T& operator*() const
	{
		return *Ptr;
	}

	FORCEINLINE T* Get() const
	{
		return Ptr;
	}

	/** Gives up ownership without deleting. */
	FORCEINLINE T* Release()
	{
		T* Result = Ptr;
		Ptr = nullptr;
		return Result;
	}

	/** Deletes the owned object and takes InPtr. */
	FORCEINLINE void Reset(T* InPtr = nullptr)
	{
		if (Ptr != InPtr)
		{
			// We delete last, because we don't want odd side effects if the destructor of T relies on the state of
			// this.
			T* OldPtr = Ptr;
			Ptr = InPtr;
			if (OldPtr)
			{
				GetDeleter()(OldPtr);
			}
		}
	}

	FORCEINLINE Deleter& GetDeleter()
	{
		return static_cast<Deleter&>(*this);
	}
	FORCEINLINE const Deleter& GetDeleter() const
	{
		return static_cast<const Deleter&>(*this);
	}

private:
	T* Ptr;
};

/** Array specialization: operator[] instead of -> (UE). */
template <typename T, typename Deleter>
class TUniquePtr<T[], Deleter> : private Deleter
{
public:
	using ElementType = T;

	FORCEINLINE TUniquePtr()
		: Deleter()
		, Ptr(nullptr)
	{
	}

	template <typename U, std::enable_if_t<std::is_convertible_v<U (*)[], T (*)[]>, int> = 0>
	explicit FORCEINLINE TUniquePtr(U* InPtr)
		: Deleter()
		, Ptr(InPtr)
	{
	}

	FORCEINLINE TUniquePtr(std::nullptr_t)
		: Deleter()
		, Ptr(nullptr)
	{
	}

	FORCEINLINE TUniquePtr(TUniquePtr&& Other)
		: Deleter(MoveTemp(Other.GetDeleter()))
		, Ptr(Other.Ptr)
	{
		Other.Ptr = nullptr;
	}

	TUniquePtr(const TUniquePtr&) = delete;
	TUniquePtr& operator=(const TUniquePtr&) = delete;

	FORCEINLINE TUniquePtr& operator=(TUniquePtr&& Other)
	{
		if (this != &Other)
		{
			T* OldPtr = Ptr;
			Ptr = Other.Ptr;
			Other.Ptr = nullptr;
			GetDeleter() = MoveTemp(Other.GetDeleter());
			if (OldPtr)
			{
				GetDeleter()(OldPtr);
			}
		}
		return *this;
	}

	FORCEINLINE TUniquePtr& operator=(std::nullptr_t)
	{
		Reset();
		return *this;
	}

	FORCEINLINE ~TUniquePtr()
	{
		if (Ptr)
		{
			GetDeleter()(Ptr);
		}
	}

	FORCEINLINE bool IsValid() const
	{
		return Ptr != nullptr;
	}

	FORCEINLINE explicit operator bool() const
	{
		return IsValid();
	}

	FORCEINLINE bool operator!() const
	{
		return !IsValid();
	}

	FORCEINLINE T& operator[](SIZE_T Index) const
	{
		return Ptr[Index];
	}

	FORCEINLINE T* Get() const
	{
		return Ptr;
	}

	FORCEINLINE T* Release()
	{
		T* Result = Ptr;
		Ptr = nullptr;
		return Result;
	}

	FORCEINLINE void Reset(T* InPtr = nullptr)
	{
		if (Ptr != InPtr)
		{
			T* OldPtr = Ptr;
			Ptr = InPtr;
			if (OldPtr)
			{
				GetDeleter()(OldPtr);
			}
		}
	}

	FORCEINLINE Deleter& GetDeleter()
	{
		return static_cast<Deleter&>(*this);
	}
	FORCEINLINE const Deleter& GetDeleter() const
	{
		return static_cast<const Deleter&>(*this);
	}

private:
	T* Ptr;
};

template <typename LhsT, typename RhsT>
FORCEINLINE bool operator==(const TUniquePtr<LhsT>& Lhs, const TUniquePtr<RhsT>& Rhs)
{
	return Lhs.Get() == Rhs.Get();
}
template <typename LhsT, typename RhsT>
FORCEINLINE bool operator!=(const TUniquePtr<LhsT>& Lhs, const TUniquePtr<RhsT>& Rhs)
{
	return Lhs.Get() != Rhs.Get();
}
template <typename T>
FORCEINLINE bool operator==(const TUniquePtr<T>& Lhs, std::nullptr_t)
{
	return !Lhs.IsValid();
}
template <typename T>
FORCEINLINE bool operator==(std::nullptr_t, const TUniquePtr<T>& Rhs)
{
	return !Rhs.IsValid();
}
template <typename T>
FORCEINLINE bool operator!=(const TUniquePtr<T>& Lhs, std::nullptr_t)
{
	return Lhs.IsValid();
}
template <typename T>
FORCEINLINE bool operator!=(std::nullptr_t, const TUniquePtr<T>& Rhs)
{
	return Rhs.IsValid();
}

template <typename T>
FORCEINLINE uint32 GetTypeHash(const TUniquePtr<T>& Ptr)
{
	return GetTypeHash(Ptr.Get());
}

/** Allocates a T with new and wraps it (UE: MakeUnique). */
template <typename T, typename... TArgs, std::enable_if_t<!std::is_array_v<T>, int> = 0>
FORCEINLINE TUniquePtr<T> MakeUnique(TArgs&&... Args)
{
	return TUniquePtr<T>(new T(Forward<TArgs>(Args)...));
}

/** Allocates a value-initialised array (UE: MakeUnique<T[]>). */
template <typename T, std::enable_if_t<std::is_array_v<T> && std::extent_v<T> == 0, int> = 0>
FORCEINLINE TUniquePtr<T> MakeUnique(SIZE_T Size)
{
	typedef std::remove_extent_t<T> ElementType;
	return TUniquePtr<T>(new ElementType[Size]());
}
