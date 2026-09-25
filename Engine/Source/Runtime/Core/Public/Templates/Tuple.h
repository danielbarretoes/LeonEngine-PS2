#pragma once

#include "CoreTypes.h"
#include "Templates/IntegerSequence.h"
#include "Templates/Invoke.h"
#include "Templates/TypeHash.h"
#include "Templates/UnrealTemplate.h"

#include <type_traits>
#include <utility>

// Fixed-size heterogeneous tuple (UE: Templates/Tuple.h). A two-element tuple names its members Key and Value, and
// TPair<K, V> is TTuple<K, V>.

template <typename... Types>
struct TTuple;

namespace UE::Core::Private::Tuple
{
	enum EForwardingConstructor
	{
		ForwardingConstructor
	};

	template <typename T, uint32 Index, uint32 TupleSize>
	struct TTupleBaseElement
	{
		template <typename ArgType>
		explicit TTupleBaseElement(EForwardingConstructor, ArgType&& Arg)
			: Value(Forward<ArgType>(Arg))
		{
		}

		TTupleBaseElement()
			: Value()
		{
		}

		T Value;
	};

	template <typename T>
	struct TTupleBaseElement<T, 0, 2>
	{
		template <typename ArgType>
		explicit TTupleBaseElement(EForwardingConstructor, ArgType&& Arg)
			: Key(Forward<ArgType>(Arg))
		{
		}

		TTupleBaseElement()
			: Key()
		{
		}

		T Key;
	};

	/** Access to the element Index, deducing its type from the base class. */
	template <uint32 Index, uint32 TupleSize>
	struct TTupleElementGetterByIndex
	{
		template <typename DeducedType>
		static FORCEINLINE DeducedType& Get(TTupleBaseElement<DeducedType, Index, TupleSize>& Element)
		{
			return Element.Value;
		}
		template <typename DeducedType>
		static FORCEINLINE const DeducedType& Get(const TTupleBaseElement<DeducedType, Index, TupleSize>& Element)
		{
			return Element.Value;
		}
	};

	template <>
	struct TTupleElementGetterByIndex<0, 2>
	{
		template <typename DeducedType>
		static FORCEINLINE DeducedType& Get(TTupleBaseElement<DeducedType, 0, 2>& Element)
		{
			return Element.Key;
		}
		template <typename DeducedType>
		static FORCEINLINE const DeducedType& Get(const TTupleBaseElement<DeducedType, 0, 2>& Element)
		{
			return Element.Key;
		}
	};

	template <typename Indices, typename... Types>
	struct TTupleBase;

	template <uint32... Indices, typename... Types>
	struct TTupleBase<TIntegerSequence<uint32, Indices...>, Types...>
		: TTupleBaseElement<Types, Indices, sizeof...(Types)>...
	{
		template <typename... ArgTypes>
		explicit TTupleBase(EForwardingConstructor, ArgTypes&&... Args)
			: TTupleBaseElement<Types, Indices, sizeof...(Types)>(ForwardingConstructor, Forward<ArgTypes>(Args))...
		{
		}

		TTupleBase() = default;

		template <uint32 Index>
		FORCEINLINE decltype(auto) Get() &
		{
			static_assert(Index < sizeof...(Types), "Invalid index passed to TTuple::Get");
			return TTupleElementGetterByIndex<Index, sizeof...(Types)>::Get(*this);
		}
		template <uint32 Index>
		FORCEINLINE decltype(auto) Get() const&
		{
			static_assert(Index < sizeof...(Types), "Invalid index passed to TTuple::Get");
			return TTupleElementGetterByIndex<Index, sizeof...(Types)>::Get(*this);
		}
		template <uint32 Index>
		FORCEINLINE decltype(auto) Get() &&
		{
			static_assert(Index < sizeof...(Types), "Invalid index passed to TTuple::Get");
			return MoveTempIfPossible(TTupleElementGetterByIndex<Index, sizeof...(Types)>::Get(*this));
		}

		/** Func(Args..., Elements...) (UE: ApplyAfter). */
		template <typename FuncType, typename... ArgTypes>
		decltype(auto) ApplyAfter(FuncType&& Func, ArgTypes&&... Args) const
		{
			return ::Invoke(Func, Forward<ArgTypes>(Args)..., this->template Get<Indices>()...);
		}

		/** Func(Elements..., Args...) (UE: ApplyBefore). */
		template <typename FuncType, typename... ArgTypes>
		decltype(auto) ApplyBefore(FuncType&& Func, ArgTypes&&... Args) const
		{
			return ::Invoke(Func, this->template Get<Indices>()..., Forward<ArgTypes>(Args)...);
		}

	protected:
		template <typename OtherTupleType>
		void AssignFrom(OtherTupleType&& Other)
		{
			// Comma fold: the assignments are sequenced in element order.
			((void)(this->template Get<Indices>() = Forward<OtherTupleType>(Other).template Get<Indices>()), ...);
		}
	};

	template <typename... Types>
	uint32 HashTupleElements(const Types&... Elements)
	{
		uint32 Hash = 0;
		((void)(Hash = HashCombine(Hash, GetTypeHash(Elements))), ...);
		return Hash;
	}
} // namespace UE::Core::Private::Tuple

template <typename... Types>
struct TTuple : UE::Core::Private::Tuple::TTupleBase<TMakeIntegerSequence<uint32, sizeof...(Types)>, Types...>
{
private:
	typedef UE::Core::Private::Tuple::TTupleBase<TMakeIntegerSequence<uint32, sizeof...(Types)>, Types...> Super;

	template <typename... OtherTypes>
	friend struct TTuple;

public:
	TTuple() = default;

	template <typename... ArgTypes,
		std::enable_if_t<sizeof...(Types) == sizeof...(ArgTypes) && sizeof...(Types) != 0 &&
				std::conjunction_v<std::is_constructible<Types, ArgTypes&&>...>,
			int> = 0>
	TTuple(ArgTypes&&... Args)
		: Super(UE::Core::Private::Tuple::ForwardingConstructor, Forward<ArgTypes>(Args)...)
	{
	}

	TTuple(const TTuple&) = default;
	TTuple(TTuple&&) = default;

	/** Element-wise copy (assigns through reference elements, so Tie(A, B) = OtherTuple works). */
	TTuple& operator=(const TTuple& Other)
	{
		this->AssignFrom(Other);
		return *this;
	}
	TTuple& operator=(TTuple&& Other)
	{
		this->AssignFrom(MoveTemp(Other));
		return *this;
	}
	template <typename... OtherTypes, std::enable_if_t<sizeof...(OtherTypes) == sizeof...(Types), int> = 0>
	TTuple& operator=(const TTuple<OtherTypes...>& Other)
	{
		this->AssignFrom(Other);
		return *this;
	}
	template <typename... OtherTypes, std::enable_if_t<sizeof...(OtherTypes) == sizeof...(Types), int> = 0>
	TTuple& operator=(TTuple<OtherTypes...>&& Other)
	{
		this->AssignFrom(MoveTemp(Other));
		return *this;
	}
};

template <>
struct TTuple<>
{
	template <typename FuncType, typename... ArgTypes>
	decltype(auto) ApplyAfter(FuncType&& Func, ArgTypes&&... Args) const
	{
		return ::Invoke(Func, Forward<ArgTypes>(Args)...);
	}
	template <typename FuncType, typename... ArgTypes>
	decltype(auto) ApplyBefore(FuncType&& Func, ArgTypes&&... Args) const
	{
		return ::Invoke(Func, Forward<ArgTypes>(Args)...);
	}
};

template <typename KeyType, typename ValueType>
using TPair = TTuple<KeyType, ValueType>;

/** Number of elements of a tuple type (UE: TTupleArity). */
template <typename T>
struct TTupleArity;
template <typename... Types>
struct TTupleArity<const TTuple<Types...>> : TTupleArity<TTuple<Types...>>
{
};
template <typename... Types>
struct TTupleArity<TTuple<Types...>>
{
	enum
	{
		Value = sizeof...(Types)
	};
};

/** Type of element Index of a tuple (UE: TTupleElement). */
template <uint32 Index, typename TupleType>
struct TTupleElement;
template <typename First, typename... Rest>
struct TTupleElement<0, TTuple<First, Rest...>>
{
	using Type = First;
};
template <uint32 Index, typename First, typename... Rest>
struct TTupleElement<Index, TTuple<First, Rest...>>
{
	using Type = typename TTupleElement<Index - 1, TTuple<Rest...>>::Type;
};

template <typename T>
struct TIsTuple
{
	enum
	{
		Value = false
	};
};
template <typename... Types>
struct TIsTuple<TTuple<Types...>>
{
	enum
	{
		Value = true
	};
};

template <typename... Types>
FORCEINLINE TTuple<std::decay_t<Types>...> MakeTuple(Types&&... Args)
{
	return TTuple<std::decay_t<Types>...>(Forward<Types>(Args)...);
}

/** Tuple of references, for unpacking: Tie(A, B) = MakeTuple(1, 2.0f). */
template <typename... Types>
FORCEINLINE TTuple<Types&...> Tie(Types&... Args)
{
	return TTuple<Types&...>(Args...);
}

template <typename... LhsTypes, typename... RhsTypes>
bool operator==(const TTuple<LhsTypes...>& Lhs, const TTuple<RhsTypes...>& Rhs)
{
	static_assert(sizeof...(LhsTypes) == sizeof...(RhsTypes), "Cannot compare tuples of different sizes");
	return Lhs.ApplyAfter([&Rhs](const auto&... LhsElements)
		{ return Rhs.ApplyAfter([&](const auto&... RhsElements) { return ((LhsElements == RhsElements) && ...); }); });
}

template <typename... LhsTypes, typename... RhsTypes>
bool operator!=(const TTuple<LhsTypes...>& Lhs, const TTuple<RhsTypes...>& Rhs)
{
	return !(Lhs == Rhs);
}

namespace UE::Core::Private::Tuple
{
	template <uint32 Index, uint32 Count>
	struct TTupleLess
	{
		template <typename LhsType, typename RhsType>
		static bool Do(const LhsType& Lhs, const RhsType& Rhs)
		{
			if (Lhs.template Get<Index>() < Rhs.template Get<Index>())
			{
				return true;
			}
			if (Rhs.template Get<Index>() < Lhs.template Get<Index>())
			{
				return false;
			}
			return TTupleLess<Index + 1, Count>::Do(Lhs, Rhs);
		}
	};
	template <uint32 Count>
	struct TTupleLess<Count, Count>
	{
		template <typename LhsType, typename RhsType>
		static bool Do(const LhsType&, const RhsType&)
		{
			return false;
		}
	};
} // namespace UE::Core::Private::Tuple

/** Lexicographic ordering. */
template <typename... LhsTypes, typename... RhsTypes>
bool operator<(const TTuple<LhsTypes...>& Lhs, const TTuple<RhsTypes...>& Rhs)
{
	static_assert(sizeof...(LhsTypes) == sizeof...(RhsTypes), "Cannot compare tuples of different sizes");
	return UE::Core::Private::Tuple::TTupleLess<0, sizeof...(LhsTypes)>::Do(Lhs, Rhs);
}

template <typename... Types>
FORCEINLINE uint32 GetTypeHash(const TTuple<Types...>& Tuple)
{
	return Tuple.ApplyAfter(
		[](const auto&... Elements) { return UE::Core::Private::Tuple::HashTupleElements(Elements...); });
}

FORCEINLINE uint32 GetTypeHash(const TTuple<>&)
{
	return 0;
}

/** Calls Func with each element of the tuple(s) in order (UE: VisitTupleElements). */
template <typename FuncType, typename... Types>
FORCEINLINE void VisitTupleElements(FuncType&& Func, TTuple<Types...>& Tuple)
{
	Tuple.ApplyAfter([&Func](auto&... Elements) { (::Invoke(Func, Elements), ...); });
}

// Structured bindings: auto& [Key, Value] = Pair;
template <uint32 Index, typename... Types>
FORCEINLINE decltype(auto) get(TTuple<Types...>& Tuple)
{
	return Tuple.template Get<Index>();
}
template <uint32 Index, typename... Types>
FORCEINLINE decltype(auto) get(const TTuple<Types...>& Tuple)
{
	return Tuple.template Get<Index>();
}
template <uint32 Index, typename... Types>
FORCEINLINE decltype(auto) get(TTuple<Types...>&& Tuple)
{
	return MoveTemp(Tuple).template Get<Index>();
}

namespace std
{
	template <typename... Types>
	struct tuple_size<TTuple<Types...>> : integral_constant<size_t, sizeof...(Types)>
	{
	};

	template <size_t Index, typename... Types>
	struct tuple_element<Index, TTuple<Types...>>
	{
		using type = typename TTupleElement<uint32(Index), TTuple<Types...>>::Type;
	};
} // namespace std
