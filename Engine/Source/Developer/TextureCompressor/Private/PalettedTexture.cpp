#include "PalettedTexture.h"

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

bool FPalettedTextureBuilder::Build(const uint8* Rgba, int32 Width, int32 Height, FPalettedTexture& Out)
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

	// The distinct colours in value order, with their pixel counts.
	TArray<uint32> Sorted = Texels;
	Sorted.Sort();
	TArray<FColorCount> Colors;
	for (const uint32 Color : Sorted)
	{
		if (Colors.Num() > 0 && Colors.Last().Color == Color)
		{
			++Colors.Last().Count;
		}
		else
		{
			Colors.Add({Color, 1});
		}
	}
	Out.NumSourceColors = Colors.Num();

	const bool bIndex4 = Colors.Num() <= 16;
	const int32 PaletteSize = bIndex4 ? 16 : 256;
	TArray<FColorBox> Boxes = MedianCut(Colors, PaletteSize);
	TArray<uint32> Palette;
	Palette.SetNumZeroed(PaletteSize);
	TMap<uint32, uint8> IndexOf;
	for (int32 BoxIndex = 0; BoxIndex < Boxes.Num(); ++BoxIndex)
	{
		const FColorBox& Box = Boxes[BoxIndex];
		uint64 Sum[4] = {0, 0, 0, 0};
		int64 Count = 0;
		for (int32 Index = Box.First; Index < Box.First + Box.Num; ++Index)
		{
			for (int32 Channel = 0; Channel < 4; ++Channel)
			{
				Sum[Channel] += uint64(GetChannel(Colors[Index].Color, Channel)) * uint64(Colors[Index].Count);
			}
			Count += Colors[Index].Count;
			IndexOf.Add(Colors[Index].Color, uint8(BoxIndex));
		}
		uint32 Mean = 0;
		for (int32 Channel = 0; Channel < 4; ++Channel)
		{
			Mean |= uint32((Sum[Channel] + uint64(Count / 2)) / uint64(Count)) << (Channel * 8);
		}
		Palette[BoxIndex] = Mean;
	}

	Out.SizeX = SizeX;
	Out.SizeY = SizeY;
	Out.Format = bIndex4 ? PF_P4 : PF_P8;
	Out.Data.SetNumZeroed(int32(GetPixelFormatDataSize(Out.Format, SizeX, SizeY)));
	for (int32 Index = 0; Index < PaletteSize; ++Index)
	{
		for (int32 Channel = 0; Channel < 4; ++Channel)
		{
			Out.Data[(Index * 4) + Channel] = GetChannel(Palette[Index], Channel);
		}
	}
	uint8* Indices = Out.Data.GetData() + (PaletteSize * 4);
	for (int32 Texel = 0; Texel < Texels.Num(); ++Texel)
	{
		const uint8 Index = IndexOf.FindChecked(Texels[Texel]);
		if (bIndex4)
		{
			Indices[Texel / 2] |= uint8((Texel % 2) == 0 ? Index : (Index << 4));
		}
		else
		{
			Indices[Texel] = Index;
		}
	}
	return true;
}
