#pragma once

#include "Containers/Array.h"
#include "CoreTypes.h"
#include "Delegates/DelegateInstancesImpl.h"
#include "Delegates/IDelegateInstance.h"
#include "Misc/AssertionMacros.h"
#include "Templates/SharedPointer.h"
#include "Templates/UnrealTemplate.h"
#include "Templates/UnrealTypeTraits.h"

#include <type_traits>

// Delegates (UE: Delegates/Delegate.h). A TDelegate holds at most one binding; a TMulticastDelegate holds many.
// Bindings: static functions, lambdas, raw object pointers and shared pointers, each with optional payload values
// passed after the call parameters. UObject and dynamic delegates come with CoreUObject.

template <typename FuncType>
class TDelegate;

template <typename FuncType>
class TMulticastDelegate;

/** Single-cast delegate (UE: TDelegate / TBaseDelegate). */
template <typename InRetValType, typename... ParamTypes>
class TDelegate<InRetValType(ParamTypes...)>
{
	typedef TDelegateInstanceBase<InRetValType, ParamTypes...> FInstance;

	template <typename>
	friend class TMulticastDelegate;

public:
	typedef InRetValType RetValType;
	typedef InRetValType TFuncType(ParamTypes...);

	TDelegate() = default;

	TDelegate(std::nullptr_t)
	{
	}

	TDelegate(const TDelegate& Other)
		: Instance(Other.Instance ? Other.Instance->Clone() : nullptr)
	{
	}

	TDelegate(TDelegate&& Other)
		: Instance(Other.Instance)
	{
		Other.Instance = nullptr;
	}

	~TDelegate()
	{
		Unbind();
	}

	TDelegate& operator=(const TDelegate& Other)
	{
		if (this != &Other)
		{
			FInstance* NewInstance = Other.Instance ? Other.Instance->Clone() : nullptr;
			Unbind();
			Instance = NewInstance;
		}
		return *this;
	}

	TDelegate& operator=(TDelegate&& Other)
	{
		if (this != &Other)
		{
			Unbind();
			Instance = Other.Instance;
			Other.Instance = nullptr;
		}
		return *this;
	}

	// Factories ------------------------------------------------------------------------------------------------------

	template <typename... VarTypes>
	[[nodiscard]] static TDelegate CreateStatic(
		typename TIdentity<InRetValType (*)(ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		TDelegate Result;
		Result.BindStatic(InFunc, MoveTemp(Vars)...);
		return Result;
	}

	template <typename FunctorType, typename... VarTypes>
	[[nodiscard]] static TDelegate CreateLambda(FunctorType&& InFunctor, VarTypes... Vars)
	{
		TDelegate Result;
		Result.BindLambda(Forward<FunctorType>(InFunctor), MoveTemp(Vars)...);
		return Result;
	}

	template <typename UserClass, typename... VarTypes>
	[[nodiscard]] static TDelegate CreateRaw(UserClass* InUserObject,
		typename TMemFunPtrType<false, UserClass, InRetValType(ParamTypes..., VarTypes...)>::Type InFunc,
		VarTypes... Vars)
	{
		TDelegate Result;
		Result.BindRaw(InUserObject, InFunc, MoveTemp(Vars)...);
		return Result;
	}

	template <typename UserClass, typename... VarTypes>
	[[nodiscard]] static TDelegate CreateRaw(const UserClass* InUserObject,
		typename TMemFunPtrType<true, UserClass, InRetValType(ParamTypes..., VarTypes...)>::Type InFunc,
		VarTypes... Vars)
	{
		TDelegate Result;
		Result.BindRaw(InUserObject, InFunc, MoveTemp(Vars)...);
		return Result;
	}

	template <typename UserClass, ESPMode Mode, typename... VarTypes>
	[[nodiscard]] static TDelegate CreateSP(const TSharedRef<UserClass, Mode>& InUserObjectRef,
		typename TMemFunPtrType<false, UserClass, InRetValType(ParamTypes..., VarTypes...)>::Type InFunc,
		VarTypes... Vars)
	{
		TDelegate Result;
		Result.BindSP(InUserObjectRef, InFunc, MoveTemp(Vars)...);
		return Result;
	}

	template <typename UserClass, ESPMode Mode, typename... VarTypes>
	[[nodiscard]] static TDelegate CreateSP(const TSharedPtr<UserClass, Mode>& InUserObject,
		typename TMemFunPtrType<false, UserClass, InRetValType(ParamTypes..., VarTypes...)>::Type InFunc,
		VarTypes... Vars)
	{
		TDelegate Result;
		Result.BindSP(InUserObject, InFunc, MoveTemp(Vars)...);
		return Result;
	}

	/** Binds to an object deriving from TSharedFromThis. */
	template <typename UserClass, typename... VarTypes>
	[[nodiscard]] static TDelegate CreateSP(UserClass* InUserObject,
		typename TMemFunPtrType<false, UserClass, InRetValType(ParamTypes..., VarTypes...)>::Type InFunc,
		VarTypes... Vars)
	{
		TDelegate Result;
		Result.BindSP(InUserObject, InFunc, MoveTemp(Vars)...);
		return Result;
	}

	// Binding --------------------------------------------------------------------------------------------------------

	template <typename... VarTypes>
	void BindStatic(typename TIdentity<InRetValType (*)(ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		using FCaller = UE::Core::Private::Delegates::TPayloadCaller<decltype(InFunc), VarTypes...>;
		Bind(nullptr, FCaller{InFunc, TTuple<VarTypes...>(MoveTemp(Vars)...)});
	}

	template <typename FunctorType, typename... VarTypes>
	void BindLambda(FunctorType&& InFunctor, VarTypes... Vars)
	{
		using FCaller = UE::Core::Private::Delegates::TPayloadCaller<std::decay_t<FunctorType>, VarTypes...>;
		Bind(nullptr, FCaller{Forward<FunctorType>(InFunctor), TTuple<VarTypes...>(MoveTemp(Vars)...)});
	}

	/** Binds a member function of a raw object; the object must outlive the binding (UE: BindRaw). */
	template <typename UserClass, typename... VarTypes>
	void BindRaw(UserClass* InUserObject,
		typename TMemFunPtrType<false, UserClass, InRetValType(ParamTypes..., VarTypes...)>::Type InFunc,
		VarTypes... Vars)
	{
		check(InUserObject != nullptr);
		using FCaller = UE::Core::Private::Delegates::TMethodCaller<UserClass, decltype(InFunc), VarTypes...>;
		Bind(InUserObject, FCaller{InUserObject, InFunc, TTuple<VarTypes...>(MoveTemp(Vars)...)});
	}

	template <typename UserClass, typename... VarTypes>
	void BindRaw(const UserClass* InUserObject,
		typename TMemFunPtrType<true, UserClass, InRetValType(ParamTypes..., VarTypes...)>::Type InFunc,
		VarTypes... Vars)
	{
		check(InUserObject != nullptr);
		using FCaller = UE::Core::Private::Delegates::TMethodCaller<const UserClass, decltype(InFunc), VarTypes...>;
		Bind(InUserObject, FCaller{InUserObject, InFunc, TTuple<VarTypes...>(MoveTemp(Vars)...)});
	}

	/** Binds a member function of a shared object; the binding goes inert when the object dies (UE: BindSP). */
	template <typename UserClass, ESPMode Mode, typename... VarTypes>
	void BindSP(const TSharedPtr<UserClass, Mode>& InUserObject,
		typename TMemFunPtrType<false, UserClass, InRetValType(ParamTypes..., VarTypes...)>::Type InFunc,
		VarTypes... Vars)
	{
		check(InUserObject.IsValid());
		using FCaller = UE::Core::Private::Delegates::TSPMethodCaller<decltype(InFunc), VarTypes...>;
		using FSPInstance = TSPDelegateInstance<UserClass, Mode, FCaller, InRetValType, ParamTypes...>;
		Unbind();
		Instance = new FSPInstance(FDelegateHandle(FDelegateHandle::GenerateNewHandle), InUserObject,
			FCaller{InFunc, TTuple<VarTypes...>(MoveTemp(Vars)...)});
	}

	template <typename UserClass, ESPMode Mode, typename... VarTypes>
	void BindSP(const TSharedRef<UserClass, Mode>& InUserObjectRef,
		typename TMemFunPtrType<false, UserClass, InRetValType(ParamTypes..., VarTypes...)>::Type InFunc,
		VarTypes... Vars)
	{
		BindSP(TSharedPtr<UserClass, Mode>(InUserObjectRef), InFunc, MoveTemp(Vars)...);
	}

	/** Binds to an object deriving from TSharedFromThis (UE: BindSP(this, ...)). */
	template <typename UserClass, typename... VarTypes>
	void BindSP(UserClass* InUserObject,
		typename TMemFunPtrType<false, UserClass, InRetValType(ParamTypes..., VarTypes...)>::Type InFunc,
		VarTypes... Vars)
	{
		BindSP(StaticCastSharedRef<UserClass>(InUserObject->AsShared()), InFunc, MoveTemp(Vars)...);
	}

	// State ----------------------------------------------------------------------------------------------------------

	/** True when bound and the bound object (if any) is alive (UE: IsBound). */
	FORCEINLINE bool IsBound() const
	{
		return Instance != nullptr && Instance->IsSafeToExecute();
	}

	FORCEINLINE bool IsBoundToObject(const void* InUserObject) const
	{
		return Instance != nullptr && Instance->HasSameObject(InUserObject);
	}

	FORCEINLINE void Unbind()
	{
		delete Instance;
		Instance = nullptr;
	}

	FORCEINLINE FDelegateHandle GetHandle() const
	{
		return Instance ? Instance->GetHandle() : FDelegateHandle();
	}

	/** Calls the binding; it must be bound (UE: Execute). */
	FORCEINLINE RetValType Execute(ParamTypes... Params) const
	{
		checkf(IsBound(), "Executing an unbound delegate");
		return Instance->Execute(Forward<ParamTypes>(Params)...);
	}

	/** Calls the binding when bound; only for void delegates (UE: ExecuteIfBound). */
	template <typename DummyRetValType = InRetValType, std::enable_if_t<std::is_void_v<DummyRetValType>, int> = 0>
	FORCEINLINE bool ExecuteIfBound(ParamTypes... Params) const
	{
		if (IsBound())
		{
			Instance->Execute(Forward<ParamTypes>(Params)...);
			return true;
		}
		return false;
	}

private:
	template <typename CallerType>
	void Bind(const void* UserObject, CallerType&& Caller)
	{
		using FFunctorInstance = TFunctorDelegateInstance<std::decay_t<CallerType>, InRetValType, ParamTypes...>;
		Unbind();
		Instance = new FFunctorInstance(
			FDelegateHandle(FDelegateHandle::GenerateNewHandle), UserObject, Forward<CallerType>(Caller));
	}

	FInstance* Instance = nullptr;
};

/** Multi-cast delegate; Broadcast calls the bindings in reverse order of addition, like UE4 (UE: TMulticastDelegate).
 */
template <typename... ParamTypes>
class TMulticastDelegate<void(ParamTypes...)>
{
public:
	typedef TDelegate<void(ParamTypes...)> FDelegate;

	TMulticastDelegate() = default;

	/** Adds a binding; returns its handle for Remove. */
	FDelegateHandle Add(FDelegate&& InNewDelegate)
	{
		FDelegateHandle Result;
		if (InNewDelegate.Instance)
		{
			Result = InNewDelegate.GetHandle();
			InvocationList.Add(MoveTemp(InNewDelegate));
		}
		return Result;
	}

	FDelegateHandle Add(const FDelegate& InNewDelegate)
	{
		return Add(FDelegate(InNewDelegate));
	}

	/** Adds the binding unless one with the same handle exists. */
	FDelegateHandle AddUnique(const FDelegate& InNewDelegate)
	{
		const FDelegateHandle Handle = InNewDelegate.GetHandle();
		for (const FDelegate& Existing : InvocationList)
		{
			if (Existing.GetHandle() == Handle)
			{
				return Handle;
			}
		}
		return Add(InNewDelegate);
	}

	template <typename... VarTypes>
	FDelegateHandle AddStatic(typename TIdentity<void (*)(ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		return Add(FDelegate::CreateStatic(InFunc, MoveTemp(Vars)...));
	}

	template <typename FunctorType, typename... VarTypes>
	FDelegateHandle AddLambda(FunctorType&& InFunctor, VarTypes... Vars)
	{
		return Add(FDelegate::CreateLambda(Forward<FunctorType>(InFunctor), MoveTemp(Vars)...));
	}

	template <typename UserClass, typename... VarTypes>
	FDelegateHandle AddRaw(UserClass* InUserObject,
		typename TMemFunPtrType<false, UserClass, void(ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		return Add(FDelegate::CreateRaw(InUserObject, InFunc, MoveTemp(Vars)...));
	}

	template <typename UserClass, typename... VarTypes>
	FDelegateHandle AddRaw(const UserClass* InUserObject,
		typename TMemFunPtrType<true, UserClass, void(ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		return Add(FDelegate::CreateRaw(InUserObject, InFunc, MoveTemp(Vars)...));
	}

	template <typename UserClass, ESPMode Mode, typename... VarTypes>
	FDelegateHandle AddSP(const TSharedRef<UserClass, Mode>& InUserObjectRef,
		typename TMemFunPtrType<false, UserClass, void(ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		return Add(FDelegate::CreateSP(InUserObjectRef, InFunc, MoveTemp(Vars)...));
	}

	template <typename UserClass, ESPMode Mode, typename... VarTypes>
	FDelegateHandle AddSP(const TSharedPtr<UserClass, Mode>& InUserObject,
		typename TMemFunPtrType<false, UserClass, void(ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		return Add(FDelegate::CreateSP(InUserObject, InFunc, MoveTemp(Vars)...));
	}

	template <typename UserClass, typename... VarTypes>
	FDelegateHandle AddSP(UserClass* InUserObject,
		typename TMemFunPtrType<false, UserClass, void(ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		return Add(FDelegate::CreateSP(InUserObject, InFunc, MoveTemp(Vars)...));
	}

	/** Removes the binding with the handle; true when found. Safe during Broadcast. */
	bool Remove(FDelegateHandle Handle)
	{
		for (int32 Index = 0; Index < InvocationList.Num(); ++Index)
		{
			if (InvocationList[Index].GetHandle() == Handle)
			{
				RemoveAtIndex(Index);
				return true;
			}
		}
		return false;
	}

	/** Removes every binding to a member function of InUserObject; returns how many. Safe during Broadcast. */
	int32 RemoveAll(const void* InUserObject)
	{
		int32 Removed = 0;
		for (int32 Index = InvocationList.Num() - 1; Index >= 0; --Index)
		{
			if (InvocationList[Index].IsBoundToObject(InUserObject))
			{
				RemoveAtIndex(Index);
				++Removed;
			}
		}
		return Removed;
	}

	/** Removes every binding. */
	void Clear()
	{
		if (BroadcastDepth > 0)
		{
			for (FDelegate& Delegate : InvocationList)
			{
				Delegate.Unbind();
			}
			bNeedsCompaction = true;
		}
		else
		{
			InvocationList.Empty();
		}
	}

	/** True when any binding can execute. */
	bool IsBound() const
	{
		for (const FDelegate& Delegate : InvocationList)
		{
			if (Delegate.IsBound())
			{
				return true;
			}
		}
		return false;
	}

	bool IsBoundToObject(const void* InUserObject) const
	{
		for (const FDelegate& Delegate : InvocationList)
		{
			if (Delegate.IsBoundToObject(InUserObject))
			{
				return true;
			}
		}
		return false;
	}

	/** Calls every binding (latest first); bindings added during the broadcast wait for the next one. */
	void Broadcast(ParamTypes... Params) const
	{
		TMulticastDelegate* Self = const_cast<TMulticastDelegate*>(this);
		++Self->BroadcastDepth;
		for (int32 Index = InvocationList.Num() - 1; Index >= 0; --Index)
		{
			const FDelegate& Delegate = InvocationList[Index];
			if (Delegate.IsBound())
			{
				Delegate.Instance->Execute(Params...);
			}
		}
		if (--Self->BroadcastDepth == 0 && Self->bNeedsCompaction)
		{
			Self->InvocationList.RemoveAll([](const FDelegate& Delegate) { return Delegate.Instance == nullptr; });
			Self->bNeedsCompaction = false;
		}
	}

private:
	void RemoveAtIndex(int32 Index)
	{
		if (BroadcastDepth > 0)
		{
			// Keep indices stable while broadcasting; compact afterwards.
			InvocationList[Index].Unbind();
			bNeedsCompaction = true;
		}
		else
		{
			InvocationList.RemoveAt(Index);
		}
	}

	TArray<FDelegate> InvocationList;
	int32 BroadcastDepth = 0;
	bool bNeedsCompaction = false;
};

// Declaration macros (UE: Delegates/DelegateCombinations.h).

#define DECLARE_DELEGATE(DelegateName) typedef TDelegate<void()> DelegateName;
#define DECLARE_DELEGATE_OneParam(DelegateName, Param1Type) typedef TDelegate<void(Param1Type)> DelegateName;
#define DECLARE_DELEGATE_TwoParams(DelegateName, Param1Type, Param2Type)                                               \
	typedef TDelegate<void(Param1Type, Param2Type)> DelegateName;
#define DECLARE_DELEGATE_ThreeParams(DelegateName, Param1Type, Param2Type, Param3Type)                                 \
	typedef TDelegate<void(Param1Type, Param2Type, Param3Type)> DelegateName;
#define DECLARE_DELEGATE_FourParams(DelegateName, Param1Type, Param2Type, Param3Type, Param4Type)                      \
	typedef TDelegate<void(Param1Type, Param2Type, Param3Type, Param4Type)> DelegateName;
#define DECLARE_DELEGATE_FiveParams(DelegateName, Param1Type, Param2Type, Param3Type, Param4Type, Param5Type)          \
	typedef TDelegate<void(Param1Type, Param2Type, Param3Type, Param4Type, Param5Type)> DelegateName;
#define DECLARE_DELEGATE_SixParams(                                                                                    \
	DelegateName, Param1Type, Param2Type, Param3Type, Param4Type, Param5Type, Param6Type)                              \
	typedef TDelegate<void(Param1Type, Param2Type, Param3Type, Param4Type, Param5Type, Param6Type)> DelegateName;

#define DECLARE_DELEGATE_RetVal(RetValType, DelegateName) typedef TDelegate<RetValType()> DelegateName;
#define DECLARE_DELEGATE_RetVal_OneParam(RetValType, DelegateName, Param1Type)                                         \
	typedef TDelegate<RetValType(Param1Type)> DelegateName;
#define DECLARE_DELEGATE_RetVal_TwoParams(RetValType, DelegateName, Param1Type, Param2Type)                            \
	typedef TDelegate<RetValType(Param1Type, Param2Type)> DelegateName;
#define DECLARE_DELEGATE_RetVal_ThreeParams(RetValType, DelegateName, Param1Type, Param2Type, Param3Type)              \
	typedef TDelegate<RetValType(Param1Type, Param2Type, Param3Type)> DelegateName;
#define DECLARE_DELEGATE_RetVal_FourParams(RetValType, DelegateName, Param1Type, Param2Type, Param3Type, Param4Type)   \
	typedef TDelegate<RetValType(Param1Type, Param2Type, Param3Type, Param4Type)> DelegateName;

#define DECLARE_MULTICAST_DELEGATE(DelegateName) typedef TMulticastDelegate<void()> DelegateName;
#define DECLARE_MULTICAST_DELEGATE_OneParam(DelegateName, Param1Type)                                                  \
	typedef TMulticastDelegate<void(Param1Type)> DelegateName;
#define DECLARE_MULTICAST_DELEGATE_TwoParams(DelegateName, Param1Type, Param2Type)                                     \
	typedef TMulticastDelegate<void(Param1Type, Param2Type)> DelegateName;
#define DECLARE_MULTICAST_DELEGATE_ThreeParams(DelegateName, Param1Type, Param2Type, Param3Type)                       \
	typedef TMulticastDelegate<void(Param1Type, Param2Type, Param3Type)> DelegateName;
#define DECLARE_MULTICAST_DELEGATE_FourParams(DelegateName, Param1Type, Param2Type, Param3Type, Param4Type)            \
	typedef TMulticastDelegate<void(Param1Type, Param2Type, Param3Type, Param4Type)> DelegateName;
#define DECLARE_MULTICAST_DELEGATE_FiveParams(                                                                         \
	DelegateName, Param1Type, Param2Type, Param3Type, Param4Type, Param5Type)                                          \
	typedef TMulticastDelegate<void(Param1Type, Param2Type, Param3Type, Param4Type, Param5Type)> DelegateName;

/** A multicast delegate only its owning class may Broadcast (UE: DECLARE_EVENT; Leon: a plain multicast). */
#define DECLARE_EVENT(OwningType, EventName) typedef TMulticastDelegate<void()> EventName;
#define DECLARE_EVENT_OneParam(OwningType, EventName, Param1Type)                                                      \
	typedef TMulticastDelegate<void(Param1Type)> EventName;
#define DECLARE_EVENT_TwoParams(OwningType, EventName, Param1Type, Param2Type)                                         \
	typedef TMulticastDelegate<void(Param1Type, Param2Type)> EventName;
