#pragma once

#include "CoreTypes.h"
#include "Templates/AlignmentTemplates.h"
#include "Templates/UnrealTypeTraits.h"

#include <new>
#include <type_traits>
#include <utility>

// Core template helpers (UE: Templates/UnrealTemplate.h).

/** Number of elements of a C array, as a compile-time constant (UE: UE_ARRAY_COUNT). */
template <typename T, uint32 N>
char (&UEArrayCountHelper(const T (&)[N]))[N + 1];
#define UE_ARRAY_COUNT(Array) (sizeof(UEArrayCountHelper(Array)) - 1)

/** Cast to an rvalue; static_asserts the argument is a non-const lvalue (UE: MoveTemp). */
template <typename T>
FORCEINLINE constexpr std::remove_reference_t<T>&& MoveTemp(T&& Obj)
{
	typedef std::remove_reference_t<T> CastType;
	static_assert(std::is_lvalue_reference_v<T>, "MoveTemp called on an rvalue");
	static_assert(!std::is_const_v<CastType>, "MoveTemp called on a const object");
	return static_cast<CastType&&>(Obj);
}

/** Like MoveTemp, without the checks: moves when possible (UE: MoveTempIfPossible). */
template <typename T>
FORCEINLINE constexpr std::remove_reference_t<T>&& MoveTempIfPossible(T&& Obj)
{
	return static_cast<std::remove_reference_t<T>&&>(Obj);
}

/** Perfect forwarding (UE: Forward). */
template <typename T>
FORCEINLINE constexpr T&& Forward(std::remove_reference_t<T>& Obj)
{
	return static_cast<T&&>(Obj);
}
template <typename T>
FORCEINLINE constexpr T&& Forward(std::remove_reference_t<T>&& Obj)
{
	return static_cast<T&&>(Obj);
}

/** Copy of an object as a prvalue (UE: CopyTemp). */
template <typename T>
FORCEINLINE T CopyTemp(const T& Val)
{
	return Val;
}

template <typename T>
FORCEINLINE void Swap(T& A, T& B)
{
	T Temp = MoveTemp(A);
	A = MoveTemp(B);
	B = MoveTemp(Temp);
}

/** Replaces Value with NewValue and returns the old value (UE: Exchange / std::exchange). */
template <typename T, typename U = T>
FORCEINLINE T Exchange(T& Value, U&& NewValue)
{
	T OldValue = MoveTemp(Value);
	Value = Forward<U>(NewValue);
	return OldValue;
}

/** Restores a variable's value at scope exit (UE: TGuardValue). */
template <typename RefType, typename AssignedType = RefType>
struct TGuardValue
{
	TGuardValue(RefType& ReferenceValue, const AssignedType& NewValue)
		: RefValue(ReferenceValue)
		, OldValue(ReferenceValue)
	{
		RefValue = NewValue;
	}
	~TGuardValue()
	{
		RefValue = OldValue;
	}
	TGuardValue(const TGuardValue&) = delete;
	TGuardValue& operator=(const TGuardValue&) = delete;

	const AssignedType& operator*() const
	{
		return OldValue;
	}

private:
	RefType& RefValue;
	AssignedType OldValue;
};

/** Base class that deletes copy operations (UE: FNoncopyable). */
class FNoncopyable
{
protected:
	FNoncopyable() = default;
	~FNoncopyable() = default;

public:
	FNoncopyable(const FNoncopyable&) = delete;
	FNoncopyable& operator=(const FNoncopyable&) = delete;
};

/** Data pointer of a contiguous container or C array (UE: GetData). */
template <typename T, SIZE_T N>
constexpr T* GetData(T (&Container)[N])
{
	return Container;
}
template <typename ContainerType, typename = decltype(std::declval<ContainerType&>().GetData())>
constexpr auto GetData(ContainerType&& Container) -> decltype(Container.GetData())
{
	return Container.GetData();
}

/** Element count of a contiguous container or C array (UE: GetNum). */
template <typename T, SIZE_T N>
constexpr SIZE_T GetNum(T (&)[N])
{
	return N;
}
template <typename ContainerType, typename = decltype(std::declval<ContainerType&>().Num())>
constexpr auto GetNum(ContainerType&& Container) -> decltype(Container.Num())
{
	return Container.Num();
}

/** Rvalue reference to lvalue reference (UE: TRValueToLValueReference). */
template <typename T>
struct TRValueToLValueReference
{
	typedef T Type;
};
template <typename T>
struct TRValueToLValueReference<T&&>
{
	typedef T& Type;
};
