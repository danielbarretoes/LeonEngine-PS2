#include "GSTextureLayout.h"

void FGSTextureLayout::GetPageSize(EGSPixelFormat Format, uint32& OutWidth, uint32& OutHeight)
{
	switch (GSBitsPerPixel(Format))
	{
		case 4:
			OutWidth = 128;
			OutHeight = 128;
			break;
		case 8:
			OutWidth = 128;
			OutHeight = 64;
			break;
		case 16:
			OutWidth = 64;
			OutHeight = 64;
			break;
		default:
			OutWidth = 64;
			OutHeight = 32;
			break;
	}
}

uint8 FGSTextureLayout::GetBufferWidth(EGSPixelFormat Format, uint32 Width)
{
	uint32 PageWidth = 0;
	uint32 PageHeight = 0;
	GetPageSize(Format, PageWidth, PageHeight);
	const uint32 PagesAcross = FMath::Max(1u, (Width + PageWidth - 1) / PageWidth);
	return uint8((PagesAcross * PageWidth) / 64);
}

uint32 FGSTextureLayout::GetNumBlocks(EGSPixelFormat Format, uint32 Width, uint32 Height)
{
	uint32 PageWidth = 0;
	uint32 PageHeight = 0;
	GetPageSize(Format, PageWidth, PageHeight);
	const uint32 PagesAcross = FMath::Max(1u, (Width + PageWidth - 1) / PageWidth);
	const uint32 PagesDown = FMath::Max(1u, (Height + PageHeight - 1) / PageHeight);
	return PagesAcross * PagesDown * BlocksPerPage;
}

uint32 FGSTextureLayout::GetClutBlocks(EGSPixelFormat Format)
{
	switch (Format)
	{
		case EGSPixelFormat::PSMT8:
			return 4;
		case EGSPixelFormat::PSMT4:
			return 1;
		default:
			return 0;
	}
}

void FGSTextureLayout::MakeClutImage(
	TArrayView<const uint32> Palette, TArray<uint8>& OutImage, uint16& OutWidth, uint16& OutHeight)
{
	const bool bIndex8 = Palette.Num() > 16;
	OutWidth = bIndex8 ? 16 : 8;
	OutHeight = bIndex8 ? 16 : 2;
	const int32 NumEntries = int32(OutWidth) * OutHeight;
	OutImage.SetNumZeroed(NumEntries * 4);
	for (int32 Index = 0; Index < NumEntries && Index < Palette.Num(); ++Index)
	{
		// CSM1 (manual 3.4.7): bits 3 and 4 of a 256-entry CLUT's index are swapped in its rectangle.
		const int32 Position = bIndex8 ? ((Index & ~0x18) | ((Index & 0x08) << 1) | ((Index & 0x10) >> 1)) : Index;
		const uint32 Color = Palette[Index];
		uint8* Texel = &OutImage[Position * 4];
		Texel[0] = uint8(Color);
		Texel[1] = uint8(Color >> 8);
		Texel[2] = uint8(Color >> 16);
		Texel[3] = uint8(((Color >> 24) * 0x80u + 127u) / 255u);
	}
}
