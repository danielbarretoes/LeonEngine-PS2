#pragma once

#include "HAL/Platform.h"

#include <cmath>
#include <cstring>

#if defined(_MSC_VER)
	#include <intrin.h>
#endif

/**
 * Math helpers with platform overrides (UE: FGenericPlatformMath). FMath (Math/UnrealMathUtility.h) extends it.
 * Float only: every function takes and returns float (the EE has no double-precision FPU).
 */
struct CORE_API FGenericPlatformMath
{
	/** Sine / cosine of an angle in 1/256 turns (Leon extension; PS2 uses a table, no libm). */
	static float Sin256(uint32 Angle256);
	static float Cos256(uint32 Angle256);

	// Float functions ------------------------------------------------------------------------------------------------

	static FORCEINLINE int32 TruncToInt(float F)
	{
		return static_cast<int32>(F);
	}

	static FORCEINLINE float TruncToFloat(float F)
	{
		return std::trunc(F);
	}

	static FORCEINLINE int32 FloorToInt(float F)
	{
		return static_cast<int32>(std::floor(F));
	}

	static FORCEINLINE float FloorToFloat(float F)
	{
		return std::floor(F);
	}

	static FORCEINLINE int32 RoundToInt(float F)
	{
		return FloorToInt(F + 0.5f);
	}

	static FORCEINLINE float RoundToFloat(float F)
	{
		return FloorToFloat(F + 0.5f);
	}

	static FORCEINLINE int32 CeilToInt(float F)
	{
		return static_cast<int32>(std::ceil(F));
	}

	static FORCEINLINE float CeilToFloat(float F)
	{
		return std::ceil(F);
	}

	/** Fractional part, always >= 0 (UE: Frac). */
	static FORCEINLINE float Frac(float Value)
	{
		return Value - FloorToFloat(Value);
	}

	/** Fractional part with the sign of Value (UE: Fractional). */
	static FORCEINLINE float Fractional(float Value)
	{
		return Value - TruncToFloat(Value);
	}

	/** Returns the fractional part of Value and stores the integral part in OutIntPart (UE: Modf). */
	static FORCEINLINE float Modf(const float InValue, float* OutIntPart)
	{
		return std::modf(InValue, OutIntPart);
	}

	static FORCEINLINE float Exp(float Value)
	{
		return std::exp(Value);
	}
	static FORCEINLINE float Exp2(float Value)
	{
		return std::exp2(Value);
	}
	static FORCEINLINE float Loge(float Value)
	{
		return std::log(Value);
	}
	static FORCEINLINE float LogX(float Base, float Value)
	{
		return Loge(Value) / Loge(Base);
	}
	static FORCEINLINE float Log2(float Value)
	{
		return std::log2(Value);
	}

	/** Floating-point remainder with the sign of X; Y == 0 returns 0 (UE: Fmod). */
	static FORCEINLINE float Fmod(float X, float Y)
	{
		return (Y == 0.0f) ? 0.0f : std::fmod(X, Y);
	}

	static FORCEINLINE float Sin(float Value)
	{
		return std::sin(Value);
	}
	static FORCEINLINE float Asin(float Value)
	{
		return std::asin((Value < -1.0f) ? -1.0f : ((Value < 1.0f) ? Value : 1.0f));
	}
	static FORCEINLINE float Sinh(float Value)
	{
		return std::sinh(Value);
	}
	static FORCEINLINE float Cos(float Value)
	{
		return std::cos(Value);
	}
	static FORCEINLINE float Acos(float Value)
	{
		return std::acos((Value < -1.0f) ? -1.0f : ((Value < 1.0f) ? Value : 1.0f));
	}
	static FORCEINLINE float Tan(float Value)
	{
		return std::tan(Value);
	}
	static FORCEINLINE float Atan(float Value)
	{
		return std::atan(Value);
	}
	static FORCEINLINE float Atan2(float Y, float X)
	{
		return std::atan2(Y, X);
	}
	static FORCEINLINE float Sqrt(float Value)
	{
		return std::sqrt(Value);
	}
	static FORCEINLINE float Pow(float A, float B)
	{
		return std::pow(A, B);
	}

	/** 1 / Sqrt(F) (UE: InvSqrt). */
	static FORCEINLINE float InvSqrt(float F)
	{
		return 1.0f / std::sqrt(F);
	}

	/** InvSqrt with lower precision allowed (UE: InvSqrtEst). */
	static FORCEINLINE float InvSqrtEst(float F)
	{
		return InvSqrt(F);
	}

	static FORCEINLINE bool IsNaN(float A)
	{
		uint32 Bits = 0;
		std::memcpy(&Bits, &A, sizeof(Bits));
		return (Bits & 0x7FFFFFFFu) > 0x7F800000u;
	}

	static FORCEINLINE bool IsFinite(float A)
	{
		uint32 Bits = 0;
		std::memcpy(&Bits, &A, sizeof(Bits));
		return (Bits & 0x7F800000u) != 0x7F800000u;
	}

	static FORCEINLINE bool IsNegativeFloat(const float& A)
	{
		uint32 Bits = 0;
		std::memcpy(&Bits, &A, sizeof(Bits));
		return Bits >= 0x80000000u;
	}

	/** Comparand >= 0 ? ValueGEZero : ValueLTZero (UE: FloatSelect). */
	static constexpr FORCEINLINE float FloatSelect(float Comparand, float ValueGEZero, float ValueLTZero)
	{
		return Comparand >= 0.0f ? ValueGEZero : ValueLTZero;
	}

	// Random numbers (deterministic LCG, the same sequence on every platform)
	// -------------------------------------------

	/** Seeds Rand / FRand (UE: RandInit). */
	static void RandInit(int32 Seed);

	/** Random integer in [0, 0x7fff] like the C rand() range (UE: Rand). */
	static int32 Rand();

	/** Random float in [0, 1) (UE: FRand). */
	static float FRand();

	/** Seeds SRand (UE: SRandInit). */
	static void SRandInit(int32 Seed);

	/** Seeded random float in [0, 1) (UE: SRand). */
	static float SRand();

	static int32 GetRandSeed();

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
