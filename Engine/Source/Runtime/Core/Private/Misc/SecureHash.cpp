#include "Misc/SecureHash.h"

#include "Misc/CString.h"

#include <cstring>

// MD5 after the RSA Data Security reference implementation (RFC 1321), as UE's FMD5 does.

namespace
{
	// Per-round shift amounts.
	constexpr uint32 S11 = 7;
	constexpr uint32 S12 = 12;
	constexpr uint32 S13 = 17;
	constexpr uint32 S14 = 22;
	constexpr uint32 S21 = 5;
	constexpr uint32 S22 = 9;
	constexpr uint32 S23 = 14;
	constexpr uint32 S24 = 20;
	constexpr uint32 S31 = 4;
	constexpr uint32 S32 = 11;
	constexpr uint32 S33 = 16;
	constexpr uint32 S34 = 23;
	constexpr uint32 S41 = 6;
	constexpr uint32 S42 = 10;
	constexpr uint32 S43 = 15;
	constexpr uint32 S44 = 21;

	const uint8 Padding[64] = {0x80};

	FORCEINLINE uint32 F(uint32 X, uint32 Y, uint32 Z)
	{
		return (X & Y) | (~X & Z);
	}
	FORCEINLINE uint32 G(uint32 X, uint32 Y, uint32 Z)
	{
		return (X & Z) | (Y & ~Z);
	}
	FORCEINLINE uint32 H(uint32 X, uint32 Y, uint32 Z)
	{
		return X ^ Y ^ Z;
	}
	FORCEINLINE uint32 I(uint32 X, uint32 Y, uint32 Z)
	{
		return Y ^ (X | ~Z);
	}
	FORCEINLINE uint32 RotateLeft(uint32 X, uint32 N)
	{
		return (X << N) | (X >> (32 - N));
	}

	FORCEINLINE void FF(uint32& A, uint32 B, uint32 C, uint32 D, uint32 X, uint32 S, uint32 AC)
	{
		A += F(B, C, D) + X + AC;
		A = RotateLeft(A, S) + B;
	}
	FORCEINLINE void GG(uint32& A, uint32 B, uint32 C, uint32 D, uint32 X, uint32 S, uint32 AC)
	{
		A += G(B, C, D) + X + AC;
		A = RotateLeft(A, S) + B;
	}
	FORCEINLINE void HH(uint32& A, uint32 B, uint32 C, uint32 D, uint32 X, uint32 S, uint32 AC)
	{
		A += H(B, C, D) + X + AC;
		A = RotateLeft(A, S) + B;
	}
	FORCEINLINE void II(uint32& A, uint32 B, uint32 C, uint32 D, uint32 X, uint32 S, uint32 AC)
	{
		A += I(B, C, D) + X + AC;
		A = RotateLeft(A, S) + B;
	}
} // namespace

FMD5::FMD5()
{
	Context.Count[0] = Context.Count[1] = 0;
	// Load magic initialization constants.
	Context.State[0] = 0x67452301;
	Context.State[1] = 0xefcdab89;
	Context.State[2] = 0x98badcfe;
	Context.State[3] = 0x10325476;
	std::memset(Context.Buffer, 0, sizeof(Context.Buffer));
}

void FMD5::Update(const uint8* Input, uint64 InputLen)
{
	// Compute number of bytes mod 64.
	uint32 Index = (Context.Count[0] >> 3) & 0x3F;

	// Update number of bits.
	const uint32 LowBits = uint32(InputLen << 3);
	Context.Count[0] += LowBits;
	if (Context.Count[0] < LowBits)
	{
		Context.Count[1]++;
	}
	Context.Count[1] += uint32(InputLen >> 29);

	const uint32 PartLen = 64 - Index;

	// Transform as many times as possible.
	uint64 InputIndex = 0;
	if (InputLen >= PartLen)
	{
		std::memcpy(&Context.Buffer[Index], Input, PartLen);
		Transform(Context.State, Context.Buffer);
		for (InputIndex = PartLen; InputIndex + 63 < InputLen; InputIndex += 64)
		{
			Transform(Context.State, &Input[InputIndex]);
		}
		Index = 0;
	}

	// Buffer remaining input.
	std::memcpy(&Context.Buffer[Index], &Input[InputIndex], size_t(InputLen - InputIndex));
}

void FMD5::Final(uint8* Digest)
{
	uint8 Bits[8];

	// Save number of bits.
	Encode(Bits, Context.Count, 8);

	// Pad out to 56 mod 64.
	const uint32 Index = (Context.Count[0] >> 3) & 0x3f;
	const uint32 PadLen = (Index < 56) ? (56 - Index) : (120 - Index);
	Update(Padding, PadLen);

	// Append length (before padding).
	Update(Bits, 8);

	// Store state in digest.
	Encode(Digest, Context.State, 16);

	// Zeroize sensitive information.
	std::memset(&Context, 0, sizeof(Context));
}

FString FMD5::HashAnsiString(const TCHAR* String)
{
	return HashBytes(reinterpret_cast<const uint8*>(String), uint64(FCString::Strlen(String)));
}

FString FMD5::HashBytes(const uint8* Input, uint64 InputLen)
{
	uint8 Digest[16];

	FMD5 Md5Gen;
	Md5Gen.Update(Input, InputLen);
	Md5Gen.Final(Digest);

	FString MD5;
	for (int32 Index = 0; Index < 16; Index++)
	{
		MD5 += FString::Printf("%02x", Digest[Index]);
	}
	return MD5;
}

void FMD5::Transform(uint32* State, const uint8* Block)
{
	uint32 A = State[0];
	uint32 B = State[1];
	uint32 C = State[2];
	uint32 D = State[3];
	uint32 X[16];

	Decode(X, Block, 64);

	// Round 1.
	FF(A, B, C, D, X[0], S11, 0xd76aa478);
	FF(D, A, B, C, X[1], S12, 0xe8c7b756);
	FF(C, D, A, B, X[2], S13, 0x242070db);
	FF(B, C, D, A, X[3], S14, 0xc1bdceee);
	FF(A, B, C, D, X[4], S11, 0xf57c0faf);
	FF(D, A, B, C, X[5], S12, 0x4787c62a);
	FF(C, D, A, B, X[6], S13, 0xa8304613);
	FF(B, C, D, A, X[7], S14, 0xfd469501);
	FF(A, B, C, D, X[8], S11, 0x698098d8);
	FF(D, A, B, C, X[9], S12, 0x8b44f7af);
	FF(C, D, A, B, X[10], S13, 0xffff5bb1);
	FF(B, C, D, A, X[11], S14, 0x895cd7be);
	FF(A, B, C, D, X[12], S11, 0x6b901122);
	FF(D, A, B, C, X[13], S12, 0xfd987193);
	FF(C, D, A, B, X[14], S13, 0xa679438e);
	FF(B, C, D, A, X[15], S14, 0x49b40821);

	// Round 2.
	GG(A, B, C, D, X[1], S21, 0xf61e2562);
	GG(D, A, B, C, X[6], S22, 0xc040b340);
	GG(C, D, A, B, X[11], S23, 0x265e5a51);
	GG(B, C, D, A, X[0], S24, 0xe9b6c7aa);
	GG(A, B, C, D, X[5], S21, 0xd62f105d);
	GG(D, A, B, C, X[10], S22, 0x02441453);
	GG(C, D, A, B, X[15], S23, 0xd8a1e681);
	GG(B, C, D, A, X[4], S24, 0xe7d3fbc8);
	GG(A, B, C, D, X[9], S21, 0x21e1cde6);
	GG(D, A, B, C, X[14], S22, 0xc33707d6);
	GG(C, D, A, B, X[3], S23, 0xf4d50d87);
	GG(B, C, D, A, X[8], S24, 0x455a14ed);
	GG(A, B, C, D, X[13], S21, 0xa9e3e905);
	GG(D, A, B, C, X[2], S22, 0xfcefa3f8);
	GG(C, D, A, B, X[7], S23, 0x676f02d9);
	GG(B, C, D, A, X[12], S24, 0x8d2a4c8a);

	// Round 3.
	HH(A, B, C, D, X[5], S31, 0xfffa3942);
	HH(D, A, B, C, X[8], S32, 0x8771f681);
	HH(C, D, A, B, X[11], S33, 0x6d9d6122);
	HH(B, C, D, A, X[14], S34, 0xfde5380c);
	HH(A, B, C, D, X[1], S31, 0xa4beea44);
	HH(D, A, B, C, X[4], S32, 0x4bdecfa9);
	HH(C, D, A, B, X[7], S33, 0xf6bb4b60);
	HH(B, C, D, A, X[10], S34, 0xbebfbc70);
	HH(A, B, C, D, X[13], S31, 0x289b7ec6);
	HH(D, A, B, C, X[0], S32, 0xeaa127fa);
	HH(C, D, A, B, X[3], S33, 0xd4ef3085);
	HH(B, C, D, A, X[6], S34, 0x04881d05);
	HH(A, B, C, D, X[9], S31, 0xd9d4d039);
	HH(D, A, B, C, X[12], S32, 0xe6db99e5);
	HH(C, D, A, B, X[15], S33, 0x1fa27cf8);
	HH(B, C, D, A, X[2], S34, 0xc4ac5665);

	// Round 4.
	II(A, B, C, D, X[0], S41, 0xf4292244);
	II(D, A, B, C, X[7], S42, 0x432aff97);
	II(C, D, A, B, X[14], S43, 0xab9423a7);
	II(B, C, D, A, X[5], S44, 0xfc93a039);
	II(A, B, C, D, X[12], S41, 0x655b59c3);
	II(D, A, B, C, X[3], S42, 0x8f0ccc92);
	II(C, D, A, B, X[10], S43, 0xffeff47d);
	II(B, C, D, A, X[1], S44, 0x85845dd1);
	II(A, B, C, D, X[8], S41, 0x6fa87e4f);
	II(D, A, B, C, X[15], S42, 0xfe2ce6e0);
	II(C, D, A, B, X[6], S43, 0xa3014314);
	II(B, C, D, A, X[13], S44, 0x4e0811a1);
	II(A, B, C, D, X[4], S41, 0xf7537e82);
	II(D, A, B, C, X[11], S42, 0xbd3af235);
	II(C, D, A, B, X[2], S43, 0x2ad7d2bb);
	II(B, C, D, A, X[9], S44, 0xeb86d391);

	State[0] += A;
	State[1] += B;
	State[2] += C;
	State[3] += D;

	// Zeroize sensitive information.
	std::memset(X, 0, sizeof(X));
}

void FMD5::Encode(uint8* Output, const uint32* Input, uint32 Len)
{
	for (uint32 WordIndex = 0, ByteIndex = 0; ByteIndex < Len; WordIndex++, ByteIndex += 4)
	{
		Output[ByteIndex] = uint8(Input[WordIndex] & 0xff);
		Output[ByteIndex + 1] = uint8((Input[WordIndex] >> 8) & 0xff);
		Output[ByteIndex + 2] = uint8((Input[WordIndex] >> 16) & 0xff);
		Output[ByteIndex + 3] = uint8((Input[WordIndex] >> 24) & 0xff);
	}
}

void FMD5::Decode(uint32* Output, const uint8* Input, uint32 Len)
{
	for (uint32 WordIndex = 0, ByteIndex = 0; ByteIndex < Len; WordIndex++, ByteIndex += 4)
	{
		Output[WordIndex] = uint32(Input[ByteIndex]) | (uint32(Input[ByteIndex + 1]) << 8) |
			(uint32(Input[ByteIndex + 2]) << 16) | (uint32(Input[ByteIndex + 3]) << 24);
	}
}

// SHA-1 after FIPS 180-1 (UE: FSHA1). Big-endian words, 64-byte blocks, 80 rounds.

namespace
{
	FORCEINLINE uint32 Sha1RotateLeft(uint32 Value, uint32 Bits)
	{
		return (Value << Bits) | (Value >> (32 - Bits));
	}
} // namespace

FSHA1::FSHA1()
{
	Reset();
}

void FSHA1::Reset()
{
	State[0] = 0x67452301;
	State[1] = 0xEFCDAB89;
	State[2] = 0x98BADCFE;
	State[3] = 0x10325476;
	State[4] = 0xC3D2E1F0;
	TotalBytes = 0;
	std::memset(Buffer, 0, sizeof(Buffer));
	std::memset(Digest, 0, sizeof(Digest));
}

void FSHA1::Update(const uint8* Data, uint64 Length)
{
	uint32 Used = uint32(TotalBytes & 63);
	TotalBytes += Length;
	while (Length > 0)
	{
		const uint32 Take = uint32(Length < uint64(64 - Used) ? Length : uint64(64 - Used));
		std::memcpy(Buffer + Used, Data, Take);
		Used += Take;
		Data += Take;
		Length -= Take;
		if (Used == 64)
		{
			Transform(Buffer);
			Used = 0;
		}
	}
}

void FSHA1::Final()
{
	const uint64 BitLength = TotalBytes * 8;
	const uint8 One = 0x80;
	Update(&One, 1);
	const uint8 Zero = 0;
	while ((TotalBytes & 63) != 56)
	{
		Update(&Zero, 1);
	}
	uint8 LengthBytes[8];
	for (int32 Index = 0; Index < 8; ++Index)
	{
		LengthBytes[Index] = uint8(BitLength >> (56 - 8 * Index));
	}
	Update(LengthBytes, 8);
	for (int32 Index = 0; Index < 20; ++Index)
	{
		Digest[Index] = uint8(State[Index / 4] >> (24 - 8 * (Index % 4)));
	}
}

void FSHA1::GetHash(uint8* OutHash) const
{
	std::memcpy(OutHash, Digest, sizeof(Digest));
}

FSHAHash FSHA1::Finalize()
{
	Final();
	FSHAHash Result;
	GetHash(Result.Hash);
	return Result;
}

void FSHA1::HashBuffer(const void* Data, uint64 DataSize, uint8* OutHash)
{
	FSHA1 Sha;
	Sha.Update(static_cast<const uint8*>(Data), DataSize);
	Sha.Final();
	Sha.GetHash(OutHash);
}

FSHAHash FSHA1::HashBuffer(const void* Data, uint64 DataSize)
{
	FSHAHash Result;
	HashBuffer(Data, DataSize, Result.Hash);
	return Result;
}

void FSHA1::Transform(const uint8* Block)
{
	uint32 W[80];
	for (int32 Index = 0; Index < 16; ++Index)
	{
		W[Index] = (uint32(Block[Index * 4]) << 24) | (uint32(Block[Index * 4 + 1]) << 16) |
			(uint32(Block[Index * 4 + 2]) << 8) | uint32(Block[Index * 4 + 3]);
	}
	for (int32 Index = 16; Index < 80; ++Index)
	{
		W[Index] = Sha1RotateLeft(W[Index - 3] ^ W[Index - 8] ^ W[Index - 14] ^ W[Index - 16], 1);
	}

	uint32 A = State[0];
	uint32 B = State[1];
	uint32 C = State[2];
	uint32 D = State[3];
	uint32 E = State[4];
	for (int32 Index = 0; Index < 80; ++Index)
	{
		uint32 F;
		uint32 K;
		if (Index < 20)
		{
			F = (B & C) | (~B & D);
			K = 0x5A827999;
		}
		else if (Index < 40)
		{
			F = B ^ C ^ D;
			K = 0x6ED9EBA1;
		}
		else if (Index < 60)
		{
			F = (B & C) | (B & D) | (C & D);
			K = 0x8F1BBCDC;
		}
		else
		{
			F = B ^ C ^ D;
			K = 0xCA62C1D6;
		}
		const uint32 Temp = Sha1RotateLeft(A, 5) + F + E + K + W[Index];
		E = D;
		D = C;
		C = Sha1RotateLeft(B, 30);
		B = A;
		A = Temp;
	}
	State[0] += A;
	State[1] += B;
	State[2] += C;
	State[3] += D;
	State[4] += E;
}
