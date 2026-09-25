#pragma once

#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "Serialization/Archive.h"

/** MD5 digest (RFC 1321), fed in pieces (UE: FMD5). For content hashes, not for security. */
class CORE_API FMD5
{
public:
	FMD5();

	/** Adds InputLen bytes to the digest. */
	void Update(const uint8* Input, uint64 InputLen);

	/** Writes the 16-byte digest; the object must not be updated afterwards. */
	void Final(uint8* Digest);

	/** Lower-case hex MD5 of the string's bytes (UE: HashAnsiString). */
	static FString HashAnsiString(const TCHAR* String);

	/** Lower-case hex MD5 of a block of memory (UE: HashBytes). */
	static FString HashBytes(const uint8* Input, uint64 InputLen);

private:
	struct FContext
	{
		uint32 State[4];
		uint32 Count[2];
		uint8 Buffer[64];
	};

	static void Transform(uint32* State, const uint8* Block);
	static void Encode(uint8* Output, const uint32* Input, uint32 Len);
	static void Decode(uint32* Output, const uint8* Input, uint32 Len);

	FContext Context;
};

/** A finished MD5 digest (UE: FMD5Hash). */
struct CORE_API FMD5Hash
{
	FMD5Hash()
		: bIsValid(false)
		, Bytes{}
	{
	}

	bool IsValid() const
	{
		return bIsValid;
	}

	/** Takes the digest of MD5 (UE: Set). */
	void Set(FMD5& MD5)
	{
		MD5.Final(Bytes);
		bIsValid = true;
	}

	friend bool operator==(const FMD5Hash& LHS, const FMD5Hash& RHS)
	{
		if (LHS.bIsValid != RHS.bIsValid)
		{
			return false;
		}
		for (int32 Index = 0; Index < 16; ++Index)
		{
			if (LHS.Bytes[Index] != RHS.Bytes[Index])
			{
				return false;
			}
		}
		return true;
	}

	friend bool operator!=(const FMD5Hash& LHS, const FMD5Hash& RHS)
	{
		return !(LHS == RHS);
	}

	const uint8* GetBytes() const
	{
		return Bytes;
	}

	int32 GetSize() const
	{
		return int32(sizeof(Bytes));
	}

	/** Upper-case hex of the 16 bytes; empty when not valid (UE: LexToString). */
	friend FString LexToString(const FMD5Hash& Hash)
	{
		return Hash.bIsValid ? BytesToHex(Hash.Bytes, 16) : FString();
	}

	/** Validity, then the 16 bytes when valid (UE). */
	friend FArchive& operator<<(FArchive& Ar, FMD5Hash& Hash)
	{
		Ar << Hash.bIsValid;
		if (Hash.bIsValid)
		{
			Ar.Serialize(Hash.Bytes, 16);
		}
		return Ar;
	}

private:
	bool bIsValid;
	uint8 Bytes[16];
};
