#include "CoreMinimal.h"
#include "GSCommandList.h"
#include "GSConformanceScenes.h"
#include "GSReferenceRasterizer.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// The reference rasterizer against the GS User's Manual's rules, one area per test (Docs/PLANS/ps2-gs-parity.md, P2).
// Every scene draws into a 64 x 32 frame buffer at FBP 0 (FBW 1) with window coordinates equal to the primitive's.

namespace
{

	using GSConformance::FrameHeight;
	using GSConformance::FrameWidth;
	using GSConformance::MakeFrame;
	using GSConformance::MakeZBuf;

	const FColor& PixelAt(const TArray<FColor>& Pixels, uint32 X, uint32 Y)
	{
		return Pixels[int32((Y * FrameWidth) + X)];
	}

	TArray<FColor> Render(const FGSCommandList& List, EGSPixelFormat Format = EGSPixelFormat::PSMCT32)
	{
		FGSReferenceRasterizer Rasterizer;
		Rasterizer.Execute(List);
		return Rasterizer.ReadFrame(MakeFrame(Format), FrameWidth, FrameHeight);
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSReferenceTransferTest, "System.GSReference.Transfer.Formats",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSReferenceTransferTest::RunTest(const FString& Parameters)
{
	// Host to local transfers unpack the manual's transfer format (4.3) into the buffer's pixels.
	FGSCommandList List;
	TArray<uint8> Bytes32;
	for (uint32 Index = 0; Index < 8 * 2 * 4; ++Index)
	{
		Bytes32.Add(uint8(Index));
	}
	FGSBitBltBuf Buf32;
	Buf32.DBP = 0;
	Buf32.DBW = 1;
	Buf32.DPSM = EGSPixelFormat::PSMCT32;
	List.UploadImage(Buf32, 4, 1, 8, 2, Bytes32);

	// 16 x 2 4-bit texels: 0x21 holds texel 0 = 1 in its low nibble and texel 1 = 2.
	TArray<uint8> Bytes4;
	for (uint32 Index = 0; Index < 16; ++Index)
	{
		Bytes4.Add(uint8(0x21 + Index));
	}
	FGSBitBltBuf Buf4;
	Buf4.DBP = 64;
	Buf4.DBW = 2;
	Buf4.DPSM = EGSPixelFormat::PSMT4;
	List.UploadImage(Buf4, 0, 0, 16, 2, Bytes4);

	// 8 x 2 24-bit pixels: 3 bytes each, 48 bytes.
	TArray<uint8> Bytes24;
	for (uint32 Index = 0; Index < 48; ++Index)
	{
		Bytes24.Add(uint8(0x80 + Index));
	}
	FGSBitBltBuf Buf24;
	Buf24.DBP = 128;
	Buf24.DBW = 1;
	Buf24.DPSM = EGSPixelFormat::PSMCT24;
	List.UploadImage(Buf24, 0, 0, 8, 2, Bytes24);

	FGSReferenceRasterizer Rasterizer;
	Rasterizer.Execute(List);
	const FGSLocalMemory& Memory = Rasterizer.GetMemory();
	TestEqual("32 bits, little endian, at (4, 1)", Memory.ReadPixel(0, 1, EGSPixelFormat::PSMCT32, 4, 1), 0x03020100u);
	TestEqual("The second row", Memory.ReadPixel(0, 1, EGSPixelFormat::PSMCT32, 5, 2), 0x27262524u);
	TestEqual("4 bits: low nibble first", Memory.ReadPixel(64, 2, EGSPixelFormat::PSMT4, 0, 0), 1u);
	TestEqual("4 bits: then the high nibble", Memory.ReadPixel(64, 2, EGSPixelFormat::PSMT4, 1, 0), 2u);
	TestEqual("24 bits packed", Memory.ReadPixel(128, 1, EGSPixelFormat::PSMCT24, 1, 0) & 0xffffffu, 0x858483u);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSReferenceRasterRulesTest, "System.GSReference.Raster.DrawingRules",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSReferenceRasterRulesTest::RunTest(const FString& Parameters)
{
	// Pixel centers on integer window coordinates, top and left sides drawn, bottom and right not (2.4.4, 3.2.9):
	// two triangles sharing a diagonal cover their square once, a sprite covers [X0, X1) x [Y0, Y1).
	FGSCommandList List;
	GSConformance::BuildDrawingRules(List);
	const TArray<FColor> Pixels = Render(List);

	int32 Once = 0;
	int32 Twice = 0;
	for (uint32 Y = 0; Y < 10; ++Y)
	{
		for (uint32 X = 0; X < 10; ++X)
		{
			Once += PixelAt(Pixels, X, Y).R == 0x10 ? 1 : 0;
			Twice += PixelAt(Pixels, X, Y).R == 0x20 ? 1 : 0;
		}
	}
	TestEqual("The square once", Once, 64);
	TestEqual("No pixel twice", Twice, 0);
	TestTrue("Right side not drawn", PixelAt(Pixels, 8, 3).R == 0);
	TestTrue("Bottom side not drawn", PixelAt(Pixels, 3, 8).R == 0);
	TestTrue("Sprite's top-left", PixelAt(Pixels, 20, 2).R == 0xff);
	TestTrue("Sprite's last pixel", PixelAt(Pixels, 23, 4).R == 0xff);
	TestTrue("Sprite's right side", PixelAt(Pixels, 24, 3).R == 0);
	TestTrue("Sprite's bottom side", PixelAt(Pixels, 21, 5).R == 0);
	TestTrue("One pixel",
		PixelAt(Pixels, 30, 10).G == 0xff && PixelAt(Pixels, 31, 10).G == 0 && PixelAt(Pixels, 30, 11).G == 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSReferencePrimitivesTest, "System.GSReference.Raster.Primitives",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSReferencePrimitivesTest::RunTest(const FString& Parameters)
{
	// A strip and a fan of two triangles each cover their square once; XYZ3 adds a vertex without drawing; a point
	// draws its nearest pixel; a line leaves its end point out; flat shading takes the kicking vertex's color.
	FGSCommandList List;
	GSConformance::BuildPrimitives(List);
	const TArray<FColor> Pixels = Render(List);

	int32 StripOnce = 0;
	int32 FanOnce = 0;
	for (uint32 Y = 0; Y < 4; ++Y)
	{
		for (uint32 X = 0; X < 4; ++X)
		{
			StripOnce += PixelAt(Pixels, X, Y).R == 0x10 ? 1 : 0;
			FanOnce += PixelAt(Pixels, X + 10, Y).R == 0x10 ? 1 : 0;
		}
	}
	TestEqual("Strip covers its square once", StripOnce, 16);
	TestEqual("Fan covers its square once", FanOnce, 16);
	TestTrue("Point on its nearest pixel", PixelAt(Pixels, 20, 4).G == 0xff);
	TestTrue("Line's start", PixelAt(Pixels, 30, 1).B == 0xff && PixelAt(Pixels, 33, 1).B == 0xff);
	TestTrue("Line's end point not drawn", PixelAt(Pixels, 34, 1).B == 0);
	TestTrue("Flat color", PixelAt(Pixels, 41, 1).B == 0x40 && PixelAt(Pixels, 41, 1).R == 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSReferenceShadingTest, "System.GSReference.Raster.GouraudAndScissor",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSReferenceShadingTest::RunTest(const FString& Parameters)
{
	// Gouraud interpolates the vertex colors; the scissor rectangle includes its borders.
	FGSCommandList List;
	GSConformance::BuildGouraudAndScissor(List);
	const TArray<FColor> Pixels = Render(List);

	TestEqual("Halfway along the red edge", int32(PixelAt(Pixels, 10, 0).R), 100);
	TestEqual("A quarter", int32(PixelAt(Pixels, 5, 0).R), 50);
	int32 Inside = 0;
	for (uint32 Y = 0; Y < 10; ++Y)
	{
		for (uint32 X = 30; X < 60; ++X)
		{
			Inside += PixelAt(Pixels, X, Y).G == 0xff ? 1 : 0;
		}
	}
	TestEqual("Scissor (40..42 x 1..2)", Inside, 6);
	TestTrue("Its corner", PixelAt(Pixels, 42, 2).G == 0xff);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSReferenceTextureSamplingTest, "System.GSReference.Texture.Sampling",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSReferenceTextureSamplingTest::RunTest(const FString& Parameters)
{
	// An 8 x 2 texture whose columns are 0, 32, ..., 224 in red: point sampling takes the texel under the point
	// (centers at .5), bilinear averages the two texels around it, and the wrap modes place coordinates outside.
	FGSCommandList List;
	GSConformance::BuildTextureSampling(List);
	const TArray<FColor> Pixels = Render(List);

	TestEqual("Point: texel 3", int32(PixelAt(Pixels, 3, 0).R), 96);
	TestEqual("Repeat: u = 19 is texel 3", int32(PixelAt(Pixels, 3, 1).R), 96);
	TestEqual("Clamp: u = 12 is texel 7", int32(PixelAt(Pixels, 12, 2).R), 224);
	TestEqual("Bilinear: between texels 2 and 3", int32(PixelAt(Pixels, 3, 3).R), 80);
	TestEqual("Region repeat: (6 & 1) | 4", int32(PixelAt(Pixels, 6, 4).R), 128);
	TestEqual("Region repeat: (7 & 1) | 4", int32(PixelAt(Pixels, 7, 4).R), 160);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSReferenceClutTest, "System.GSReference.Texture.ClutAndFormats",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSReferenceClutTest::RunTest(const FString& Parameters)
{
	// IDTEX8 through a CSM1 CLUT (bits 3 and 4 of the index swapped in its 16 x 16 rectangle), IDTEX4 through the
	// temporary buffer at CSA * 16, and 16-bit texels shifted left 3 with TEXA's alpha (2.7.3, 3.4.6, 3.4.7).
	FGSCommandList List;
	GSConformance::BuildClutAndFormats(List);
	FGSReferenceRasterizer Rasterizer;
	Rasterizer.Execute(List);
	const TArray<FColor> Pixels = Rasterizer.ReadFrame(MakeFrame(), FrameWidth, FrameHeight);
	TestTrue("Index 8: CLUT (0, 1)", PixelAt(Pixels, 0, 0).R == 0xff && PixelAt(Pixels, 0, 0).G == 0);
	TestTrue("Index 16: CLUT (8, 0)", PixelAt(Pixels, 1, 0).G == 0xff && PixelAt(Pixels, 1, 0).R == 0);
	TestTrue("IDTEX4 at CSA 1", PixelAt(Pixels, 0, 1).B == 0xff);
	TestTrue("5 bits shifted left 3", PixelAt(Pixels, 0, 2).R == 0xf8 && PixelAt(Pixels, 0, 2).B == 0xf8);
	TestEqual("A = 0: TA0", int32(PixelAt(Pixels, 0, 2).A), 0x20);
	TestEqual("A = 1: TA1", int32(PixelAt(Pixels, 1, 2).A), 0x60);
	TestEqual("Black with AEM: 0", int32(PixelAt(Pixels, 2, 2).A), 0);

	// Paletted MIPMAP levels through one CLUT (3.4.11, 3.4.12): Q falls from 1 at x 0, LOD = log2(1 / Q); CLD 2 / 3
	// load and set CBP0 / CBP1, CLD 4 finds CBP0 equal and keeps the buffer (3.4.7).
	const FColor Red(255, 0, 0, 0x80);
	const FColor Green(0, 255, 0, 0x80);
	const FColor Blue(0, 0, 255, 0x80);
	const FColor Yellow(255, 255, 0, 0x80);
	const FColor Magenta(255, 0, 255, 0x80);
	TestTrue("PSMT8, LOD 0: level 0 (entry 1)", PixelAt(Pixels, 0, 7) == Red);
	TestTrue("PSMT8, Q 0.5, LOD 1: level 1 (entry 2)", PixelAt(Pixels, 36, 7) == Green);
	TestTrue("PSMT8, LOD 2.85 past MXL 2: level 2 (entry 3)", PixelAt(Pixels, 62, 7) == Blue);
	const FColor& Between = PixelAt(Pixels, 21, 7);
	TestTrue("PSMT8, LOD 0.5: levels 0 and 1 blended", Between.R > 64 && Between.G > 64 && Between.B == 0);
	TestTrue("PSMT4 at CSA 1, LOD 0: level 0 (entry 1)", PixelAt(Pixels, 0, 14) == Yellow);
	TestTrue("PSMT4, LOD 1.93 past MXL 1: level 1 (entry 2)", PixelAt(Pixels, 62, 14) == Magenta);
	bool bSameRamp = true;
	for (uint32 X = 0; X < FrameWidth; ++X)
	{
		bSameRamp &= PixelAt(Pixels, X, 21) == PixelAt(Pixels, X, 7);
	}
	TestTrue("CLD 4, CBP0 equal: no load, the PSMT8 ramp again (PSMT4 at CSA 1 left entries 1..3)", bSameRamp);
	TestTrue("CLD 3 after the CLUT was rewritten: loaded, level 0 white",
		PixelAt(Pixels, 0, 28) == FColor(255, 255, 255, 0x80));
	TestTrue("level 1 dark grey", PixelAt(Pixels, 62, 28) == FColor(0x40, 0x40, 0x40, 0x80));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSReferenceTwoPalettesTest, "System.GSReference.Texture.TwoPalettes",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSReferenceTwoPalettesTest::RunTest(const FString& Parameters)
{
	// Two 16 x 16 PSMT8 textures (texel = entry 16 y + x) with CLUTs 4 blocks apart (manual 8.4: a 16 x 16 PSMCT32
	// CLUT takes 4 blocks): each draws through its own palette, the first (red to blue) also after the second (green)
	// was loaded.
	FGSCommandList List;
	GSConformance::BuildTwoPalettes(List);
	FGSReferenceRasterizer Rasterizer;
	Rasterizer.Execute(List);
	const TArray<FColor> Pixels = Rasterizer.ReadFrame(MakeFrame(), FrameWidth, FrameHeight);
	bool bFirst = true;
	bool bSecond = true;
	bool bFirstAgain = true;
	for (uint32 Y = 0; Y < 16; ++Y)
	{
		for (uint32 X = 0; X < 16; ++X)
		{
			const uint32 Entry = (Y * 16) + X;
			const FColor Red = FColor(uint8(Entry), 0, uint8(255 - Entry), 0x80);
			const FColor Green = FColor(0x20, uint8(Entry), 0x40, 0x80);
			bFirst &= PixelAt(Pixels, X, Y) == Red;
			bSecond &= PixelAt(Pixels, 20 + X, Y) == Green;
			bFirstAgain &= PixelAt(Pixels, 40 + X, Y) == Red;
		}
	}
	TestTrue("The first texture through its palette", bFirst);
	TestTrue("The second through its own", bSecond);
	TestTrue("The first again", bFirstAgain);
	TestTrue("Nothing between them", PixelAt(Pixels, 18, 4) == FColor(0, 0, 0, 0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSReferenceTextureFunctionTest, "System.GSReference.Texture.FunctionsFogAndMipmap",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSReferenceTextureFunctionTest::RunTest(const FString& Parameters)
{
	// MODULATE with a 0x80 fragment keeps the texture and halves it at 0x40, HIGHLIGHT adds the fragment's alpha
	// (3.4.9); fog mixes toward FOGCOL with >> 8 (3.5); a fixed LOD of 1 samples MIPMAP level 1 (3.4.12).
	FGSCommandList List;
	GSConformance::BuildFunctionsFogAndMipmap(List);
	const TArray<FColor> Pixels = Render(List);

	TestEqual("MODULATE 0x80", int32(PixelAt(Pixels, 0, 0).R), 200);
	TestEqual("MODULATE 0x40", int32(PixelAt(Pixels, 1, 0).R), 100);
	TestEqual("HIGHLIGHT: + Af", int32(PixelAt(Pixels, 2, 0).R), 116);
	TestEqual("Fog", int32(PixelAt(Pixels, 4, 0).R), 100);
	TestEqual("MIPMAP level 1", int32(PixelAt(Pixels, 6, 0).R), 100);
	TestEqual("Its green", int32(PixelAt(Pixels, 6, 0).G), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSReferencePixelTestsTest, "System.GSReference.Pixel.Tests",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSReferencePixelTestsTest::RunTest(const FString& Parameters)
{
	// The alpha test with AFAIL (KEEP writes nothing, FB_ONLY the color but not Z), and the depth test GEQUAL /
	// GREATER against what an earlier draw left in the Z buffer (3.7).
	FGSCommandList List;
	GSConformance::BuildPixelTests(List);
	FGSReferenceRasterizer Rasterizer;
	Rasterizer.Execute(List);
	const TArray<FColor> Pixels = Rasterizer.ReadFrame(MakeFrame(), FrameWidth, FrameHeight);
	const FGSZBuf ZBuf = MakeZBuf(false);
	TestTrue("KEEP: nothing", PixelAt(Pixels, 0, 0).R == 0x10 && Rasterizer.ReadZ(ZBuf, FrameWidth, 0, 0) == 100);
	TestTrue("FB_ONLY: the color", PixelAt(Pixels, 2, 0).R == 0xff);
	TestEqual("FB_ONLY: not Z", Rasterizer.ReadZ(ZBuf, FrameWidth, 2, 0), 100u);
	TestTrue("GEQUAL: equal passes", PixelAt(Pixels, 4, 0).G == 0xff);
	TestTrue("GEQUAL: less fails", PixelAt(Pixels, 5, 0).G == 0);
	TestTrue("GREATER: equal fails", PixelAt(Pixels, 6, 0).B == 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSReferenceBlendTest, "System.GSReference.Pixel.BlendAndWrite",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSReferenceBlendTest::RunTest(const FString& Parameters)
{
	// (A - B) * C >> 7 + D (3.8), COLCLAMP clamping or wrapping (3.9.2), PABE, FBA's alpha MSB (3.9.3) and FBMSK
	// (3.9.5).
	FGSCommandList List;
	GSConformance::BuildBlendAndWrite(List);
	const TArray<FColor> Pixels = Render(List);

	TestEqual("Translucent: (200 - 100) * 0x40 >> 7 + 100", int32(PixelAt(Pixels, 0, 0).R), 150);
	TestEqual("Additive, clamped", int32(PixelAt(Pixels, 1, 0).R), 255);
	TestEqual("Additive, wrapped", int32(PixelAt(Pixels, 2, 0).R), (300 & 0xff));
	TestEqual("PABE: MSB clear, not blended", int32(PixelAt(Pixels, 3, 0).R), 50);
	TestEqual("FBA", int32(PixelAt(Pixels, 4, 0).A), 0x90);
	TestTrue("FBMSK keeps green",
		PixelAt(Pixels, 5, 0).R == 1 && PixelAt(Pixels, 5, 0).G == 100 && PixelAt(Pixels, 5, 0).B == 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSReferenceDitherTest, "System.GSReference.Pixel.Dither16",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSReferenceDitherTest::RunTest(const FString& Parameters)
{
	// A 16-bit frame buffer: DIMX[Y % 4][X % 4] is added before the 8 to 5 bit conversion (3.9.1), so 103 becomes
	// 99 (12) at (0, 0) and 105 (13) at (1, 0); without DTHE both are 103 (12).
	FGSCommandList List;
	GSConformance::BuildDither16(List);
	const TArray<FColor> Pixels = Render(List, EGSPixelFormat::PSMCT16S);

	TestEqual("Dithered -4", int32(PixelAt(Pixels, 0, 0).R), 96);
	TestEqual("Dithered +2", int32(PixelAt(Pixels, 1, 0).R), 104);
	TestEqual("Not dithered", int32(PixelAt(Pixels, 1, 1).R), 96);
	TestEqual("The 16-bit alpha bit", int32(PixelAt(Pixels, 0, 0).A), 0x80);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSReferenceMipmapTest, "System.GSReference.Texture.MipmapLod",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSReferenceMipmapTest::RunTest(const FString& Parameters)
{
	// LOD = (log2(1 / |Q|) << L) + K or K (3.4.12): 0 or less takes MMAG on level 0; NEAREST_MIPMAP_NEAREST rounds it
	// to a level, the *_MIPMAP_LINEAR filters blend levels m and m + 1 by its fraction, MXL caps the level (3.4.11). Q
	// falls from 1 at x 0 to 1/16 at x 63 in the first three rows; the levels of texture A are red, green, blue, yellow
	// and magenta.
	FGSCommandList List;
	GSConformance::BuildMipmapLod(List);
	const TArray<FColor> Pixels = Render(List);
	const FColor Red(255, 0, 0, 0x80);
	const FColor Green(0, 255, 0, 0x80);
	const FColor Blue(0, 0, 255, 0x80);
	const FColor Yellow(255, 255, 0, 0x80);
	const FColor Magenta(255, 0, 255, 0x80);
	TestTrue("Q = 1: LOD 0, level 0", PixelAt(Pixels, 0, 1) == Red);
	TestTrue("LOD 0.23 rounds to level 0", PixelAt(Pixels, 10, 1) == Red);
	TestTrue("LOD 0.85 rounds to level 1", PixelAt(Pixels, 30, 1) == Green);
	TestTrue("LOD 1.60 rounds to level 2", PixelAt(Pixels, 45, 1) == Blue);
	TestTrue("LOD 2.87 rounds to level 3", PixelAt(Pixels, 58, 1) == Yellow);
	TestTrue("LOD 3.69 rounds to level 4", PixelAt(Pixels, 62, 1) == Magenta);
	TestTrue("LOD 0.85 blends levels 0 and 1", PixelAt(Pixels, 30, 5) == FColor(37, 218, 0, 0x80));
	TestTrue("L = 1, K = -0.5: LOD 1.21 blends levels 1 and 2", PixelAt(Pixels, 30, 9) == FColor(0, 202, 53, 0x80));
	TestTrue("MXL 3 caps LOD 6.9 at level 3", PixelAt(Pixels, 62, 9) == Yellow);
	TestTrue("K = -1: MMAG", PixelAt(Pixels, 4, 13) == Red);
	TestTrue("K = 0: MMAG", PixelAt(Pixels, 12, 13) == Red);
	TestTrue("K = 0.4375 rounds to 0", PixelAt(Pixels, 20, 13) == Red);
	TestTrue("K = 0.5625 rounds to 1", PixelAt(Pixels, 28, 13) == Green);
	TestTrue("K = 1.75 rounds to 2", PixelAt(Pixels, 36, 13) == Blue);
	TestTrue("K = 2.25 blends 2 and 3 by a quarter", PixelAt(Pixels, 44, 13) == FColor(64, 64, 191, 0x80));
	TestTrue("K = 3.25 past MXL 3", PixelAt(Pixels, 52, 13) == Yellow);
	TestTrue("K = 5 past MXL 4", PixelAt(Pixels, 60, 13) == Magenta);
	// Texture B (32 x 32): level 1 texel (u, v) is (255 - 16 u, 16 v, 0xc0); level 0 texel (8 u, 8 v, 0x40).
	TestTrue("Level 1, point: texel (1, 0)", PixelAt(Pixels, 20, 16) == FColor(239, 0, 192, 0x80));
	TestTrue(
		"Level 1, bilinear: texels 15 and 0 both ways (REPEAT)", PixelAt(Pixels, 0, 16) == FColor(135, 120, 192, 0x80));
	TestTrue("Q = 2: LOD -1, MMAG bilinear on level 0", PixelAt(Pixels, 50, 18) == FColor(4, 4, 64, 0x80));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSReferenceAlphaTestTest, "System.GSReference.Pixel.AlphaTest",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSReferenceAlphaTestTest::RunTest(const FString& Parameters)
{
	// The eight ATST methods against AREF 0x78 (texel x has alpha 8 + 16 x, so texel 7 equals it), and what AFAIL still
	// writes (3.7.2): KEEP nothing, FB_ONLY the colour and alpha, ZB_ONLY Z, RGB_ONLY the colour (a 32-bit frame).
	FGSCommandList List;
	GSConformance::BuildAlphaTest(List);
	const TArray<FColor> Pixels = Render(List);
	const auto Drawn = [&Pixels](uint32 X, uint32 Y) { return PixelAt(Pixels, X, Y).A != 0; };
	TestTrue("NEVER", !Drawn(0, 0) && !Drawn(15, 1));
	TestTrue("ALWAYS", Drawn(0, 2) && Drawn(15, 3) && PixelAt(Pixels, 3, 2).R == 48);
	TestTrue("LESS", Drawn(6, 4) && !Drawn(7, 4));
	TestTrue("LEQUAL", Drawn(7, 6) && !Drawn(8, 6));
	TestTrue("EQUAL", Drawn(7, 8) && !Drawn(6, 8) && !Drawn(8, 8));
	TestTrue("GEQUAL", !Drawn(6, 10) && Drawn(7, 10));
	TestTrue("GREATER", !Drawn(7, 12) && Drawn(8, 12));
	TestTrue("NOTEQUAL", Drawn(6, 14) && !Drawn(7, 14));
	const FColor Background(0x20, 0x20, 0x20, 0x10);
	const FColor Green(0, 0xff, 0, 0x70);
	TestTrue("KEEP: nothing", PixelAt(Pixels, 25, 2) == Background && PixelAt(Pixels, 25, 12) == Green);
	TestTrue(
		"FB_ONLY: RGBA, not Z", PixelAt(Pixels, 35, 2) == FColor(0xff, 0, 0, 0x60) && PixelAt(Pixels, 35, 12) == Green);
	TestTrue("ZB_ONLY: Z only", PixelAt(Pixels, 45, 2) == Background && PixelAt(Pixels, 45, 12) == Background);
	TestTrue(
		"RGB_ONLY: RGB only", PixelAt(Pixels, 55, 2) == FColor(0xff, 0, 0, 0x10) && PixelAt(Pixels, 55, 12) == Green);
	TestTrue(
		"FB_ONLY on a ramp: the failing part left Z alone", PixelAt(Pixels, 10, 18) == FColor(0x10, 0xe0, 0x10, 0x80));
	TestTrue("The passing part wrote Z", PixelAt(Pixels, 50, 18) == FColor(0xc0, 0x60, 0x20, 102));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSReferenceFogTest, "System.GSReference.Pixel.Fog",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSReferenceFogTest::RunTest(const FString& Parameters)
{
	// C = (F x C + (0xff - F) x FOGCOL) >> 8 with FOGCOL (0x20, 0x40, 0xc0) (3.5): F interpolated from the vertices, a
	// sprite's from its second vertex, XYZ2's from the FOG register; XYZF3 draws no triangle (3.2.5).
	FGSCommandList List;
	GSConformance::BuildFog(List);
	const TArray<FColor> Pixels = Render(List);
	TestTrue("F = 0: the fog colour's 255 / 256", PixelAt(Pixels, 0, 2) == FColor(31, 63, 191, 0x80));
	TestTrue("F = 251", PixelAt(Pixels, 62, 2) == FColor(196, 99, 52, 0x80));
	TestTrue("A sprite: F = 0x60 from its second vertex", PixelAt(Pixels, 10, 17) == FColor(113, 133, 213, 0x80));
	TestTrue("A sprite: F = 0", PixelAt(Pixels, 40, 17) == FColor(31, 63, 191, 0x80));
	TestTrue("The FOG register: F = 0x10", PixelAt(Pixels, 0, 21) == FColor(32, 73, 184, 0x80));
	TestTrue("XYZF3: triangle (2, 3, 4) not drawn", PixelAt(Pixels, 18, 29) == FColor(0, 0, 0, 0));
	TestTrue("Triangle (1, 2, 3) drawn", PixelAt(Pixels, 3, 25).A == 0x80);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSReferenceTexATest, "System.GSReference.Texture.TexAAndFunctions",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSReferenceTexATest::RunTest(const FString& Parameters)
{
	// TEXA's alpha for 16-bit (A = 0: TA0, A = 1: TA1, black with AEM: 0) and 24-bit texels (TA0, black with AEM: 0)
	// (3.4.6), and the texture functions with TCC (3.4.9): the fragment (0x60, 0x80, 0xa0, 0x50) on (200, 100, 50,
	// 0x40).
	FGSCommandList List;
	GSConformance::BuildTexAAndFunctions(List);
	const TArray<FColor> Pixels = Render(List);
	TestTrue("16 bits, A = 0: TA0", PixelAt(Pixels, 2, 1) == FColor(248, 248, 248, 0x30));
	TestTrue("16 bits, A = 1: TA1", PixelAt(Pixels, 6, 1) == FColor(248, 0, 0, 0xa0));
	TestEqual("Black, A = 0, AEM off: TA0", int32(PixelAt(Pixels, 10, 1).A), 0x30);
	TestEqual("Black, A = 0, AEM on: 0", int32(PixelAt(Pixels, 26, 1).A), 0);
	TestEqual("Black, A = 1, AEM on: TA1", int32(PixelAt(Pixels, 30, 1).A), 0xa0);
	TestEqual("24 bits: TA0", int32(PixelAt(Pixels, 34, 1).A), 0x50);
	TestEqual("24 bits, black, AEM on: 0", int32(PixelAt(Pixels, 50, 1).A), 0);
	TestTrue("MODULATE, RGB: Af", PixelAt(Pixels, 4, 4) == FColor(150, 100, 62, 0x50));
	TestTrue("MODULATE, RGBA: At x Af", PixelAt(Pixels, 4, 6) == FColor(150, 100, 62, 40));
	TestTrue("DECAL, RGB", PixelAt(Pixels, 4, 8) == FColor(200, 100, 50, 0x50));
	TestTrue("DECAL, RGBA", PixelAt(Pixels, 4, 10) == FColor(200, 100, 50, 0x40));
	TestTrue("HIGHLIGHT, RGB: + Af", PixelAt(Pixels, 4, 12) == FColor(230, 180, 142, 0x50));
	TestTrue("HIGHLIGHT, RGBA: At + Af", PixelAt(Pixels, 4, 14) == FColor(230, 180, 142, 0x90));
	TestTrue("HIGHLIGHT2, RGB", PixelAt(Pixels, 4, 16) == FColor(230, 180, 142, 0x50));
	TestTrue("HIGHLIGHT2, RGBA: At", PixelAt(Pixels, 4, 18) == FColor(230, 180, 142, 0x40));
	TestTrue("MODULATE by 0xff, clamped", PixelAt(Pixels, 28, 6) == FColor(255, 199, 99, 255));
	TestEqual("TEXA changed: TA1 0xff", int32(PixelAt(Pixels, 12, 24).A), 255);
	TestEqual("TEXA changed: TA0 0", int32(PixelAt(Pixels, 4, 24).A), 0);
	TestEqual("And again: TA0 0x7f", int32(PixelAt(Pixels, 36, 24).A), 0x7f);
	TestEqual("TA1 1", int32(PixelAt(Pixels, 44, 24).A), 1);
	TestEqual("Black with AEM", int32(PixelAt(Pixels, 52, 24).A), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSReferenceClampModesTest, "System.GSReference.Texture.ClampModes",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSReferenceClampModesTest::RunTest(const FString& Parameters)
{
	// The wrap modes (3.4.5) on texel (u, v) = (16 u + 8, 16 v + 8): REPEAT wraps, CLAMP holds the border, REGION_CLAMP
	// holds MIN / MAX, REGION_REPEAT is (u & UMSK) | UFIX, both fields shifted right by the level on level n; level 1
	// texel (u, v) is (32 u + 16, 255 - 32 v).
	FGSCommandList List;
	GSConformance::BuildClampModes(List);
	const TArray<FColor> Pixels = Render(List);
	TestTrue("REPEAT: u = 20 is texel 4", PixelAt(Pixels, 10, 5).R == 72 && PixelAt(Pixels, 10, 5).G == 72);
	TestTrue("CLAMP: texel 15", PixelAt(Pixels, 26, 5).R == 248 && PixelAt(Pixels, 26, 5).G == 248);
	TestTrue("REGION_CLAMP: MINU 3, MINV 2", PixelAt(Pixels, 32, 0) == FColor(56, 40, 0xc0, 0x80));
	TestTrue("REGION_CLAMP: MAXU 12, MAXV 9", PixelAt(Pixels, 42, 5) == FColor(200, 152, 0xc0, 0x80));
	TestTrue("REGION_REPEAT: (20 & 7) | 8, (20 & 3) | 4", PixelAt(Pixels, 58, 5) == FColor(200, 72, 0x80, 0x80));
	TestTrue("Level 1, REPEAT", PixelAt(Pixels, 10, 17).R == 80 && PixelAt(Pixels, 10, 17).G == 191);
	TestTrue("Level 1, REGION_CLAMP: MIN >> 1", PixelAt(Pixels, 32, 16).R == 80 && PixelAt(Pixels, 32, 16).G == 223);
	TestTrue("Level 1, REGION_CLAMP: MAX >> 1", PixelAt(Pixels, 40, 19).R == 176 && PixelAt(Pixels, 40, 19).G == 63);
	TestTrue("Level 1, REGION_REPEAT: (1 & 1) | 2, (2 & 0) | 1",
		PixelAt(Pixels, 49, 17).R == 112 && PixelAt(Pixels, 49, 17).G == 223);
	TestTrue("Negative s, REPEAT", PixelAt(Pixels, 0, 24).R == 8 && PixelAt(Pixels, 0, 24).G == 136);
	TestTrue("Negative s, CLAMP", PixelAt(Pixels, 16, 24).R == 8 && PixelAt(Pixels, 16, 24).G == 8);
	TestTrue("Negative s, REGION_REPEAT", PixelAt(Pixels, 49, 25).R == 168 && PixelAt(Pixels, 49, 25).G == 104);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSReferenceStripsAndSpritesTest, "System.GSReference.Raster.StripsAndSprites",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSReferenceStripsAndSpritesTest::RunTest(const FString& Parameters)
{
	// XYZ3 advances a strip's or a fan's vertex queue without drawing (3.2.5); a sprite's texture spans it from its
	// first vertex to its second whichever corners they are, with the second vertex's Q (3.2.8); lines leave their end
	// point out and points take their nearest pixel (3.2.9).
	FGSCommandList List;
	GSConformance::BuildStripsAndSprites(List);
	const TArray<FColor> Pixels = Render(List);
	TestTrue("Strip: triangle (1, 2, 3)", PixelAt(Pixels, 2, 2).A == 0x80);
	TestTrue("Strip: (2, 3, 4) skipped", PixelAt(Pixels, 8, 6) == FColor(0, 0, 0, 0));
	TestTrue("Strip: (3, 4, 5) drawn from the skipped vertex", PixelAt(Pixels, 12, 2).A == 0x80);
	TestTrue("Fan: (1, 2, 3), flat", PixelAt(Pixels, 36, 1) == FColor(100, 200, 150, 0x80));
	TestTrue("Fan: (1, 3, 4) skipped", PixelAt(Pixels, 44, 4) == FColor(0, 0, 0, 0));
	TestTrue("Mirrored sprite: u = 16.25 - x, v = 16.25 - y", PixelAt(Pixels, 5, 10) == FColor(176, 192, 0x80, 0x80));
	TestTrue("STQ sprite: s = S / Q with the second Q", PixelAt(Pixels, 20, 10) == FColor(64, 32, 0x80, 0x80));
	TestTrue("Line: its start", PixelAt(Pixels, 33, 17) == FColor(0xff, 0xff, 0, 0x80));
	TestTrue("Line: its end left out", PixelAt(Pixels, 61, 22) == FColor(0, 0, 0, 0));
	TestTrue("Point: the nearest pixel", PixelAt(Pixels, 21, 19) == FColor(0xff, 0, 0xff, 0x80));
	TestTrue("Point: .5 rounds up", PixelAt(Pixels, 23, 27) == FColor(0xff, 0, 0xff, 0x80));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSReferenceBlendEquationTest, "System.GSReference.Pixel.BlendEquation",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSReferenceBlendEquationTest::RunTest(const FString& Parameters)
{
	// (A - B) * C >> 7 + D (3.8) of Cs (180, 60, 90) over Cd (40, 120, 200) with Ad 0x40 (row 0): negative products
	// shift toward minus infinity, C above 0x80 is not clamped, the result is clamped or wrapped by COLCLAMP (3.9.2).
	FGSCommandList List;
	GSConformance::BuildBlendEquation(List);
	const TArray<FColor> Pixels = Render(List);
	TestTrue("Cs, Cd, As, Cd", PixelAt(Pixels, 0, 0) == FColor(145, 75, 117, 0x60));
	TestTrue("Cd, Cs, FIX 0x80, 0: Cd - Cs clamped", PixelAt(Pixels, 8, 0) == FColor(0, 60, 110, 0x60));
	TestTrue("Cs, Cd, Ad, Cd", PixelAt(Pixels, 20, 0) == FColor(110, 90, 145, 0x60));
	TestTrue("FIX 0xff overshoots", PixelAt(Pixels, 24, 0) == FColor(255, 0, 0, 0x60));
	TestTrue("Cd - Cs wrapped", PixelAt(Pixels, 56, 0) == FColor(116, 60, 110, 0x60));
	TestTrue("As 0xff added, wrapped", PixelAt(Pixels, 60, 0) == FColor(142, 239, 123, 0xff));
	TestTrue("Ad 0: Cd", PixelAt(Pixels, 20, 24) == FColor(10, 250, 128, 0x60));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSReferencePabeFbaDateTest, "System.GSReference.Pixel.PabeFbaDate",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSReferencePabeFbaDateTest::RunTest(const FString& Parameters)
{
	// PABE blends only a source alpha with its MSB set (3.8.2); FBA sets the written alpha's MSB (3.9.3); DATE passes
	// the pixels whose alpha bit 7 is DATM (3.7.3); FBMSK keeps its bits of the destination (3.9.5).
	FGSCommandList List;
	GSConformance::BuildPabeFbaDate(List);
	const TArray<FColor> Pixels = Render(List);
	TestTrue("PABE, As 0x7f: not blended", PixelAt(Pixels, 1, 2) == FColor(60, 90, 30, 0x7f));
	TestTrue("PABE, As 0x80: added", PixelAt(Pixels, 5, 2) == FColor(160, 190, 130, 0x80));
	TestTrue("PABE, As 0xff: added x 255 / 128", PixelAt(Pixels, 9, 2) == FColor(219, 255, 159, 0xff));
	TestTrue("PABE, As 0: not blended", PixelAt(Pixels, 13, 2) == FColor(60, 90, 30, 0));
	TestTrue("PABE, FIX 0x40", PixelAt(Pixels, 5, 20) == FColor(175, 55, 95, 0x80));
	TestEqual("FBA: 0x10 -> 0x90", int32(PixelAt(Pixels, 18, 2).A), 0x90);
	TestTrue("FBA on a blend", PixelAt(Pixels, 18, 18) == FColor(115, 30, 45, 0xc0));
	TestEqual("FBA off", int32(PixelAt(Pixels, 18, 26).A), 0x10);
	TestTrue("DATM 0 over alpha 0: passes", PixelAt(Pixels, 25, 2) == FColor(0xff, 0, 0, 0x33));
	TestTrue("DATM 0 over 0x80: fails", PixelAt(Pixels, 29, 2) == FColor(50, 50, 50, 0x80));
	TestTrue("DATM 0 over 0x7f: passes", PixelAt(Pixels, 33, 2).R == 0xff);
	TestTrue("DATM 1 over 0: fails", PixelAt(Pixels, 25, 20) == FColor(50, 50, 50, 0));
	TestTrue("DATM 1 over 0xff: passes", PixelAt(Pixels, 37, 20) == FColor(0, 0xff, 0, 0x44));
	TestTrue("FBMSK 0x0f0ff00f", PixelAt(Pixels, 42, 2) == FColor(0xfa, 0xa0, 0xfc, 0x01));
	TestTrue("FBMSK 0xff over a blend", PixelAt(Pixels, 50, 2) == FColor(0x5a, 82, 157, 0x40));
	TestTrue("FBMSK 0x80000000", PixelAt(Pixels, 58, 2) == FColor(0x11, 0x22, 0x33, 0x80));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSReferenceDither16BlendTest, "System.GSReference.Pixel.Dither16Blend",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSReferenceDither16BlendTest::RunTest(const FString& Parameters)
{
	// A 16-bit frame: the blend reads the 16-bit destination (channels shifted left 3, alpha 0 or 0x80), then the
	// result is dithered, clamped or wrapped and cut to 5 bits (3.8.1, 3.9); FBMSK counts on the 16-bit pixel's bits
	// (3.9.5).
	FGSCommandList List;
	GSConformance::BuildDither16Blend(List);
	const TArray<FColor> Pixels = Render(List, EGSPixelFormat::PSMCT16S);
	TestTrue("The ramp dithered -4", PixelAt(Pixels, 0, 0) == FColor(0, 96, 192, 0x80));
	TestTrue("The ramp dithered +2", PixelAt(Pixels, 1, 0) == FColor(0, 96, 200, 0x80));
	TestTrue("Blended over the 16-bit pixel, dithered", PixelAt(Pixels, 0, 8) == FColor(152, 40, 136, 0));
	TestTrue("Blended, not dithered", PixelAt(Pixels, 0, 16) == FColor(152, 48, 144, 0));
	TestTrue("Added, dithered, wrapped", PixelAt(Pixels, 0, 20) == FColor(192, 32, 192, 0x80));
	TestTrue("Ad of alpha bit 0: Cd", PixelAt(Pixels, 40, 24) == FColor(72, 128, 8, 0));
	TestTrue("FBMSK over the 5-bit channels", PixelAt(Pixels, 0, 28) == FColor(24, 248, 192, 0x80));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSReferenceClutLoadsTest, "System.GSReference.Texture.ClutLoads",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSReferenceClutLoadsTest::RunTest(const FString& Parameters)
{
	// CLD (3.4.7): 1 loads, 2 / 3 load and set CBP0 / CBP1, 4 / 5 load only when CBP0 / CBP1 differs from CBP, 0 keeps
	// the temporary buffer; entry 16 of CLUT A is (16, 0, 0x20), of B (0x20, 16, 0), of C (0, 0x20, 16). Two PSMT4
	// CLUTs sit at CSA 0 and 1 together.
	FGSCommandList List;
	GSConformance::BuildClutLoads(List);
	const TArray<FColor> Pixels = Render(List);
	const FColor A(16, 0, 0x20, 0x80);
	const FColor B(0x20, 16, 0, 0x80);
	const FColor C(0, 0x20, 16, 0x80);
	TestTrue("CLD 2: A", PixelAt(Pixels, 6, 1) == A);
	TestTrue("CLD 4, CBP0 differs: B", PixelAt(Pixels, 6, 4) == B);
	TestTrue("CLD 4, CBP0 equal: B kept over C's upload", PixelAt(Pixels, 6, 7) == B);
	TestTrue("CLD 1: C", PixelAt(Pixels, 6, 10) == C);
	TestTrue("CLD 0: C kept", PixelAt(Pixels, 6, 13) == C);
	TestTrue("CLD 3: A", PixelAt(Pixels, 6, 16) == A);
	TestTrue("CLD 5, CBP1 equal: C kept", PixelAt(Pixels, 6, 19) == C);
	TestTrue("CLD 5, CBP1 differs: A", PixelAt(Pixels, 6, 22) == A);
	TestTrue("PSMT4 at CSA 0", PixelAt(Pixels, 6, 25) == FColor(16, 0x80, 0, 0x80));
	TestTrue("PSMT4 at CSA 1", PixelAt(Pixels, 6, 29) == FColor(0, 16, 0x80, 0x80));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
