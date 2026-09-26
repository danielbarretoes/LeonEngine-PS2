#include "GSConformanceScenes.h"

namespace
{

	/**
	 * The environment every scene starts with: frame, Z buffer, no offset, the whole frame as scissor, Z ALWAYS and
	 * the common registers the scenes change back at their defaults. The frame and the Z buffer are cleared to 0 (the
	 * reference's memory starts that way, the GS's holds the previous scene).
	 */
	void SetUpScene(FGSCommandList& List, EGSPixelFormat Format = EGSPixelFormat::PSMCT32, bool bZMask = true)
	{
		List.SetPrimModeFromPrim();
		List.SetFrame(0, GSConformance::MakeFrame(Format));
		List.SetZBuf(0, GSConformance::MakeZBuf(false));
		List.SetXYOffset(0, FGSXYOffset());
		FGSScissor Scissor;
		Scissor.SCAX1 = GSConformance::FrameWidth - 1;
		Scissor.SCAY1 = GSConformance::FrameHeight - 1;
		List.SetScissor(0, Scissor);
		List.SetTest(0, FGSTest());
		List.SetAlpha(0, FGSAlpha());
		List.SetFba(0, false);
		List.SetTex1(0, FGSTex1());
		List.SetClamp(0, FGSClamp());
		List.SetColorClamp(true);
		List.SetDither(false);
		List.SetPixelAlphaBlend(false);
		List.SetTexA(FGSTexA());

		FGSPrim Clear;
		Clear.Type = EGSPrimitive::Sprite;
		List.SetPrim(Clear);
		List.SetRGBAQ(FGSRGBAQ{0, 0, 0, 0});
		List.AddVertex(FGSXYZ{0, 0, 0});
		List.AddVertex(FGSXYZ{
			GSToFixed4(float(GSConformance::FrameWidth), 16), GSToFixed4(float(GSConformance::FrameHeight), 16), 0});
		List.SetZBuf(0, GSConformance::MakeZBuf(bZMask));
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

FGSFrame GSConformance::MakeFrame(EGSPixelFormat Format)
{
	FGSFrame Frame;
	Frame.FBP = 0;
	Frame.FBW = 1;
	Frame.PSM = Format;
	return Frame;
}

FGSZBuf GSConformance::MakeZBuf(bool bMask)
{
	FGSZBuf ZBuf;
	ZBuf.ZBP = 32;
	ZBuf.PSM = EGSPixelFormat::PSMZ32;
	ZBuf.bMask = bMask;
	return ZBuf;
}

void GSConformance::BuildDrawingRules(FGSCommandList& List)
{
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
}

void GSConformance::BuildPrimitives(FGSCommandList& List)
{
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
}

void GSConformance::BuildGouraudAndScissor(FGSCommandList& List)
{
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
}

void GSConformance::BuildTextureSampling(FGSCommandList& List)
{
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
}

void GSConformance::BuildClutAndFormats(FGSCommandList& List)
{
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
}

void GSConformance::BuildFunctionsFogAndMipmap(FGSCommandList& List)
{
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
}

void GSConformance::BuildPixelTests(FGSCommandList& List)
{
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
}

void GSConformance::BuildBlendAndWrite(FGSCommandList& List)
{
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
}

void GSConformance::BuildDither16(FGSCommandList& List)
{
	SetUpScene(List, EGSPixelFormat::PSMCT16S);
	List.SetDimx(FGSDimx::Default());
	List.SetDither(true);
	AddSprite(List, 0, 0, 2, 1, Color(103, 103, 103));
	List.SetDither(false);
	AddSprite(List, 0, 1, 2, 2, Color(103, 103, 103));
}

TArrayView<const FGSConformanceScene> GSConformance::GetScenes()
{
	static const FGSConformanceScene Scenes[] = {
		{"DrawingRules", EGSPixelFormat::PSMCT32, &BuildDrawingRules},
		{"Primitives", EGSPixelFormat::PSMCT32, &BuildPrimitives},
		{"GouraudAndScissor", EGSPixelFormat::PSMCT32, &BuildGouraudAndScissor},
		{"TextureSampling", EGSPixelFormat::PSMCT32, &BuildTextureSampling},
		{"ClutAndFormats", EGSPixelFormat::PSMCT32, &BuildClutAndFormats},
		{"FunctionsFogAndMipmap", EGSPixelFormat::PSMCT32, &BuildFunctionsFogAndMipmap},
		{"PixelTests", EGSPixelFormat::PSMCT32, &BuildPixelTests},
		{"BlendAndWrite", EGSPixelFormat::PSMCT32, &BuildBlendAndWrite},
		{"Dither16", EGSPixelFormat::PSMCT16S, &BuildDither16},
	};
	return MakeArrayView(Scenes, UE_ARRAY_COUNT(Scenes));
}
