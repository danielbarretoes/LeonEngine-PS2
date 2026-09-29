#include "CoreMinimal.h"
#include "GSTextureLayout.h"
#include "Misc/AutomationTest.h"
#include "PalettedTexture.h"

#if WITH_DEV_AUTOMATION_TESTS

// The PS2 cook's texture conversion (Docs/PLANS/ps2-engine.md E3): sizes, the exact palettes, and median cut.

namespace
{

	/** The RGBA8 colour of texel Index of a paletted texture. */
	uint32 PalettedTexel(const FPalettedTexture& Texture, int32 Index)
	{
		const int32 PaletteSize = GetPixelFormatPaletteSize(Texture.Format);
		const uint8* Indices = Texture.Data.GetData() + (PaletteSize * 4);
		const int32 Entry =
			Texture.Format == PF_P4 ? ((Indices[Index / 2] >> ((Index % 2) * 4)) & 0xf) : Indices[Index];
		return Texture.Palette[Entry];
	}

	uint32 PackColor(uint8 R, uint8 G, uint8 B, uint8 A = 255)
	{
		return uint32(R) | (uint32(G) << 8) | (uint32(B) << 16) | (uint32(A) << 24);
	}

	TArray<uint8> ToBytes(const TArray<uint32>& Colors)
	{
		TArray<uint8> Bytes;
		for (const uint32 Color : Colors)
		{
			for (int32 Channel = 0; Channel < 4; ++Channel)
			{
				Bytes.Add(uint8(Color >> (Channel * 8)));
			}
		}
		return Bytes;
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPalettedSizesTest, "System.TextureCompressor.Paletted.PowerOfTwoSizes",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FPalettedSizesTest::RunTest(const FString& Parameters)
{
	// The nearest power of two between 8 and 256, the larger on a tie.
	TestEqual("1", FPalettedTextureBuilder::GetPowerOfTwoSize(1), 8);
	TestEqual("20", FPalettedTextureBuilder::GetPowerOfTwoSize(20), 16);
	TestEqual("95", FPalettedTextureBuilder::GetPowerOfTwoSize(95), 64);
	TestEqual("96 (a tie)", FPalettedTextureBuilder::GetPowerOfTwoSize(96), 128);
	TestEqual("128", FPalettedTextureBuilder::GetPowerOfTwoSize(128), 128);
	TestEqual("1000", FPalettedTextureBuilder::GetPowerOfTwoSize(1000), 256);

	// A 300 x 20 texture becomes 256 x 16, each texel the mean of what it covers.
	TArray<uint32> Colors;
	for (int32 Y = 0; Y < 20; ++Y)
	{
		for (int32 X = 0; X < 300; ++X)
		{
			Colors.Add(PackColor(uint8(X % 2 == 0 ? 0 : 200), 10, 20));
		}
	}
	FPalettedTexture Texture;
	TestTrue("Built", FPalettedTextureBuilder::Build(ToBytes(Colors).GetData(), 300, 20, true, Texture));
	TestEqual("Width", Texture.SizeX, 256);
	TestEqual("Height", Texture.SizeY, 16);
	TestEqual("Its data", int64(Texture.Data.Num()), GetPixelFormatDataSize(Texture.Format, 256, 16));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPalettedExactTest, "System.TextureCompressor.Paletted.ExactPalettes",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FPalettedExactTest::RunTest(const FString& Parameters)
{
	// Up to 16 colours: PSMT4; up to 256: PSMT8; both give every texel back exactly.
	for (const int32 NumColors : {3, 16, 17, 200, 256})
	{
		TArray<uint32> Colors;
		for (int32 Index = 0; Index < 16 * 16; ++Index)
		{
			const int32 Color = (Index * 7) % NumColors;
			Colors.Add(
				PackColor(uint8(Color), uint8(255 - Color), uint8(Color * 3), uint8(Color % 2 == 0 ? 255 : 128)));
		}
		FPalettedTexture Texture;
		const FString Name = FString::Printf("%d colours", NumColors);
		if (!TestTrue(*Name, FPalettedTextureBuilder::Build(ToBytes(Colors).GetData(), 16, 16, true, Texture)))
		{
			continue;
		}
		TestEqual(*(Name + TEXT(": format")), int32(Texture.Format), int32(NumColors <= 16 ? PF_P4 : PF_P8));
		TestEqual(*(Name + TEXT(": counted")), Texture.NumSourceColors, NumColors);
		bool bExact = true;
		for (int32 Index = 0; Index < Colors.Num(); ++Index)
		{
			bExact &= PalettedTexel(Texture, Index) == Colors[Index];
		}
		TestTrue(*(Name + TEXT(": exact")), bExact);
		// The data starts with the CLUT as the GS reads it (N23): CSM1 order, alpha 0..0x80, uploaded as it is.
		TArray<uint8> Clut;
		uint16 ClutWidth = 0;
		uint16 ClutHeight = 0;
		FGSTextureLayout::MakeClutImage(Texture.Palette, Clut, ClutWidth, ClutHeight);
		TestTrue(*(Name + TEXT(": the GS's CLUT first")),
			Texture.Palette.Num() == GetPixelFormatPaletteSize(Texture.Format) &&
				FMemory::Memcmp(Texture.Data.GetData(), Clut.GetData(), SIZE_T(Clut.Num())) == 0);
		const int32 Entry = Texture.Format == PF_P8 ? 9 : 1;
		const uint8* Gs = &Texture.Data[FGSTextureLayout::GetClutImagePosition(Entry, Texture.Format == PF_P8) * 4];
		const uint32 Rgba = Texture.Palette[Entry];
		TestTrue(*(Name + TEXT(": an entry's alpha on the GS's scale")),
			Gs[0] == uint8(Rgba) && Gs[3] == uint8((((Rgba >> 24) * 0x80u) + 127u) / 255u));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPalettedMedianCutTest, "System.TextureCompressor.Paletted.MedianCut",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FPalettedMedianCutTest::RunTest(const FString& Parameters)
{
	// 4096 colours reduce to 256, the same bytes on every build, each texel near its colour.
	TArray<uint32> Colors;
	for (int32 Y = 0; Y < 64; ++Y)
	{
		for (int32 X = 0; X < 64; ++X)
		{
			Colors.Add(PackColor(uint8(X * 4), uint8(Y * 4), uint8((X + Y) * 2)));
		}
	}
	const TArray<uint8> Bytes = ToBytes(Colors);
	FPalettedTexture First;
	FPalettedTexture Second;
	TestTrue("Built", FPalettedTextureBuilder::Build(Bytes.GetData(), 64, 64, true, First));
	TestTrue("Built again", FPalettedTextureBuilder::Build(Bytes.GetData(), 64, 64, true, Second));
	TestEqual("PSMT8", int32(First.Format), int32(PF_P8));
	TestEqual("4096 colours before", First.NumSourceColors, 4096);
	TestTrue("Deterministic", First.Data == Second.Data);
	int32 WorstError = 0;
	for (int32 Index = 0; Index < Colors.Num(); ++Index)
	{
		const uint32 Paletted = PalettedTexel(First, Index);
		for (int32 Channel = 0; Channel < 4; ++Channel)
		{
			const int32 Error =
				FMath::Abs(int32((Paletted >> (Channel * 8)) & 0xff) - int32((Colors[Index] >> (Channel * 8)) & 0xff));
			WorstError = FMath::Max(WorstError, Error);
		}
	}
	TestTrue(*FString::Printf("Near the source (worst channel error %d)", WorstError), WorstError <= 24);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPalettedMipChainTest, "System.TextureCompressor.Paletted.MipChain",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FPalettedMipChainTest::RunTest(const FString& Parameters)
{
	// The mip chain (ps2-shipping N13): halved down to 8 texels on the shorter side, every mip through mip 0's palette,
	// each texel the box average of the four under it in linear space (sRGB); mip 0 stays exact, and two builds give
	// the same bytes.
	TestEqual("256 x 256: 6 levels", FPalettedTextureBuilder::GetNumMips(256, 256), 6);
	TestEqual("256 x 16: 2 levels", FPalettedTextureBuilder::GetNumMips(256, 16), 2);
	TestEqual("8 x 64: 1 level", FPalettedTextureBuilder::GetNumMips(8, 64), 1);

	// A black and white checker, one texel a square: its mips are grey, linear 0.5 (sRGB 188; 128 unencoded).
	TArray<uint32> Checker;
	for (int32 Y = 0; Y < 32; ++Y)
	{
		for (int32 X = 0; X < 32; ++X)
		{
			Checker.Add(((X + Y) % 2) == 0 ? PackColor(0, 0, 0) : PackColor(255, 255, 255));
		}
	}
	const TArray<uint8> CheckerBytes = ToBytes(Checker);
	for (const bool bSRGB : {true, false})
	{
		FPalettedTexture Texture;
		if (!TestTrue("Built", FPalettedTextureBuilder::Build(CheckerBytes.GetData(), 32, 32, bSRGB, Texture)))
		{
			return false;
		}
		const uint8 Grey = bSRGB ? 188 : 128;
		TestEqual("PSMT4 (mip 0 has 2 colours)", int32(Texture.Format), int32(PF_P4));
		TestEqual("Mips 16 and 8", Texture.Mips.Num(), 2);
		bool bExact = true;
		for (int32 Index = 0; Index < Checker.Num(); ++Index)
		{
			bExact &= PalettedTexel(Texture, Index) == Checker[Index];
		}
		TestTrue("Mip 0 exact", bExact);
		bool bGrey = true;
		for (int32 Mip = 0; Mip < Texture.Mips.Num(); ++Mip)
		{
			const int32 Size = 16 >> Mip;
			TestEqual(
				"Mip size", int64(Texture.Mips[Mip].Num()), GetPixelFormatMipDataSize(PF_P4, Size, Size, Mip + 1));
			for (int32 Texel = 0; Texel < Size * Size; ++Texel)
			{
				// The mip's index into mip 0's palette.
				const int32 Entry = (Texture.Mips[Mip][Texel / 2] >> ((Texel % 2) * 4)) & 0xf;
				const uint8* Color = reinterpret_cast<const uint8*>(&Texture.Palette[Entry]);
				bGrey &= Color[0] == Grey && Color[1] == Grey && Color[2] == Grey && Color[3] == 255;
			}
		}
		TestTrue(bSRGB ? TEXT("Mips grey in linear space") : TEXT("Mips grey"), bGrey);
	}

	// More than 256 colours: one palette for every level, reduced together; the same bytes every build.
	TArray<uint32> Colors;
	for (int32 Y = 0; Y < 64; ++Y)
	{
		for (int32 X = 0; X < 64; ++X)
		{
			Colors.Add(PackColor(uint8(X * 4), uint8(Y * 4), uint8((X + Y) * 2)));
		}
	}
	const TArray<uint8> Bytes = ToBytes(Colors);
	FPalettedTexture First;
	FPalettedTexture Second;
	TestTrue("Built", FPalettedTextureBuilder::Build(Bytes.GetData(), 64, 64, true, First));
	TestTrue("Built again", FPalettedTextureBuilder::Build(Bytes.GetData(), 64, 64, true, Second));
	TestEqual("PSMT8", int32(First.Format), int32(PF_P8));
	TestEqual("Mips 32, 16 and 8", First.Mips.Num(), 3);
	bool bSame = First.Data == Second.Data && First.Mips.Num() == Second.Mips.Num();
	for (int32 Mip = 0; bSame && Mip < First.Mips.Num(); ++Mip)
	{
		bSame &= First.Mips[Mip] == Second.Mips[Mip];
	}
	TestTrue("Deterministic, mips included", bSame);
	// Mip 3 (8 x 8) against the box average of each 8 x 8 square of the source, in linear space.
	int32 WorstError = 0;
	for (int32 Texel = 0; Texel < 64; ++Texel)
	{
		const int32 X = Texel % 8;
		const int32 Y = Texel / 8;
		float Sum[3] = {0.0f, 0.0f, 0.0f};
		for (int32 SourceY = Y * 8; SourceY < (Y * 8) + 8; ++SourceY)
		{
			for (int32 SourceX = X * 8; SourceX < (X * 8) + 8; ++SourceX)
			{
				for (int32 Channel = 0; Channel < 3; ++Channel)
				{
					Sum[Channel] +=
						FLinearColor::sRGBToLinearTable[(Colors[(SourceY * 64) + SourceX] >> (Channel * 8)) & 0xff];
				}
			}
		}
		const FColor Expected = FLinearColor(Sum[0] / 64.0f, Sum[1] / 64.0f, Sum[2] / 64.0f, 1.0f).ToFColor(true);
		const uint8* Color = reinterpret_cast<const uint8*>(&First.Palette[First.Mips[2][Texel]]);
		WorstError = FMath::Max(WorstError,
			FMath::Max(FMath::Abs(int32(Color[0]) - int32(Expected.R)),
				FMath::Max(
					FMath::Abs(int32(Color[1]) - int32(Expected.G)), FMath::Abs(int32(Color[2]) - int32(Expected.B)))));
	}
	TestTrue(*FString::Printf("Mip 3 near the linear average (worst channel error %d)", WorstError), WorstError <= 24);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
