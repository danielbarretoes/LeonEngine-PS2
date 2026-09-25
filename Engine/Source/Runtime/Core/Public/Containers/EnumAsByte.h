#pragma once

#include "CoreTypes.h"

#include <type_traits>

/**
 * Stores an old-style (unscoped) enum in one byte (UE: TEnumAsByte). Reflected as an FByteProperty with the enum
 * attached; new code uses an enum class with a uint8 underlying type instead.
 */
template <typename TEnum>
class TEnumAsByte
{
public:
	typedef TEnum EnumType;

	TEnumAsByte() = default;
	TEnumAsByte(const TEnumAsByte&) = default;
	TEnumAsByte& operator=(const TEnumAsByte&) = default;

	FORCEINLINE TEnumAsByte(TEnum InValue)
		: Value(static_cast<uint8>(InValue))
	{
	}

	explicit FORCEINLINE TEnumAsByte(int32 InValue)
		: Value(static_cast<uint8>(InValue))
	{
	}

	explicit FORCEINLINE TEnumAsByte(uint8 InValue)
		: Value(InValue)
	{
	}

	FORCEINLINE bool operator==(TEnum InValue) const
	{
		return static_cast<TEnum>(Value) == InValue;
	}

	FORCEINLINE bool operator==(TEnumAsByte InValue) const
	{
		return Value == InValue.Value;
	}

	FORCEINLINE bool operator!=(TEnum InValue) const
	{
		return static_cast<TEnum>(Value) != InValue;
	}

	FORCEINLINE bool operator!=(TEnumAsByte InValue) const
	{
		return Value != InValue.Value;
	}

	FORCEINLINE operator TEnum() const
	{
		return static_cast<TEnum>(Value);
	}

	FORCEINLINE TEnum GetValue() const
	{
		return static_cast<TEnum>(Value);
	}

	FORCEINLINE uint8 GetIntValue() const
	{
		return Value;
	}

	FORCEINLINE friend uint32 GetTypeHash(const TEnumAsByte& Enum)
	{
		return Enum.Value;
	}

private:
	/** The enum value, zero when default-constructed in zeroed memory. */
	uint8 Value;
};
