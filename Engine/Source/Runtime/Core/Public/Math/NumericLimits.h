#pragma once

#include "CoreTypes.h"

// Numeric limits (UE: Math/NumericLimits.h).

#define MIN_uint8 ((uint8)0x00)
#define MIN_uint16 ((uint16)0x0000)
#define MIN_uint32 ((uint32)0x00000000)
#define MIN_uint64 ((uint64)0x0000000000000000)
#define MIN_int8 ((int8) - 128)
#define MIN_int16 ((int16) - 32768)
#define MIN_int32 ((int32)0x80000000)
#define MIN_int64 ((int64)0x8000000000000000)

#define MAX_uint8 ((uint8)0xff)
#define MAX_uint16 ((uint16)0xffff)
#define MAX_uint32 ((uint32)0xffffffff)
#define MAX_uint64 ((uint64)0xffffffffffffffff)
#define MAX_int8 ((int8)0x7f)
#define MAX_int16 ((int16)0x7fff)
#define MAX_int32 ((int32)0x7fffffff)
#define MAX_int64 ((int64)0x7fffffffffffffff)

#define MIN_flt (1.175494351e-38F)
#define MAX_flt (3.402823466e+38F)
#define MIN_dbl (2.2250738585072014e-308)
#define MAX_dbl (1.7976931348623158e+308)

/** Min / Max / Lowest of the numeric types (UE: TNumericLimits). */
template <typename NumericType>
struct TNumericLimits;

template <typename NumericType>
struct TNumericLimits<const NumericType> : public TNumericLimits<NumericType>
{
};

template <typename NumericType>
struct TNumericLimits<volatile NumericType> : public TNumericLimits<NumericType>
{
};

template <typename NumericType>
struct TNumericLimits<const volatile NumericType> : public TNumericLimits<NumericType>
{
};

#define LEON_NUMERIC_LIMITS(Type, MinValue, MaxValue, LowestValue)                                                     \
	template <>                                                                                                        \
	struct TNumericLimits<Type>                                                                                        \
	{                                                                                                                  \
		typedef Type NumericType;                                                                                      \
		static constexpr NumericType Min()                                                                             \
		{                                                                                                              \
			return MinValue;                                                                                           \
		}                                                                                                              \
		static constexpr NumericType Max()                                                                             \
		{                                                                                                              \
			return MaxValue;                                                                                           \
		}                                                                                                              \
		static constexpr NumericType Lowest()                                                                          \
		{                                                                                                              \
			return LowestValue;                                                                                        \
		}                                                                                                              \
	};

LEON_NUMERIC_LIMITS(uint8, MIN_uint8, MAX_uint8, MIN_uint8)
LEON_NUMERIC_LIMITS(uint16, MIN_uint16, MAX_uint16, MIN_uint16)
LEON_NUMERIC_LIMITS(uint32, MIN_uint32, MAX_uint32, MIN_uint32)
LEON_NUMERIC_LIMITS(uint64, MIN_uint64, MAX_uint64, MIN_uint64)
LEON_NUMERIC_LIMITS(int8, MIN_int8, MAX_int8, MIN_int8)
LEON_NUMERIC_LIMITS(int16, MIN_int16, MAX_int16, MIN_int16)
LEON_NUMERIC_LIMITS(int32, MIN_int32, MAX_int32, MIN_int32)
LEON_NUMERIC_LIMITS(int64, MIN_int64, MAX_int64, MIN_int64)
LEON_NUMERIC_LIMITS(float, MIN_flt, MAX_flt, -MAX_flt)
LEON_NUMERIC_LIMITS(double, MIN_dbl, MAX_dbl, -MAX_dbl)

#undef LEON_NUMERIC_LIMITS
