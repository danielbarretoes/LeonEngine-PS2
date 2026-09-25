#pragma once

#include "CoreTypes.h"
#include "Templates/UnrealTemplate.h"

#include <type_traits>

// Invokes callables, member function pointers and member data pointers uniformly (UE: Templates/Invoke.h).

/** Class of a pointer-to-member type (UE: TMemberFunctionPtrOuter). */
template <typename T>
struct TMemberFunctionPtrOuter;
template <typename ReturnType, typename ObjectType>
struct TMemberFunctionPtrOuter<ReturnType ObjectType::*>
{
	using Type = ObjectType;
};
template <typename T>
using TMemberFunctionPtrOuter_T = typename TMemberFunctionPtrOuter<T>::Type;

namespace UE::Core::Private
{
	// An object of (a class derived from) OuterType is used directly; anything else is a pointer to dereference.
	template <typename OuterType, typename TargetType>
	FORCEINLINE auto DereferenceIfNecessary(TargetType&& Target)
		-> std::enable_if_t<std::is_base_of_v<OuterType, std::decay_t<TargetType>>, TargetType&&>
	{
		return (TargetType&&)Target;
	}

	template <typename OuterType, typename TargetType>
	FORCEINLINE auto DereferenceIfNecessary(TargetType&& Target)
		-> std::enable_if_t<!std::is_base_of_v<OuterType, std::decay_t<TargetType>>, decltype(*(TargetType&&)Target)>
	{
		return *(TargetType&&)Target;
	}
} // namespace UE::Core::Private

template <typename FuncType, typename... ArgTypes>
FORCEINLINE auto Invoke(FuncType&& Func, ArgTypes&&... Args)
	-> decltype(Forward<FuncType>(Func)(Forward<ArgTypes>(Args)...))
{
	return Forward<FuncType>(Func)(Forward<ArgTypes>(Args)...);
}

template <typename ReturnType, typename ObjType, typename TargetType>
FORCEINLINE auto Invoke(ReturnType ObjType::* MemberData, TargetType&& Target)
	-> decltype(UE::Core::Private::DereferenceIfNecessary<ObjType>(Forward<TargetType>(Target)).*MemberData)
{
	return UE::Core::Private::DereferenceIfNecessary<ObjType>(Forward<TargetType>(Target)).*MemberData;
}

template <typename PtrMemFunType, typename TargetType, typename... ArgTypes,
	typename ObjType = TMemberFunctionPtrOuter_T<PtrMemFunType>>
FORCEINLINE auto Invoke(PtrMemFunType MemberFunction, TargetType&& Target, ArgTypes&&... Args)
	-> decltype((UE::Core::Private::DereferenceIfNecessary<ObjType>(Forward<TargetType>(Target)).*MemberFunction)(
		Forward<ArgTypes>(Args)...))
{
	return (UE::Core::Private::DereferenceIfNecessary<ObjType>(Forward<TargetType>(Target)).*MemberFunction)(
		Forward<ArgTypes>(Args)...);
}
