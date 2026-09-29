#include "GSTextureLayout.h"

void FGSTextureLayout::GetPageSize(EGSPixelFormat Format, uint32& OutWidth, uint32& OutHeight)
{
	FGSLocalMemory::GetPageSize(Format, OutWidth, OutHeight);
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
	return FGSLocalMemory::GetBlockSpan(GetBufferWidth(Format, Width), Format, Width, Height);
}

uint32 FGSTextureLayout::GetBaseAlignment(EGSPixelFormat Format, uint32 Width, uint32 Height)
{
	uint32 PageWidth = 0;
	uint32 PageHeight = 0;
	GetPageSize(Format, PageWidth, PageHeight);
	return Width <= PageWidth && Height <= PageHeight ? 1 : BlocksPerPage;
}

void FGSTextureLayout::GetFootprint(
	EGSPixelFormat Format, uint32 Width, uint32 Height, int32 NumLevels, FFootprint& Out)
{
	Out = FFootprint();
	Out.NumLevels = FMath::Clamp(NumLevels, 1, MaxLevels);
	Out.Alignment = GetBaseAlignment(Format, Width, Height);
	uint32 Next = 0;
	for (int32 Level = 0; Level < Out.NumLevels; ++Level)
	{
		const uint32 LevelWidth = FMath::Max(1u, Width >> Level);
		const uint32 LevelHeight = FMath::Max(1u, Height >> Level);
		const uint32 Alignment = GetBaseAlignment(Format, LevelWidth, LevelHeight);
		Out.LevelBlock[Level] = ((Next + Alignment - 1) / Alignment) * Alignment;
		Out.LevelBufferWidth[Level] = GetBufferWidth(Format, LevelWidth);
		Next = Out.LevelBlock[Level] + GetNumBlocks(Format, LevelWidth, LevelHeight);
	}
	Out.ClutBlock = Next;
	Out.NumBlocks = Next + GetClutBlocks(Format);
}

uint32 FGSTextureLayout::GetClutBlocks(EGSPixelFormat Format)
{
	switch (Format)
	{
		case EGSPixelFormat::PSMT8:
			return FGSLocalMemory::GetBlockSpan(1, EGSPixelFormat::PSMCT32, 16, 16);
		case EGSPixelFormat::PSMT4:
			return FGSLocalMemory::GetBlockSpan(1, EGSPixelFormat::PSMCT32, 8, 2);
		default:
			return 0;
	}
}

void FGSTextureLayout::MakeClutImage(
	TArrayView<const uint32> Palette, TArray<uint8>& OutImage, uint16& OutWidth, uint16& OutHeight)
{
	const bool bIndex8 = Palette.Num() > 16;
	GetClutImageSize(bIndex8, OutWidth, OutHeight);
	const int32 NumEntries = int32(OutWidth) * OutHeight;
	OutImage.SetNumZeroed(NumEntries * 4);
	for (int32 Index = 0; Index < NumEntries && Index < Palette.Num(); ++Index)
	{
		// CSM1 (manual 3.4.7): bits 3 and 4 of a 256-entry CLUT's index are swapped in its rectangle.
		const int32 Position = GetClutImagePosition(Index, bIndex8);
		const uint32 Color = Palette[Index];
		uint8* Texel = &OutImage[Position * 4];
		Texel[0] = uint8(Color);
		Texel[1] = uint8(Color >> 8);
		Texel[2] = uint8(Color >> 16);
		Texel[3] = uint8(((Color >> 24) * 0x80u + 127u) / 255u);
	}
}
