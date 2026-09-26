#include "GSLocalMemory.h"

FGSLocalMemory::FGSLocalMemory()
{
	Bytes.SetNumZeroed(SizeInBytes);
}

uint32 FGSLocalMemory::StorageBits(EGSPixelFormat Format)
{
	const uint32 Bits = GSBitsPerPixel(Format);
	return Bits == 24 ? 32 : Bits;
}

uint64 FGSLocalMemory::BitAddress(uint32 BaseWords, uint32 WidthPixels, EGSPixelFormat Format, uint32 X, uint32 Y)
{
	const uint64 Pixel = (uint64(Y) * WidthPixels) + X;
	const uint64 Bit = (uint64(BaseWords) * 32) + (Pixel * StorageBits(Format));
	return Bit % (uint64(SizeInBytes) * 8);
}

uint32 FGSLocalMemory::ReadPixel(uint32 BaseWords, uint32 WidthPixels, EGSPixelFormat Format, uint32 X, uint32 Y) const
{
	const uint64 Bit = BitAddress(BaseWords, WidthPixels, Format, X, Y);
	const uint32 Byte = uint32(Bit / 8);
	switch (StorageBits(Format))
	{
		case 4:
			return (Bytes[Byte] >> (Bit % 8)) & 0xf;
		case 8:
			return Bytes[Byte];
		case 16:
			return uint32(Bytes[Byte]) | (uint32(Bytes[(Byte + 1) % SizeInBytes]) << 8);
		default:
		{
			uint32 Value = 0;
			for (uint32 Index = 0; Index < 4; ++Index)
			{
				Value |= uint32(Bytes[(Byte + Index) % SizeInBytes]) << (Index * 8);
			}
			return Value;
		}
	}
}

void FGSLocalMemory::WritePixel(
	uint32 BaseWords, uint32 WidthPixels, EGSPixelFormat Format, uint32 X, uint32 Y, uint32 Value)
{
	const uint64 Bit = BitAddress(BaseWords, WidthPixels, Format, X, Y);
	const uint32 Byte = uint32(Bit / 8);
	switch (StorageBits(Format))
	{
		case 4:
		{
			// The first pixel of a byte in its low nibble (the manual's IDTEX4 order).
			const uint32 Shift = uint32(Bit % 8);
			Bytes[Byte] = uint8((Bytes[Byte] & ~(0xf << Shift)) | ((Value & 0xf) << Shift));
			break;
		}
		case 8:
			Bytes[Byte] = uint8(Value);
			break;
		case 16:
			Bytes[Byte] = uint8(Value);
			Bytes[(Byte + 1) % SizeInBytes] = uint8(Value >> 8);
			break;
		default:
			for (uint32 Index = 0; Index < 4; ++Index)
			{
				Bytes[(Byte + Index) % SizeInBytes] = uint8(Value >> (Index * 8));
			}
			break;
	}
}
