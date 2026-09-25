#pragma once

#include "CoreTypes.h"
#include "Templates/UnrealTemplate.h"

/** Returns its argument unchanged; the default projection of the Algo functions (UE: FIdentityFunctor). */
struct FIdentityFunctor
{
	template <typename T>
	FORCEINLINE T&& operator()(T&& Val) const
	{
		return (T&&)Val;
	}
};
