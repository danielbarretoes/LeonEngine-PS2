#pragma once

#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Misc/Crc.h"
#include "Serialization/Archive.h"

/** Text forms of a GUID (UE: EGuidFormats). */
enum class EGuidFormats
{
	/** 32 digits: "00000000000000000000000000000000". */
	Digits,

	/** "00000000-0000-0000-0000-000000000000". */
	DigitsWithHyphens,

	/** "{00000000-0000-0000-0000-000000000000}". */
	DigitsWithHyphensInBraces,

	/** "(00000000-0000-0000-0000-000000000000)". */
	DigitsWithHyphensInParentheses,

	/** "{0x00000000,0x0000,0x0000,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}}". */
	HexValuesInBraces,

	/** "00000000-00000000-00000000-00000000". */
	UniqueObjectGuid,
};

/** 128-bit globally unique identifier; all zero is invalid (UE: FGuid). */
struct CORE_API FGuid
{
	uint32 A;
	uint32 B;
	uint32 C;
	uint32 D;

	/** The invalid (zero) GUID. */
	constexpr FGuid()
		: A(0)
		, B(0)
		, C(0)
		, D(0)
	{
	}

	constexpr FGuid(uint32 InA, uint32 InB, uint32 InC, uint32 InD)
		: A(InA)
		, B(InB)
		, C(InC)
		, D(InD)
	{
	}

	/** Parses any of EGuidFormats; an unparsable string gives the invalid GUID. */
	explicit FGuid(const FString& InGuidStr)
	{
		if (!Parse(InGuidStr, *this))
		{
			Invalidate();
		}
	}

	friend bool operator==(const FGuid& X, const FGuid& Y)
	{
		return ((X.A ^ Y.A) | (X.B ^ Y.B) | (X.C ^ Y.C) | (X.D ^ Y.D)) == 0;
	}

	friend bool operator!=(const FGuid& X, const FGuid& Y)
	{
		return ((X.A ^ Y.A) | (X.B ^ Y.B) | (X.C ^ Y.C) | (X.D ^ Y.D)) != 0;
	}

	friend bool operator<(const FGuid& X, const FGuid& Y)
	{
		return ((X.A < Y.A)
				? true
				: ((X.A > Y.A)
						  ? false
						  : ((X.B < Y.B)
									? true
									: ((X.B > Y.B)
											  ? false
											  : ((X.C < Y.C)
														? true
														: ((X.C > Y.C)
																  ? false
																  : ((X.D < Y.D) ? true
																				 : ((X.D > Y.D) ? false : false))))))));
	}

	uint32& operator[](int32 Index)
	{
		checkSlow(Index >= 0 && Index < 4);
		return (&A)[Index];
	}

	const uint32& operator[](int32 Index) const
	{
		checkSlow(Index >= 0 && Index < 4);
		return (&A)[Index];
	}

	void Invalidate()
	{
		A = B = C = D = 0;
	}

	bool IsValid() const
	{
		return ((A | B | C | D) != 0);
	}

	FString ToString(EGuidFormats Format = EGuidFormats::Digits) const;

	/** A new random GUID (UE: NewGuid). */
	static FGuid NewGuid();

	/**
	 * The same GUID for the same text and seed, on every platform: saved packages get stable ids from object paths
	 * (UE 5 API; Leon hashes with MD5).
	 */
	static FGuid NewDeterministicGuid(const FString& ObjectPath, uint64 Seed = 0);

	/** Parses any of EGuidFormats (UE: Parse). */
	static bool Parse(const FString& GuidString, FGuid& OutGuid);

	static bool ParseExact(const FString& GuidString, EGuidFormats Format, FGuid& OutGuid);

	friend uint32 GetTypeHash(const FGuid& Guid)
	{
		return FCrc::MemCrc32(&Guid, sizeof(FGuid));
	}
};

inline FString LexToString(const FGuid& Value)
{
	return Value.ToString();
}

inline FArchive& operator<<(FArchive& Ar, FGuid& V)
{
	return Ar << V.A << V.B << V.C << V.D;
}
