#pragma once

#include "CoreTypes.h"

#include <cstring>
#include <type_traits>

// Hash functions used by TSet / TMap (UE: Templates/TypeHash.h). Types provide
// `friend uint32 GetTypeHash(const T&)` to become hashable.

/** Bob Jenkins' 96-bit mix, reduced to combine two 32-bit hashes (UE: HashCombine). */
inline uint32 HashCombine(uint32 A, uint32 C)
{
	uint32 B = 0x9e3779b9;
	A += B;

	A -= B;
	A -= C;
	A ^= (C >> 13);
	B -= C;
	B -= A;
	B ^= (A << 8);
	C -= A;
	C -= B;
	C ^= (B >> 13);
	A -= B;
	A -= C;
	A ^= (C >> 12);
	B -= C;
	B -= A;
	B ^= (A << 16);
	C -= A;
	C -= B;
	C ^= (B >> 5);
	A -= B;
	A -= C;
	A ^= (C >> 3);
	B -= C;
	B -= A;
	B ^= (A << 10);
	C -= A;
	C -= B;
	C ^= (B >> 15);

	return C;
}

/** Hash of a 64-bit value folded to 32 bits. */
inline uint32 GetTypeHash64(uint64 Value)
{
	return static_cast<uint32>(Value) + (static_cast<uint32>(Value >> 32) * 23);
}

/** Hash of a pointer value; the low alignment bits are dropped (UE: PointerHash). */
inline uint32 PointerHash(const void* Key, uint32 C = 0)
{
	const UPTRINT PtrInt = reinterpret_cast<UPTRINT>(Key) >> 4;
	return HashCombine(
		sizeof(UPTRINT) > 4 ? GetTypeHash64(static_cast<uint64>(PtrInt)) : static_cast<uint32>(PtrInt), C);
}

/** Hash of any scalar: integers, floats, enums, pointers (UE: the GetTypeHash scalar overloads). */
template <typename ScalarType, std::enable_if_t<std::is_scalar_v<ScalarType>, int> = 0>
inline uint32 GetTypeHash(ScalarType Value)
{
	if constexpr (std::is_integral_v<ScalarType>)
	{
		if constexpr (sizeof(ScalarType) <= 4)
		{
			return static_cast<uint32>(Value);
		}
		else
		{
			return GetTypeHash64(static_cast<uint64>(Value));
		}
	}
	else if constexpr (std::is_floating_point_v<ScalarType>)
	{
		if constexpr (sizeof(ScalarType) == 4)
		{
			uint32 Bits;
			std::memcpy(&Bits, &Value, sizeof(Bits));
			return Bits;
		}
		else
		{
			uint64 Bits;
			std::memcpy(&Bits, &Value, sizeof(Bits));
			return GetTypeHash64(Bits);
		}
	}
	else if constexpr (std::is_enum_v<ScalarType>)
	{
		return GetTypeHash(static_cast<std::underlying_type_t<ScalarType>>(Value));
	}
	else if constexpr (std::is_pointer_v<ScalarType>)
	{
		return PointerHash(Value);
	}
	else
	{
		return 0; // nullptr_t, member pointers
	}
}
