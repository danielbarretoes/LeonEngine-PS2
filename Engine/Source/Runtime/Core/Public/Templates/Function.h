#pragma once

#include "CoreTypes.h"
#include "HAL/UnrealMemory.h"
#include "Misc/AssertionMacros.h"
#include "Templates/Invoke.h"
#include "Templates/UnrealTemplate.h"

#include <cstddef>
#include <new>
#include <type_traits>

// Type-erased callables (UE: Templates/Function.h):
//   TFunctionRef<Sig>    non-owning reference to a callable (cheapest; the callable must outlive it)
//   TFunction<Sig>       owning, copyable
//   TUniqueFunction<Sig> owning, move-only
// Callables up to TFunctionInlineSize bytes live inside the object; larger ones are heap-allocated with FMemory.

template <typename FuncType>
class TFunctionRef;
template <typename FuncType>
class TFunction;
template <typename FuncType>
class TUniqueFunction;

namespace UE::Core::Private::Function
{
	/** Inline storage for small callables (four pointers). */
	constexpr SIZE_T TFunctionInlineSize = 4 * sizeof(void*);
	constexpr SIZE_T TFunctionInlineAlignment = alignof(std::max_align_t);

	template <typename T>
	struct TIsTFunctionType
	{
		enum
		{
			Value = false
		};
	};
	template <typename Sig>
	struct TIsTFunctionType<TFunctionRef<Sig>>
	{
		enum
		{
			Value = true
		};
	};
	template <typename Sig>
	struct TIsTFunctionType<TFunction<Sig>>
	{
		enum
		{
			Value = true
		};
	};
	template <typename Sig>
	struct TIsTFunctionType<TUniqueFunction<Sig>>
	{
		enum
		{
			Value = true
		};
	};

	/** Null function pointers / member pointers / TFunctions count as unset (UE: IsBound). */
	template <typename T>
	FORCEINLINE bool IsBound(const T& Func)
	{
		if constexpr (std::is_pointer_v<T> || std::is_member_pointer_v<T> || TIsTFunctionType<T>::Value)
		{
			return !!Func;
		}
		else
		{
			return true;
		}
	}

	/** Operations on the stored callable of one type. */
	template <typename Ret, typename... ParamTypes>
	struct TFunctionOps
	{
		Ret (*Call)(void* Callable, ParamTypes&&... Params);
		void (*Destroy)(void* Callable);
		void (*CopyConstruct)(void* Dest, const void* Source); // nullptr for move-only storage
		void (*MoveConstruct)(void* Dest, void* Source);
	};

	template <typename FunctorType, typename Ret, typename... ParamTypes>
	struct TFunctionOpsFor
	{
		static Ret Call(void* Callable, ParamTypes&&... Params)
		{
			if constexpr (std::is_void_v<Ret>)
			{
				::Invoke(*static_cast<FunctorType*>(Callable), Forward<ParamTypes>(Params)...);
			}
			else
			{
				return ::Invoke(*static_cast<FunctorType*>(Callable), Forward<ParamTypes>(Params)...);
			}
		}

		static void Destroy(void* Callable)
		{
			static_cast<FunctorType*>(Callable)->~FunctorType();
		}

		static void CopyConstruct(void* Dest, const void* Source)
		{
			new (Dest) FunctorType(*static_cast<const FunctorType*>(Source));
		}

		static void MoveConstruct(void* Dest, void* Source)
		{
			new (Dest) FunctorType(MoveTemp(*static_cast<FunctorType*>(Source)));
		}

		static const TFunctionOps<Ret, ParamTypes...>* Get(bool bCopyable)
		{
			static const TFunctionOps<Ret, ParamTypes...> CopyableOps = {
				&Call, &Destroy, &CopyConstructIfPossible, &MoveConstruct};
			static const TFunctionOps<Ret, ParamTypes...> UniqueOps = {&Call, &Destroy, nullptr, &MoveConstruct};
			return bCopyable ? &CopyableOps : &UniqueOps;
		}

		static void CopyConstructIfPossible(void* Dest, const void* Source)
		{
			if constexpr (std::is_copy_constructible_v<FunctorType>)
			{
				CopyConstruct(Dest, Source);
			}
			else
			{
				checkNoEntry();
			}
		}
	};

	/**
	 * Owning storage shared by TFunction / TUniqueFunction: small callables inline, larger ones on the heap.
	 * Heap-stored callables keep their pointer in the inline buffer.
	 */
	template <typename Ret, typename... ParamTypes>
	class TFunctionStorage
	{
	public:
		using FOps = TFunctionOps<Ret, ParamTypes...>;

		TFunctionStorage() = default;

		template <typename FunctorType>
		void Bind(FunctorType&& Functor, bool bCopyable)
		{
			using DecayedType = std::decay_t<FunctorType>;
			Ops = TFunctionOpsFor<DecayedType, Ret, ParamTypes...>::Get(bCopyable);
			bHeap = !FitsInline<DecayedType>();
			new (AllocateFor(sizeof(DecayedType), alignof(DecayedType))) DecayedType(Forward<FunctorType>(Functor));
		}

		void CopyFrom(const TFunctionStorage& Other)
		{
			if (Other.Ops)
			{
				checkf(Other.Ops->CopyConstruct != nullptr, "Copying a move-only function");
				Ops = Other.Ops;
				bHeap = Other.bHeap;
				if (bHeap)
				{
					HeapSize = Other.HeapSize;
					HeapAlignment = Other.HeapAlignment;
				}
				Ops->CopyConstruct(AllocateFor(Other.HeapSize, Other.HeapAlignment), Other.GetCallable());
			}
		}

		void MoveFrom(TFunctionStorage& Other)
		{
			if (!Other.Ops)
			{
				return;
			}
			Ops = Other.Ops;
			bHeap = Other.bHeap;
			if (bHeap)
			{
				// Steal the heap block.
				*reinterpret_cast<void**>(Inline) = *reinterpret_cast<void**>(Other.Inline);
				HeapSize = Other.HeapSize;
				HeapAlignment = Other.HeapAlignment;
			}
			else
			{
				Ops->MoveConstruct(Inline, Other.Inline);
				Ops->Destroy(Other.Inline);
			}
			Other.Ops = nullptr;
			Other.bHeap = false;
		}

		void Unbind()
		{
			if (Ops)
			{
				void* Callable = GetCallable();
				Ops->Destroy(Callable);
				if (bHeap)
				{
					FMemory::Free(Callable);
				}
				Ops = nullptr;
				bHeap = false;
			}
		}

		FORCEINLINE bool IsSet() const
		{
			return Ops != nullptr;
		}

		FORCEINLINE Ret Call(ParamTypes&&... Params) const
		{
			checkf(Ops, "Attempting to call an unbound TFunction!");
			return Ops->Call(GetCallable(), Forward<ParamTypes>(Params)...);
		}

	private:
		template <typename T>
		static constexpr bool FitsInline()
		{
			return sizeof(T) <= TFunctionInlineSize && alignof(T) <= TFunctionInlineAlignment;
		}

		void* AllocateFor(SIZE_T Size, SIZE_T Alignment)
		{
			if (!bHeap)
			{
				return Inline;
			}
			HeapSize = Size;
			HeapAlignment = Alignment;
			void* Block = FMemory::Malloc(Size, uint32(Alignment));
			*reinterpret_cast<void**>(Inline) = Block;
			return Block;
		}

		FORCEINLINE void* GetCallable() const
		{
			return bHeap ? *reinterpret_cast<void* const*>(Inline) : const_cast<uint8*>(Inline);
		}

		alignas(TFunctionInlineAlignment) uint8 Inline[TFunctionInlineSize];
		const FOps* Ops = nullptr;
		SIZE_T HeapSize = 0;
		SIZE_T HeapAlignment = 0;
		bool bHeap = false;
	};

	template <typename T, typename Ret, typename... ParamTypes>
	constexpr bool TIsInvocableAs = std::is_invocable_r_v<Ret, T&, ParamTypes...>;
} // namespace UE::Core::Private::Function

/** Non-owning reference to a callable; pass it by value to functions that call it before returning (UE). */
template <typename Ret, typename... ParamTypes>
class TFunctionRef<Ret(ParamTypes...)>
{
public:
	template <typename FunctorType,
		std::enable_if_t<!std::is_same_v<std::decay_t<FunctorType>, TFunctionRef> &&
				UE::Core::Private::Function::TIsInvocableAs<std::remove_reference_t<FunctorType>, Ret, ParamTypes...>,
			int> = 0>
	TFunctionRef(FunctorType&& InFunc)
		: Callable((void*)&InFunc)
		, Caller(&CallImpl<std::remove_reference_t<FunctorType>>)
	{
		checkf(UE::Core::Private::Function::IsBound(InFunc), "Cannot bind a null/unbound callable to a TFunctionRef");
	}

	TFunctionRef(const TFunctionRef&) = default;
	TFunctionRef& operator=(const TFunctionRef&) = delete;

	FORCEINLINE Ret operator()(ParamTypes... Params) const
	{
		return Caller(Callable, Forward<ParamTypes>(Params)...);
	}

private:
	template <typename FunctorType>
	static Ret CallImpl(void* Obj, ParamTypes&&... Params)
	{
		if constexpr (std::is_void_v<Ret>)
		{
			::Invoke(*static_cast<FunctorType*>(Obj), Forward<ParamTypes>(Params)...);
		}
		else
		{
			return ::Invoke(*static_cast<FunctorType*>(Obj), Forward<ParamTypes>(Params)...);
		}
	}

	void* Callable;
	Ret (*Caller)(void*, ParamTypes&&...);
};

/** Owning, copyable callable (UE: TFunction). Empty until assigned; calling an empty TFunction is a fatal error. */
template <typename Ret, typename... ParamTypes>
class TFunction<Ret(ParamTypes...)>
{
	template <typename>
	friend class TUniqueFunction;

public:
	TFunction(std::nullptr_t = nullptr)
	{
	}

	template <typename FunctorType,
		std::enable_if_t<!std::is_same_v<std::decay_t<FunctorType>, TFunction> &&
				UE::Core::Private::Function::TIsInvocableAs<std::decay_t<FunctorType>, Ret, ParamTypes...>,
			int> = 0>
	TFunction(FunctorType&& InFunc)
	{
		static_assert(std::is_copy_constructible_v<std::decay_t<FunctorType>>,
			"TFunction needs a copyable callable (use TUniqueFunction)");
		if (UE::Core::Private::Function::IsBound(InFunc))
		{
			Storage.Bind(Forward<FunctorType>(InFunc), true);
		}
	}

	TFunction(const TFunction& Other)
	{
		Storage.CopyFrom(Other.Storage);
	}

	TFunction(TFunction&& Other)
	{
		Storage.MoveFrom(Other.Storage);
	}

	~TFunction()
	{
		Storage.Unbind();
	}

	TFunction& operator=(const TFunction& Other)
	{
		if (this != &Other)
		{
			TFunction Temp(Other);
			*this = MoveTemp(Temp);
		}
		return *this;
	}

	TFunction& operator=(TFunction&& Other)
	{
		if (this != &Other)
		{
			Storage.Unbind();
			Storage.MoveFrom(Other.Storage);
		}
		return *this;
	}

	TFunction& operator=(std::nullptr_t)
	{
		Storage.Unbind();
		return *this;
	}

	template <typename FunctorType,
		std::enable_if_t<!std::is_same_v<std::decay_t<FunctorType>, TFunction> &&
				UE::Core::Private::Function::TIsInvocableAs<std::decay_t<FunctorType>, Ret, ParamTypes...>,
			int> = 0>
	TFunction& operator=(FunctorType&& InFunc)
	{
		return *this = TFunction(Forward<FunctorType>(InFunc));
	}

	FORCEINLINE explicit operator bool() const
	{
		return Storage.IsSet();
	}

	FORCEINLINE bool IsSet() const
	{
		return Storage.IsSet();
	}

	void Reset()
	{
		Storage.Unbind();
	}

	FORCEINLINE Ret operator()(ParamTypes... Params) const
	{
		return Storage.Call(Forward<ParamTypes>(Params)...);
	}

	/** Checks that the function is set before calling (UE: CheckCallable). */
	FORCEINLINE void CheckCallable() const
	{
		checkf(IsSet(), "Attempting to call an unbound TFunction!");
	}

private:
	mutable UE::Core::Private::Function::TFunctionStorage<Ret, ParamTypes...> Storage;
};

/** Owning, move-only callable (UE: TUniqueFunction); accepts move-only lambdas. */
template <typename Ret, typename... ParamTypes>
class TUniqueFunction<Ret(ParamTypes...)>
{
public:
	TUniqueFunction(std::nullptr_t = nullptr)
	{
	}

	template <typename FunctorType,
		std::enable_if_t<!std::is_same_v<std::decay_t<FunctorType>, TUniqueFunction> &&
				!std::is_same_v<std::decay_t<FunctorType>, TFunction<Ret(ParamTypes...)>> &&
				UE::Core::Private::Function::TIsInvocableAs<std::decay_t<FunctorType>, Ret, ParamTypes...>,
			int> = 0>
	TUniqueFunction(FunctorType&& InFunc)
	{
		if (UE::Core::Private::Function::IsBound(InFunc))
		{
			Storage.Bind(Forward<FunctorType>(InFunc), false);
		}
	}

	/** Takes over a TFunction's callable. */
	TUniqueFunction(TFunction<Ret(ParamTypes...)>&& Other)
	{
		Storage.MoveFrom(Other.Storage);
	}

	/** Copies a TFunction's callable. */
	TUniqueFunction(const TFunction<Ret(ParamTypes...)>& Other)
	{
		Storage.CopyFrom(Other.Storage);
	}

	TUniqueFunction(TUniqueFunction&& Other)
	{
		Storage.MoveFrom(Other.Storage);
	}

	TUniqueFunction(const TUniqueFunction&) = delete;
	TUniqueFunction& operator=(const TUniqueFunction&) = delete;

	~TUniqueFunction()
	{
		Storage.Unbind();
	}

	TUniqueFunction& operator=(TUniqueFunction&& Other)
	{
		if (this != &Other)
		{
			Storage.Unbind();
			Storage.MoveFrom(Other.Storage);
		}
		return *this;
	}

	TUniqueFunction& operator=(std::nullptr_t)
	{
		Storage.Unbind();
		return *this;
	}

	template <typename FunctorType,
		std::enable_if_t<!std::is_same_v<std::decay_t<FunctorType>, TUniqueFunction> &&
				UE::Core::Private::Function::TIsInvocableAs<std::decay_t<FunctorType>, Ret, ParamTypes...>,
			int> = 0>
	TUniqueFunction& operator=(FunctorType&& InFunc)
	{
		return *this = TUniqueFunction(Forward<FunctorType>(InFunc));
	}

	FORCEINLINE explicit operator bool() const
	{
		return Storage.IsSet();
	}

	FORCEINLINE bool IsSet() const
	{
		return Storage.IsSet();
	}

	void Reset()
	{
		Storage.Unbind();
	}

	FORCEINLINE Ret operator()(ParamTypes... Params) const
	{
		return Storage.Call(Forward<ParamTypes>(Params)...);
	}

	FORCEINLINE void CheckCallable() const
	{
		checkf(IsSet(), "Attempting to call an unbound TUniqueFunction!");
	}

private:
	mutable UE::Core::Private::Function::TFunctionStorage<Ret, ParamTypes...> Storage;
};

template <typename FuncType>
FORCEINLINE bool operator==(std::nullptr_t, const TFunction<FuncType>& Func)
{
	return !Func;
}
template <typename FuncType>
FORCEINLINE bool operator==(const TFunction<FuncType>& Func, std::nullptr_t)
{
	return !Func;
}
template <typename FuncType>
FORCEINLINE bool operator!=(std::nullptr_t, const TFunction<FuncType>& Func)
{
	return (bool)Func;
}
template <typename FuncType>
FORCEINLINE bool operator!=(const TFunction<FuncType>& Func, std::nullptr_t)
{
	return (bool)Func;
}
