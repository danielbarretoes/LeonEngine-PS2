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

/** A SHA-1 digest, 20 bytes (UE: FSHAHash). */
class CORE_API FSHAHash
{
public:
	uint8 Hash[20];

	FSHAHash()
		: Hash{}
	{
	}

	/** Upper-case hex of the 20 bytes (UE: ToString). */
	FString ToString() const
	{
		return BytesToHex(Hash, 20);
	}

	friend bool operator==(const FSHAHash& X, const FSHAHash& Y)
	{
		for (int32 Index = 0; Index < 20; ++Index)
		{
			if (X.Hash[Index] != Y.Hash[Index])
			{
				return false;
			}
		}
		return true;
	}

	friend bool operator!=(const FSHAHash& X, const FSHAHash& Y)
	{
		return !(X == Y);
	}

	/** The 20 bytes (UE). */
	friend FArchive& operator<<(FArchive& Ar, FSHAHash& G)
	{
		Ar.Serialize(G.Hash, 20);
		return Ar;
	}
};

/**
 * SHA-1 (FIPS 180-1), fed in pieces (UE: FSHA1): Update, then Final, then GetHash. The pak files hash their entries and
 * their index with it, as UE's do. For content hashes, not for security.
 */
class CORE_API FSHA1
{
public:
	enum
	{
		DigestSize = 20
	};

	FSHA1();

	/** Starts a new digest (UE: Reset). */
	void Reset();

	/** Adds Length bytes (UE: Update). */
	void Update(const uint8* Data, uint64 Length);

	/** Ends the digest: pads the message and adds its bit length (UE: Final). */
	void Final();

	/** Copies the 20-byte digest after Final (UE: GetHash). */
	void GetHash(uint8* OutHash) const;

	/** Final, then the digest (UE: Finalize). */
	FSHAHash Finalize();

	/** The SHA-1 of a buffer (UE: HashBuffer). */
	static void HashBuffer(const void* Data, uint64 DataSize, uint8* OutHash);
	static FSHAHash HashBuffer(const void* Data, uint64 DataSize);

private:
	void Transform(const uint8* Block);

	uint32 State[5];
	uint64 TotalBytes;
	uint8 Buffer[64];
	uint8 Digest[20];
};
