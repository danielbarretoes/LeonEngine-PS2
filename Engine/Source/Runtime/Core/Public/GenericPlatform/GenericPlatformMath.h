#pragma once

#include "HAL/Platform.h"

#if defined(_MSC_VER)
	#include <intrin.h>
#endif

/** Math helpers with platform overrides (UE: FGenericPlatformMath). FMath (Math/UnrealMathUtility.h) extends it. */
struct CORE_API FGenericPlatformMath
{
	/** Sine / cosine of an angle in 1/256 turns (Leon extension; PS2 uses a table, no libm). */
	static float Sin256(uint32 Angle256);
	static float Cos256(uint32 Angle256);

	template <class T>
	static constexpr FORCEINLINE T Abs(const T A)
	{
		return (A < static_cast<T>(0)) ? -A : A;
	}

	template <class T>
	static constexpr FORCEINLINE T Sign(const T A)
	{
		return (A > static_cast<T>(0)) ? static_cast<T>(1)
									   : ((A < static_cast<T>(0)) ? static_cast<T>(-1) : static_cast<T>(0));
	}

	template <class T>
	static constexpr FORCEINLINE T Max(const T A, const T B)
	{
		return (B < A) ? A : B;
	}

	template <class T>
	static constexpr FORCEINLINE T Min(const T A, const T B)
	{
		return (A < B) ? A : B;
	}

	/** Number of leading zero bits; 32 for zero. */
	static FORCEINLINE uint32 CountLeadingZeros(uint32 Value)
	{
		if (Value == 0)
		{
			return 32;
		}
#if defined(_MSC_VER)
		unsigned long BitIndex = 0;
		_BitScanReverse(&BitIndex, Value);
		return 31 - static_cast<uint32>(BitIndex);
#else
		return static_cast<uint32>(__builtin_clz(Value));
#endif
	}

	/** Number of leading zero bits; 64 for zero. */
	static FORCEINLINE uint64 CountLeadingZeros64(uint64 Value)
	{
		if (Value == 0)
		{
			return 64;
		}
#if defined(_MSC_VER)
		unsigned long BitIndex = 0;
		_BitScanReverse64(&BitIndex, Value);
		return 63 - static_cast<uint64>(BitIndex);
#else
		return static_cast<uint64>(__builtin_clzll(Value));
#endif
	}

	/** Number of trailing zero bits; 32 for zero. */
	static FORCEINLINE uint32 CountTrailingZeros(uint32 Value)
	{
		if (Value == 0)
		{
			return 32;
		}
#if defined(_MSC_VER)
		unsigned long BitIndex = 0;
		_BitScanForward(&BitIndex, Value);
		return static_cast<uint32>(BitIndex);
#else
		return static_cast<uint32>(__builtin_ctz(Value));
#endif
	}

	/** Number of trailing zero bits; 64 for zero. */
	static FORCEINLINE uint64 CountTrailingZeros64(uint64 Value)
	{
		if (Value == 0)
		{
			return 64;
		}
#if defined(_MSC_VER)
		unsigned long BitIndex = 0;
		_BitScanForward64(&BitIndex, Value);
		return static_cast<uint64>(BitIndex);
#else
		return static_cast<uint64>(__builtin_ctzll(Value));
#endif
	}

	/** floor(log2(Value)); 0 for zero. */
	static FORCEINLINE uint32 FloorLog2(uint32 Value)
	{
		return Value == 0 ? 0 : 31 - CountLeadingZeros(Value);
	}

	static FORCEINLINE uint64 FloorLog2_64(uint64 Value) // NOLINT(readability-identifier-naming): UE name
	{
		return Value == 0 ? 0 : 63 - CountLeadingZeros64(Value);
	}

	/** ceil(log2(Value)); 0 for zero and one. */
	static FORCEINLINE uint32 CeilLogTwo(uint32 Value)
	{
		return Value <= 1 ? 0 : 32 - CountLeadingZeros(Value - 1);
	}

	static FORCEINLINE uint64 CeilLogTwo64(uint64 Value)
	{
		return Value <= 1 ? 0 : 64 - CountLeadingZeros64(Value - 1);
	}

	/** Smallest power of two >= Value (1 for zero). */
	static FORCEINLINE uint32 RoundUpToPowerOfTwo(uint32 Value)
	{
		return 1u << CeilLogTwo(Value);
	}

	static FORCEINLINE uint64 RoundUpToPowerOfTwo64(uint64 Value)
	{
		return uint64(1) << CeilLogTwo64(Value);
	}

	/** Number of set bits. */
	static FORCEINLINE int32 CountBits(uint64 Bits)
	{
		Bits = Bits - ((Bits >> 1) & 0x5555555555555555ull);
		Bits = (Bits & 0x3333333333333333ull) + ((Bits >> 2) & 0x3333333333333333ull);
		Bits = (Bits + (Bits >> 4)) & 0x0f0f0f0f0f0f0f0full;
		return static_cast<int32>((Bits * 0x0101010101010101ull) >> 56);
	}
};
