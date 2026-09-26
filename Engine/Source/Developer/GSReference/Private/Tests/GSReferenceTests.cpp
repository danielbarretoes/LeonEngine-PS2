#include "CoreMinimal.h"
#include "GSCommandList.h"
#include "GSReferenceRasterizer.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// The reference rasterizer against the GS User's Manual's rules, one area per test (Docs/PLANS/ps2-gs-parity.md, P2).
// Every scene draws into a 64 x 32 frame buffer at FBP 0 (FBW 1) with window coordinates equal to the primitive's.

namespace
{

	constexpr uint32 FrameWidth = 64;
	constexpr uint32 FrameHeight = 32;

	FGSFrame MakeFrame(EGSPixelFormat Format = EGSPixelFormat::PSMCT32)
	{
		FGSFrame Frame;
		Frame.FBP = 0;
		Frame.FBW = 1;
		Frame.PSM = Format;
		return Frame;
	}

	FGSZBuf MakeZBuf(bool bMask = true)
	{
		FGSZBuf ZBuf;
		ZBuf.ZBP = 32;
		ZBuf.PSM = EGSPixelFormat::PSMZ32;
		ZBuf.bMask = bMask;
		return ZBuf;
	}

	/** The environment every scene starts with: frame, Z buffer, no offset, the whole frame as scissor, Z ALWAYS. */
	void SetUpScene(FGSCommandList& List, EGSPixelFormat Format = EGSPixelFormat::PSMCT32, bool bZMask = true)
	{
		List.SetPrimModeFromPrim();
		List.SetFrame(0, MakeFrame(Format));
		List.SetZBuf(0, MakeZBuf(bZMask));
		List.SetXYOffset(0, FGSXYOffset());
		FGSScissor Scissor;
		Scissor.SCAX1 = FrameWidth - 1;
		Scissor.SCAY1 = FrameHeight - 1;
		List.SetScissor(0, Scissor);
		List.SetTest(0, FGSTest());
		List.SetColorClamp(true);
	}

	FGSXYZ Vertex(float X, float Y, uint32 Z = 0)
	{
		FGSXYZ Result;
		Result.X = GSToFixed4(X, 16);
		Result.Y = GSToFixed4(Y, 16);
		Result.Z = Z;
		return Result;
	}

	FGSRGBAQ Color(uint8 R, uint8 G, uint8 B, uint8 A = 0x80)
	{
		FGSRGBAQ Result;
		Result.R = R;
		Result.G = G;
		Result.B = B;
		Result.A = A;
		return Result;
	}

	FGSPrim Primitive(EGSPrimitive Type)
	{
		FGSPrim Prim;
		Prim.Type = Type;
		return Prim;
	}

	/** An untextured sprite over the pixels [X0, X1) x [Y0, Y1). */
	void AddSprite(FGSCommandList& List, float X0, float Y0, float X1, float Y1, const FGSRGBAQ& SpriteColor,
		const FGSPrim& Prim = Primitive(EGSPrimitive::Sprite), uint32 Z = 0)
	{
		List.SetPrim(Prim);
		List.SetRGBAQ(SpriteColor);
		List.AddVertex(Vertex(X0, Y0, Z));
		List.AddVertex(Vertex(X1, Y1, Z));
	}

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

	/** Uploads Texels (PSMCT32, row by row) at TBP of a texture 64 pixels wide. */
	void UploadTexture32(FGSCommandList& List, uint16 TBP, uint16 Width, uint16 Height, const TArray<uint32>& Texels)
	{
		TArray<uint8> Bytes;
		for (const uint32 Texel : Texels)
		{
			for (uint32 Byte = 0; Byte < 4; ++Byte)
			{
				Bytes.Add(uint8(Texel >> (Byte * 8)));
			}
		}
		FGSBitBltBuf Destination;
		Destination.DBP = TBP;
		Destination.DBW = 1;
		Destination.DPSM = EGSPixelFormat::PSMCT32;
		List.UploadImage(Destination, 0, 0, Width, Height, Bytes);
		List.TexFlush();
	}

	/** Additive blending with the source alpha at 0x80 (1.0): each draw adds its color once. */
	void UseAdditive(FGSCommandList& List)
	{
		List.SetAlpha(0, FGSAlpha::Additive());
	}

	FGSPrim Blended(EGSPrimitive Type)
	{
		FGSPrim Prim = Primitive(Type);
		Prim.bAlphaBlend = true;
		return Prim;
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
	SetUpScene(List);
	UseAdditive(List);
	List.SetPrim(Blended(EGSPrimitive::Triangle));
	List.SetRGBAQ(Color(0x10, 0x10, 0x10));
	List.AddVertex(Vertex(0, 0));
	List.AddVertex(Vertex(8, 0));
	List.AddVertex(Vertex(8, 8));
	List.AddVertex(Vertex(0, 0));
	List.AddVertex(Vertex(8, 8));
	List.AddVertex(Vertex(0, 8));
	AddSprite(List, 20, 2, 24, 5, Color(0xff, 0, 0));
	// A one-pixel triangle: only the pixel on its top-left corner.
	List.SetPrim(Primitive(EGSPrimitive::Triangle));
	List.SetRGBAQ(Color(0, 0xff, 0));
	List.AddVertex(Vertex(30, 10));
	List.AddVertex(Vertex(31, 10));
	List.AddVertex(Vertex(30, 11));
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
	SetUpScene(List);
	UseAdditive(List);
	List.SetRGBAQ(Color(0x10, 0, 0));
	List.SetPrim(Blended(EGSPrimitive::TriangleStrip));
	List.AddVertexNoKick(Vertex(0, 0));
	List.AddVertexNoKick(Vertex(4, 0));
	List.AddVertex(Vertex(0, 4));
	List.AddVertex(Vertex(4, 4));
	List.SetPrim(Blended(EGSPrimitive::TriangleFan));
	List.AddVertex(Vertex(10, 0));
	List.AddVertex(Vertex(14, 0));
	List.AddVertex(Vertex(14, 4));
	List.AddVertex(Vertex(10, 4));
	List.SetPrim(Primitive(EGSPrimitive::Point));
	List.SetRGBAQ(Color(0, 0xff, 0));
	List.AddVertex(Vertex(20.4f, 3.6f));
	List.SetPrim(Primitive(EGSPrimitive::Line));
	List.SetRGBAQ(Color(0, 0, 0xff));
	List.AddVertex(Vertex(30, 1));
	List.AddVertex(Vertex(34, 1));
	// Flat: the second triangle's color is its kicking vertex's.
	List.SetPrim(Primitive(EGSPrimitive::Triangle));
	List.SetRGBAQ(Color(0xff, 0, 0));
	List.AddVertex(Vertex(40, 0));
	List.AddVertex(Vertex(48, 0));
	List.SetRGBAQ(Color(0, 0, 0x40));
	List.AddVertex(Vertex(40, 8));
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
	SetUpScene(List);
	FGSPrim Gouraud = Primitive(EGSPrimitive::Triangle);
	Gouraud.bGouraud = true;
	List.SetPrim(Gouraud);
	List.SetRGBAQ(Color(0, 0, 0));
	List.AddVertex(Vertex(0, 0));
	List.SetRGBAQ(Color(200, 0, 0));
	List.AddVertex(Vertex(20, 0));
	List.SetRGBAQ(Color(0, 0, 0));
	List.AddVertex(Vertex(0, 20));
	FGSScissor Scissor;
	Scissor.SCAX0 = 40;
	Scissor.SCAX1 = 42;
	Scissor.SCAY0 = 1;
	Scissor.SCAY1 = 2;
	List.SetScissor(0, Scissor);
	AddSprite(List, 30, 0, 60, 10, Color(0, 0xff, 0));
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
	SetUpScene(List);
	TArray<uint32> Texels;
	for (uint32 Row = 0; Row < 2; ++Row)
	{
		for (uint32 Column = 0; Column < 8; ++Column)
		{
			Texels.Add((Column * 32) | 0x80000000u);
		}
	}
	UploadTexture32(List, 64, 8, 2, Texels);
	FGSTex0 Tex0;
	Tex0.TBP0 = 64;
	Tex0.TBW = 1;
	Tex0.PSM = EGSPixelFormat::PSMCT32;
	Tex0.TW = 3;
	Tex0.TH = 1;
	Tex0.TFX = EGSTextureFunction::Decal;
	List.SetTex0(0, Tex0);
	FGSPrim Textured = Primitive(EGSPrimitive::Sprite);
	Textured.bTextured = true;
	Textured.bUseUV = true;

	const auto AddTexturedRow = [&List, &Textured](float Y, float U0, float U1)
	{
		List.SetPrim(Textured);
		FGSUV UV;
		UV.U = GSToFixed4(U0, 14);
		UV.V = GSToFixed4(0.5f, 14);
		List.SetUV(UV);
		List.AddVertex(Vertex(0, Y));
		UV.U = GSToFixed4(U1, 14);
		List.SetUV(UV);
		List.AddVertex(Vertex(16, Y + 1));
	};
	// Row 0: point sampling, u = x at pixel x; row 1: u = x + 16 with REPEAT; row 2: CLAMP; row 3: bilinear.
	List.SetTex1(0, FGSTex1());
	AddTexturedRow(0, 0, 16);
	AddTexturedRow(1, 16, 32);
	FGSClamp Clamp;
	Clamp.WMS = EGSWrapMode::Clamp;
	List.SetClamp(0, Clamp);
	AddTexturedRow(2, 0, 16);
	List.SetClamp(0, FGSClamp());
	FGSTex1 Bilinear;
	Bilinear.MMAG = EGSFilter::Linear;
	List.SetTex1(0, Bilinear);
	AddTexturedRow(3, 0, 16);
	FGSClamp RegionRepeat;
	RegionRepeat.WMS = EGSWrapMode::RegionRepeat;
	RegionRepeat.MINU = 1;
	RegionRepeat.MAXU = 4;
	List.SetTex1(0, FGSTex1());
	List.SetClamp(0, RegionRepeat);
	AddTexturedRow(4, 0, 16);
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
	SetUpScene(List);

	// The 8-bit CLUT at CBP 128: entry 8 sits at (0, 1), entry 16 at (8, 0).
	TArray<uint32> Clut8;
	Clut8.SetNumZeroed(16 * 16);
	Clut8[(1 * 16) + 0] = 0x800000ffu;
	Clut8[(0 * 16) + 8] = 0x8000ff00u;
	UploadTexture32(List, 128, 16, 16, Clut8);
	TArray<uint8> Indices8 = {8, 16, 8, 16, 8, 16, 8, 16, 8, 16, 8, 16, 8, 16, 8, 16};
	FGSBitBltBuf Buf8;
	Buf8.DBP = 192;
	Buf8.DBW = 1;
	Buf8.DPSM = EGSPixelFormat::PSMT8;
	List.UploadImage(Buf8, 0, 0, 8, 2, Indices8);

	// The 4-bit CLUT at CBP 256 (8 x 2): entry 1 blue.
	TArray<uint32> Clut4;
	Clut4.SetNumZeroed(8 * 2);
	Clut4[1] = 0x80ff0000u;
	UploadTexture32(List, 256, 8, 2, Clut4);
	TArray<uint8> Indices4;
	for (uint32 Index = 0; Index < 16; ++Index)
	{
		Indices4.Add(0x11);
	}
	FGSBitBltBuf Buf4;
	Buf4.DBP = 320;
	Buf4.DBW = 1;
	Buf4.DPSM = EGSPixelFormat::PSMT4;
	List.UploadImage(Buf4, 0, 0, 16, 2, Indices4);

	// A 16-bit texture: white with A = 0, white with A = 1, black with A = 0 (x 2 rows).
	TArray<uint8> Texels16;
	for (uint32 Row = 0; Row < 2; ++Row)
	{
		for (const uint16 Texel : {uint16(0x7fff), uint16(0xffff), uint16(0x0000), uint16(0x0000)})
		{
			Texels16.Add(uint8(Texel));
			Texels16.Add(uint8(Texel >> 8));
		}
	}
	FGSBitBltBuf Buf16;
	Buf16.DBP = 384;
	Buf16.DBW = 1;
	Buf16.DPSM = EGSPixelFormat::PSMCT16;
	List.UploadImage(Buf16, 0, 0, 4, 2, Texels16);
	List.TexFlush();
	FGSTexA TexA;
	TexA.TA0 = 0x20;
	TexA.TA1 = 0x60;
	TexA.bAlphaExpandBlack = true;
	List.SetTexA(TexA);
	List.SetTex1(0, FGSTex1());

	FGSPrim Textured = Primitive(EGSPrimitive::Sprite);
	Textured.bTextured = true;
	Textured.bUseUV = true;
	const auto AddTexturedRow = [&List, &Textured](const FGSTex0& Tex0, float Y)
	{
		List.SetTex0(0, Tex0);
		List.SetPrim(Textured);
		FGSUV UV;
		UV.V = GSToFixed4(0.5f, 14);
		List.SetUV(UV);
		List.AddVertex(Vertex(0, Y));
		UV.U = GSToFixed4(8, 14);
		List.SetUV(UV);
		List.AddVertex(Vertex(8, Y + 1));
	};
	FGSTex0 Tex8;
	Tex8.TBP0 = 192;
	Tex8.TBW = 1;
	Tex8.PSM = EGSPixelFormat::PSMT8;
	Tex8.TW = 3;
	Tex8.TH = 1;
	Tex8.TFX = EGSTextureFunction::Decal;
	Tex8.CBP = 128;
	Tex8.CLD = 1;
	AddTexturedRow(Tex8, 0);
	FGSTex0 Tex4 = Tex8;
	Tex4.TBP0 = 320;
	Tex4.PSM = EGSPixelFormat::PSMT4;
	Tex4.TW = 4;
	Tex4.CBP = 256;
	Tex4.CSA = 1;
	AddTexturedRow(Tex4, 1);
	FGSTex0 Tex16 = Tex8;
	Tex16.TBP0 = 384;
	Tex16.PSM = EGSPixelFormat::PSMCT16;
	Tex16.TW = 2;
	Tex16.CLD = 0;
	List.SetTex0(0, Tex16);
	List.SetPrim(Textured);
	FGSUV UV16;
	UV16.V = GSToFixed4(0.5f, 14);
	List.SetUV(UV16);
	List.AddVertex(Vertex(0, 2));
	UV16.U = GSToFixed4(4, 14);
	List.SetUV(UV16);
	List.AddVertex(Vertex(4, 3));

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
	SetUpScene(List);
	TArray<uint32> Level0;
	Level0.Init(0x80c8c8c8u, 8 * 8);
	UploadTexture32(List, 64, 8, 8, Level0);
	TArray<uint32> Level1;
	Level1.Init(0x80000064u, 4 * 4);
	UploadTexture32(List, 128, 4, 4, Level1);
	FGSTex0 Tex0;
	Tex0.TBP0 = 64;
	Tex0.TBW = 1;
	Tex0.TW = 3;
	Tex0.TH = 3;
	FGSPrim Textured = Primitive(EGSPrimitive::Sprite);
	Textured.bTextured = true;
	Textured.bUseUV = true;

	const auto AddTexturedPixel = [&List, &Textured](float X, const FGSRGBAQ& Fragment)
	{
		List.SetPrim(Textured);
		List.SetRGBAQ(Fragment);
		FGSUV UV;
		UV.U = GSToFixed4(1, 14);
		UV.V = GSToFixed4(1, 14);
		List.SetUV(UV);
		List.AddVertex(Vertex(X, 0));
		List.AddVertex(Vertex(X + 1, 1));
	};
	List.SetTex1(0, FGSTex1());
	List.SetTex0(0, Tex0);
	AddTexturedPixel(0, Color(0x80, 0x80, 0x80));
	AddTexturedPixel(1, Color(0x40, 0x40, 0x40));
	FGSTex0 Highlight = Tex0;
	Highlight.TFX = EGSTextureFunction::Highlight;
	List.SetTex0(0, Highlight);
	AddTexturedPixel(2, Color(0x40, 0x40, 0x40, 0x10));

	// Fog: F = 0x80, FOGCOL black.
	List.SetFogCol(FGSFogCol());
	FGSPrim Fogged = Primitive(EGSPrimitive::Sprite);
	Fogged.bFog = true;
	List.SetPrim(Fogged);
	List.SetRGBAQ(Color(200, 200, 200));
	FGSXYZF Corner;
	Corner.X = GSToFixed4(4, 16);
	Corner.Y = 0;
	Corner.F = 0x80;
	List.AddVertex(Corner);
	Corner.X = GSToFixed4(5, 16);
	Corner.Y = GSToFixed4(1, 16);
	List.AddVertex(Corner);

	// MIPMAP: LCM = 1, K = 1.0, level 1 at TBP 128.
	FGSTex1 Mip;
	Mip.bFixedLOD = true;
	Mip.K = 16;
	Mip.MXL = 1;
	Mip.MMIN = EGSFilter::NearestMipmapNearest;
	List.SetTex1(0, Mip);
	FGSMipTbp MipTbp;
	MipTbp.TBP[0] = 128;
	MipTbp.TBW[0] = 1;
	List.SetMipTbp1(0, MipTbp);
	FGSTex0 Decal = Tex0;
	Decal.TFX = EGSTextureFunction::Decal;
	List.SetTex0(0, Decal);
	AddTexturedPixel(6, Color(0x80, 0x80, 0x80));
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
	SetUpScene(List, EGSPixelFormat::PSMCT32, false);
	AddSprite(List, 0, 0, 8, 1, Color(0x10, 0, 0), Primitive(EGSPrimitive::Sprite), 100);
	FGSTest AlphaKeep;
	AlphaKeep.bAlphaTest = true;
	AlphaKeep.ATST = EGSAlphaTest::GreaterEqual;
	AlphaKeep.AREF = 0x80;
	AlphaKeep.AFAIL = EGSAlphaFail::Keep;
	List.SetTest(0, AlphaKeep);
	AddSprite(List, 0, 0, 2, 1, Color(0xff, 0, 0, 0x40), Primitive(EGSPrimitive::Sprite), 200);
	FGSTest AlphaFrameOnly = AlphaKeep;
	AlphaFrameOnly.AFAIL = EGSAlphaFail::FrameBufferOnly;
	List.SetTest(0, AlphaFrameOnly);
	AddSprite(List, 2, 0, 4, 1, Color(0xff, 0, 0, 0x40), Primitive(EGSPrimitive::Sprite), 200);
	FGSTest DepthGEqual;
	DepthGEqual.ZTST = EGSDepthTest::GreaterEqual;
	List.SetTest(0, DepthGEqual);
	AddSprite(List, 4, 0, 5, 1, Color(0, 0xff, 0), Primitive(EGSPrimitive::Sprite), 100);
	AddSprite(List, 5, 0, 6, 1, Color(0, 0xff, 0), Primitive(EGSPrimitive::Sprite), 99);
	FGSTest DepthGreater;
	DepthGreater.ZTST = EGSDepthTest::Greater;
	List.SetTest(0, DepthGreater);
	AddSprite(List, 6, 0, 7, 1, Color(0, 0, 0xff), Primitive(EGSPrimitive::Sprite), 100);

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
	SetUpScene(List);
	AddSprite(List, 0, 0, 8, 1, Color(100, 100, 100));
	List.SetAlpha(0, FGSAlpha::Translucent());
	AddSprite(List, 0, 0, 1, 1, Color(200, 200, 200, 0x40), Blended(EGSPrimitive::Sprite));
	List.SetAlpha(0, FGSAlpha::Additive());
	AddSprite(List, 1, 0, 2, 1, Color(200, 200, 200, 0x80), Blended(EGSPrimitive::Sprite));
	List.SetColorClamp(false);
	AddSprite(List, 2, 0, 3, 1, Color(200, 200, 200, 0x80), Blended(EGSPrimitive::Sprite));
	List.SetColorClamp(true);
	List.SetPixelAlphaBlend(true);
	AddSprite(List, 3, 0, 4, 1, Color(50, 50, 50, 0x40), Blended(EGSPrimitive::Sprite));
	List.SetPixelAlphaBlend(false);
	List.SetFba(0, true);
	AddSprite(List, 4, 0, 5, 1, Color(10, 10, 10, 0x10));
	List.SetFba(0, false);
	FGSFrame Masked = MakeFrame();
	Masked.FBMSK = 0x0000ff00u;
	List.SetFrame(0, Masked);
	AddSprite(List, 5, 0, 6, 1, Color(1, 2, 3));
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
	SetUpScene(List, EGSPixelFormat::PSMCT16S);
	List.SetDimx(FGSDimx::Default());
	List.SetDither(true);
	AddSprite(List, 0, 0, 2, 1, Color(103, 103, 103));
	List.SetDither(false);
	AddSprite(List, 0, 1, 2, 2, Color(103, 103, 103));
	const TArray<FColor> Pixels = Render(List, EGSPixelFormat::PSMCT16S);

	TestEqual("Dithered -4", int32(PixelAt(Pixels, 0, 0).R), 96);
	TestEqual("Dithered +2", int32(PixelAt(Pixels, 1, 0).R), 104);
	TestEqual("Not dithered", int32(PixelAt(Pixels, 1, 1).R), 96);
	TestEqual("The 16-bit alpha bit", int32(PixelAt(Pixels, 0, 0).A), 0x80);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
