#pragma once

#include "CoreTypes.h"
#include "HAL/PlatformMath.h"

/** Engine math helpers (UE: FMath). Integer / generic helpers for now; the float math arrives with Core math. */
struct FMath : public FPlatformMath
{
	template <class T>
	static constexpr FORCEINLINE T Clamp(const T X, const T InMin, const T InMax)
	{
		return X < InMin ? InMin : (X < InMax ? X : InMax);
	}

	template <class T>
	static constexpr FORCEINLINE T Max3(const T A, const T B, const T C)
	{
		return Max(Max(A, B), C);
	}

	template <class T>
	static constexpr FORCEINLINE T Min3(const T A, const T B, const T C)
	{
		return Min(Min(A, B), C);
	}

	template <class T>
	static constexpr FORCEINLINE T Square(const T A)
	{
		return A * A;
	}

	template <class T>
	static constexpr FORCEINLINE bool IsPowerOfTwo(T Value)
	{
		return (Value & (Value - 1)) == static_cast<T>(0);
	}

	template <class T>
	static constexpr FORCEINLINE T DivideAndRoundUp(T Dividend, T Divisor)
	{
		return (Dividend + Divisor - 1) / Divisor;
	}

	template <class T>
	static constexpr FORCEINLINE T DivideAndRoundDown(T Dividend, T Divisor)
	{
		return Dividend / Divisor;
	}
};
