#include "GSTexelDecoder.h"

namespace
{

	/** Where IDTEX8's entry Index sits in its 16 x 16 CLUT rectangle (CSM1: bits 3 and 4 swapped, manual 3.4.7). */
	void ClutPosition8(uint32 Index, uint32& OutX, uint32& OutY)
	{
		const uint32 Position = (Index & ~0x18u) | ((Index & 0x08u) << 1) | ((Index & 0x10u) >> 1);
		OutX = Position % 16;
		OutY = Position / 16;
	}

	[[nodiscard]] bool IsColor16(EGSPixelFormat Format)
	{
		return Format == EGSPixelFormat::PSMCT16 || Format == EGSPixelFormat::PSMCT16S;
	}

	[[nodiscard]] FColor Color32(uint32 Value)
	{
		return FColor(uint8(Value), uint8(Value >> 8), uint8(Value >> 16), uint8(Value >> 24));
	}

} // namespace

void FGSClutBuffer::Load(const FGSLocalMemory& Memory, const FGSTex0& Tex0)
{
	// CSM1: the CLUT is an image at CBP, in a buffer 64 pixels wide (it never leaves the first page's blocks).
	const bool bIndex8 = Tex0.PSM == EGSPixelFormat::PSMT8;
	const uint32 NumEntries = bIndex8 ? 256 : 16;
	for (uint32 Index = 0; Index < NumEntries; ++Index)
	{
		uint32 X = Index % 8;
		uint32 Y = Index / 8;
		if (bIndex8)
		{
			ClutPosition8(Index, X, Y);
		}
		// The temporary buffer takes the entries at CSA * 16 (manual 3.4.7).
		Entries[((uint32(Tex0.CSA) * 16) + Index) % 512] = Memory.ReadPixel(Tex0.CBP, 1, Tex0.CPSM, X, Y);
	}
}

bool FGSClutBuffer::Update(const FGSLocalMemory& Memory, const FGSTex0& Tex0)
{
	if (Tex0.PSM != EGSPixelFormat::PSMT8 && Tex0.PSM != EGSPixelFormat::PSMT4)
	{
		return false;
	}
	switch (Tex0.CLD)
	{
		case 1:
			break;
		case 2:
			CBP0 = Tex0.CBP;
			break;
		case 3:
			CBP1 = Tex0.CBP;
			break;
		case 4:
			if (CBP0 == Tex0.CBP)
			{
				return false;
			}
			CBP0 = Tex0.CBP;
			break;
		case 5:
			if (CBP1 == Tex0.CBP)
			{
				return false;
			}
			CBP1 = Tex0.CBP;
			break;
		default:
			return false;
	}
	Load(Memory, Tex0);
	return true;
}

FColor FGSTexelDecoder::ExpandColor(uint32 Value, EGSPixelFormat Format, const FGSTexA& TexA)
{
	// 5-bit colors shifted left 3; the alpha from TEXA (manual 3.4.6).
	if (IsColor16(Format))
	{
		const uint8 R = uint8((Value & 0x1f) << 3);
		const uint8 G = uint8(((Value >> 5) & 0x1f) << 3);
		const uint8 B = uint8(((Value >> 10) & 0x1f) << 3);
		const bool bAlphaBit = ((Value >> 15) & 1) != 0;
		const bool bBlack = (R | G | B) == 0;
		const uint8 A = bAlphaBit ? TexA.TA1 : (TexA.bAlphaExpandBlack && bBlack ? 0 : TexA.TA0);
		return FColor(R, G, B, A);
	}
	const uint8 R = uint8(Value);
	const uint8 G = uint8(Value >> 8);
	const uint8 B = uint8(Value >> 16);
	const uint8 A = TexA.bAlphaExpandBlack && (R | G | B) == 0 ? 0 : TexA.TA0;
	return FColor(R, G, B, A);
}

int32 FGSTexelDecoder::Wrap(EGSWrapMode Mode, int32 Coordinate, int32 Size, uint32 Level, uint16 Min, uint16 Max)
{
	switch (Mode)
	{
		case EGSWrapMode::Repeat:
			return ((Coordinate % Size) + Size) % Size;
		case EGSWrapMode::Clamp:
			return FMath::Clamp(Coordinate, 0, Size - 1);
		case EGSWrapMode::RegionClamp:
			return FMath::Clamp(Coordinate, int32(Min >> Level), int32(Max >> Level));
		case EGSWrapMode::RegionRepeat:
			// MINU / MINV are the masks, MAXU / MAXV the fixed bits, shifted by the level as REGION_CLAMP's range is
			// (the manual says so only for REGION_CLAMP; PCSX2's software GS shifts both).
			return (Coordinate & int32(Min >> Level)) | int32(Max >> Level);
	}
	return Coordinate;
}

FColor FGSTexelDecoder::Decode(const FGSLocalMemory& Memory, const FGSTex0& Tex0, uint32 BasePointer,
	uint32 BufferWidth, const FGSTexA& TexA, const FGSClutBuffer& Clut, uint32 U, uint32 V)
{
	const uint32 Raw = Memory.ReadPixel(BasePointer, BufferWidth, Tex0.PSM, U, V);
	switch (Tex0.PSM)
	{
		case EGSPixelFormat::PSMCT32:
			return Color32(Raw);
		case EGSPixelFormat::PSMCT24:
			return ExpandColor(Raw & 0xffffffu, EGSPixelFormat::PSMCT24, TexA);
		case EGSPixelFormat::PSMCT16:
		case EGSPixelFormat::PSMCT16S:
			return ExpandColor(Raw, EGSPixelFormat::PSMCT16, TexA);
		default:
		{
			// Through the CLUT's temporary buffer: IDTEX8 from entry 0, IDTEX4 from CSA * 16 (manual 3.4.7).
			const uint32 Entry = Tex0.PSM == EGSPixelFormat::PSMT8 ? Raw : (uint32(Tex0.CSA) * 16) + Raw;
			const uint32 Color = Clut.Entries[Entry % 512];
			return Tex0.CPSM == EGSPixelFormat::PSMCT32 ? Color32(Color)
														: ExpandColor(Color, EGSPixelFormat::PSMCT16, TexA);
		}
	}
}
