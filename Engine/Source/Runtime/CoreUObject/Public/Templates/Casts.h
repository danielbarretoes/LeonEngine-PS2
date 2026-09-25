#pragma once

// Checked downcasts between UObject classes, without RTTI (UE: Templates/Casts.h).

#include "CoreMinimal.h"
#include "UObject/Class.h"

#include <type_traits>

namespace UE::CoreUObject::Private
{
	/** True when a class type declares its own cast bit (the Cast fast path). */
	template <typename To>
	FORCEINLINE bool IsAByCastFlags(const UObject* Src)
	{
		if constexpr (To::StaticClassCastFlags() != CASTCLASS_None)
		{
			return Src->GetClass()->HasAnyCastFlag(To::StaticClassCastFlags());
		}
		else
		{
			return Src->IsA(To::StaticClass());
		}
	}
} // namespace UE::CoreUObject::Private

/**
 * Src as a To, or nullptr when Src is null or not a To (UE: Cast). Classes with a cast bit (UClass, UFunction,
 * UScriptStruct, ...) test ClassCastFlags; the others walk the super class chain.
 */
template <typename To, typename From>
FORCEINLINE To* Cast(From* Src)
{
	static_assert(std::is_base_of_v<UObject, From> && std::is_base_of_v<UObject, To>,
		"Cast only converts between UObject classes; use CastField for properties");
	return Src && UE::CoreUObject::Private::IsAByCastFlags<To>(Src) ? (To*)Src : nullptr;
}

template <typename To, typename From>
FORCEINLINE const To* Cast(const From* Src)
{
	static_assert(std::is_base_of_v<UObject, From> && std::is_base_of_v<UObject, To>,
		"Cast only converts between UObject classes; use CastField for properties");
	return Src && UE::CoreUObject::Private::IsAByCastFlags<To>(Src) ? (const To*)Src : nullptr;
}

/** How CastChecked treats a null Src (UE: ECastCheckedType). */
namespace ECastCheckedType
{
	enum Type
	{
		/** A null Src is an error. */
		NullChecked,
		/** A null Src returns nullptr. */
		NullAllowed
	};
} // namespace ECastCheckedType

namespace UE::CoreUObject::Private
{
	COREUOBJECT_API void CastCheckedFailed(const UObject* Src, const UClass* ToClass);
} // namespace UE::CoreUObject::Private

/** Cast that must succeed: a failure is a fatal error naming both classes (UE: CastChecked). */
template <typename To, typename From>
FORCEINLINE To* CastChecked(From* Src, ECastCheckedType::Type CheckType = ECastCheckedType::NullChecked)
{
	To* Result = Cast<To>(Src);
	if (!Result && (Src || CheckType == ECastCheckedType::NullChecked))
	{
		UE::CoreUObject::Private::CastCheckedFailed(Src, To::StaticClass());
	}
	return Result;
}

template <typename To, typename From>
FORCEINLINE const To* CastChecked(const From* Src, ECastCheckedType::Type CheckType = ECastCheckedType::NullChecked)
{
	const To* Result = Cast<To>(Src);
	if (!Result && (Src || CheckType == ECastCheckedType::NullChecked))
	{
		UE::CoreUObject::Private::CastCheckedFailed(Src, To::StaticClass());
	}
	return Result;
}

/** Src when its class is exactly To (not a subclass), else nullptr (UE: ExactCast). */
template <typename To>
FORCEINLINE To* ExactCast(UObject* Src)
{
	return Src && Src->GetClass() == To::StaticClass() ? (To*)Src : nullptr;
}

template <typename To>
FORCEINLINE const To* ExactCast(const UObject* Src)
{
	return Src && Src->GetClass() == To::StaticClass() ? (const To*)Src : nullptr;
}
