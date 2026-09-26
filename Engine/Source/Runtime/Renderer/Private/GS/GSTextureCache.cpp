#include "GSTextureCache.h"

#include "Engine/Texture2D.h"

namespace
{

	/** 64-word blocks in a GS page. */
	constexpr uint32 BlocksPerPage = 32;

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
	if (!Texture.HasValidPlatformData() || Data.Mips.Num() == 0 ||
		(Data.PixelFormat != PF_R8G8B8A8 && Data.PixelFormat != PF_B8G8R8A8))
	{
		return false;
	}
	const FByteBulkData& BulkData = Data.Mips[0].BulkData;
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
	// A buffer at least 64 texels wide (TBW's unit), in whole pages of 64 x 32 PSMCT32 texels.
	const uint32 BufferWidth = uint32(FMath::Max(1, SideX / 64));
	const uint32 NumBlocks = BufferWidth * uint32((SideY + 31) / 32) * BlocksPerPage;
	if (NumBlocks > ArenaBlocks)
	{
		return false;
	}
	if (NextBlock + NumBlocks > ArenaBlocks)
	{
		// Full: start over; what the next draws sample uploads again.
		Reset();
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
	Destination.DBP = uint16(ArenaFirst + NextBlock);
	Destination.DBW = uint8(BufferWidth);
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
	NextBlock += NumBlocks;
	++NumUploads;
	OutTex0 = Entry.Tex0;
	return true;
}
