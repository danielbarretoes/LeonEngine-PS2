#pragma once

#include "CoreTypes.h"

#include <type_traits>

// Type traits used by the containers (UE: Templates/UnrealTypeTraits.h and the small Is*.h headers).
// Each trait exposes a `Value` enum; the standard library does the work.

template <typename... Types>
struct TAnd;
template <>
struct TAnd<>
{
	enum
	{
		Value = true
	};
};
template <typename LHS, typename... RHS>
struct TAnd<LHS, RHS...>
{
	enum
	{
		Value = LHS::Value && TAnd<RHS...>::Value
	};
};

template <typename... Types>
struct TOr;
template <>
struct TOr<>
{
	enum
	{
		Value = false
	};
};
template <typename LHS, typename... RHS>
struct TOr<LHS, RHS...>
{
	enum
	{
		Value = LHS::Value || TOr<RHS...>::Value
	};
};

template <typename Type>
struct TNot
{
	enum
	{
		Value = !Type::Value
	};
};

template <bool Predicate, typename Result = void>
using TEnableIf = std::enable_if<Predicate, Result>;

template <bool Predicate, typename TrueClass, typename FalseClass>
struct TChooseClass
{
	typedef std::conditional_t<Predicate, TrueClass, FalseClass> Result;
};

template <typename T>
struct TIdentity
{
	typedef T Type;
};

template <typename T>
struct TRemoveReference
{
	typedef std::remove_reference_t<T> Type;
};

template <typename T>
struct TRemoveCV
{
	typedef std::remove_cv_t<T> Type;
};

template <typename T>
struct TDecay
{
	typedef std::decay_t<T> Type;
};

template <typename A, typename B>
struct TIsSame
{
	enum
	{
		Value = std::is_same_v < A,
		B >
	};
};

template <typename From, typename To>
struct TIsConvertible
{
	static constexpr bool Value = std::is_convertible_v<From, To>;
};

/** True when DerivedType is BaseType or derives from it (UE argument order: derived first). */
template <typename DerivedType, typename BaseType>
struct TIsDerivedFrom
{
	static constexpr bool Value = std::is_base_of_v<BaseType, DerivedType>;
};

template <typename T>
struct TIsArithmetic
{
	enum
	{
		Value = std::is_arithmetic_v<T>
	};
};

template <typename T>
struct TIsIntegral
{
	enum
	{
		Value = std::is_integral_v<T>
	};
};

template <typename T>
struct TIsFloatingPoint
{
	enum
	{
		Value = std::is_floating_point_v<T>
	};
};

template <typename T>
struct TIsSigned
{
	enum
	{
		Value = std::is_signed_v<T>
	};
};

template <typename T>
struct TIsPointer
{
	enum
	{
		Value = std::is_pointer_v<T>
	};
};

template <typename T>
struct TIsEnum
{
	enum
	{
		Value = std::is_enum_v<T>
	};
};

template <typename T>
struct TIsFundamentalType
{
	enum
	{
		Value = std::is_fundamental_v<T>
	};
};

template <typename T>
struct TIsPODType
{
	enum
	{
		Value = std::is_trivial_v<T> && std::is_standard_layout_v<T>
	};
};

template <typename T>
struct TIsTriviallyDestructible
{
	enum
	{
		Value = std::is_trivially_destructible_v<T>
	};
};

template <typename T>
struct TIsTriviallyCopyConstructible
{
	enum
	{
		Value = std::is_trivially_copy_constructible_v<T>
	};
};

template <typename T>
struct TIsTriviallyCopyAssignable
{
	enum
	{
		Value = std::is_trivially_copy_assignable_v<T>
	};
};

template <typename T>
struct TIsTrivial
{
	enum
	{
		Value = std::is_trivial_v<T>
	};
};

/** Types whose zero bit pattern is their default value; containers memset them (UE: TIsZeroConstructType). */
template <typename T>
struct TIsZeroConstructType
{
	enum
	{
		Value = TOr < TIsEnum<T>,
		TIsArithmetic<T>,
		TIsPointer < T >> ::Value
	};
};

/** A Dest can be built from a Source with memcpy (UE: TIsBitwiseConstructible). */
template <typename DestinationElementType, typename SourceElementType>
struct TIsBitwiseConstructible
{
	enum
	{
		Value = std::is_same_v < std::remove_cv_t<DestinationElementType>,
		std::remove_cv_t < SourceElementType >> &&std::is_trivially_copy_constructible_v<DestinationElementType>
	};
};

/** Types that are compared with memcmp by CompareItems (UE: TTypeTraits::IsBytewiseComparable). */
template <typename T>
struct TIsBytewiseComparable
{
	enum
	{
		Value = TOr < TIsEnum<T>,
		TIsIntegral<T>,
		TIsPointer < T >> ::Value
	};
};

/** Parameter type for passing T by value when cheap, else by const reference (UE: TCallTraits::ParamType). */
template <typename T>
struct TCallTraits
{
	typedef std::conditional_t<(std::is_arithmetic_v<T> || std::is_pointer_v<T> || std::is_enum_v<T>), const T,
		const T&>
		ParamType;
	typedef const T& ConstReference;
	typedef T& Reference;
	typedef const T* ConstPointerType;
};

/** Containers with contiguous storage that GetData / GetNum understand (UE: TIsContiguousContainer). */
template <typename T>
struct TIsContiguousContainer
{
	enum
	{
		Value = false
	};
};
template <typename T>
struct TIsContiguousContainer<T&> : TIsContiguousContainer<T>
{
};
template <typename T>
struct TIsContiguousContainer<const T> : TIsContiguousContainer<T>
{
};
template <typename T, SIZE_T N>
struct TIsContiguousContainer<T[N]>
{
	enum
	{
		Value = true
	};
};

/** Number of elements in a TCHAR literal type, and similar compile-time helpers. */
template <typename T>
struct TIsCharType
{
	enum
	{
		Value = std::is_same_v < std::remove_cv_t<T>,
		ANSICHAR > || std::is_same_v < std::remove_cv_t<T>,
		WIDECHAR >
	};
};
