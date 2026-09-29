#include "GSTextureCache.h"

#include "Engine/Texture2D.h"
#include "HAL/LowLevelMemTracker.h"
#include "Templates/AlignmentTemplates.h"

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

	/** The bytes of a Width x Height image of a GS texture format. */
	[[nodiscard]] int32 GetImageBytes(EGSPixelFormat Psm, int32 Width, int32 Height)
	{
		return (Width * Height * int32(GSBitsPerPixel(Psm))) / 8;
	}

	/** The texels of a UTexture2D's mips, locked for reading while the scope lives. */
	class FScopedMipLock
	{
	public:
		FScopedMipLock() = default;
		FScopedMipLock(const FScopedMipLock&) = delete;
		FScopedMipLock& operator=(const FScopedMipLock&) = delete;
		~FScopedMipLock()
		{
			for (int32 Index = 0; Index < NumLocked; ++Index)
			{
				Locked[Index]->Unlock();
			}
		}

		[[nodiscard]] const uint8* Lock(const FByteBulkData& BulkData)
		{
			check(NumLocked < FGSTextureLayout::MaxLevels);
			Locked[NumLocked++] = &BulkData;
			return static_cast<const uint8*>(BulkData.LockReadOnly());
		}

	private:
		const FByteBulkData* Locked[FGSTextureLayout::MaxLevels] = {};
		int32 NumLocked = 0;
	};

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
	BlockBits.Reset();
	BlockBits.SetNumZeroed(int32((ArenaBlocks + 31) / 32));
	UsedBlocks = 0;
	ClutSlots[0] = FClutSlot();
	ClutSlots[1] = FClutSlot();
	LastIndex4Slot = 0;
}

void FGSTextureCache::SetTextureConverter(FTextureConverter InConverter)
{
	Converter = InConverter;
	Converted.Reset();
	Reset();
}

void FGSTextureCache::BeginFrame()
{
	++FrameNumber;
	Counters = FFrameCounters();
}

bool FGSTextureCache::Release(const void* Key)
{
	const bool bResident = Entries.Contains(Key);
	Evict(Key);
	Converted.Remove(Key);
	return bResident;
}

void FGSTextureCache::Evict(const void* Key)
{
	if (const FEntry* Entry = Entries.Find(Key))
	{
		MarkBlocks(Entry->FirstBlock, Entry->NumBlocks, false);
		Entries.Remove(Key);
	}
}

void FGSTextureCache::MarkBlocks(uint32 FirstBlock, uint32 NumBlocks, bool bUsed)
{
	for (uint32 Block = FirstBlock; Block < FirstBlock + NumBlocks; ++Block)
	{
		uint32& Word = BlockBits[int32(Block / 32)];
		const uint32 Bit = 1u << (Block % 32);
		Word = bUsed ? (Word | Bit) : (Word & ~Bit);
	}
	UsedBlocks = bUsed ? UsedBlocks + NumBlocks : UsedBlocks - NumBlocks;
}

bool FGSTextureCache::FindFreeRun(uint32 NumBlocks, uint32 Alignment, uint32& OutFirstBlock) const
{
	uint32 Start = 0;
	while (Start + NumBlocks <= ArenaBlocks)
	{
		// The last taken block of the run, if any: the next try starts after it.
		int64 Taken = -1;
		for (uint32 Block = Start + NumBlocks; Block-- > Start;)
		{
			if ((BlockBits[int32(Block / 32)] & (1u << (Block % 32))) != 0)
			{
				Taken = int64(Block);
				break;
			}
		}
		if (Taken < 0)
		{
			OutFirstBlock = Start;
			return true;
		}
		// The arena starts on a page, so aligning within it aligns the block address.
		Start = Align(uint32(Taken) + 1, Alignment);
	}
	return false;
}

bool FGSTextureCache::Allocate(uint32 NumBlocks, uint32 Alignment, uint32& OutFirstBlock)
{
	if (NumBlocks == 0 || NumBlocks > ArenaBlocks)
	{
		return false;
	}
	for (;;)
	{
		if (FindFreeRun(NumBlocks, Alignment, OutFirstBlock))
		{
			MarkBlocks(OutFirstBlock, NumBlocks, true);
			return true;
		}
		// No room: the least recently bound texture this frame has not bound goes.
		const void* Victim = nullptr;
		uint64 Oldest = MAX_uint64;
		for (const TPair<const void*, FEntry>& Pair : Entries)
		{
			if (Pair.Value.LastFrame != FrameNumber && Pair.Value.LastUse < Oldest)
			{
				Oldest = Pair.Value.LastUse;
				Victim = Pair.Key;
			}
		}
		if (Victim == nullptr)
		{
			return false;
		}
		Evict(Victim);
		++Counters.Evictions;
	}
}

void FGSTextureCache::InvalidateClutSlots(uint32 FirstBlock, uint32 NumBlocks)
{
	for (FClutSlot& Slot : ClutSlots)
	{
		const uint32 ClutBlocks = Slot.bIndex8 ? 4u : 1u;
		if (Slot.bKnown && uint32(Slot.Cbp) < FirstBlock + NumBlocks && FirstBlock < uint32(Slot.Cbp) + ClutBlocks)
		{
			Slot.bLoaded = false;
		}
	}
}

bool FGSTextureCache::FitsBudget(int32 Bytes) const
{
	return UploadBudgetBytes == 0 || Counters.UploadBytes == 0 || Counters.UploadBytes + Bytes <= UploadBudgetBytes;
}

int32 FGSTextureCache::GetUploadBytes(const FTexels& Texels, int32 FirstLevel, int32 LastLevel, bool bClut)
{
	int32 Bytes = 0;
	for (int32 Level = FirstLevel; Level <= LastLevel; ++Level)
	{
		Bytes += GetImageBytes(Texels.Psm, Texels.SizeX >> Level, Texels.SizeY >> Level);
	}
	if (bClut && Texels.Clut != nullptr)
	{
		Bytes += (Texels.Psm == EGSPixelFormat::PSMT8 ? 256 : 16) * 4;
	}
	return Bytes;
}

FColor FGSTextureCache::GetAverageColor(const FTexels& Texels)
{
	const int32 Level = Texels.NumLevels - 1;
	const int32 Width = Texels.SizeX >> Level;
	const int32 Height = Texels.SizeY >> Level;
	const uint8* Data = Texels.Levels[Level];
	uint32 Sum[4] = {0, 0, 0, 0};
	const int32 Count = Width * Height;
	for (int32 Texel = 0; Texel < Count; ++Texel)
	{
		uint8 Color[4] = {255, 255, 255, 255};
		// PSMCT32 words (a texel's, or a CLUT entry's) hold the GS's alpha (0..0x80).
		const auto GSAlpha = [](uint8 Alpha) { return uint8(FMath::Min(255u, (uint32(Alpha) * 255u + 64u) / 128u)); };
		if (Texels.Clut != nullptr)
		{
			const bool bIndex8 = Texels.Psm == EGSPixelFormat::PSMT8;
			const int32 Entry = !bIndex8 ? (Data[Texel / 2] >> ((Texel % 2) * 4)) & 0xf : Data[Texel];
			const uint8* Word = &Texels.Clut[FGSTextureLayout::GetClutImagePosition(Entry, bIndex8) * 4];
			FMemory::Memcpy(Color, Word, 3);
			Color[3] = GSAlpha(Word[3]);
		}
		else
		{
			FMemory::Memcpy(Color, &Data[Texel * 4], 3);
			Color[3] = Texels.bAlpha ? GSAlpha(Data[(Texel * 4) + 3]) : 255;
		}
		for (int32 Channel = 0; Channel < 4; ++Channel)
		{
			Sum[Channel] += Color[Channel];
		}
	}
	const auto Mean = [Count](uint32 Value) { return uint8((Value + uint32(Count / 2)) / uint32(Count)); };
	return FColor(Mean(Sum[0]), Mean(Sum[1]), Mean(Sum[2]), Mean(Sum[3]));
}

bool FGSTextureCache::GetTexels(
	const UTexture2D& Texture, const uint8* const* MipData, FTexels& OutTexels, TArray<uint8>& OutScratch)
{
	if (!Texture.HasValidPlatformData())
	{
		return false;
	}
	const FTexturePlatformData& Data = Texture.GetPlatformData();
	const auto FillPaletted = [&OutTexels](int32 SizeX, int32 SizeY, EPixelFormat Format, const uint8* Level0)
	{
		OutTexels.SizeX = SizeX;
		OutTexels.SizeY = SizeY;
		OutTexels.Psm = Format == PF_P4 ? EGSPixelFormat::PSMT4 : EGSPixelFormat::PSMT8;
		// The cooked blob (N23): the CLUT image, then level 0's indices, both uploaded where they are.
		OutTexels.Clut = Level0;
		OutTexels.Levels[0] = Level0 + (GetPixelFormatPaletteSize(Format) * 4);
		OutTexels.NumLevels = 1;
		OutTexels.bAlpha = true;
		OutTexels.bInPlace = true;
	};
	// A mip the GS can sample: the next half size, at least MinTextureSize a side, whole.
	const auto IsLevel =
		[](int32 SizeX, int32 SizeY, EPixelFormat Format, int32 Level, int32 MipSizeX, int32 MipSizeY, int64 NumBytes)
	{
		return Level < FGSTextureLayout::MaxLevels && MipSizeX == (SizeX >> Level) && MipSizeY == (SizeY >> Level) &&
			MipSizeX >= MinTextureSize && MipSizeY >= MinTextureSize &&
			NumBytes == GetPixelFormatMipDataSize(Format, MipSizeX, MipSizeY, Level);
	};
	if (Data.PixelFormat == PF_P8 || Data.PixelFormat == PF_P4)
	{
		if (!IsPowerOfTwoSide(Data.SizeX) || !IsPowerOfTwoSide(Data.SizeY))
		{
			return false;
		}
		FillPaletted(Data.SizeX, Data.SizeY, Data.PixelFormat, MipData[0]);
		for (int32 Level = 1; Level < Data.Mips.Num(); ++Level)
		{
			const FTexture2DMipMap& Mip = Data.Mips[Level];
			if (!IsLevel(Data.SizeX, Data.SizeY, Data.PixelFormat, Level, Mip.SizeX, Mip.SizeY,
					Mip.BulkData.GetBulkDataSize()))
			{
				break;
			}
			OutTexels.Levels[Level] = MipData[Level];
			OutTexels.NumLevels = Level + 1;
		}
		return true;
	}
	if (GetPixelFormatBytes(Data.PixelFormat) != 4)
	{
		return false;
	}
	const FConverted* Done = Converted.Find(&Texture);
	const uint8* Texels = MipData[0];
	const int32 NumBytes = Data.SizeX * Data.SizeY * 4;
	TArray<uint8> Rgba;
	if (Done == nullptr)
	{
		Rgba.SetNumUninitialized(NumBytes);
		const bool bBgra = Data.PixelFormat == PF_B8G8R8A8;
		for (int32 Index = 0; Index < NumBytes; Index += 4)
		{
			Rgba[Index + 0] = Texels[Index + (bBgra ? 2 : 0)];
			Rgba[Index + 1] = Texels[Index + 1];
			Rgba[Index + 2] = Texels[Index + (bBgra ? 0 : 2)];
			Rgba[Index + 3] = Texels[Index + 3];
		}
		FConverted Result;
		if (Converter != nullptr && Converter(Rgba.GetData(), Data.SizeX, Data.SizeY, Texture.SRGB != 0, Result) &&
			(Result.Format == PF_P8 || Result.Format == PF_P4) && IsPowerOfTwoSide(Result.SizeX) &&
			IsPowerOfTwoSide(Result.SizeY) &&
			Result.Data.Num() == GetPixelFormatDataSize(Result.Format, Result.SizeX, Result.SizeY))
		{
			Done = &Converted.Add(&Texture, MoveTemp(Result));
		}
	}
	if (Done != nullptr)
	{
		FillPaletted(Done->SizeX, Done->SizeY, Done->Format, Done->Data.GetData());
		for (int32 Level = 1; Level <= Done->Mips.Num(); ++Level)
		{
			const TArray<uint8>& Mip = Done->Mips[Level - 1];
			if (!IsLevel(Done->SizeX, Done->SizeY, Done->Format, Level, Done->SizeX >> Level, Done->SizeY >> Level,
					Mip.Num()))
			{
				break;
			}
			OutTexels.Levels[Level] = Mip.GetData();
			OutTexels.NumLevels = Level + 1;
		}
		return true;
	}

	// PSMCT32 level 0, resampled nearest to the power of two sides; the alpha ignored (TCC RGB).
	const int32 SideX = PowerOfTwoSide(Data.SizeX);
	const int32 SideY = PowerOfTwoSide(Data.SizeY);
	OutScratch.SetNumUninitialized(SideX * SideY * 4);
	for (int32 Y = 0; Y < SideY; ++Y)
	{
		const int32 SourceY = (Y * Data.SizeY) / SideY;
		for (int32 X = 0; X < SideX; ++X)
		{
			const uint8* Source = &Rgba[((SourceY * Data.SizeX) + ((X * Data.SizeX) / SideX)) * 4];
			uint8* Target = &OutScratch[((Y * SideX) + X) * 4];
			Target[0] = Source[0];
			Target[1] = Source[1];
			Target[2] = Source[2];
			Target[3] = uint8((uint32(Source[3]) * 0x80u + 127u) / 255u);
		}
	}
	OutTexels.SizeX = SideX;
	OutTexels.SizeY = SideY;
	OutTexels.Psm = EGSPixelFormat::PSMCT32;
	OutTexels.Levels[0] = OutScratch.GetData();
	OutTexels.NumLevels = 1;
	OutTexels.bAlpha = false;
	return true;
}

bool FGSTextureCache::BindTexture(const UTexture2D& Texture, FGSCommandList& List, FGSTextureBinding& OutBinding)
{
	LLM_SCOPE(ELLMTag::Textures);
	OutBinding = FGSTextureBinding();
	if (ArenaBlocks == 0)
	{
		return false;
	}
	if (FEntry* Entry = Entries.Find(&Texture))
	{
		if (Entry->FirstResidentLevel == 0)
		{
			Entry->LastFrame = FrameNumber;
			Entry->LastUse = ++UseCounter;
			MakeBinding(*Entry, OutBinding);
			return true;
		}
	}
	// Something to upload (the texture, or the rest of it), or a flat stand-in: the texels are needed.
	FScopedMipLock Lock;
	const FTexturePlatformData& Data = Texture.GetPlatformData();
	const uint8* MipData[FGSTextureLayout::MaxLevels] = {};
	for (int32 Level = 0; Level < Data.Mips.Num() && Level < FGSTextureLayout::MaxLevels; ++Level)
	{
		MipData[Level] = Lock.Lock(Data.Mips[Level].BulkData);
	}
	FTexels Texels;
	TArray<uint8> Scratch;
	if (!GetTexels(Texture, MipData, Texels, Scratch))
	{
		return false;
	}
	return Bind(&Texture, Texels, true, List, OutBinding);
}

bool FGSTextureCache::BindTexels(const void* Key, int32 Width, int32 Height, TArrayView<const uint8> Rgba, bool bAlpha,
	FGSCommandList& List, FGSTex0& OutTex0)
{
	if (ArenaBlocks == 0)
	{
		return false;
	}
	FGSTextureBinding Binding;
	if (FEntry* Entry = Entries.Find(Key))
	{
		Entry->LastFrame = FrameNumber;
		Entry->LastUse = ++UseCounter;
		MakeBinding(*Entry, Binding);
		OutTex0 = Binding.Tex0;
		return true;
	}
	if (Width <= 0 || Height <= 0 || Rgba.Num() < Width * Height * 4)
	{
		return false;
	}
	// Nearest resampling to the power of two sides; alpha 0..255 becomes the GS's 0..0x80.
	const int32 SideX = PowerOfTwoSide(Width);
	const int32 SideY = PowerOfTwoSide(Height);
	TArray<uint8> Scratch;
	Scratch.SetNumUninitialized(SideX * SideY * 4);
	for (int32 Y = 0; Y < SideY; ++Y)
	{
		const int32 SourceY = (Y * Height) / SideY;
		for (int32 X = 0; X < SideX; ++X)
		{
			const uint8* Source = &Rgba[((SourceY * Width) + ((X * Width) / SideX)) * 4];
			uint8* Target = &Scratch[((Y * SideX) + X) * 4];
			Target[0] = Source[0];
			Target[1] = Source[1];
			Target[2] = Source[2];
			Target[3] = uint8((uint32(Source[3]) * 0x80u + 127u) / 255u);
		}
	}
	FTexels Texels;
	Texels.SizeX = SideX;
	Texels.SizeY = SideY;
	Texels.Psm = EGSPixelFormat::PSMCT32;
	Texels.Levels[0] = Scratch.GetData();
	Texels.bAlpha = bAlpha;
	if (!Bind(Key, Texels, false, List, Binding) || Binding.bFlat)
	{
		return false;
	}
	OutTex0 = Binding.Tex0;
	return true;
}

bool FGSTextureCache::Bind(
	const void* Key, const FTexels& Texels, bool bBudgeted, FGSCommandList& List, FGSTextureBinding& OutBinding)
{
	const int32 LastLevel = Texels.NumLevels - 1;
	const auto StandIn = [this, &Texels, &OutBinding]()
	{
		OutBinding.bFlat = true;
		OutBinding.FlatColor = GetAverageColor(Texels);
		++Counters.Deferred;
		return true;
	};
	FEntry* Entry = Entries.Find(Key);
	if (Entry == nullptr)
	{
		// All of it within the budget, or its smallest level and its CLUT, or nothing yet.
		const bool bWhole = !bBudgeted || FitsBudget(GetUploadBytes(Texels, 0, LastLevel, true));
		if (!bWhole && (LastLevel == 0 || !FitsBudget(GetUploadBytes(Texels, LastLevel, LastLevel, true))))
		{
			return StandIn();
		}
		FGSTextureLayout::FFootprint Footprint;
		FGSTextureLayout::GetFootprint(
			Texels.Psm, uint32(Texels.SizeX), uint32(Texels.SizeY), Texels.NumLevels, Footprint);
		uint32 FirstBlock = 0;
		if (!Allocate(Footprint.NumBlocks, Footprint.Alignment, FirstBlock))
		{
			return StandIn();
		}
		// Whatever the blocks held is gone: a CLUT the GS's buffer names from them no longer matches it.
		InvalidateClutSlots(ArenaFirst + FirstBlock, Footprint.NumBlocks);
		FEntry NewEntry;
		NewEntry.FirstBlock = FirstBlock;
		NewEntry.NumBlocks = Footprint.NumBlocks;
		NewEntry.Footprint = Footprint;
		NewEntry.Tex0.PSM = Texels.Psm;
		NewEntry.Tex0.TW = Log2(Texels.SizeX);
		NewEntry.Tex0.TH = Log2(Texels.SizeY);
		NewEntry.Tex0.TFX = EGSTextureFunction::Modulate;
		if (Texels.Clut != nullptr)
		{
			// The CLUT keeps the cooked alpha (0..0x80): TCC takes it (MODULATE: the vertex alpha times the
			// texel's), so a paletted texture's cut-outs and translucency reach the blend; an opaque texel is 0x80,
			// which changes nothing.
			NewEntry.Tex0.bRGBA = true;
			NewEntry.Tex0.CBP = uint16(ArenaFirst + FirstBlock + Footprint.ClutBlock);
			NewEntry.Tex0.CPSM = EGSPixelFormat::PSMCT32;
		}
		else
		{
			NewEntry.Tex0.bRGBA = Texels.bAlpha;
		}
		NewEntry.FirstResidentLevel = bWhole ? 0 : LastLevel;
		Entry = &Entries.Add(Key, NewEntry);
		UploadLevels(*Entry, Texels, Entry->FirstResidentLevel, LastLevel, true, List);
		++Counters.Uploads;
		Counters.Deferred += bWhole ? 0 : 1;
	}
	else if (Entry->FirstResidentLevel > 0)
	{
		// The smallest level is resident: the others when the budget allows.
		const int32 Missing = Entry->FirstResidentLevel - 1;
		if (!bBudgeted || FitsBudget(GetUploadBytes(Texels, 0, Missing, false)))
		{
			UploadLevels(*Entry, Texels, 0, Missing, false, List);
			Entry->FirstResidentLevel = 0;
			++Counters.Uploads;
		}
		else
		{
			++Counters.Deferred;
		}
	}
	Entry->LastFrame = FrameNumber;
	Entry->LastUse = ++UseCounter;
	MakeBinding(*Entry, OutBinding);
	return true;
}

void FGSTextureCache::UploadLevels(
	const FEntry& Entry, const FTexels& Texels, int32 FirstLevel, int32 LastLevel, bool bClut, FGSCommandList& List)
{
	const uint32 Base = ArenaFirst + Entry.FirstBlock;
	for (int32 Level = FirstLevel; Level <= LastLevel; ++Level)
	{
		// The texels as stored (bottom row first, a PSMT4 byte's first texel in its low nibble, as the GS takes them).
		const int32 Width = Texels.SizeX >> Level;
		const int32 Height = Texels.SizeY >> Level;
		FGSBitBltBuf Destination;
		Destination.DBP = uint16(Base + Entry.Footprint.LevelBlock[Level]);
		Destination.DBW = Entry.Footprint.LevelBufferWidth[Level];
		Destination.DPSM = Texels.Psm;
		const TArrayView<const uint8> Pixels(Texels.Levels[Level], GetImageBytes(Texels.Psm, Width, Height));
		if (Texels.bInPlace)
		{
			List.UploadImageInPlace(Destination, 0, 0, uint16(Width), uint16(Height), Pixels);
		}
		else
		{
			List.UploadImage(Destination, 0, 0, uint16(Width), uint16(Height), Pixels);
		}
	}
	if (bClut && Texels.Clut != nullptr)
	{
		// The cooked CLUT image as it is: CSM1 order and the GS's alpha already.
		const bool bIndex8 = Texels.Psm == EGSPixelFormat::PSMT8;
		uint16 ClutWidth = 0;
		uint16 ClutHeight = 0;
		FGSTextureLayout::GetClutImageSize(bIndex8, ClutWidth, ClutHeight);
		FGSBitBltBuf ClutDestination;
		ClutDestination.DBP = uint16(Base + Entry.Footprint.ClutBlock);
		ClutDestination.DBW = 1;
		ClutDestination.DPSM = EGSPixelFormat::PSMCT32;
		List.UploadImageInPlace(ClutDestination, 0, 0, ClutWidth, ClutHeight,
			TArrayView<const uint8>(Texels.Clut, int32(ClutWidth) * ClutHeight * 4));
	}
	List.TexFlush();
	Counters.UploadBytes += GetUploadBytes(Texels, FirstLevel, LastLevel, bClut);
}

void FGSTextureCache::MakeBinding(const FEntry& Entry, FGSTextureBinding& OutBinding)
{
	const FGSTextureLayout::FFootprint& Footprint = Entry.Footprint;
	const uint32 Base = ArenaFirst + Entry.FirstBlock;
	const int32 First = Entry.FirstResidentLevel;
	OutBinding.Tex0 = Entry.Tex0;
	OutBinding.Tex0.TBP0 = uint16(Base + Footprint.LevelBlock[First]);
	OutBinding.Tex0.TBW = Footprint.LevelBufferWidth[First];
	OutBinding.Tex0.TW = uint8(Entry.Tex0.TW - First);
	OutBinding.Tex0.TH = uint8(Entry.Tex0.TH - First);
	OutBinding.NumLevels = Footprint.NumLevels - First;
	// MIPTBP1 / MIPTBP2 for levels 1 to 6 of what is bound, the last level repeated past the chain.
	for (int32 Level = 1; Level < FGSTextureLayout::MaxLevels; ++Level)
	{
		const int32 Source = FMath::Min(First + Level, Footprint.NumLevels - 1);
		FGSMipTbp& MipTbp = Level <= 3 ? OutBinding.MipTbp1 : OutBinding.MipTbp2;
		MipTbp.TBP[(Level - 1) % 3] = uint16(Base + Footprint.LevelBlock[Source]);
		MipTbp.TBW[(Level - 1) % 3] = Footprint.LevelBufferWidth[Source];
	}
	OutBinding.bFlat = false;
	if (Entry.Tex0.PSM != EGSPixelFormat::PSMT8 && Entry.Tex0.PSM != EGSPixelFormat::PSMT4)
	{
		return;
	}

	// The CLUT's load control: a PSMT8 CLUT in slot 0 (CBP0), a PSMT4 one in the slot that holds it or the one not
	// bound last (CSA 0 with CBP0, CSA 1 with CBP1).
	const bool bIndex8 = Entry.Tex0.PSM == EGSPixelFormat::PSMT8;
	const uint16 Cbp = Entry.Tex0.CBP;
	const auto Holds = [Cbp, bIndex8](const FClutSlot& Slot)
	{ return Slot.bKnown && Slot.bLoaded && Slot.Cbp == Cbp && Slot.bIndex8 == bIndex8; };
	int32 SlotIndex = 0;
	if (!bIndex8)
	{
		SlotIndex = Holds(ClutSlots[0]) ? 0 : (Holds(ClutSlots[1]) ? 1 : 1 - LastIndex4Slot);
		LastIndex4Slot = SlotIndex;
	}
	FClutSlot& Slot = ClutSlots[SlotIndex];
	const bool bHeld = Holds(Slot);
	// CLD 4 / 5 loads when the register differs; a register that names this CLUT while the buffer does not hold it
	// (or one never written) needs CLD 2 / 3, which always loads.
	const bool bForce = !bHeld && (!Slot.bKnown || Slot.Cbp == Cbp);
	OutBinding.Tex0.CSA = uint8(SlotIndex);
	OutBinding.Tex0.CLD = uint8((bForce ? 2 : 4) + SlotIndex);
	if (bHeld)
	{
		return;
	}
	++Counters.ClutLoads;
	Slot.bKnown = true;
	Slot.bLoaded = true;
	Slot.bIndex8 = bIndex8;
	Slot.Cbp = Cbp;
	// The entries the load wrote: a PSMT8 CLUT all of them (slot 1's too), a PSMT4 one at CSA 1 part of a PSMT8 one.
	if (bIndex8)
	{
		ClutSlots[1].bLoaded = false;
	}
	else if (SlotIndex == 1 && ClutSlots[0].bIndex8)
	{
		ClutSlots[0].bLoaded = false;
	}
}
