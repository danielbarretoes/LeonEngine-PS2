#include "GSTextureCache.h"

#include "Engine/Texture2D.h"
#include "GSTextureLayout.h"

namespace
{

	[[nodiscard]] bool IsPowerOfTwoSide(int32 Size)
	{
		return Size >= FGSTextureCache::MinTextureSize && Size <= FGSTextureCache::MaxTextureSize &&
			(Size & (Size - 1)) == 0;
	}

	[[nodiscard]] int32 PowerOfTwoSide(int32 Size)
	{
		int32 Side = FGSTextureCache::MinTextureSize;
		while (Side < Size && Side < FGSTextureCache::MaxTextureSize)
		{
			Side *= 2;
		}
		return Side;
	}

	[[nodiscard]] uint8 Log2(int32 Side)
	{
		uint8 Log = 0;
		while ((1 << Log) < Side)
		{
			++Log;
		}
		return Log;
	}

} // namespace

void FGSTextureCache::SetArena(uint32 FirstBlock, uint32 NumBlocks)
{
	ArenaFirst = FirstBlock;
	ArenaBlocks = NumBlocks;
	Reset();
}

void FGSTextureCache::Reset()
{
	Entries.Reset();
	NextBlock = 0;
	ClutBlocks = 0;
}

bool FGSTextureCache::Allocate(uint32 NumBlocks, uint32 NumClutBlocks, uint32& OutBlock, uint32& OutClutBlock)
{
	if (NumBlocks + NumClutBlocks > ArenaBlocks)
	{
		return false;
	}
	if (NextBlock + NumBlocks + ClutBlocks + NumClutBlocks > ArenaBlocks)
	{
		// Full: start over; what the next draws sample uploads again.
		Reset();
	}
	OutBlock = ArenaFirst + NextBlock;
	NextBlock += NumBlocks;
	ClutBlocks += NumClutBlocks;
	OutClutBlock = ArenaFirst + ArenaBlocks - ClutBlocks;
	return true;
}

void FGSTextureCache::Release(const void* Key)
{
	Entries.Remove(Key);
}

bool FGSTextureCache::BindTexture(const UTexture2D& Texture, FGSCommandList& List, FGSTex0& OutTex0)
{
	if (const FEntry* Entry = Entries.Find(&Texture))
	{
		OutTex0 = Entry->Tex0;
		return true;
	}
	const FTexturePlatformData& Data = Texture.GetPlatformData();
	if (!Texture.HasValidPlatformData())
	{
		return false;
	}
	const FByteBulkData& BulkData = Data.Mips[0].BulkData;
	if (Data.PixelFormat == PF_P8 || Data.PixelFormat == PF_P4)
	{
		const bool bUploaded = UploadPaletted(&Texture, Data.SizeX, Data.SizeY, Data.PixelFormat,
			static_cast<const uint8*>(BulkData.LockReadOnly()), List, OutTex0);
		BulkData.Unlock();
		return bUploaded;
	}
	if (GetPixelFormatBytes(Data.PixelFormat) != 4)
	{
		return false;
	}
	const uint8* Texels = static_cast<const uint8*>(BulkData.LockReadOnly());
	const int32 NumBytes = Data.SizeX * Data.SizeY * 4;
	TArray<uint8> Rgba;
	Rgba.SetNumUninitialized(NumBytes);
	for (int32 Index = 0; Index < NumBytes; Index += 4)
	{
		const bool bBgra = Data.PixelFormat == PF_B8G8R8A8;
		Rgba[Index + 0] = Texels[Index + (bBgra ? 2 : 0)];
		Rgba[Index + 1] = Texels[Index + 1];
		Rgba[Index + 2] = Texels[Index + (bBgra ? 0 : 2)];
		Rgba[Index + 3] = Texels[Index + 3];
	}
	BulkData.Unlock();
	return Upload(&Texture, Data.SizeX, Data.SizeY, Rgba, false, List, OutTex0);
}

bool FGSTextureCache::BindTexels(const void* Key, int32 Width, int32 Height, TArrayView<const uint8> Rgba, bool bAlpha,
	FGSCommandList& List, FGSTex0& OutTex0)
{
	if (const FEntry* Entry = Entries.Find(Key))
	{
		OutTex0 = Entry->Tex0;
		return true;
	}
	return Upload(Key, Width, Height, Rgba, bAlpha, List, OutTex0);
}

bool FGSTextureCache::Upload(const void* Key, int32 Width, int32 Height, TArrayView<const uint8> Rgba, bool bAlpha,
	FGSCommandList& List, FGSTex0& OutTex0)
{
	if (ArenaBlocks == 0 || Width <= 0 || Height <= 0 || Rgba.Num() < Width * Height * 4)
	{
		return false;
	}
	const int32 SideX = PowerOfTwoSide(Width);
	const int32 SideY = PowerOfTwoSide(Height);
	uint32 Block = 0;
	uint32 ClutBlock = 0;
	if (!Allocate(
			FGSTextureLayout::GetNumBlocks(EGSPixelFormat::PSMCT32, uint32(SideX), uint32(SideY)), 0, Block, ClutBlock))
	{
		return false;
	}

	// Nearest resampling to the power of two sides; alpha 0..255 becomes the GS's 0..0x80.
	TArray<uint8> Texels;
	Texels.SetNumUninitialized(SideX * SideY * 4);
	for (int32 Y = 0; Y < SideY; ++Y)
	{
		const int32 SourceY = (Y * Height) / SideY;
		for (int32 X = 0; X < SideX; ++X)
		{
			const int32 SourceX = (X * Width) / SideX;
			const uint8* Source = &Rgba[((SourceY * Width) + SourceX) * 4];
			uint8* Target = &Texels[((Y * SideX) + X) * 4];
			Target[0] = Source[0];
			Target[1] = Source[1];
			Target[2] = Source[2];
			Target[3] = uint8((uint32(Source[3]) * 0x80u + 127u) / 255u);
		}
	}

	FGSBitBltBuf Destination;
	Destination.DBP = uint16(Block);
	Destination.DBW = FGSTextureLayout::GetBufferWidth(EGSPixelFormat::PSMCT32, uint32(SideX));
	Destination.DPSM = EGSPixelFormat::PSMCT32;
	List.UploadImage(Destination, 0, 0, uint16(SideX), uint16(SideY), Texels);
	List.TexFlush();

	FEntry Entry;
	Entry.Tex0.TBP0 = Destination.DBP;
	Entry.Tex0.TBW = Destination.DBW;
	Entry.Tex0.PSM = EGSPixelFormat::PSMCT32;
	Entry.Tex0.TW = Log2(SideX);
	Entry.Tex0.TH = Log2(SideY);
	Entry.Tex0.bRGBA = bAlpha;
	Entry.Tex0.TFX = EGSTextureFunction::Modulate;
	Entries.Add(Key, Entry);
	++NumUploads;
	OutTex0 = Entry.Tex0;
	return true;
}

bool FGSTextureCache::UploadPaletted(const void* Key, int32 SizeX, int32 SizeY, EPixelFormat Format, const uint8* Data,
	FGSCommandList& List, FGSTex0& OutTex0)
{
	if (ArenaBlocks == 0 || !IsPowerOfTwoSide(SizeX) || !IsPowerOfTwoSide(SizeY))
	{
		return false;
	}
	const EGSPixelFormat Psm = Format == PF_P4 ? EGSPixelFormat::PSMT4 : EGSPixelFormat::PSMT8;
	uint32 Block = 0;
	uint32 ClutBlock = 0;
	if (!Allocate(FGSTextureLayout::GetNumBlocks(Psm, uint32(SizeX), uint32(SizeY)),
			FGSTextureLayout::GetClutBlocks(Psm), Block, ClutBlock))
	{
		return false;
	}

	// The indices as stored (bottom row first, a PSMT4 byte's first texel in its low nibble, as the GS takes them).
	const int32 PaletteSize = GetPixelFormatPaletteSize(Format);
	const int64 IndexBytes = GetPixelFormatDataSize(Format, SizeX, SizeY) - (PaletteSize * 4);
	FGSBitBltBuf Destination;
	Destination.DBP = uint16(Block);
	Destination.DBW = FGSTextureLayout::GetBufferWidth(Psm, uint32(SizeX));
	Destination.DPSM = Psm;
	List.UploadImage(Destination, 0, 0, uint16(SizeX), uint16(SizeY),
		TArrayView<const uint8>(Data + (PaletteSize * 4), int32(IndexBytes)));

	TArray<uint32> Palette;
	Palette.SetNumUninitialized(PaletteSize);
	for (int32 Index = 0; Index < PaletteSize; ++Index)
	{
		const uint8* Entry = &Data[Index * 4];
		Palette[Index] =
			uint32(Entry[0]) | (uint32(Entry[1]) << 8) | (uint32(Entry[2]) << 16) | (uint32(Entry[3]) << 24);
	}
	TArray<uint8> ClutImage;
	uint16 ClutWidth = 0;
	uint16 ClutHeight = 0;
	FGSTextureLayout::MakeClutImage(Palette, ClutImage, ClutWidth, ClutHeight);
	FGSBitBltBuf ClutDestination;
	ClutDestination.DBP = uint16(ClutBlock);
	ClutDestination.DBW = 1;
	ClutDestination.DPSM = EGSPixelFormat::PSMCT32;
	List.UploadImage(ClutDestination, 0, 0, ClutWidth, ClutHeight, ClutImage);
	List.TexFlush();

	FEntry Entry;
	Entry.Tex0.TBP0 = Destination.DBP;
	Entry.Tex0.TBW = Destination.DBW;
	Entry.Tex0.PSM = Psm;
	Entry.Tex0.TW = Log2(SizeX);
	Entry.Tex0.TH = Log2(SizeY);
	Entry.Tex0.bRGBA = false;
	Entry.Tex0.TFX = EGSTextureFunction::Modulate;
	Entry.Tex0.CBP = ClutDestination.DBP;
	Entry.Tex0.CPSM = EGSPixelFormat::PSMCT32;
	Entry.Tex0.CLD = 1;
	Entries.Add(Key, Entry);
	++NumUploads;
	OutTex0 = Entry.Tex0;
	return true;
}
