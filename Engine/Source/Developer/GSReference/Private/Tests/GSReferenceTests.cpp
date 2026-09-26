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
	Buf4.DBW = 1;
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
	TestEqual("32 bits, little endian, at (4, 1)", Memory.ReadPixel(0, 64, EGSPixelFormat::PSMCT32, 4, 1), 0x03020100u);
	TestEqual("The second row", Memory.ReadPixel(0, 64, EGSPixelFormat::PSMCT32, 5, 2), 0x27262524u);
	TestEqual("4 bits: low nibble first", Memory.ReadPixel(64 * 64, 64, EGSPixelFormat::PSMT4, 0, 0), 1u);
	TestEqual("4 bits: then the high nibble", Memory.ReadPixel(64 * 64, 64, EGSPixelFormat::PSMT4, 1, 0), 2u);
	TestEqual("24 bits packed", Memory.ReadPixel(128 * 64, 64, EGSPixelFormat::PSMCT24, 1, 0) & 0xffffffu, 0x858483u);
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

#endif // WITH_DEV_AUTOMATION_TESTS
