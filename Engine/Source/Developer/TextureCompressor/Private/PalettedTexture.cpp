#include "PalettedTexture.h"

#include "GSTextureLayout.h"

namespace
{

	[[nodiscard]] uint8 GetChannel(uint32 Color, int32 Index)
	{
		return uint8(Color >> (Index * 8));
	}

	/** Averages the source texels each target texel covers (nearest when enlarging). */
	void Resample(const uint8* Rgba, int32 Width, int32 Height, int32 SizeX, int32 SizeY, TArray<uint32>& OutTexels)
	{
		OutTexels.SetNumUninitialized(SizeX * SizeY);
		for (int32 Y = 0; Y < SizeY; ++Y)
		{
			const int32 Y0 = (Y * Height) / SizeY;
			const int32 Y1 = FMath::Max(Y0 + 1, ((Y + 1) * Height) / SizeY);
			for (int32 X = 0; X < SizeX; ++X)
			{
				const int32 X0 = (X * Width) / SizeX;
				const int32 X1 = FMath::Max(X0 + 1, ((X + 1) * Width) / SizeX);
				uint32 Sum[4] = {0, 0, 0, 0};
				for (int32 SourceY = Y0; SourceY < Y1; ++SourceY)
				{
					for (int32 SourceX = X0; SourceX < X1; ++SourceX)
					{
						const uint8* Texel = &Rgba[((SourceY * Width) + SourceX) * 4];
						for (int32 Index = 0; Index < 4; ++Index)
						{
							Sum[Index] += Texel[Index];
						}
					}
				}
				const uint32 Count = uint32((Y1 - Y0) * (X1 - X0));
				uint32 Color = 0;
				for (int32 Index = 0; Index < 4; ++Index)
				{
					Color |= ((Sum[Index] + (Count / 2)) / Count) << (Index * 8);
				}
				OutTexels[(Y * SizeX) + X] = Color;
			}
		}
	}

	struct FColorCount
	{
		uint32 Color = 0;
		int32 Count = 0;
	};

	/** A median cut box: a range of the colour array. */
	struct FColorBox
	{
		int32 First = 0;
		int32 Num = 0;
		int32 WidestChannel = 0;
		int32 WidestRange = 0;
	};

	void MeasureBox(const TArray<FColorCount>& Colors, FColorBox& Box)
	{
		Box.WidestChannel = 0;
		Box.WidestRange = 0;
		for (int32 Channel = 0; Channel < 4; ++Channel)
		{
			int32 Min = 255;
			int32 Max = 0;
			for (int32 Index = Box.First; Index < Box.First + Box.Num; ++Index)
			{
				const int32 Value = GetChannel(Colors[Index].Color, Channel);
				Min = FMath::Min(Min, Value);
				Max = FMath::Max(Max, Value);
			}
			if (Max - Min > Box.WidestRange)
			{
				Box.WidestRange = Max - Min;
				Box.WidestChannel = Channel;
			}
		}
	}

	/** Median cut of Colors (sorted by value) into at most MaxBoxes boxes. */
	TArray<FColorBox> MedianCut(TArray<FColorCount>& Colors, int32 MaxBoxes)
	{
		TArray<FColorBox> Boxes;
		FColorBox All;
		All.Num = Colors.Num();
		MeasureBox(Colors, All);
		Boxes.Add(All);
		while (Boxes.Num() < MaxBoxes)
		{
			int32 Split = INDEX_NONE;
			for (int32 Index = 0; Index < Boxes.Num(); ++Index)
			{
				if (Boxes[Index].Num > 1 &&
					(Split == INDEX_NONE || Boxes[Index].WidestRange > Boxes[Split].WidestRange))
				{
					Split = Index;
				}
			}
			if (Split == INDEX_NONE)
			{
				break;
			}
			FColorBox& Box = Boxes[Split];
			const int32 SortChannel = Box.WidestChannel;
			FColorCount* Begin = Colors.GetData() + Box.First;
			TArrayView<FColorCount>(Begin, Box.Num)
				.StableSort(
					[SortChannel](const FColorCount& A, const FColorCount& B)
					{
						const uint8 ValueA = GetChannel(A.Color, SortChannel);
						const uint8 ValueB = GetChannel(B.Color, SortChannel);
						return ValueA != ValueB ? ValueA < ValueB : A.Color < B.Color;
					});
			int64 Total = 0;
			for (int32 Index = 0; Index < Box.Num; ++Index)
			{
				Total += Begin[Index].Count;
			}
			// The pixel median, leaving a colour on each side.
			int64 Running = 0;
			int32 Cut = 1;
			for (; Cut < Box.Num - 1; ++Cut)
			{
				Running += Begin[Cut - 1].Count;
				if (Running * 2 >= Total)
				{
					break;
				}
			}
			FColorBox Upper;
			Upper.First = Box.First + Cut;
			Upper.Num = Box.Num - Cut;
			Box.Num = Cut;
			MeasureBox(Colors, Box);
			MeasureBox(Colors, Upper);
			Boxes.Insert(Upper, Split + 1);
		}
		return Boxes;
	}

	/** A texel of the mip chain in linear space, 0..1: its colour (not premultiplied) and its alpha. */
	struct FLinearTexel
	{
		float R = 0.0f;
		float G = 0.0f;
		float B = 0.0f;
		float A = 0.0f;
	};

	[[nodiscard]] float DecodeChannel(uint8 Value, bool bSRGB)
	{
		return bSRGB ? FLinearColor::sRGBToLinearTable[Value] : float(Value) * (1.0f / 255.0f);
	}

	/**
	 * A linear value back to a byte: the sRGB byte whose linear value is nearest (the table increases), so a byte
	 * averaged with itself comes back as itself; a linear channel rounded.
	 */
	[[nodiscard]] uint8 EncodeChannel(float Value, bool bSRGB)
	{
		if (!bSRGB)
		{
			return uint8(FMath::Clamp(FMath::RoundToInt(Value * 255.0f), 0, 255));
		}
		const float* Table = FLinearColor::sRGBToLinearTable;
		int32 Low = 0;
		int32 High = 255;
		while (Low < High)
		{
			const int32 Middle = (Low + High) / 2;
			if (Table[Middle] < Value)
			{
				Low = Middle + 1;
			}
			else
			{
				High = Middle;
			}
		}
		// Low is the first byte at or above Value (or 255): it or the one below it.
		return uint8(Low > 0 && (Value - Table[Low - 1]) <= (Table[Low] - Value) ? Low - 1 : Low);
	}

	[[nodiscard]] FLinearTexel DecodeTexel(uint32 Color, bool bSRGB)
	{
		FLinearTexel Texel;
		Texel.R = DecodeChannel(GetChannel(Color, 0), bSRGB);
		Texel.G = DecodeChannel(GetChannel(Color, 1), bSRGB);
		Texel.B = DecodeChannel(GetChannel(Color, 2), bSRGB);
		Texel.A = float(GetChannel(Color, 3)) * (1.0f / 255.0f);
		return Texel;
	}

	[[nodiscard]] uint32 EncodeTexel(const FLinearTexel& Texel, bool bSRGB)
	{
		return uint32(EncodeChannel(Texel.R, bSRGB)) | (uint32(EncodeChannel(Texel.G, bSRGB)) << 8) |
			(uint32(EncodeChannel(Texel.B, bSRGB)) << 16) | (uint32(EncodeChannel(Texel.A, false)) << 24);
	}

	/**
	 * The next mip of a SizeX x SizeY level: each texel the box average of the four under it, the colour weighted by
	 * their alpha (the plain average when all four are transparent).
	 */
	void Downsample(const TArray<FLinearTexel>& Level, int32 SizeX, int32 SizeY, TArray<FLinearTexel>& OutNext)
	{
		const int32 NextX = SizeX / 2;
		const int32 NextY = SizeY / 2;
		OutNext.SetNum(NextX * NextY);
		for (int32 Y = 0; Y < NextY; ++Y)
		{
			for (int32 X = 0; X < NextX; ++X)
			{
				const FLinearTexel* Under[4] = {&Level[((Y * 2) * SizeX) + (X * 2)],
					&Level[((Y * 2) * SizeX) + (X * 2) + 1], &Level[(((Y * 2) + 1) * SizeX) + (X * 2)],
					&Level[(((Y * 2) + 1) * SizeX) + (X * 2) + 1]};
				FLinearTexel Sum;
				FLinearTexel Weighted;
				for (const FLinearTexel* Texel : Under)
				{
					Sum.R += Texel->R;
					Sum.G += Texel->G;
					Sum.B += Texel->B;
					Sum.A += Texel->A;
					Weighted.R += Texel->R * Texel->A;
					Weighted.G += Texel->G * Texel->A;
					Weighted.B += Texel->B * Texel->A;
				}
				FLinearTexel& Next = OutNext[(Y * NextX) + X];
				if (Sum.A > 0.0f)
				{
					Next.R = Weighted.R / Sum.A;
					Next.G = Weighted.G / Sum.A;
					Next.B = Weighted.B / Sum.A;
				}
				else
				{
					Next.R = Sum.R * 0.25f;
					Next.G = Sum.G * 0.25f;
					Next.B = Sum.B * 0.25f;
				}
				Next.A = Sum.A * 0.25f;
			}
		}
	}

	/** The distinct colours of Texels in value order, with their pixel counts. */
	void CountColors(const TArray<uint32>& Texels, TArray<FColorCount>& OutColors)
	{
		TArray<uint32> Sorted = Texels;
		Sorted.Sort();
		OutColors.Reset();
		for (const uint32 Color : Sorted)
		{
			if (OutColors.Num() > 0 && OutColors.Last().Color == Color)
			{
				++OutColors.Last().Count;
			}
			else
			{
				OutColors.Add({Color, 1});
			}
		}
	}

	/** A and B (each in value order) merged in value order, the counts of a colour in both added. */
	[[nodiscard]] TArray<FColorCount> MergeColors(const TArray<FColorCount>& A, const TArray<FColorCount>& B)
	{
		TArray<FColorCount> Merged;
		Merged.Reserve(A.Num() + B.Num());
		int32 IndexA = 0;
		int32 IndexB = 0;
		while (IndexA < A.Num() || IndexB < B.Num())
		{
			if (IndexB >= B.Num() || (IndexA < A.Num() && A[IndexA].Color < B[IndexB].Color))
			{
				Merged.Add(A[IndexA++]);
			}
			else if (IndexA >= A.Num() || B[IndexB].Color < A[IndexA].Color)
			{
				Merged.Add(B[IndexB++]);
			}
			else
			{
				Merged.Add({A[IndexA].Color, A[IndexA].Count + B[IndexB].Count});
				++IndexA;
				++IndexB;
			}
		}
		return Merged;
	}

	/** A box's colour: the mean of its pixels, per channel, rounded. */
	[[nodiscard]] uint32 BoxMean(const TArray<FColorCount>& Colors, const FColorBox& Box)
	{
		uint64 Sum[4] = {0, 0, 0, 0};
		int64 Count = 0;
		for (int32 Index = Box.First; Index < Box.First + Box.Num; ++Index)
		{
			for (int32 Channel = 0; Channel < 4; ++Channel)
			{
				Sum[Channel] += uint64(GetChannel(Colors[Index].Color, Channel)) * uint64(Colors[Index].Count);
			}
			Count += Colors[Index].Count;
		}
		uint32 Mean = 0;
		for (int32 Channel = 0; Channel < 4; ++Channel)
		{
			Mean |= uint32((Sum[Channel] + uint64(Count / 2)) / uint64(Count)) << (Channel * 8);
		}
		return Mean;
	}

	/** The palette entry nearest Color (squared RGBA distance), the lowest index on a tie. */
	[[nodiscard]] uint8 NearestEntry(uint32 Color, const TArray<uint32>& Palette, int32 NumEntries)
	{
		int32 Best = 0;
		int32 BestDistance = MAX_int32;
		for (int32 Entry = 0; Entry < NumEntries; ++Entry)
		{
			int32 Distance = 0;
			for (int32 Channel = 0; Channel < 4; ++Channel)
			{
				const int32 Difference = int32(GetChannel(Color, Channel)) - int32(GetChannel(Palette[Entry], Channel));
				Distance += Difference * Difference;
			}
			if (Distance < BestDistance)
			{
				BestDistance = Distance;
				Best = Entry;
			}
		}
		return uint8(Best);
	}

	/** Texels as indices (PF_P4: two a byte, the first in the low nibble) into Out, which is zeroed. */
	void WriteIndices(const TArray<uint32>& Texels, const TMap<uint32, uint8>& IndexOf, bool bIndex4, uint8* Out)
	{
		for (int32 Texel = 0; Texel < Texels.Num(); ++Texel)
		{
			const uint8 Index = IndexOf.FindChecked(Texels[Texel]);
			if (bIndex4)
			{
				Out[Texel / 2] |= uint8((Texel % 2) == 0 ? Index : (Index << 4));
			}
			else
			{
				Out[Texel] = Index;
			}
		}
	}

} // namespace

int32 FPalettedTextureBuilder::GetPowerOfTwoSize(int32 Size)
{
	int32 Side = MinSize;
	while (Side < MaxSize && (Side * 2) - Size <= Size - Side)
	{
		Side *= 2;
	}
	return Side;
}

int32 FPalettedTextureBuilder::GetNumMips(int32 SizeX, int32 SizeY)
{
	int32 NumMips = 1;
	for (int32 Shorter = FMath::Min(SizeX, SizeY); Shorter > MinSize && NumMips < MaxMips; Shorter /= 2)
	{
		++NumMips;
	}
	return NumMips;
}

bool FPalettedTextureBuilder::Build(const uint8* Rgba, int32 Width, int32 Height, bool bSRGB, FPalettedTexture& Out)
{
	Out = FPalettedTexture();
	if (Rgba == nullptr || Width <= 0 || Height <= 0)
	{
		return false;
	}
	const int32 SizeX = GetPowerOfTwoSize(Width);
	const int32 SizeY = GetPowerOfTwoSize(Height);
	TArray<uint32> Texels;
	Resample(Rgba, Width, Height, SizeX, SizeY, Texels);

	// The mip chain in linear space, each mip from the one before, then as RGBA8 texels.
	const int32 NumMips = GetNumMips(SizeX, SizeY);
	TArray<TArray<uint32>> MipTexels;
	MipTexels.SetNum(NumMips - 1);
	TArray<FLinearTexel> Linear;
	Linear.SetNum(Texels.Num());
	for (int32 Index = 0; Index < Texels.Num(); ++Index)
	{
		Linear[Index] = DecodeTexel(Texels[Index], bSRGB);
	}
	TArray<FLinearTexel> Next;
	for (int32 Mip = 1; Mip < NumMips; ++Mip)
	{
		Downsample(Linear, SizeX >> (Mip - 1), SizeY >> (Mip - 1), Next);
		Swap(Linear, Next);
		TArray<uint32>& Level = MipTexels[Mip - 1];
		Level.SetNumUninitialized(Linear.Num());
		for (int32 Index = 0; Index < Linear.Num(); ++Index)
		{
			Level[Index] = EncodeTexel(Linear[Index], bSRGB);
		}
	}

	// The distinct colours of mip 0 and of the other mips, in value order, with their pixel counts.
	TArray<FColorCount> Colors;
	CountColors(Texels, Colors);
	Out.NumSourceColors = Colors.Num();
	TArray<FColorCount> MipColors;
	for (const TArray<uint32>& Level : MipTexels)
	{
		TArray<FColorCount> LevelColors;
		CountColors(Level, LevelColors);
		MipColors = MergeColors(MipColors, LevelColors);
	}

	const bool bIndex4 = Colors.Num() <= 16;
	const int32 PaletteSize = bIndex4 ? 16 : 256;
	TArray<uint32> Palette;
	Palette.SetNumZeroed(PaletteSize);
	TMap<uint32, uint8> IndexOf;
	if (Colors.Num() <= PaletteSize)
	{
		// Mip 0 exact: each of its colours an entry (the median cut gives each its own box), the mips' other colours
		// reduced into the free entries, and each of those colours the nearest entry.
		const TArray<FColorBox> Boxes = MedianCut(Colors, PaletteSize);
		int32 NumEntries = 0;
		for (const FColorBox& Box : Boxes)
		{
			Palette[NumEntries] = BoxMean(Colors, Box);
			for (int32 Index = Box.First; Index < Box.First + Box.Num; ++Index)
			{
				IndexOf.Add(Colors[Index].Color, uint8(NumEntries));
			}
			++NumEntries;
		}
		TArray<FColorCount> Others;
		for (const FColorCount& Color : MipColors)
		{
			if (!IndexOf.Contains(Color.Color))
			{
				Others.Add(Color);
			}
		}
		if (Others.Num() > 0 && NumEntries < PaletteSize)
		{
			TArray<FColorCount> Reduced = Others;
			for (const FColorBox& Box : MedianCut(Reduced, PaletteSize - NumEntries))
			{
				Palette[NumEntries++] = BoxMean(Reduced, Box);
			}
		}
		for (const FColorCount& Color : Others)
		{
			IndexOf.Add(Color.Color, NearestEntry(Color.Color, Palette, NumEntries));
		}
	}
	else
	{
		// More than 256 colours: mip 0's and the mips' reduced together, each colour to its box.
		TArray<FColorCount> All = MergeColors(Colors, MipColors);
		const TArray<FColorBox> Boxes = MedianCut(All, PaletteSize);
		for (int32 BoxIndex = 0; BoxIndex < Boxes.Num(); ++BoxIndex)
		{
			const FColorBox& Box = Boxes[BoxIndex];
			Palette[BoxIndex] = BoxMean(All, Box);
			for (int32 Index = Box.First; Index < Box.First + Box.Num; ++Index)
			{
				IndexOf.Add(All[Index].Color, uint8(BoxIndex));
			}
		}
	}

	Out.SizeX = SizeX;
	Out.SizeY = SizeY;
	Out.Format = bIndex4 ? PF_P4 : PF_P8;
	Out.Data.SetNumZeroed(int32(GetPixelFormatDataSize(Out.Format, SizeX, SizeY)));
	Out.Palette.SetNumUninitialized(PaletteSize);
	for (int32 Index = 0; Index < PaletteSize; ++Index)
	{
		uint32 Color = 0;
		for (int32 Channel = 0; Channel < 4; ++Channel)
		{
			Color |= uint32(GetChannel(Palette[Index], Channel)) << (Channel * 8);
		}
		Out.Palette[Index] = Color;
	}
	// The CLUT as the GS takes it: the runtime uploads these bytes as they are.
	TArray<uint8> Clut;
	uint16 ClutWidth = 0;
	uint16 ClutHeight = 0;
	FGSTextureLayout::MakeClutImage(Out.Palette, Clut, ClutWidth, ClutHeight);
	check(Clut.Num() == PaletteSize * 4);
	FMemory::Memcpy(Out.Data.GetData(), Clut.GetData(), SIZE_T(Clut.Num()));
	WriteIndices(Texels, IndexOf, bIndex4, Out.Data.GetData() + (PaletteSize * 4));
	Out.Mips.SetNum(NumMips - 1);
	for (int32 Mip = 1; Mip < NumMips; ++Mip)
	{
		TArray<uint8>& Indices = Out.Mips[Mip - 1];
		Indices.SetNumZeroed(int32(GetPixelFormatMipDataSize(Out.Format, SizeX >> Mip, SizeY >> Mip, Mip)));
		WriteIndices(MipTexels[Mip - 1], IndexOf, bIndex4, Indices.GetData());
	}
	return true;
}
