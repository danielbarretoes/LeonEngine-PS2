#include "CoreMinimal.h"
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
		const uint8* Color = &Texture.Data[Entry * 4];
		return uint32(Color[0]) | (uint32(Color[1]) << 8) | (uint32(Color[2]) << 16) | (uint32(Color[3]) << 24);
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
	TestTrue("Built", FPalettedTextureBuilder::Build(ToBytes(Colors).GetData(), 300, 20, Texture));
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
		if (!TestTrue(*Name, FPalettedTextureBuilder::Build(ToBytes(Colors).GetData(), 16, 16, Texture)))
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
	TestTrue("Built", FPalettedTextureBuilder::Build(Bytes.GetData(), 64, 64, First));
	TestTrue("Built again", FPalettedTextureBuilder::Build(Bytes.GetData(), 64, 64, Second));
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

#endif // WITH_DEV_AUTOMATION_TESTS
