#pragma once

#include "CoreTypes.h"
#include "Delegates/IDelegateInstance.h"
#include "Misc/AssertionMacros.h"
#include "Templates/Invoke.h"
#include "Templates/SharedPointer.h"
#include "Templates/Tuple.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/WeakObjectPtrTemplatesFwd.h"

#include <type_traits>

/** Member function pointer type of a signature (UE: TMemFunPtrType). */
template <bool bConst, typename Class, typename FuncType>
struct TMemFunPtrType;

template <typename Class, typename RetType, typename... ArgTypes>
struct TMemFunPtrType<false, Class, RetType(ArgTypes...)>
{
	typedef RetType (Class::*Type)(ArgTypes...);
};

template <typename Class, typename RetType, typename... ArgTypes>
struct TMemFunPtrType<true, Class, RetType(ArgTypes...)>
{
	typedef RetType (Class::*Type)(ArgTypes...) const;
};

/** Executable binding of a TDelegate signature. */
template <typename RetValType, typename... ParamTypes>
class TDelegateInstanceBase : public IDelegateInstance
{
public:
	explicit TDelegateInstanceBase(FDelegateHandle InHandle)
		: Handle(InHandle)
	{
	}

	virtual RetValType Execute(ParamTypes... Params) const = 0;

	/** A copy with the same handle (copying a delegate keeps its identity, like UE). */
	virtual TDelegateInstanceBase* Clone() const = 0;

	virtual FDelegateHandle GetHandle() const override
	{
		return Handle;
	}

private:
	FDelegateHandle Handle;
};

/**
 * Binding to a callable that already carries its object and payload: static functions, lambdas and raw object
 * pointers. UserObject only identifies the binding (IsBoundToObject / RemoveAll); it is not checked for lifetime.
 */
template <typename FunctorType, typename RetValType, typename... ParamTypes>
class TFunctorDelegateInstance final : public TDelegateInstanceBase<RetValType, ParamTypes...>
{
	typedef TDelegateInstanceBase<RetValType, ParamTypes...> Super;

public:
	template <typename InFunctorType>
	TFunctorDelegateInstance(FDelegateHandle InHandle, const void* InUserObject, InFunctorType&& InFunctor)
		: Super(InHandle)
		, UserObject(InUserObject)
		, Functor(Forward<InFunctorType>(InFunctor))
	{
	}

	virtual RetValType Execute(ParamTypes... Params) const override
	{
		return ::Invoke(Functor, Forward<ParamTypes>(Params)...);
	}

	virtual Super* Clone() const override
	{
		return new TFunctorDelegateInstance(*this);
	}

	virtual bool IsSafeToExecute() const override
	{
		return true;
	}

	virtual bool HasSameObject(const void* InUserObject) const override
	{
		return UserObject != nullptr && UserObject == InUserObject;
	}

	virtual const void* GetObjectForTimerManager() const override
	{
		return UserObject;
	}

private:
	const void* UserObject;
	mutable FunctorType Functor;
};

/** Binding to a member function of an object owned by a TSharedPtr; skipped once the object is gone (UE: BindSP). */
template <typename UserClass, ESPMode Mode, typename FunctorType, typename RetValType, typename... ParamTypes>
class TSPDelegateInstance final : public TDelegateInstanceBase<RetValType, ParamTypes...>
{
	typedef TDelegateInstanceBase<RetValType, ParamTypes...> Super;

public:
	template <typename InFunctorType>
	TSPDelegateInstance(
		FDelegateHandle InHandle, const TSharedPtr<UserClass, Mode>& InUserObject, InFunctorType&& InFunctor)
		: Super(InHandle)
		, UserObject(InUserObject)
		, RawUserObject(InUserObject.Get())
		, Functor(Forward<InFunctorType>(InFunctor))
	{
	}

	virtual RetValType Execute(ParamTypes... Params) const override
	{
		// Keep the object alive during the call.
		TSharedPtr<UserClass, Mode> Pinned = UserObject.Pin();
		checkf(Pinned.IsValid(), "Executing a delegate whose shared object was destroyed");
		return ::Invoke(Functor, Pinned.Get(), Forward<ParamTypes>(Params)...);
	}

	virtual Super* Clone() const override
	{
		return new TSPDelegateInstance(*this);
	}

	virtual bool IsSafeToExecute() const override
	{
		return UserObject.IsValid();
	}

	virtual bool HasSameObject(const void* InUserObject) const override
	{
		return RawUserObject == InUserObject;
	}

	virtual const void* GetObjectForTimerManager() const override
	{
		return RawUserObject;
	}

private:
	TWeakPtr<UserClass, Mode> UserObject;
	const void* RawUserObject;
	mutable FunctorType Functor;
};

/**
 * Binding to a member function of a UObject, held through a TWeakObjectPtr: not safe to execute (IsBound() false,
 * skipped by Broadcast) once the object is garbage collected or pending kill (UE: TBaseUObjectMethodDelegateInstance).
 * Core only names TWeakObjectPtr; the code that binds includes CoreUObject's UObject/WeakObjectPtrTemplates.h.
 */
template <typename UserClass, typename FunctorType, typename RetValType, typename... ParamTypes>
class TUObjectDelegateInstance final : public TDelegateInstanceBase<RetValType, ParamTypes...>
{
	typedef TDelegateInstanceBase<RetValType, ParamTypes...> Super;

public:
	template <typename InFunctorType>
	TUObjectDelegateInstance(FDelegateHandle InHandle, UserClass* InUserObject, InFunctorType&& InFunctor)
		: Super(InHandle)
		, UserObject(InUserObject)
		, Functor(Forward<InFunctorType>(InFunctor))
	{
	}

	virtual RetValType Execute(ParamTypes... Params) const override
	{
		UserClass* Object = UserObject.Get();
		checkf(Object != nullptr, "Executing a delegate whose UObject was destroyed");
		return ::Invoke(Functor, Object, Forward<ParamTypes>(Params)...);
	}

	virtual Super* Clone() const override
	{
		return new TUObjectDelegateInstance(*this);
	}

	virtual bool IsSafeToExecute() const override
	{
		return UserObject.Get() != nullptr;
	}

	virtual bool HasSameObject(const void* InUserObject) const override
	{
		return InUserObject != nullptr && (const void*)UserObject.Get() == InUserObject;
	}

	virtual const void* GetObjectForTimerManager() const override
	{
		return UserObject.Get();
	}

private:
	TWeakObjectPtr<UserClass> UserObject;
	mutable FunctorType Functor;
};

namespace UE::Core::Private::Delegates
{
	/** Calls Func(Params..., Payload...) (UE: payload variables follow the delegate parameters). */
	template <typename FuncType, typename... VarTypes>
	struct TPayloadCaller
	{
		FuncType Func;
		TTuple<VarTypes...> Payload;

		template <typename... ParamTypes>
		decltype(auto) operator()(ParamTypes&&... Params)
		{
			return Payload.ApplyAfter(Func, Forward<ParamTypes>(Params)...);
		}
	};

	/** Calls (Object->*Method)(Params..., Payload...). */
	template <typename UserClass, typename MethodType, typename... VarTypes>
	struct TMethodCaller
	{
		UserClass* Object;
		MethodType Method;
		TTuple<VarTypes...> Payload;

		template <typename... ParamTypes>
		decltype(auto) operator()(ParamTypes&&... Params)
		{
			return Payload.ApplyAfter(Method, Object, Forward<ParamTypes>(Params)...);
		}
	};

	/** Calls (Object->*Method)(Params..., Payload...) on an object passed at call time (shared-pointer and UObject
	 * bindings). */
	template <typename MethodType, typename... VarTypes>
	struct TSPMethodCaller
	{
		MethodType Method;
		TTuple<VarTypes...> Payload;

		template <typename UserClass, typename... ParamTypes>
		decltype(auto) operator()(UserClass* Object, ParamTypes&&... Params)
		{
			return Payload.ApplyAfter(Method, Object, Forward<ParamTypes>(Params)...);
		}
	};
} // namespace UE::Core::Private::Delegates
